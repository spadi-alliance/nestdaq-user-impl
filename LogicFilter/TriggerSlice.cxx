/*
 * TriggerSlice.cxx
 *
 *
 * -------------------------------------
 * Trigger.cxx keeps one uint32_t per time bin and one bit per signal, so the
 * signal count cannot pass 32 and the scan needs a 2^N truth table.
 * Here the axes are swapped: one bitmap per signal, one bit per time bin.
 *
 *   fSlice[sig * fNword + w] , bit j  ==  signal sig at time bin 32*w + j
 *
 * -------------------------------------
 * The file is split by __CUDACC__ into two halves that never overlap:
 *
 *   __CUDACC__ defined    the kernels and the TriggerSliceGpu implementation.
 *                         Only this half is compiled, so including the file in
 *                         a device source does not duplicate the CPU class.
 *   __CUDACC__ undefined  the Trigger class (CPU engine). When USE_CUDA is not
 *                         defined either, TriggerSliceGpu is also given stubs
 *                         that report "no GPU", so a plain g++ build of
 *                         LogicFilterSlice.cxx needs no CUDA at all.
 *
 *   CUDA off:  g++ -c LogicFilterSlice.cxx
 *   CUDA on :  g++ -DUSE_CUDA -c LogicFilterSlice.cxx
 *              nvcc -DUSE_CUDA -x cu -c TriggerSlice.cxx
 */

#ifndef TriggerSlice_cxx
#define TriggerSlice_cxx

#include <cstdint>
#include <string>
#include <vector>

// Opcodes of the compiled expression. Shared by both halves of this file.
enum TriggerSliceOp {
	kTsOpSig = 0,
	kTsOpAnd,
	kTsOpOr,
	kTsOpNot,
	kTsOpSwap,
	kTsOpDrop,
};

// Expression stack depth of the GPU evaluator. A formula that needs more is
// rejected before the GPU is used, so that both backends agree.
#define TRIGGERSLICE_GPU_STACK 64
#define TRIGGERSLICE_MAX_CH 256

/*
 * GPU backend interface. Declared in both halves: the CPU class calls it, the
 * __CUDACC__ half (or the no-CUDA stub below) defines it.
 */
namespace TriggerSliceGpu {

// A CUDA device is present and usable.
bool Available();

// Allocates the sliced region and the scratch buffers.
bool Init(uint32_t nword, int nsignal, uint32_t timeRegionSize);
void Release();

// Uploads the per-FEM signal table, the RPN expression, and the flattened
// AND/OR tree (cCode/cN). ncop == 0 keeps the RPN interpreter.
bool SetConfig(const int32_t *entOff, const int32_t *entCnt, int nslot,
	const int32_t *entCh, const int32_t *entDelay, const int32_t *entSig, int nent,
	const int32_t *opCode, const int32_t *opSig, int nop, int skipZero,
	const int32_t *cCode, const int32_t *cN, int ncop);

// Marks and scans one heartbeat. segData[i] holds segNword[i] raw 64-bit words
// of the FEM in slot segSlot[i], unpacked as segFemType[i].
bool MarkScan(const uint64_t * const *segData, const int32_t *segNword,
	const int32_t *segSlot, const uint32_t *segFemType, int nseg,
	int markLen, std::vector<uint32_t> &hitsOut);

// Same work as MarkScan for B heartbeats in one submission. hitsPerHbf[h] is
// the rising-edge list of heartbeat h, in ascending time.
bool MarkScanBatch(int B,
	const uint64_t * const *segData, const int32_t *segNword,
	const int32_t *segSlot, const uint32_t *segFemType, const int32_t *segHbf,
	int nseg, int markLen, std::vector<std::vector<uint32_t>> &hitsPerHbf);

const char *LastError();

}  // namespace TriggerSliceGpu


#ifdef __CUDACC__
/* =======================================================================
 *  GPU half: compiled by nvcc only.
 * ======================================================================= */

#include <cuda_runtime.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "SubTimeFrameHeader.h"  // SubTimeFrame::TDC* constants

namespace {

// ---- device unpackers, the bit fields of UnpackTdc.h ----
struct TsTdc {
	int ch;
	int tdc4n;
};

// leading-edge TDC word, the only type Trigger::Mark marks
__device__ __forceinline__ int tsTdcType(uint64_t d)
{
	return static_cast<int>((d & 0xfc00000000000000ull) >> 58);
}
static constexpr int kTsTdcLeading = ((0x2cU) >> 2);

__device__ __forceinline__ bool tsUnpack(uint64_t d, uint32_t femType, TsTdc *o)
{
	if (tsTdcType(d) != kTsTdcLeading) return false;

	if ((femType == SubTimeFrame::TDC64H) || (femType == SubTimeFrame::TDC64H_V3)) {
		o->ch = static_cast<int>((d & 0x03f8000000000000ull) >> 51);
		o->tdc4n = static_cast<int>((d & 0x000000001fffffffull) >> 12);
		return true;
	}
	if (femType == SubTimeFrame::TDC64L) {
		o->ch = static_cast<int>((d & 0x03f8000000000000ull) >> 51);
		o->tdc4n = static_cast<int>((d & 0x00000007fffc0000ull) >> 18);
		return true;
	}
	if (femType == SubTimeFrame::TDC64L_V3) {
		o->ch = static_cast<int>((d & 0x03fc000000000000ull) >> 50);
		o->tdc4n = static_cast<int>((d & 0x00000003fffe0000ull) >> 17);
		return true;
	}

	return false;
}

// ---- mark: one thread per raw word of one segment (blockIdx.y) ----
static constexpr int kTsMaxCh = TRIGGERSLICE_MAX_CH;

__device__ __forceinline__ void tsOrWindow(uint32_t *base, uint32_t nword,
	uint32_t *marked, uint32_t blo, uint32_t bhi)
{
	const uint32_t wl = blo / 32;
	const uint32_t wh = bhi / 32;

	if (wl == wh) {
		const uint32_t nbit = bhi - blo + 1;
		const uint32_t mask = (nbit >= 32)
			? 0xffffffffu
			: (((1u << nbit) - 1u) << (blo % 32));
		atomicOr(&base[wl], mask);
	} else {
		atomicOr(&base[wl], 0xffffffffu << (blo % 32));
		for (uint32_t x = wl + 1 ; x < wh ; x++) atomicOr(&base[x], 0xffffffffu);
		const uint32_t tail = bhi % 32;
		atomicOr(&base[wh], (tail == 31) ? 0xffffffffu : ((1u << (tail + 1)) - 1u));
	}

	if (marked != nullptr) {
		const uint32_t dhi = (wh < nword) ? wh : (nword - 1);
		for (uint32_t x = wl ; x <= dhi ; x++) {
			atomicOr(&marked[x >> 5], 1u << (x & 31));
		}
	}
}

__global__ void tsMarkKernel(uint32_t *slice, uint32_t *marked, uint32_t nword, uint32_t regionSize,
	int nsignal, int B,
	const uint64_t *words,
	const int32_t *segOff, const int32_t *segNword, const int32_t *segSlot,
	const int32_t *segHbf, const uint32_t *segFemType,
	const int32_t *chHead, const int32_t *chCnt,
	const int32_t *entOff, const int32_t *entCnt,
	const int32_t *entCh, const int32_t *entDelay, const int32_t *entSig,
	int half)
{
	const int s = static_cast<int>(blockIdx.y);
	const uint32_t wlocal = blockIdx.x * blockDim.x + threadIdx.x;
	const int n = segNword[s];
	if (wlocal >= static_cast<uint32_t>(n)) return;

	const int slot = segSlot[s];
	if (slot < 0) return;
	const int h = segHbf[s];
	if (h < 0 || h >= B) return;

	TsTdc tdc;
	if (! tsUnpack(words[static_cast<uint32_t>(segOff[s]) + wlocal], segFemType[s], &tdc)) {
		return;
	}

	const uint32_t packs = (nword + 31) / 32;
	uint32_t *slice_h = slice + static_cast<size_t>(h) * nsignal * nword;
	uint32_t *marked_h = marked + static_cast<size_t>(h) * packs;

	int e0 = -1;
	int e1 = -1;
	if (tdc.ch >= 0 && tdc.ch < kTsMaxCh && chHead != nullptr) {
		const int idx = slot * kTsMaxCh + tdc.ch;
		e0 = chHead[idx];
		e1 = e0 + chCnt[idx];
	} else if (entOff != nullptr) {
		e0 = entOff[slot];
		e1 = e0 + entCnt[slot];
	}
	if (e0 < 0 || e1 <= e0) return;

	for (int e = e0 ; e < e1 ; e++) {
		if (entCh[e] != tdc.ch) continue;

		const int32_t hit = tdc.tdc4n + entDelay[e];
		if (hit < 0) continue;
		if (static_cast<uint32_t>(hit) >= regionSize - static_cast<uint32_t>(half)) continue;

		const int sig = entSig[e];
		if (sig < 0 || sig >= nsignal) continue;

		const uint32_t blo = (hit >= half) ? static_cast<uint32_t>(hit - half) : 0u;
		uint32_t bhi = static_cast<uint32_t>(hit + half);
		// The guard above bounds bhi only while half < regionSize. The host
		// keeps the wider case on the CPU, and this keeps the writes inside
		// the signal row in any event.
		if (blo >= regionSize) continue;
		if (bhi >= regionSize) bhi = regionSize - 1u;
		tsOrWindow(slice_h + static_cast<size_t>(sig) * nword, nword, marked_h, blo, bhi);
	}
}

__device__ __forceinline__ uint32_t tsEvalWord(const uint32_t *slice, uint32_t nword,
	int nsignal, const int32_t *opCode, const int32_t *opSig, int nop, uint32_t w)
{
	uint32_t st[TRIGGERSLICE_GPU_STACK];
	int sp = 0;

	for (int i = 0 ; i < nop ; i++) {
		switch (opCode[i]) {
		case kTsOpSig: {
			uint32_t v = 0;
			const int s = opSig[i];
			if (s >= 0 && s < nsignal) v = slice[static_cast<size_t>(s) * nword + w];
			if (sp < TRIGGERSLICE_GPU_STACK) st[sp++] = v;
			break;
		}
		case kTsOpAnd:
			if (sp > 1) { const uint32_t a = st[--sp]; const uint32_t b = st[--sp]; st[sp++] = a & b; }
			break;
		case kTsOpOr:
			if (sp > 1) { const uint32_t a = st[--sp]; const uint32_t b = st[--sp]; st[sp++] = a | b; }
			break;
		case kTsOpNot:
			if (sp > 0) { const uint32_t a = st[--sp]; st[sp++] = ~a; }
			break;
		case kTsOpSwap:
			if (sp > 1) { const uint32_t a = st[sp - 1]; st[sp - 1] = st[sp - 2]; st[sp - 2] = a; }
			break;
		case kTsOpDrop:
			if (sp > 0) sp--;
			break;
		default:
			break;
		}
	}

	return (sp > 0) ? st[sp - 1] : 0u;
}

__device__ __forceinline__ uint32_t tsEvalCompiled(const uint32_t *slice, uint32_t nword,
	int nsignal, const int32_t *cCode, const int32_t *cN, int ncop, uint32_t w)
{
	uint32_t st[TRIGGERSLICE_GPU_STACK];
	int sp = 0;

	for (int i = 0 ; i < ncop ; i++) {
		const int code = cCode[i];
		const int n = cN[i];
		if (code == kTsOpSig) {
			uint32_t v = 0;
			if (n >= 0 && n < nsignal) v = slice[static_cast<size_t>(n) * nword + w];
			if (sp < TRIGGERSLICE_GPU_STACK) st[sp++] = v;
		} else if (code == kTsOpAnd) {
			uint32_t a = ~0u;
			for (int k = 0 ; k < n && sp > 0 ; k++) a &= st[--sp];
			if (sp < TRIGGERSLICE_GPU_STACK) st[sp++] = a;
		} else if (code == kTsOpOr) {
			uint32_t a = 0;
			for (int k = 0 ; k < n && sp > 0 ; k++) a |= st[--sp];
			if (sp < TRIGGERSLICE_GPU_STACK) st[sp++] = a;
		} else if (code == kTsOpNot) {
			if (sp > 0) st[sp - 1] = ~st[sp - 1];
		}
	}

	return (sp > 0) ? st[sp - 1] : 0u;
}

__device__ __forceinline__ uint32_t tsEvalAt(const uint32_t *slice, uint32_t nword, int nsignal,
	int compiled, const int32_t *cCode, const int32_t *cN, int ncop,
	const int32_t *opCode, const int32_t *opSig, int nop, uint32_t w)
{
	return compiled
		? tsEvalCompiled(slice, nword, nsignal, cCode, cN, ncop, w)
		: tsEvalWord(slice, nword, nsignal, opCode, opSig, nop, w);
}

__device__ __forceinline__ int tsIsMarked(const uint32_t *marked, uint32_t w)
{
	return (marked[w >> 5] >> (w & 31)) & 1u;
}

__device__ __forceinline__ uint32_t tsRiseBits(uint32_t r, uint32_t carry, uint32_t w,
	uint32_t regionSize)
{
	uint32_t rise = r & ~((r << 1) | carry);
	if (w == 0) rise &= ~0x1u;
	const uint32_t b0 = w * 32;
	if (b0 >= regionSize) return 0u;
	if (b0 + 32 > regionSize) {
		const uint32_t nkeep = regionSize - b0;
		rise &= (nkeep >= 32) ? ~0u : ((1u << nkeep) - 1u);
	}
	return rise;
}

// Zero the slice words that the previous batch marked, so a full memset of
// nsignal * nword * B is not needed when the formula can skip zeros.
__global__ void tsClearMarkedKernel(uint32_t *slice, const uint32_t *marked,
	uint32_t nword, int nsignal, int B)
{
	const uint32_t packs = (nword + 31) / 32;
	const uint32_t p = blockIdx.x * blockDim.x + threadIdx.x;
	if (p >= packs * static_cast<uint32_t>(B)) return;

	uint32_t bits = marked[p];
	if (bits == 0) return;

	const int h = static_cast<int>(p / packs);
	const uint32_t pack = p % packs;
	uint32_t *base = slice + static_cast<size_t>(h) * nsignal * nword;

	while (bits != 0) {
		const uint32_t j = __ffs(bits) - 1;
		const uint32_t w = pack * 32 + j;
		if (w < nword) {
			for (int s = 0 ; s < nsignal ; s++) base[static_cast<size_t>(s) * nword + w] = 0;
		}
		bits &= bits - 1;
	}
}

__global__ void tsPackPopc(const uint32_t *marked, uint32_t npack, uint32_t *cnt)
{
	const uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
	if (i < npack) cnt[i] = __popc(marked[i]);
}

__global__ void tsExSumLocal(const uint32_t *cnt, uint32_t *off, uint32_t n, uint32_t *blockSum)
{
	__shared__ uint32_t s[256];
	const uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
	const uint32_t v = (i < n) ? cnt[i] : 0u;
	s[threadIdx.x] = v;
	__syncthreads();

	for (int stride = 1 ; stride < 256 ; stride *= 2) {
		const uint32_t t = (threadIdx.x >= static_cast<uint32_t>(stride))
			? s[threadIdx.x - stride] : 0u;
		__syncthreads();
		s[threadIdx.x] += t;
		__syncthreads();
	}

	if (i < n) off[i] = s[threadIdx.x] - v;
	if (threadIdx.x == 255) blockSum[blockIdx.x] = s[255];
}

__global__ void tsExSumAdd(uint32_t *off, uint32_t n, const uint32_t *blockPref)
{
	const uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
	if (i < n && blockIdx.x > 0) off[i] += blockPref[blockIdx.x];
}

__global__ void tsExSumSerial(const uint32_t *cnt, uint32_t *off, uint32_t n, uint32_t *total)
{
	if (threadIdx.x != 0 || blockIdx.x != 0) return;
	uint32_t s = 0;
	for (uint32_t i = 0 ; i < n ; i++) {
		off[i] = s;
		s += cnt[i];
	}
	if (total) *total = s;
}

__global__ void tsExpandMarked(const uint32_t *marked, const uint32_t *off,
	uint32_t npack, uint32_t packsPerHbf, uint32_t nword,
	int32_t *outHbf, uint32_t *outWord)
{
	const uint32_t p = blockIdx.x * blockDim.x + threadIdx.x;
	if (p >= npack) return;

	uint32_t bits = marked[p];
	if (bits == 0) return;

	uint32_t k = off[p];
	const int h = static_cast<int>(p / packsPerHbf);
	const uint32_t pack = p % packsPerHbf;

	while (bits != 0) {
		const uint32_t j = __ffs(bits) - 1;
		const uint32_t w = pack * 32 + j;
		if (w < nword) {
			outHbf[k] = h;
			outWord[k] = w;
			k++;
		}
		bits &= bits - 1;
	}
}

__global__ void tsFillAllWords(int B, uint32_t nword, int32_t *outHbf, uint32_t *outWord)
{
	const uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
	const uint32_t n = static_cast<uint32_t>(B) * nword;
	if (i >= n) return;
	outHbf[i] = static_cast<int32_t>(i / nword);
	outWord[i] = i % nword;
}

__global__ void tsScanMarked(
	const uint32_t *slice, const uint32_t *marked, int skipZero,
	uint32_t nword, uint32_t regionSize, int nsignal,
	int compiled, const int32_t *cCode, const int32_t *cN, int ncop,
	const int32_t *opCode, const int32_t *opSig, int nop,
	const int32_t *markedHbf, const uint32_t *markedWord, uint32_t nMarked,
	uint32_t *riseOut, uint32_t *cntOut)
{
	const uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
	const int tx = static_cast<int>(threadIdx.x);

	uint32_t r = 0;
	int h = -1;
	uint32_t w = 0;
	if (i < nMarked) {
		h = markedHbf[i];
		w = markedWord[i];
		const uint32_t *sl = slice + static_cast<size_t>(h) * nsignal * nword;
		r = tsEvalAt(sl, nword, nsignal, compiled, cCode, cN, ncop, opCode, opSig, nop, w);
	}

	__shared__ uint32_t sW[256];
	__shared__ uint32_t sR[256];
	__shared__ int sH[256];
	sW[tx] = (i < nMarked) ? w : ~0u;
	sR[tx] = r;
	sH[tx] = h;
	__syncthreads();

	if (i >= nMarked) return;

	uint32_t carry = 0;
	if (w > 0) {
		if (tx > 0 && sH[tx - 1] == h && sW[tx - 1] == w - 1) {
			carry = sR[tx - 1] >> 31;
		} else {
			const uint32_t packs = (nword + 31) / 32;
			const uint32_t *marked_h = marked + static_cast<size_t>(h) * packs;
			const int need = (! skipZero) || tsIsMarked(marked_h, w - 1);
			if (need) {
				const uint32_t *sl = slice + static_cast<size_t>(h) * nsignal * nword;
				carry = tsEvalAt(sl, nword, nsignal, compiled, cCode, cN, ncop,
					opCode, opSig, nop, w - 1) >> 31;
			}
		}
	}

	const uint32_t rise = tsRiseBits(r, carry, w, regionSize);
	riseOut[i] = rise;
	cntOut[i] = __popc(rise);
}

__global__ void tsWriteHits(const int32_t *markedHbf, const uint32_t *markedWord,
	const uint32_t *riseIn, const uint32_t *hitOff, uint32_t nMarked,
	uint32_t *hits, int32_t *hbfCount)
{
	const uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
	if (i >= nMarked) return;

	uint32_t rise = riseIn[i];
	uint32_t k = hitOff[i];
	const uint32_t w = markedWord[i];
	const int h = markedHbf[i];
	uint32_t n = 0;

	while (rise != 0) {
		const uint32_t j = __ffs(rise) - 1;
		hits[k++] = w * 32 + j;
		n++;
		rise &= rise - 1;
	}

	if (n > 0 && hbfCount != nullptr) atomicAdd(&hbfCount[h], static_cast<int>(n));
}

// ---- host state ----
struct TsGpu {
	bool inited = false;
	cudaStream_t stream = nullptr;

	uint32_t nword = 0;
	int nsignal = 0;
	uint32_t regionSize = 0;
	int Bcap = 0;
	int lastB = 0;

	uint32_t *dSlice = nullptr;
	uint32_t *dMarked = nullptr;
	int skipZero = 0;
	int compiled = 0;

	uint64_t *dWords = nullptr;
	uint64_t *hWords = nullptr;
	size_t wordCap = 0;

	int32_t *dSegOff = nullptr;
	int32_t *dSegNword = nullptr;
	int32_t *dSegSlot = nullptr;
	int32_t *dSegHbf = nullptr;
	uint32_t *dSegFem = nullptr;
	int32_t *hSegOff = nullptr;
	int32_t *hSegNword = nullptr;
	int32_t *hSegSlot = nullptr;
	int32_t *hSegHbf = nullptr;
	uint32_t *hSegFem = nullptr;
	size_t segCap = 0;

	int32_t *dEntOff = nullptr;
	int32_t *dEntCnt = nullptr;
	int32_t *dEntCh = nullptr;
	int32_t *dEntDelay = nullptr;
	int32_t *dEntSig = nullptr;
	int32_t *dChHead = nullptr;
	int32_t *dChCnt = nullptr;
	int32_t *dOpCode = nullptr;
	int32_t *dOpSig = nullptr;
	int32_t *dCOpCode = nullptr;
	int32_t *dCOpN = nullptr;
	int nop = 0;
	int ncop = 0;
	int nslot = 0;

	int32_t *dMarkedHbf = nullptr;
	uint32_t *dMarkedWord = nullptr;
	uint32_t *dRise = nullptr;
	uint32_t *dHitCnt = nullptr;
	uint32_t *dHitOff = nullptr;
	uint32_t *dPackCnt = nullptr;
	uint32_t *dPackOff = nullptr;
	uint32_t *dBlockSum = nullptr;
	uint32_t *dBlockPref = nullptr;
	uint32_t *dNMarked = nullptr;
	uint32_t *dNHits = nullptr;
	size_t scanCap = 0;
	size_t packCap = 0;
	size_t blockCap = 0;

	uint32_t *dHits = nullptr;
	int32_t *dHbfCount = nullptr;
	uint32_t *hHits = nullptr;
	int32_t *hHbfCount = nullptr;
	uint32_t hitCap = 0;
	int hbfCountCap = 0;

	std::string err;
};

TsGpu g;

bool tsFail(const char *what, cudaError_t e)
{
	g.err = std::string(what) + ": " + cudaGetErrorString(e);
	return false;
}

bool tsAlloc(void **p, size_t bytes, const char *what)
{
	if (*p) { cudaFree(*p); *p = nullptr; }
	const cudaError_t e = cudaMalloc(p, bytes);
	if (e != cudaSuccess) return tsFail(what, e);
	return true;
}

bool tsUpload(int32_t **dst, const int32_t *src, int n, const char *what)
{
	if (! tsAlloc(reinterpret_cast<void **>(dst), sizeof(int32_t) * (n > 0 ? n : 1), what)) {
		return false;
	}
	if (n <= 0) return true;
	const cudaError_t e = cudaMemcpy(*dst, src, sizeof(int32_t) * n, cudaMemcpyHostToDevice);
	if (e != cudaSuccess) return tsFail(what, e);
	return true;
}

bool tsEnsureWords(size_t n)
{
	if (n <= g.wordCap && g.dWords != nullptr) return true;

	const size_t cap = (n > g.wordCap * 2) ? n : (g.wordCap * 2 + 4096);
	if (! tsAlloc(reinterpret_cast<void **>(&g.dWords), sizeof(uint64_t) * cap, "words")) {
		return false;
	}
	if (g.hWords) { cudaFreeHost(g.hWords); g.hWords = nullptr; }
	const cudaError_t e = cudaMallocHost(reinterpret_cast<void **>(&g.hWords),
		sizeof(uint64_t) * cap);
	if (e != cudaSuccess) return tsFail("pinned words", e);

	g.wordCap = cap;

	return true;
}

bool tsEnsureSegs(size_t n)
{
	if (n + 1 <= g.segCap && g.dSegOff != nullptr) return true;

	const size_t cap = n + 1 + 64;
	if (! tsAlloc(reinterpret_cast<void **>(&g.dSegOff), sizeof(int32_t) * cap, "segOff")) return false;
	if (! tsAlloc(reinterpret_cast<void **>(&g.dSegNword), sizeof(int32_t) * cap, "segNword")) return false;
	if (! tsAlloc(reinterpret_cast<void **>(&g.dSegSlot), sizeof(int32_t) * cap, "segSlot")) return false;
	if (! tsAlloc(reinterpret_cast<void **>(&g.dSegHbf), sizeof(int32_t) * cap, "segHbf")) return false;
	if (! tsAlloc(reinterpret_cast<void **>(&g.dSegFem), sizeof(uint32_t) * cap, "segFem")) return false;

	if (g.hSegOff) cudaFreeHost(g.hSegOff);
	if (g.hSegNword) cudaFreeHost(g.hSegNword);
	if (g.hSegSlot) cudaFreeHost(g.hSegSlot);
	if (g.hSegHbf) cudaFreeHost(g.hSegHbf);
	if (g.hSegFem) cudaFreeHost(g.hSegFem);
	g.hSegOff = nullptr; g.hSegNword = nullptr; g.hSegSlot = nullptr;
	g.hSegHbf = nullptr; g.hSegFem = nullptr;

	cudaError_t e = cudaMallocHost(reinterpret_cast<void **>(&g.hSegOff), sizeof(int32_t) * cap);
	if (e != cudaSuccess) return tsFail("pinned segOff", e);
	e = cudaMallocHost(reinterpret_cast<void **>(&g.hSegNword), sizeof(int32_t) * cap);
	if (e != cudaSuccess) return tsFail("pinned segNword", e);
	e = cudaMallocHost(reinterpret_cast<void **>(&g.hSegSlot), sizeof(int32_t) * cap);
	if (e != cudaSuccess) return tsFail("pinned segSlot", e);
	e = cudaMallocHost(reinterpret_cast<void **>(&g.hSegHbf), sizeof(int32_t) * cap);
	if (e != cudaSuccess) return tsFail("pinned segHbf", e);
	e = cudaMallocHost(reinterpret_cast<void **>(&g.hSegFem), sizeof(uint32_t) * cap);
	if (e != cudaSuccess) return tsFail("pinned segFem", e);

	g.segCap = cap;

	return true;
}

bool tsEnsureBatch(int B)
{
	if (B < 1) B = 1;
	if (B <= g.Bcap && g.dSlice != nullptr && g.dMarked != nullptr) return true;

	const int cap = (B > g.Bcap * 2) ? B : (g.Bcap * 2 + 1);
	const size_t slice = static_cast<size_t>(cap) * (g.nsignal > 0 ? g.nsignal : 1) * g.nword;
	if (! tsAlloc(reinterpret_cast<void **>(&g.dSlice), sizeof(uint32_t) * slice, "slice")) {
		return false;
	}
	cudaError_t e = cudaMemset(g.dSlice, 0, sizeof(uint32_t) * slice);
	if (e != cudaSuccess) return tsFail("clear new slice", e);

	const size_t packs = static_cast<size_t>(cap) * ((g.nword + 31) / 32);
	if (! tsAlloc(reinterpret_cast<void **>(&g.dMarked),
		sizeof(uint32_t) * (packs > 0 ? packs : 1), "marked")) {
		return false;
	}
	e = cudaMemset(g.dMarked, 0, sizeof(uint32_t) * (packs > 0 ? packs : 1));
	if (e != cudaSuccess) return tsFail("clear new marked", e);

	g.hitCap = (g.regionSize > 0 ? g.regionSize : 1) * static_cast<uint32_t>(cap);
	if (! tsAlloc(reinterpret_cast<void **>(&g.dHits), sizeof(uint32_t) * g.hitCap, "hits")) {
		return false;
	}
	if (g.hHits) { cudaFreeHost(g.hHits); g.hHits = nullptr; }
	e = cudaMallocHost(reinterpret_cast<void **>(&g.hHits), sizeof(uint32_t) * g.hitCap);
	if (e != cudaSuccess) return tsFail("pinned hits", e);

	if (cap > g.hbfCountCap || g.dHbfCount == nullptr) {
		if (! tsAlloc(reinterpret_cast<void **>(&g.dHbfCount), sizeof(int32_t) * cap, "hbfCount")) {
			return false;
		}
		if (g.hHbfCount) { cudaFreeHost(g.hHbfCount); g.hHbfCount = nullptr; }
		e = cudaMallocHost(reinterpret_cast<void **>(&g.hHbfCount), sizeof(int32_t) * cap);
		if (e != cudaSuccess) return tsFail("pinned hbfCount", e);
		g.hbfCountCap = cap;
	}

	g.Bcap = cap;
	g.lastB = 0;

	return true;
}

bool tsEnsureScan(uint32_t nMarkedMax, uint32_t npack)
{
	if (nMarkedMax > g.scanCap || g.dMarkedHbf == nullptr) {
		const size_t cap = (nMarkedMax > g.scanCap * 2) ? nMarkedMax : (g.scanCap * 2 + 4096);
		if (! tsAlloc(reinterpret_cast<void **>(&g.dMarkedHbf), sizeof(int32_t) * cap, "markedHbf")) return false;
		if (! tsAlloc(reinterpret_cast<void **>(&g.dMarkedWord), sizeof(uint32_t) * cap, "markedWord")) return false;
		if (! tsAlloc(reinterpret_cast<void **>(&g.dRise), sizeof(uint32_t) * cap, "rise")) return false;
		if (! tsAlloc(reinterpret_cast<void **>(&g.dHitCnt), sizeof(uint32_t) * cap, "hitCnt")) return false;
		if (! tsAlloc(reinterpret_cast<void **>(&g.dHitOff), sizeof(uint32_t) * cap, "hitOff")) return false;
		g.scanCap = cap;
	}

	if (npack > g.packCap || g.dPackCnt == nullptr) {
		const size_t cap = (npack > g.packCap * 2) ? npack : (g.packCap * 2 + 64);
		if (! tsAlloc(reinterpret_cast<void **>(&g.dPackCnt), sizeof(uint32_t) * cap, "packCnt")) return false;
		if (! tsAlloc(reinterpret_cast<void **>(&g.dPackOff), sizeof(uint32_t) * cap, "packOff")) return false;
		g.packCap = cap;
	}

	const size_t nblock = (g.scanCap + 255) / 256 + 8;
	if (nblock > g.blockCap || g.dBlockSum == nullptr) {
		if (! tsAlloc(reinterpret_cast<void **>(&g.dBlockSum), sizeof(uint32_t) * nblock, "blockSum")) return false;
		if (! tsAlloc(reinterpret_cast<void **>(&g.dBlockPref), sizeof(uint32_t) * nblock, "blockPref")) return false;
		g.blockCap = nblock;
	}

	return true;
}

bool tsExclusiveSum(uint32_t *dCnt, uint32_t *dOff, uint32_t n, uint32_t *dTotal)
{
	if (n == 0) {
		const cudaError_t e = cudaMemsetAsync(dTotal, 0, sizeof(uint32_t), g.stream);
		return (e == cudaSuccess) ? true : tsFail("zero total", e);
	}

	if (n > 65536) {
		tsExSumSerial<<<1, 1, 0, g.stream>>>(dCnt, dOff, n, dTotal);
		const cudaError_t e = cudaGetLastError();
		return (e == cudaSuccess) ? true : tsFail("exsum serial", e);
	}

	const int th = 256;
	const int nb = static_cast<int>((n + th - 1) / th);
	tsExSumLocal<<<nb, th, 0, g.stream>>>(dCnt, dOff, n, g.dBlockSum);
	cudaError_t e = cudaGetLastError();
	if (e != cudaSuccess) return tsFail("exsum local", e);

	if (nb <= 256) {
		tsExSumLocal<<<1, th, 0, g.stream>>>(g.dBlockSum, g.dBlockPref,
			static_cast<uint32_t>(nb), dTotal);
	} else {
		tsExSumSerial<<<1, 1, 0, g.stream>>>(g.dBlockSum, g.dBlockPref,
			static_cast<uint32_t>(nb), dTotal);
	}
	e = cudaGetLastError();
	if (e != cudaSuccess) return tsFail("exsum blocks", e);

	tsExSumAdd<<<nb, th, 0, g.stream>>>(dOff, n, g.dBlockPref);
	e = cudaGetLastError();
	return (e == cudaSuccess) ? true : tsFail("exsum add", e);
}

}  // namespace

namespace TriggerSliceGpu {

bool Available()
{
	int n = 0;
	const cudaError_t e = cudaGetDeviceCount(&n);
	if (e != cudaSuccess) {
		g.err = std::string("cudaGetDeviceCount: ") + cudaGetErrorString(e);
		return false;
	}
	if (n < 1) {
		g.err = "no CUDA device";
		return false;
	}

	return true;
}

bool Init(uint32_t nword, int nsignal, uint32_t timeRegionSize)
{
	if (! Available()) return false;

	const bool same = g.inited && (g.nword == nword) && (g.nsignal == nsignal)
		&& (g.regionSize == timeRegionSize);
	if (same) return true;

	g.nword = nword;
	g.nsignal = nsignal;
	g.regionSize = timeRegionSize;
	g.Bcap = 0;
	g.lastB = 0;

	if (g.stream == nullptr) {
		const cudaError_t e = cudaStreamCreate(&g.stream);
		if (e != cudaSuccess) return tsFail("stream", e);
	}

	if (g.dNMarked == nullptr && ! tsAlloc(reinterpret_cast<void **>(&g.dNMarked),
		sizeof(uint32_t), "nMarked")) {
		return false;
	}
	if (g.dNHits == nullptr && ! tsAlloc(reinterpret_cast<void **>(&g.dNHits),
		sizeof(uint32_t), "nHits")) {
		return false;
	}

	if (! tsEnsureBatch(1)) return false;

	g.inited = true;

	return true;
}

void Release()
{
	if (g.dSlice) cudaFree(g.dSlice);
	if (g.dMarked) cudaFree(g.dMarked);
	if (g.dWords) cudaFree(g.dWords);
	if (g.hWords) cudaFreeHost(g.hWords);
	if (g.dSegOff) cudaFree(g.dSegOff);
	if (g.dSegNword) cudaFree(g.dSegNword);
	if (g.dSegSlot) cudaFree(g.dSegSlot);
	if (g.dSegHbf) cudaFree(g.dSegHbf);
	if (g.dSegFem) cudaFree(g.dSegFem);
	if (g.hSegOff) cudaFreeHost(g.hSegOff);
	if (g.hSegNword) cudaFreeHost(g.hSegNword);
	if (g.hSegSlot) cudaFreeHost(g.hSegSlot);
	if (g.hSegHbf) cudaFreeHost(g.hSegHbf);
	if (g.hSegFem) cudaFreeHost(g.hSegFem);
	if (g.dEntOff) cudaFree(g.dEntOff);
	if (g.dEntCnt) cudaFree(g.dEntCnt);
	if (g.dEntCh) cudaFree(g.dEntCh);
	if (g.dEntDelay) cudaFree(g.dEntDelay);
	if (g.dEntSig) cudaFree(g.dEntSig);
	if (g.dChHead) cudaFree(g.dChHead);
	if (g.dChCnt) cudaFree(g.dChCnt);
	if (g.dOpCode) cudaFree(g.dOpCode);
	if (g.dOpSig) cudaFree(g.dOpSig);
	if (g.dCOpCode) cudaFree(g.dCOpCode);
	if (g.dCOpN) cudaFree(g.dCOpN);
	if (g.dMarkedHbf) cudaFree(g.dMarkedHbf);
	if (g.dMarkedWord) cudaFree(g.dMarkedWord);
	if (g.dRise) cudaFree(g.dRise);
	if (g.dHitCnt) cudaFree(g.dHitCnt);
	if (g.dHitOff) cudaFree(g.dHitOff);
	if (g.dPackCnt) cudaFree(g.dPackCnt);
	if (g.dPackOff) cudaFree(g.dPackOff);
	if (g.dBlockSum) cudaFree(g.dBlockSum);
	if (g.dBlockPref) cudaFree(g.dBlockPref);
	if (g.dNMarked) cudaFree(g.dNMarked);
	if (g.dNHits) cudaFree(g.dNHits);
	if (g.dHits) cudaFree(g.dHits);
	if (g.dHbfCount) cudaFree(g.dHbfCount);
	if (g.hHits) cudaFreeHost(g.hHits);
	if (g.hHbfCount) cudaFreeHost(g.hHbfCount);
	if (g.stream) cudaStreamDestroy(g.stream);

	g = TsGpu();

	return;
}

bool SetConfig(const int32_t *entOff, const int32_t *entCnt, int nslot,
	const int32_t *entCh, const int32_t *entDelay, const int32_t *entSig, int nent,
	const int32_t *opCode, const int32_t *opSig, int nop, int skipZero,
	const int32_t *cCode, const int32_t *cN, int ncop)
{
	if (! g.inited) {
		g.err = "SetConfig before Init";
		return false;
	}

	g.skipZero = skipZero ? 1 : 0;
	g.compiled = (ncop > 0) ? 1 : 0;
	g.nop = nop;
	g.ncop = ncop;
	g.nslot = nslot;

	std::vector<int32_t> head(static_cast<size_t>(nslot > 0 ? nslot : 1) * TRIGGERSLICE_MAX_CH, -1);
	std::vector<int32_t> cnt(static_cast<size_t>(nslot > 0 ? nslot : 1) * TRIGGERSLICE_MAX_CH, 0);
	std::vector<int32_t> newCh, newDl, newSg;
	std::vector<int32_t> newOff(nslot > 0 ? nslot : 1, 0);
	std::vector<int32_t> newCnt(nslot > 0 ? nslot : 1, 0);

	newCh.reserve(nent > 0 ? nent : 0);
	newDl.reserve(nent > 0 ? nent : 0);
	newSg.reserve(nent > 0 ? nent : 0);

	for (int s = 0 ; s < nslot ; s++) {
		std::vector<std::vector<int>> buck(TRIGGERSLICE_MAX_CH);
		const int e0 = entOff[s];
		const int e1 = e0 + entCnt[s];
		newOff[s] = static_cast<int32_t>(newCh.size());
		for (int e = e0 ; e < e1 ; e++) {
			const int ch = entCh[e];
			if (ch >= 0 && ch < TRIGGERSLICE_MAX_CH) buck[ch].push_back(e);
		}
		for (int ch = 0 ; ch < TRIGGERSLICE_MAX_CH ; ch++) {
			if (buck[ch].empty()) continue;
			head[static_cast<size_t>(s) * TRIGGERSLICE_MAX_CH + ch] = static_cast<int32_t>(newCh.size());
			cnt[static_cast<size_t>(s) * TRIGGERSLICE_MAX_CH + ch] = static_cast<int32_t>(buck[ch].size());
			for (int e : buck[ch]) {
				newCh.push_back(entCh[e]);
				newDl.push_back(entDelay[e]);
				newSg.push_back(entSig[e]);
			}
		}
		for (int e = e0 ; e < e1 ; e++) {
			if (entCh[e] < 0 || entCh[e] >= TRIGGERSLICE_MAX_CH) {
				newCh.push_back(entCh[e]);
				newDl.push_back(entDelay[e]);
				newSg.push_back(entSig[e]);
			}
		}
		newCnt[s] = static_cast<int32_t>(newCh.size()) - newOff[s];
	}

	if (! tsUpload(&g.dEntOff, newOff.data(), nslot, "entOff")) return false;
	if (! tsUpload(&g.dEntCnt, newCnt.data(), nslot, "entCnt")) return false;
	if (! tsUpload(&g.dEntCh, newCh.data(), static_cast<int>(newCh.size()), "entCh")) return false;
	if (! tsUpload(&g.dEntDelay, newDl.data(), static_cast<int>(newDl.size()), "entDelay")) return false;
	if (! tsUpload(&g.dEntSig, newSg.data(), static_cast<int>(newSg.size()), "entSig")) return false;
	if (! tsUpload(&g.dChHead, head.data(), nslot * TRIGGERSLICE_MAX_CH, "chHead")) return false;
	if (! tsUpload(&g.dChCnt, cnt.data(), nslot * TRIGGERSLICE_MAX_CH, "chCnt")) return false;
	if (! tsUpload(&g.dOpCode, opCode, nop, "opCode")) return false;
	if (! tsUpload(&g.dOpSig, opSig, nop, "opSig")) return false;
	if (! tsUpload(&g.dCOpCode, cCode, ncop, "cOpCode")) return false;
	if (! tsUpload(&g.dCOpN, cN, ncop, "cOpN")) return false;

	return true;
}

bool MarkScanBatch(int B,
	const uint64_t * const *segData, const int32_t *segNword,
	const int32_t *segSlot, const uint32_t *segFemType, const int32_t *segHbf,
	int nseg, int markLen, std::vector<std::vector<uint32_t>> &hitsPerHbf)
{
	hitsPerHbf.assign(B > 0 ? B : 0, {});

	if (! g.inited || (g.nop < 1 && g.ncop < 1)) {
		g.err = "MarkScan before Init/SetConfig";
		return false;
	}
	if (B < 1) return true;
	if (! tsEnsureBatch(B)) return false;

	if (B != g.lastB) {
		const size_t sliceWords = static_cast<size_t>(g.Bcap) * g.nsignal * g.nword;
		cudaError_t ce = cudaMemsetAsync(g.dSlice, 0, sizeof(uint32_t) * sliceWords, g.stream);
		if (ce != cudaSuccess) return tsFail("reset slice", ce);
		const size_t markedWords = static_cast<size_t>(g.Bcap) * ((g.nword + 31) / 32);
		ce = cudaMemsetAsync(g.dMarked, 0, sizeof(uint32_t) * markedWords, g.stream);
		if (ce != cudaSuccess) return tsFail("reset marked", ce);
		g.lastB = B;
	}

	size_t total = 0;
	int maxSeg = 0;
	for (int s = 0 ; s < nseg ; s++) {
		const int n = (segNword[s] > 0) ? segNword[s] : 0;
		total += n;
		if (n > maxSeg) maxSeg = n;
	}

	if (! tsEnsureSegs(nseg > 0 ? nseg : 1)) return false;
	if (total > 0 && ! tsEnsureWords(total)) return false;

	const uint32_t packs = (g.nword + 31) / 32;
	const uint32_t npack = packs * static_cast<uint32_t>(B);
	const uint32_t markedMax = g.nword * static_cast<uint32_t>(B);
	if (! tsEnsureScan(markedMax, npack)) return false;

	size_t off = 0;
	for (int s = 0 ; s < nseg ; s++) {
		g.hSegOff[s] = static_cast<int32_t>(off);
		g.hSegNword[s] = (segNword[s] > 0) ? segNword[s] : 0;
		g.hSegSlot[s] = segSlot[s];
		g.hSegFem[s] = segFemType[s];
		g.hSegHbf[s] = segHbf[s];
		const int n = g.hSegNword[s];
		if (n > 0) {
			std::memcpy(g.hWords + off, segData[s], sizeof(uint64_t) * n);
			off += n;
		}
	}
	g.hSegOff[nseg > 0 ? nseg : 0] = static_cast<int32_t>(off);

	const int threads = 256;
	cudaError_t e;

	if (g.skipZero) {
		const int cblocks = static_cast<int>((npack + threads - 1) / threads);
		if (cblocks > 0) {
			tsClearMarkedKernel<<<cblocks, threads, 0, g.stream>>>(
				g.dSlice, g.dMarked, g.nword, g.nsignal, B);
			e = cudaGetLastError();
			if (e != cudaSuccess) return tsFail("clear marked", e);
		}
		e = cudaMemsetAsync(g.dMarked, 0, sizeof(uint32_t) * npack, g.stream);
		if (e != cudaSuccess) return tsFail("zero marked", e);
	} else {
		const size_t sliceWords = static_cast<size_t>(B) * g.nsignal * g.nword;
		e = cudaMemsetAsync(g.dSlice, 0, sizeof(uint32_t) * sliceWords, g.stream);
		if (e != cudaSuccess) return tsFail("clear slice", e);
		e = cudaMemsetAsync(g.dMarked, 0, sizeof(uint32_t) * npack, g.stream);
		if (e != cudaSuccess) return tsFail("zero marked", e);
	}

	e = cudaMemsetAsync(g.dHbfCount, 0, sizeof(int32_t) * B, g.stream);
	if (e != cudaSuccess) return tsFail("clear hbfCount", e);
	e = cudaMemsetAsync(g.dNHits, 0, sizeof(uint32_t), g.stream);
	if (e != cudaSuccess) return tsFail("clear nHits", e);
	e = cudaMemsetAsync(g.dNMarked, 0, sizeof(uint32_t), g.stream);
	if (e != cudaSuccess) return tsFail("clear nMarked", e);

	if (off > 0 && nseg > 0 && maxSeg > 0) {
		e = cudaMemcpyAsync(g.dWords, g.hWords, sizeof(uint64_t) * off,
			cudaMemcpyHostToDevice, g.stream);
		if (e != cudaSuccess) return tsFail("upload words", e);
		e = cudaMemcpyAsync(g.dSegOff, g.hSegOff, sizeof(int32_t) * (nseg + 1),
			cudaMemcpyHostToDevice, g.stream);
		if (e != cudaSuccess) return tsFail("upload segOff", e);
		e = cudaMemcpyAsync(g.dSegNword, g.hSegNword, sizeof(int32_t) * nseg,
			cudaMemcpyHostToDevice, g.stream);
		if (e != cudaSuccess) return tsFail("upload segNword", e);
		e = cudaMemcpyAsync(g.dSegSlot, g.hSegSlot, sizeof(int32_t) * nseg,
			cudaMemcpyHostToDevice, g.stream);
		if (e != cudaSuccess) return tsFail("upload segSlot", e);
		e = cudaMemcpyAsync(g.dSegHbf, g.hSegHbf, sizeof(int32_t) * nseg,
			cudaMemcpyHostToDevice, g.stream);
		if (e != cudaSuccess) return tsFail("upload segHbf", e);
		e = cudaMemcpyAsync(g.dSegFem, g.hSegFem, sizeof(uint32_t) * nseg,
			cudaMemcpyHostToDevice, g.stream);
		if (e != cudaSuccess) return tsFail("upload segFem", e);

		const dim3 grid((maxSeg + threads - 1) / threads, nseg);
		tsMarkKernel<<<grid, threads, 0, g.stream>>>(
			g.dSlice, g.dMarked, g.nword, g.regionSize, g.nsignal, B,
			g.dWords, g.dSegOff, g.dSegNword, g.dSegSlot, g.dSegHbf, g.dSegFem,
			g.dChHead, g.dChCnt, g.dEntOff, g.dEntCnt,
			g.dEntCh, g.dEntDelay, g.dEntSig,
			markLen / 2);
		e = cudaGetLastError();
		if (e != cudaSuccess) return tsFail("mark kernel", e);
	}

	uint32_t nMarked = 0;
	if (g.skipZero) {
		const int pblocks = static_cast<int>((npack + threads - 1) / threads);
		if (pblocks > 0) {
			tsPackPopc<<<pblocks, threads, 0, g.stream>>>(g.dMarked, npack, g.dPackCnt);
			e = cudaGetLastError();
			if (e != cudaSuccess) return tsFail("pack popc", e);
		}
		if (! tsExclusiveSum(g.dPackCnt, g.dPackOff, npack, g.dNMarked)) return false;
		e = cudaMemcpyAsync(&nMarked, g.dNMarked, sizeof(uint32_t),
			cudaMemcpyDeviceToHost, g.stream);
		if (e != cudaSuccess) return tsFail("nMarked back", e);
		e = cudaStreamSynchronize(g.stream);
		if (e != cudaSuccess) return tsFail("sync nMarked", e);
		if (nMarked > markedMax) nMarked = markedMax;
		if (nMarked > 0) {
			const int eblocks = static_cast<int>((npack + threads - 1) / threads);
			tsExpandMarked<<<eblocks, threads, 0, g.stream>>>(
				g.dMarked, g.dPackOff, npack, packs, g.nword,
				g.dMarkedHbf, g.dMarkedWord);
			e = cudaGetLastError();
			if (e != cudaSuccess) return tsFail("expand marked", e);
		}
	} else {
		nMarked = markedMax;
		const int fblocks = static_cast<int>((nMarked + threads - 1) / threads);
		if (fblocks > 0) {
			tsFillAllWords<<<fblocks, threads, 0, g.stream>>>(
				B, g.nword, g.dMarkedHbf, g.dMarkedWord);
			e = cudaGetLastError();
			if (e != cudaSuccess) return tsFail("fill words", e);
		}
	}

	if (nMarked == 0) return true;

	const int sblocks = static_cast<int>((nMarked + threads - 1) / threads);
	tsScanMarked<<<sblocks, threads, 0, g.stream>>>(
		g.dSlice, g.dMarked, g.skipZero, g.nword, g.regionSize, g.nsignal,
		g.compiled, g.dCOpCode, g.dCOpN, g.ncop,
		g.dOpCode, g.dOpSig, g.nop,
		g.dMarkedHbf, g.dMarkedWord, nMarked,
		g.dRise, g.dHitCnt);
	e = cudaGetLastError();
	if (e != cudaSuccess) return tsFail("scan marked", e);

	if (! tsExclusiveSum(g.dHitCnt, g.dHitOff, nMarked, g.dNHits)) return false;

	tsWriteHits<<<sblocks, threads, 0, g.stream>>>(
		g.dMarkedHbf, g.dMarkedWord, g.dRise, g.dHitOff, nMarked,
		g.dHits, g.dHbfCount);
	e = cudaGetLastError();
	if (e != cudaSuccess) return tsFail("write hits", e);

	uint32_t nHits = 0;
	e = cudaMemcpyAsync(&nHits, g.dNHits, sizeof(uint32_t), cudaMemcpyDeviceToHost, g.stream);
	if (e != cudaSuccess) return tsFail("nHits back", e);
	e = cudaMemcpyAsync(g.hHbfCount, g.dHbfCount, sizeof(int32_t) * B,
		cudaMemcpyDeviceToHost, g.stream);
	if (e != cudaSuccess) return tsFail("hbfCount back", e);
	e = cudaStreamSynchronize(g.stream);
	if (e != cudaSuccess) return tsFail("sync hits", e);

	if (nHits > g.hitCap) nHits = g.hitCap;
	if (nHits == 0) return true;

	e = cudaMemcpy(g.hHits, g.dHits, sizeof(uint32_t) * nHits, cudaMemcpyDeviceToHost);
	if (e != cudaSuccess) return tsFail("hits back", e);

	size_t cursor = 0;
	for (int h = 0 ; h < B ; h++) {
		int nh = g.hHbfCount[h];
		if (nh < 0) nh = 0;
		if (cursor + static_cast<size_t>(nh) > nHits) nh = static_cast<int>(nHits - cursor);
		hitsPerHbf[h].assign(g.hHits + cursor, g.hHits + cursor + nh);
		cursor += static_cast<size_t>(nh);
	}

	return true;
}

bool MarkScan(const uint64_t * const *segData, const int32_t *segNword,
	const int32_t *segSlot, const uint32_t *segFemType, int nseg,
	int markLen, std::vector<uint32_t> &hitsOut)
{
	std::vector<int32_t> zeros(nseg > 0 ? nseg : 1, 0);
	std::vector<std::vector<uint32_t>> out;
	if (! MarkScanBatch(1, segData, segNword, segSlot, segFemType, zeros.data(),
		nseg, markLen, out)) {
		hitsOut.clear();
		return false;
	}
	hitsOut = out.empty() ? std::vector<uint32_t>() : std::move(out[0]);
	return true;
}

const char *LastError()
{
	return g.err.empty() ? "" : g.err.c_str();
}

}  // namespace TriggerSliceGpu


#else  // ! __CUDACC__
/* =======================================================================
 *  CPU half: the engine itself, compiled by the host compiler.
 * ======================================================================= */

#include <iostream>
#include <iomanip>
#include <sstream>
#ifdef TRIGGERSLICE_AVX2
#include <immintrin.h>
#endif
#include <map>
#include <algorithm>
#include <functional>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#include "UnpackTdc.h"
#include "SubTimeFrameHeader.h"
#include "infixtorpn.cxx"

#ifndef USE_CUDA
// Built without CUDA: the GPU backend reports that it is not there, and
// SetBackend(kGpu) stays on the CPU. Nothing else in the file needs CUDA.
namespace TriggerSliceGpu {

bool Available() { return false; }
bool Init(uint32_t, int, uint32_t) { return false; }
void Release() {}
bool SetConfig(const int32_t *, const int32_t *, int,
	const int32_t *, const int32_t *, const int32_t *, int,
	const int32_t *, const int32_t *, int, int,
	const int32_t *, const int32_t *, int) { return false; }
bool MarkScan(const uint64_t * const *, const int32_t *, const int32_t *,
	const uint32_t *, int, int, std::vector<uint32_t> &hitsOut)
{
	hitsOut.clear();
	return false;
}
bool MarkScanBatch(int B, const uint64_t * const *, const int32_t *,
	const int32_t *, const uint32_t *, const int32_t *,
	int, int, std::vector<std::vector<uint32_t>> &hitsPerHbf)
{
	hitsPerHbf.assign(B > 0 ? B : 0, {});
	return false;
}
const char *LastError() { return "built without CUDA"; }

}  // namespace TriggerSliceGpu
#endif  // ! USE_CUDA

struct HBFIndex {
	int msg_index;
	uint32_t timeFrameId;
	uint32_t femType;
	uint32_t femId;
	unsigned char* data;
	int size;
};

class Trigger
{
public:
	enum Backend {
		kCpu = 0,
		kGpu = 1,
	};

	Trigger() {};

	// TriggerSliceGpu keeps one context for the whole process, so release it
	// only when the instance that took it goes away. Trigger.cxx frees nothing
	// but its own region, and a CPU-backend instance must not pull the GPU out
	// from under another one.
	virtual ~Trigger()
	{
		if (fGpuOwned && (--GpuUsers() <= 0)) TriggerSliceGpu::Release();
	};

	void InitParam();
	bool SetTimeRegion(int);
	void CleanUpTimeRegion();
	uint32_t *GetTimeRegion();
	uint32_t GetTimeRegionSize();

	void EntryTo(int, uint32_t, int, int);
	void ClearEntry();
	bool CheckEntryFEM(uint32_t);

	// Mark() and Scan() are the CPU engine. Exec() is the entry point that
	// honours the backend, because the GPU does mark and scan in one go.
	void Mark(unsigned char *, int, int, uint32_t);
	std::vector<uint32_t> *Scan();
	std::vector<uint32_t> *Exec(std::vector<struct HBFIndex> &);
	void ExecBatch(std::vector<std::vector<struct HBFIndex> > &,
		std::vector<std::vector<uint32_t> > &);

	void SetMarkLen(int val) {fMarkLen = val;};
	int GetMarkLen() {return fMarkLen;};

	// Compiles the expression into an opcode list. Kept as MakeTable() so the
	// device code is the same as for Trigger.cxx; no truth table is built.
	void MakeTable(std::string &);

	// Returns the backend actually in use, which is kCpu whenever the GPU
	// cannot be used.
	int SetBackend(int);
	int GetBackend() const {return fBackend;};
	static bool GpuAvailable() {return TriggerSliceGpu::Available();};

	// bit-sliced geometry
	int GetSignalCount() const {return fNsignal;};

	// Channels registered, which EntryTo() lets exceed the signal count.
	int ChannelCount() const
	{
		int n = 0;
		for (const auto &kv : fEntryCh) n += static_cast<int>(kv.second.size());
		return n;
	};
	uint32_t GetWordCount() const {return fNword;};
	size_t GetRegionBytes() const {return fSlice.size() * sizeof(uint32_t);};
	std::string Info();
	void Dump();

protected:
private:
	struct Op {
		int code;
		int sig;
	};

	struct COp {
		int code;
		int n;
	};

	// Users of the single process-wide GPU context.
	static int &GpuUsers() {static int n = 0; return n;};

	int ComPushBack(const std::string &);
	void CompileExpr();
	void EnsureRegion();
	void MarkWindow(int sig, uint32_t hit);
	void AddMarkedWords(uint32_t wlo, uint32_t whi);
	uint32_t EvalWord(uint32_t w);
	uint32_t EvalCompiled(uint32_t w) const;
#ifdef TRIGGERSLICE_AVX2
	void EvalCompiled8(uint32_t w0, uint32_t r[8]) const;
#endif
	int StackDepth() const;
	int CompiledDepth() const;
	bool SyncGpu();
	bool GpuWindowOk();
	bool ExecGpu(std::vector<struct HBFIndex> &);
	bool ExecGpuBatch(std::vector<std::vector<struct HBFIndex> > &,
		std::vector<std::vector<uint32_t> > &);
	void DropToCpu(const char *why);
	void BuildChIndex();

	// per FEM signal definition. The third list holds the signal index, where
	// Trigger.cxx holds a 32-bit mark bit.
	std::map< uint32_t, std::vector<int> > fEntryCh;
	std::map< uint32_t, std::vector<int> > fEntryChDelay;
	std::map< uint32_t, std::vector<int> > fEntryChSig;

	// Highest row named by a registration, plus one. The region needs that many
	// rows whether or not every one of them carries a channel.
	int fEntryRowMax = 0;

	// ch -> registered entries, so that Mark() costs the number of entries on
	// the channel the word carries rather than the number registered on the
	// module. The device side already indexes its table this way.
	//
	// Signal and delay sit together in the entry rather than in the fEntryChSig
	// and fEntryChDelay vectors, so a hit reads one slot and one entry instead
	// of chasing an index into two more arrays.
	struct ChSlot {
		int32_t head;
		int32_t cnt;
	};
	struct ChEnt {
		int32_t sig;
		int32_t dly;
	};
	struct ChIndex {
		std::vector<ChSlot> slot;
		std::vector<ChEnt> ent;
	};
	std::map< uint32_t, ChIndex > fChIndex;

	// A heartbeat carries a segment per module and only a few are configured,
	// so rejecting the rest has to be cheaper than a map lookup each.
	struct ChFlat {
		uint32_t fem;
		const ChIndex *ix;
	};
	std::vector<ChFlat> fChFlat;
	bool fChIndexNeedsBuild = true;

	uint32_t fTimeRegionSize = 0;
	uint32_t fNword = 0;
	int fNsignal = 0;
	int fSigMax = -1;
	std::vector<uint32_t> fSlice;

	std::vector<uint32_t> fHits;
	int fMarkLen = 5;

	std::vector<struct Op> fOps;
	std::vector<struct COp> fCOps;
	std::vector<int> fUsedSig;
	bool fCompiled = false;
	bool fCanSkipZero = false;

	// One bit per word of the time region, set when the marking wrote into that
	// word. Shared by every signal row, so a word is listed once however many
	// rows carry a mark there, and the scan reads only the words listed here.
	std::vector<uint32_t> fMarkedWords;

	int fBackend = kCpu;

	// GPU side: flattened signal table, one slot per FEM
	bool fGpuNeedsSync = true;
	bool fGpuOwned = false;
	std::map<uint32_t, int> fFemSlot;
	std::vector<int32_t> fGEntOff;
	std::vector<int32_t> fGEntCnt;
	std::vector<int32_t> fGEntCh;
	std::vector<int32_t> fGEntDelay;
	std::vector<int32_t> fGEntSig;
	std::vector<int32_t> fGOpCode;
	std::vector<int32_t> fGOpSig;
	std::vector<int32_t> fGCOpCode;
	std::vector<int32_t> fGCOpN;

	// per Exec() scratch, kept to avoid reallocating every heartbeat
	std::vector<const uint64_t *> fSegData;
	std::vector<int32_t> fSegNword;
	std::vector<int32_t> fSegSlot;
	std::vector<int32_t> fSegHbf;
	std::vector<uint32_t> fSegFemType;
};


int Trigger::SetBackend(int backend)
{
	if (backend != kGpu) {
		fBackend = kCpu;
		return fBackend;
	}

	if (! TriggerSliceGpu::Available()) {
		std::cout << "#W TriggerSlice: GPU not usable ("
			<< TriggerSliceGpu::LastError() << "), staying on CPU" << std::endl;
		fBackend = kCpu;
		return fBackend;
	}

	fBackend = kGpu;
	fGpuNeedsSync = true;
	if (! fOps.empty() && ! SyncGpu()) {
		DropToCpu("configuration failed");
	}

	return fBackend;
}

void Trigger::DropToCpu(const char *why)
{
	std::cout << "#W TriggerSlice: GPU " << why << " ("
		<< TriggerSliceGpu::LastError() << "), falling back to CPU" << std::endl;
	fBackend = kCpu;

	return;
}

int Trigger::ComPushBack(const std::string &word)
{
	if (word.empty()) return fOps.size();

	bool is_number = true;
	for (char c : word) {
		if (! isdigit(static_cast<unsigned char>(c))) {
			is_number = false;
			break;
		}
	}

	if (is_number) {
		int sig = std::atoi(word.c_str());
		// No 32-signal ceiling here. The bit-sliced region grows with the
		// signal count instead of with the bins per word.
		fOps.push_back({kTsOpSig, sig});
		if (sig > fSigMax) fSigMax = sig;
	} else if ((word == "&") || (word == "*")) {
		fOps.push_back({kTsOpAnd, 0});
	} else if ((word == "|") || (word == "+")) {
		fOps.push_back({kTsOpOr, 0});
	} else if ((word == "!") || (word == "^")) {
		fOps.push_back({kTsOpNot, 0});
	} else if (word == "x") {
		fOps.push_back({kTsOpSwap, 0});
	} else if ((word == "d") || (word == "p")) {
		fOps.push_back({kTsOpDrop, 0});
	} else {
		std::cout << "#E Unknown token : " << word << std::endl;
	}

	return fOps.size();
}

void Trigger::MakeTable(std::string & formula)
{
	// Cleared before parsing, so that a rejected expression cannot leave the
	// previous program running.
	fOps.clear();
	fCOps.clear();
	fCompiled = false;
	fCanSkipZero = false;
	fUsedSig.clear();
	fSigMax = -1;
	fGpuNeedsSync = true;

	std::string rpn;
	if ((formula.substr(0, 3) == "RPN") || (formula.substr(0, 3) == "rpn")) {
		rpn = formula.substr(3);
	} else {
		// Not caught: Trigger.cxx lets the parse error out of InitTask, so a
		// bad expression has to stop the device rather than leave it running
		// with an empty program and no trigger at all.
		rpn = infixtorpn(formula)->to_rpn();
	}

	std::string word;
	for (char c : rpn) {
		if (c == ' ' || c == '\t' || c == '\n') {
			ComPushBack(word);
			word.clear();
		} else {
			word += c;
		}
	}
	ComPushBack(word);

	EnsureRegion();
	CompileExpr();

	// CompileExpr falls back to the RPN interpreter when its own program is too
	// deep, so this is the depth the interpreter would then need.
	if (! fCompiled && (StackDepth() > TRIGGERSLICE_GPU_STACK)) {
		std::cout << "#E TriggerSlice: expression stack "
			<< StackDepth() << " exceeds " << TRIGGERSLICE_GPU_STACK
			<< std::endl;
	}

	// Trigger.cxx sizes its truth table from the expression alone, and its
	// LookUp() answers false for any bit pattern above it, so there it is the
	// signals past the expression that veto the bin. Nothing vetoes here, so
	// say so rather than disagree quietly.
	if (fEntryRowMax > (fSigMax + 1)) {
		std::cout << "#W TriggerSlice: " << fEntryRowMax
			<< " signals registered but the expression stops at "
			<< fSigMax << "; signals " << (fSigMax + 1) << ".."
			<< (fEntryRowMax - 1) << " are ignored here, while Trigger.cxx"
			<< " vetoes every time bin in which they fire" << std::endl;
	}

	fGpuNeedsSync = true;
	if (fBackend == kGpu && ! SyncGpu()) DropToCpu("configuration failed");

	return;
}

void Trigger::CompileExpr()
{
	fCOps.clear();
	fUsedSig.clear();
	fCompiled = false;
	fCanSkipZero = false;

	if (fOps.empty()) return;

	bool hasNot = false;
	for (const auto &op : fOps) {
		if (op.code == kTsOpSwap || op.code == kTsOpDrop) return;
		if (op.code == kTsOpNot) hasNot = true;
	}

	struct Node {
		int code;
		int sig;
		std::vector<int> ch;
	};
	std::vector<Node> nodes;
	std::vector<int> st;

	auto add = [&](int code, int sig, std::initializer_list<int> ch) -> int {
		Node n;
		n.code = code;
		n.sig = sig;
		n.ch.assign(ch);
		nodes.push_back(std::move(n));
		return static_cast<int>(nodes.size()) - 1;
	};

	for (const auto &op : fOps) {
		if (op.code == kTsOpSig) {
			st.push_back(add(kTsOpSig, op.sig, {}));
		} else if (op.code == kTsOpNot) {
			if (st.empty()) return;
			const int a = st.back(); st.pop_back();
			st.push_back(add(kTsOpNot, 0, {a}));
		} else if (op.code == kTsOpAnd || op.code == kTsOpOr) {
			if (st.size() < 2) return;
			const int b = st.back(); st.pop_back();
			const int a = st.back(); st.pop_back();
			st.push_back(add(op.code, 0, {a, b}));
		} else {
			return;
		}
	}

	if (st.size() != 1) return;

	std::function<void(int, int, std::vector<int> &)> collect =
		[&](int ni, int code, std::vector<int> &out) {
			const Node &n = nodes[ni];
			if (n.code == code) {
				for (int c : n.ch) collect(c, code, out);
			} else {
				out.push_back(ni);
			}
		};

	// An n-ary AND/OR takes all of its operands off the stack, so emitting a
	// wide node as "push every child, then one operator" would need a stack as
	// deep as the child count: an OR over 94 signals overruns the 64-slot
	// evaluator on both backends. Fold in chunks against a running accumulator
	// instead. The opcode count is unchanged and the depth stays at kChunk + 1
	// however wide the node is.
	const int kChunk = 8;

	std::function<void(int)> emit = [&](int ni) {
		const Node &n = nodes[ni];
		if (n.code == kTsOpSig) {
			fCOps.push_back({kTsOpSig, n.sig});
			fUsedSig.push_back(n.sig);
			return;
		}
		if (n.code == kTsOpNot) {
			emit(n.ch[0]);
			fCOps.push_back({kTsOpNot, 1});
			return;
		}

		std::vector<int> kids;
		collect(ni, n.code, kids);

		const int m = static_cast<int>(kids.size());
		for (int i = 0 ; i < m ; i += kChunk) {
			const int cnt = ((m - i) < kChunk) ? (m - i) : kChunk;
			for (int k = 0 ; k < cnt ; k++) emit(kids[i + k]);
			if (cnt > 1) fCOps.push_back({n.code, cnt});
			if (i > 0) fCOps.push_back({n.code, 2});
		}
	};

	emit(st[0]);
	std::sort(fUsedSig.begin(), fUsedSig.end());
	fUsedSig.erase(std::unique(fUsedSig.begin(), fUsedSig.end()), fUsedSig.end());

	fCompiled = ! fCOps.empty();

	// Last guard on the evaluator stack. Chunked folding keeps a wide node well
	// inside it, so this needs a formula nested dozens of levels deep; hand it
	// to the RPN interpreter rather than overrun a fixed array.
	if (fCompiled && (CompiledDepth() > TRIGGERSLICE_GPU_STACK)) {
		std::cout << "#E TriggerSlice: compiled expression needs a stack of "
			<< CompiledDepth() << " > " << TRIGGERSLICE_GPU_STACK
			<< ", using the RPN interpreter" << std::endl;
		fCOps.clear();
		fCompiled = false;
	}

	// A zero slice evaluates to 0 only when the formula has no NOT.
	fCanSkipZero = fCompiled && ! hasNot;

	return;
}

// Peak stack of the compiled program, which differs from StackDepth() because
// an n-ary operator pops n values at once.
int Trigger::CompiledDepth() const
{
	int sp = 0;
	int peak = 0;

	for (const auto &op : fCOps) {
		if (op.code == kTsOpSig) {
			sp++;
		} else if (op.code == kTsOpAnd || op.code == kTsOpOr) {
			sp -= (op.n - 1);
		}
		if (sp > peak) peak = sp;
	}

	return peak;
}

int Trigger::StackDepth() const
{
	int sp = 0;
	int peak = 0;

	for (const auto &op : fOps) {
		switch (op.code) {
		case kTsOpSig: sp++; break;
		case kTsOpAnd:
		case kTsOpOr:
			if (sp > 1) sp--;
			break;
		case kTsOpDrop:
			if (sp > 0) sp--;
			break;
		default: break;
		}
		if (sp > peak) peak = sp;
	}

	return peak;
}

bool Trigger::SyncGpu()
{
	if (! fGpuNeedsSync) return true;

	// The device state is one global, so a second instance would overwrite the
	// signal table of the first. Keep it on the CPU instead.
	if (! fGpuOwned && GpuUsers() > 0) {
		std::cout << "#W TriggerSlice: another Trigger already holds the GPU"
			<< std::endl;
		return false;
	}

	// The device evaluator has the same fixed stack, and runs the compiled
	// program when there is one.
	const int need = fCompiled ? CompiledDepth() : StackDepth();
	if (need > TRIGGERSLICE_GPU_STACK) {
		std::cout << "#W TriggerSlice: expression needs a stack of "
			<< need << " > " << TRIGGERSLICE_GPU_STACK << std::endl;
		return false;
	}

	EnsureRegion();

	// one slot per FEM, in the order of the FEM id
	fFemSlot.clear();
	fGEntOff.clear();
	fGEntCnt.clear();
	fGEntCh.clear();
	fGEntDelay.clear();
	fGEntSig.clear();

	for (const auto &kv : fEntryCh) {
		const uint32_t fem = kv.first;
		const std::vector<int> &chs = kv.second;
		const std::vector<int> &dls = fEntryChDelay[fem];
		const std::vector<int> &sgs = fEntryChSig[fem];

		fFemSlot[fem] = static_cast<int>(fGEntOff.size());
		fGEntOff.emplace_back(static_cast<int32_t>(fGEntCh.size()));
		fGEntCnt.emplace_back(static_cast<int32_t>(chs.size()));
		for (size_t i = 0 ; i < chs.size() ; i++) {
			fGEntCh.emplace_back(chs[i]);
			fGEntDelay.emplace_back((i < dls.size()) ? dls[i] : 0);
			fGEntSig.emplace_back((i < sgs.size()) ? sgs[i] : -1);
		}
	}

	fGOpCode.clear();
	fGOpSig.clear();
	for (const auto &op : fOps) {
		fGOpCode.emplace_back(op.code);
		fGOpSig.emplace_back(op.sig);
	}

	fGCOpCode.clear();
	fGCOpN.clear();
	for (const auto &op : fCOps) {
		fGCOpCode.emplace_back(op.code);
		fGCOpN.emplace_back(op.n);
	}

	if (! TriggerSliceGpu::Init(fNword, fNsignal, fTimeRegionSize)) return false;

	if (! TriggerSliceGpu::SetConfig(
		fGEntOff.data(), fGEntCnt.data(), static_cast<int>(fGEntOff.size()),
		fGEntCh.data(), fGEntDelay.data(), fGEntSig.data(),
		static_cast<int>(fGEntCh.size()),
		fGOpCode.data(), fGOpSig.data(), static_cast<int>(fGOpCode.size()),
		fCanSkipZero ? 1 : 0,
		fGCOpCode.data(), fGCOpN.data(), static_cast<int>(fGCOpCode.size()))) {
		return false;
	}

	if (! fGpuOwned) {
		fGpuOwned = true;
		GpuUsers()++;
	}
	fGpuNeedsSync = false;

	return true;
}

void Trigger::EnsureRegion()
{
	int nsig = fEntryRowMax;
	if ((fSigMax + 1) > nsig) nsig = fSigMax + 1;
	if (nsig < 1) nsig = 1;

	if ((nsig == fNsignal)
		&& (fSlice.size() == static_cast<size_t>(nsig) * fNword)
		&& (fMarkedWords.size() == (fNword + 31) / 32)) {
		return;
	}

	fNsignal = nsig;
	fSlice.assign(static_cast<size_t>(fNsignal) * fNword, 0);
	fMarkedWords.assign((fNword + 31) / 32, 0);
	fGpuNeedsSync = true;

	return;
}

void Trigger::InitParam()
{
	fHits.clear();
	EnsureRegion();
	std::fill(fSlice.begin(), fSlice.end(), 0);
	std::fill(fMarkedWords.begin(), fMarkedWords.end(), 0);

	return;
}

bool Trigger::SetTimeRegion(int size)
{
	if (size <= 0) return false;

	fTimeRegionSize = static_cast<uint32_t>(size);
	fNword = (fTimeRegionSize + 31) / 32;
	fSlice.clear();
	fNsignal = 0;
	fGpuNeedsSync = true;
	EnsureRegion();

	return true;
}

// A full pass, even though fMarkedWords names the only words that can be nonzero.
// Clearing just those means one strided store per signal per word, and with the
// signal rows fNword apart that touches more cache lines than the whole region
// does sequentially: measured 23.3 us against 17.0 us per heartbeat on run 31 at
// 14 signals, where 7.5% of the words are marked. Shrinking the region by putting
// several channels on one signal is what actually brings this down.
void Trigger::CleanUpTimeRegion()
{
	EnsureRegion();
	std::fill(fSlice.begin(), fSlice.end(), 0);
	std::fill(fMarkedWords.begin(), fMarkedWords.end(), 0);

	return;
}

uint32_t *Trigger::GetTimeRegion()
{
	// Bit-sliced buffer. Word w of signal s is at index s * GetWordCount() + w,
	// so this is NOT the per-time-bin array Trigger.cxx returns.

	return fSlice.data();
}

uint32_t Trigger::GetTimeRegionSize()
{
	return fTimeRegionSize;
}

// Registers one channel on signal row sig. Channels sharing a row are OR'd by
// the marking itself, so the expression names rows rather than channels and its
// cost stops growing with the channel count.
//
// This is the only way to register. Giving every channel its own row is the
// degenerate case of it, which is what a formula needs whenever it has to tell
// the channels apart, so the caller numbers the rows and nothing here has to
// keep a second count.
void Trigger::EntryTo(int sig, uint32_t fem, int ch, int offset)
{
	if (sig < 0) return;

	fEntryCh[fem].emplace_back(ch);
	fEntryChDelay[fem].emplace_back(offset);
	fEntryChSig[fem].emplace_back(sig);
	if (sig >= fEntryRowMax) fEntryRowMax = sig + 1;

	fGpuNeedsSync = true;
	fChIndexNeedsBuild = true;
	EnsureRegion();

	return;
}

void Trigger::ClearEntry()
{
	fEntryCh.clear();
	fEntryChDelay.clear();
	fEntryChSig.clear();
	fEntryRowMax = 0;
	fGpuNeedsSync = true;
	fChIndexNeedsBuild = true;
	fChIndex.clear();
	fChFlat.clear();

	return;
}

// Channels above TRIGGERSLICE_MAX_CH keep a count of zero and fall back to the
// scan in Mark(), so the table never decides which entries exist.
void Trigger::BuildChIndex()
{
	fChIndex.clear();

	for (const auto &kv : fEntryCh) {
		const std::vector<int> &chs = kv.second;
		const std::vector<int> &dls = fEntryChDelay[kv.first];
		const std::vector<int> &sgs = fEntryChSig[kv.first];

		ChIndex ix;
		ix.slot.assign(TRIGGERSLICE_MAX_CH, {0, 0});

		for (size_t i = 0 ; i < chs.size() ; i++) {
			if (chs[i] >= 0 && chs[i] < TRIGGERSLICE_MAX_CH) ix.slot[chs[i]].cnt++;
		}

		int32_t at = 0;
		for (int c = 0 ; c < TRIGGERSLICE_MAX_CH ; c++) {
			ix.slot[c].head = at;
			at += ix.slot[c].cnt;
		}

		ix.ent.assign(static_cast<size_t>(at), {0, 0});
		std::vector<int32_t> fill(TRIGGERSLICE_MAX_CH, 0);
		for (int c = 0 ; c < TRIGGERSLICE_MAX_CH ; c++) fill[c] = ix.slot[c].head;
		for (size_t i = 0 ; i < chs.size() ; i++) {
			if (chs[i] < 0 || chs[i] >= TRIGGERSLICE_MAX_CH) continue;
			ix.ent[static_cast<size_t>(fill[chs[i]]++)]
				= {static_cast<int32_t>(sgs[i]), static_cast<int32_t>(dls[i])};
		}

		fChIndex[kv.first] = std::move(ix);
	}

	// Pointers into fChIndex, which is not touched again until the next rebuild.
	fChFlat.clear();
	for (const auto &kv : fChIndex) fChFlat.push_back({kv.first, &kv.second});

	fChIndexNeedsBuild = false;

	return;
}

bool Trigger::CheckEntryFEM(uint32_t fem)
{
	return (fEntryCh.count(fem) >= 1);
}

void Trigger::MarkWindow(int sig, uint32_t hit)
{
	const uint32_t half = static_cast<uint32_t>(fMarkLen / 2);

	// Same acceptance as Trigger.cxx, down to the unsigned wrap of
	// fTimeRegionSize - half: a hit whose delayed time wrapped below zero, or
	// which sits in the last half-window, marks nothing at all.
	if (hit >= fTimeRegionSize - half) return;
	if (sig < 0 || sig >= fNsignal) return;

	uint32_t *base = fSlice.data() + static_cast<size_t>(sig) * fNword;

	// Half-window at least as wide as the whole region. The guard above no
	// longer bounds hit + half, and Trigger.cxx tests each bin of the window
	// on its own with hit + k wrapping, so walk the window bin by bin rather
	// than treating it as one interval. Only reachable with a trigger width
	// above twice the region, and it keeps the writes inside the row.
	if (half >= fTimeRegionSize) {
		const int khalf = fMarkLen / 2;
		for (int k = -khalf ; k <= khalf ; k++) {
			const uint32_t b = hit + static_cast<uint32_t>(k);
			if (b >= fTimeRegionSize) continue;
			base[b >> 5] |= 1u << (b & 31);
			AddMarkedWords(b >> 5, b >> 5);
		}
		return;
	}

	const uint32_t lo = (hit >= half) ? (hit - half) : 0;
	const uint32_t hi = hit + half;
	const uint32_t wlo = lo / 32;
	const uint32_t whi = hi / 32;

	if (wlo == whi) {
		const uint32_t nbit = hi - lo + 1;
		const uint32_t mask = (nbit >= 32)
			? 0xffffffffu
			: (((1u << nbit) - 1u) << (lo % 32));
		base[wlo] |= mask;
		AddMarkedWords(wlo, whi);
		return;
	}

	base[wlo] |= 0xffffffffu << (lo % 32);
	for (uint32_t w = wlo + 1 ; w < whi ; w++) base[w] = 0xffffffffu;
	const uint32_t tail = hi % 32;
	base[whi] |= (tail == 31) ? 0xffffffffu : ((1u << (tail + 1)) - 1u);
	AddMarkedWords(wlo, whi);

	return;
}

void Trigger::AddMarkedWords(uint32_t wlo, uint32_t whi)
{
	if (fMarkedWords.empty()) return;
	if (whi >= fNword && fNword > 0) whi = fNword - 1;

	for (uint32_t w = wlo ; w <= whi ; w++) {
		fMarkedWords[w >> 5] |= 1u << (w & 31);
	}

	return;
}

// Leading edges, which is all Mark() looks at. UnpackTdc.h fills a struct of
// eight to ten ints and walks a chain of type tests to get there, and a
// heartbeat carries some thousands of words, so this does the same bit
// extraction for the one case that matters. Field positions and T_TDC come from
// that header, and the differential test against Trigger.cxx - which keeps using
// it - is what holds the two in step.
namespace TsLead {

// Top six bits in every layout, and the same value in all four.
constexpr uint64_t kType = (0x2c >> 2);

// TDC64H and TDC64H_V3: ch at 51..57, tdc 0..28, tdc4n = tdc >> 12.
inline bool H(uint64_t d, int &ch, int &tdc4n)
{
	if ((d >> 58) != kType) return false;
	ch = static_cast<int>((d >> 51) & 0x7f);
	tdc4n = static_cast<int>((d >> 12) & 0x1ffff);

	return true;
}

// TDC64L: ch at 51..57, tdc 16..34, tdc4n = tdc >> 2.
//
// This is the only one of the four whose namespace holds two layouts, v1 and an
// inline v2, so TDC64L::Unpack means v2 and v1 is reachable only by naming it.
// The fields below are v2's. Reading v1 off the top of the header put the time
// at bits 26..42 and the differential test caught it on 20883 of 123200 cases.
inline bool L(uint64_t d, int &ch, int &tdc4n)
{
	if ((d >> 58) != kType) return false;
	ch = static_cast<int>((d >> 51) & 0x7f);
	tdc4n = static_cast<int>((d >> 18) & 0x1ffff);

	return true;
}

// TDC64L_V3: ch at 50..57, tdc 15..33, tdc4n = tdc >> 2.
inline bool LV3(uint64_t d, int &ch, int &tdc4n)
{
	if ((d >> 58) != kType) return false;
	ch = static_cast<int>((d >> 50) & 0xff);
	tdc4n = static_cast<int>((d >> 17) & 0x1ffff);

	return true;
}

} // namespace TsLead

void Trigger::Mark(unsigned char *pdata, int len, int fem, uint32_t type)
{
	const uint32_t ufem = static_cast<uint32_t>(fem);

	if (fChIndexNeedsBuild) BuildChIndex();

	// Linear over the configured modules, of which there are a handful, rather
	// than a map lookup for every segment of the heartbeat including the many
	// that are not configured at all.
	const ChIndex *ix = nullptr;
	for (const ChFlat &f : fChFlat) {
		if (f.fem == ufem) {
			ix = f.ix;
			break;
		}
	}
	if (ix == nullptr) return;

	uint64_t *tdcval = reinterpret_cast<uint64_t *>(pdata);
	const unsigned int nword = len / sizeof(uint64_t);

	// One pass over the words, then a lookup of the registered signals. The
	// per-bin layout of Trigger.cxx unpacks once per signal instead.
	//
	// The module type is the same for every word of the segment, so it picks the
	// decoder once and each one gets its own loop. Testing it per word left the
	// chain of comparisons in the inner loop, and with a heartbeat carrying some
	// thousands of words that is the hot loop of the whole engine.
	// The channel field is seven or eight bits wide in every layout, so the
	// table always covers it and a channel can never fall outside.
	static_assert(TRIGGERSLICE_MAX_CH >= 256, "channel table must cover the field");

	auto sweep = [&](auto decode) {
		const ChSlot *slot = ix->slot.data();
		const ChEnt *ent = ix->ent.data();

		for (unsigned int j = 0 ; j < nword ; j++) {
			int ch = -1;
			int tdc4n = -1;

			if (! decode(tdcval[j], ch, tdc4n)) continue;

			const ChSlot s = slot[ch];
			for (int32_t k = 0 ; k < s.cnt ; k++) {
				const ChEnt e = ent[s.head + k];
				MarkWindow(e.sig, static_cast<uint32_t>(tdc4n + e.dly));
			}
		}
	};

	// Wrapped in a lambda rather than passed as a function pointer, so that each
	// decoder is a distinct type and gets inlined into its own loop.
	if (type == SubTimeFrame::TDC64H) {
		sweep([](uint64_t v, int &c, int &t) {return TsLead::H(v, c, t);});
	} else if (type == SubTimeFrame::TDC64L) {
		sweep([](uint64_t v, int &c, int &t) {return TsLead::L(v, c, t);});
	} else if (type == SubTimeFrame::TDC64H_V3) {
		sweep([](uint64_t v, int &c, int &t) {return TsLead::H(v, c, t);});
	} else if (type == SubTimeFrame::TDC64L_V3) {
		sweep([](uint64_t v, int &c, int &t) {return TsLead::LV3(v, c, t);});
	}

	return;
}

// Hot path: a fixed stack, no vector, no per-op bounds checks. MakeTable has
// already built a valid RPN sequence whose depth fits in TRIGGERSLICE_GPU_STACK.
inline uint32_t Trigger::EvalCompiled(uint32_t w) const
{
	const uint32_t *slice = fSlice.data();
	const uint32_t nword = fNword;
	const COp *ops = fCOps.data();
	const int nop = static_cast<int>(fCOps.size());

	uint32_t st[TRIGGERSLICE_GPU_STACK];
	int sp = 0;

	for (int i = 0 ; i < nop ; i++) {
		const int code = ops[i].code;
		const int n = ops[i].n;
		if (code == kTsOpSig) {
			st[sp++] = slice[static_cast<size_t>(n) * nword + w];
		} else if (code == kTsOpAnd) {
			uint32_t a = ~0u;
			for (int k = 0 ; k < n ; k++) a &= st[--sp];
			st[sp++] = a;
		} else if (code == kTsOpOr) {
			uint32_t a = 0;
			for (int k = 0 ; k < n ; k++) a |= st[--sp];
			st[sp++] = a;
		} else if (code == kTsOpNot) {
			if (sp > 0) st[sp - 1] = ~st[sp - 1];
		}
	}

	return (sp > 0) ? st[sp - 1] : 0u;
}

#ifdef TRIGGERSLICE_AVX2
inline void Trigger::EvalCompiled8(uint32_t w0, uint32_t r[8]) const
{
	const uint32_t *slice = fSlice.data();
	const uint32_t nword = fNword;
	const COp *ops = fCOps.data();
	const int nop = static_cast<int>(fCOps.size());

	__m256i st[TRIGGERSLICE_GPU_STACK];
	int sp = 0;

	for (int i = 0 ; i < nop ; i++) {
		const int code = ops[i].code;
		const int n = ops[i].n;
		if (code == kTsOpSig) {
			const uint32_t *p = slice + static_cast<size_t>(n) * nword + w0;
			st[sp++] = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(p));
		} else if (code == kTsOpAnd) {
			__m256i a = _mm256_set1_epi32(-1);
			for (int k = 0 ; k < n ; k++) a = _mm256_and_si256(a, st[--sp]);
			st[sp++] = a;
		} else if (code == kTsOpOr) {
			__m256i a = _mm256_setzero_si256();
			for (int k = 0 ; k < n ; k++) a = _mm256_or_si256(a, st[--sp]);
			st[sp++] = a;
		} else if (code == kTsOpNot) {
			if (sp > 0) {
				st[sp - 1] = _mm256_xor_si256(st[sp - 1], _mm256_set1_epi32(-1));
			}
		}
	}

	if (sp > 0) {
		_mm256_storeu_si256(reinterpret_cast<__m256i *>(r), st[sp - 1]);
	} else {
		for (int i = 0 ; i < 8 ; i++) r[i] = 0;
	}
}
#endif

inline uint32_t Trigger::EvalWord(uint32_t w)
{
	if (fCompiled) return EvalCompiled(w);

	const Op *ops = fOps.data();
	const int nop = static_cast<int>(fOps.size());
	const uint32_t *slice = fSlice.data();
	const uint32_t nword = fNword;

	uint32_t st[TRIGGERSLICE_GPU_STACK];
	int sp = 0;

	for (int i = 0 ; i < nop ; i++) {
		switch (ops[i].code) {
		case kTsOpSig:
			if (sp < TRIGGERSLICE_GPU_STACK) {
				st[sp++] = slice[static_cast<size_t>(ops[i].sig) * nword + w];
			}
			break;
		case kTsOpAnd:
			if (sp > 1) { const uint32_t a = st[--sp]; st[sp - 1] &= a; }
			break;
		case kTsOpOr:
			if (sp > 1) { const uint32_t a = st[--sp]; st[sp - 1] |= a; }
			break;
		case kTsOpNot:
			if (sp > 0) st[sp - 1] = ~st[sp - 1];
			break;
		case kTsOpSwap:
			if (sp > 1) {
				const uint32_t a = st[sp - 1];
				st[sp - 1] = st[sp - 2];
				st[sp - 2] = a;
			}
			break;
		case kTsOpDrop:
			if (sp > 0) --sp;
			break;
		default:
			break;
		}
	}

	return (sp > 0) ? st[sp - 1] : 0u;
}

std::vector<uint32_t> *Trigger::Scan()
{
	fHits.clear();

	if (fOps.empty()) return &fHits;

	const uint32_t nword = fNword;
	const bool skip = fCanSkipZero && ! fMarkedWords.empty();

	uint32_t last_w = ~0u;
	uint32_t last_r = 0;

	auto take = [&](uint32_t w) {
		const uint32_t carry = (w > 0 && last_w == w - 1) ? (last_r >> 31) : 0u;
		const uint32_t r = EvalWord(w);
		uint32_t rise = r & ~((r << 1) | carry);
		if (w == 0) rise &= ~0x1u;
		while (rise != 0) {
			const uint32_t j = __builtin_ctz(rise);
			const uint32_t bin = (w * 32) + j;
			if (bin < fTimeRegionSize) fHits.emplace_back(bin);
			rise &= rise - 1;
		}
		last_w = w;
		last_r = r;
	};

	if (skip) {
		const uint32_t npack = static_cast<uint32_t>(fMarkedWords.size());
		for (uint32_t p = 0 ; p < npack ; p++) {
			uint32_t bits = fMarkedWords[p];
			while (bits != 0) {
				const uint32_t j = __builtin_ctz(bits);
				const uint32_t w = (p * 32) + j;
				if (w < nword) take(w);
				bits &= bits - 1;
			}
		}
#ifdef TRIGGERSLICE_AVX2
	} else if (fCompiled && nword >= 8) {
		uint32_t w = 0;
		for ( ; w + 8 <= nword ; w += 8) {
			uint32_t rs[8];
			EvalCompiled8(w, rs);
			for (int i = 0 ; i < 8 ; i++) {
				const uint32_t ww = w + static_cast<uint32_t>(i);
				const uint32_t carry = (ww > 0 && last_w == ww - 1) ? (last_r >> 31) : 0u;
				uint32_t rise = rs[i] & ~((rs[i] << 1) | carry);
				if (ww == 0) rise &= ~0x1u;
				while (rise != 0) {
					const uint32_t j = __builtin_ctz(rise);
					const uint32_t bin = (ww * 32) + j;
					if (bin < fTimeRegionSize) fHits.emplace_back(bin);
					rise &= rise - 1;
				}
				last_w = ww;
				last_r = rs[i];
			}
		}
		for ( ; w < nword ; w++) take(w);
#endif
	} else {
		for (uint32_t w = 0 ; w < nword ; w++) take(w);
	}

	return &fHits;
}

// A half-window at least as wide as the time region makes Trigger.cxx mark the
// window bin by bin with its own unsigned wrap, which only the CPU engine here
// reproduces. Needs a trigger width above twice the region, so warn once and
// settle on the CPU rather than let the two backends disagree.
bool Trigger::GpuWindowOk()
{
	if (static_cast<uint32_t>(fMarkLen / 2) < fTimeRegionSize) return true;

	std::cout << "#W TriggerSlice: trigger width " << fMarkLen
		<< " covers the whole time region " << fTimeRegionSize
		<< ", using the CPU engine" << std::endl;
	fBackend = kCpu;

	return false;
}

bool Trigger::ExecGpuBatch(std::vector<std::vector<struct HBFIndex> > &batch,
	std::vector<std::vector<uint32_t> > &hitsPerHbf)
{
	if (! SyncGpu()) return false;

	const int B = static_cast<int>(batch.size());
	hitsPerHbf.assign(B > 0 ? B : 0, {});
	if (B < 1) return true;

	fSegData.clear();
	fSegNword.clear();
	fSegSlot.clear();
	fSegHbf.clear();
	fSegFemType.clear();

	for (int h = 0 ; h < B ; h++) {
		for (auto &seg : batch[h]) {
			auto it = fFemSlot.find(seg.femId);
			if (it == fFemSlot.end()) continue;
			const int n = seg.size / static_cast<int>(sizeof(uint64_t));
			if (n <= 0) continue;

			fSegData.emplace_back(reinterpret_cast<const uint64_t *>(seg.data));
			fSegNword.emplace_back(n);
			fSegSlot.emplace_back(it->second);
			fSegHbf.emplace_back(h);
			fSegFemType.emplace_back(seg.femType);
		}
	}

	return TriggerSliceGpu::MarkScanBatch(B,
		fSegData.data(), fSegNword.data(),
		fSegSlot.data(), fSegFemType.data(), fSegHbf.data(),
		static_cast<int>(fSegData.size()), fMarkLen, hitsPerHbf);
}

bool Trigger::ExecGpu(std::vector<struct HBFIndex> &hbf)
{
	std::vector<std::vector<struct HBFIndex> > batch(1, hbf);
	std::vector<std::vector<uint32_t> > out;
	if (! ExecGpuBatch(batch, out)) return false;
	fHits = out.empty() ? std::vector<uint32_t>() : std::move(out[0]);
	return true;
}

void Trigger::ExecBatch(std::vector<std::vector<struct HBFIndex> > &batch,
	std::vector<std::vector<uint32_t> > &hitsPerHbf)
{
	if (fBackend == kGpu && GpuWindowOk()) {
		if (ExecGpuBatch(batch, hitsPerHbf)) return;
		DropToCpu("batch exec failed");
	}

	const int B = static_cast<int>(batch.size());
	hitsPerHbf.assign(B > 0 ? B : 0, {});
	for (int h = 0 ; h < B ; h++) {
		hitsPerHbf[h] = *Trigger::Exec(batch[h]);
	}

	return;
}

std::vector<uint32_t> *Trigger::Exec(std::vector<struct HBFIndex> &hbf)
{
	if (fBackend == kGpu && GpuWindowOk()) {
		if (ExecGpu(hbf)) return &fHits;
		DropToCpu("exec failed");
	}

	Trigger::CleanUpTimeRegion();

	for (auto &seg : hbf) {
		Trigger::Mark(seg.data, seg.size, seg.femId, seg.femType);
	}

	return Trigger::Scan();
}

std::string Trigger::Info()
{
	std::ostringstream os;
	os << "TriggerSlice"
		<< " backend: " << ((fBackend == kGpu) ? "gpu" : "cpu")
		<< " bins: " << std::dec << fTimeRegionSize
		<< " words/signal: " << fNword
		<< " signals: " << fNsignal
		<< " (channels " << ChannelCount()
		<< ", formula max " << fSigMax << ")"
		<< " region: " << (GetRegionBytes() / 1024) << " kB"
		<< " markLen: " << fMarkLen
		<< " ops: " << fOps.size()
		<< " stack: " << (fCompiled ? CompiledDepth() : StackDepth())
		<< " compiled: " << (fCompiled ? "and-or" : "rpn")
		<< " skip-zero: " << (fCanSkipZero ? "yes" : "no")
		<< " gpu-scan: batch+marked-clear+compact+compiled+ordered"
#ifdef TRIGGERSLICE_AVX2
		<< " avx2: yes"
#else
		<< " avx2: no"
#endif
		<< " used: " << fUsedSig.size();

	return os.str();
}

void Trigger::Dump()
{
	std::cout << "#" << Trigger::Info() << std::endl;

	return;
}

#endif  // __CUDACC__

#endif //TriggerSlice_cxx
