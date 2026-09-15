#include "TriggerCuda.h"

#ifdef USE_CUDA

#include <algorithm>
#include <stdexcept>
#include <cstring>

#include "SubTimeFrameHeader.h" // for SubTimeFrame::TDC* constants

// -------- Device-side minimal unpackers (derived from UnpackTdc.h) --------
namespace dev_unpack {

struct tdc_basic {
    int type;
    int ch;
    int tdc4n;
};

// Canonical "T_TDC" used by Trigger.cxx comparisons
static constexpr int kT_TDC = ((0x2cU) >> 2);

// TDC64H
__device__ __forceinline__ int unpack_tdc64h(uint64_t data, tdc_basic* tdc)
{
    const int type = (int)((data & 0xfc00'0000'0000'0000ULL) >> 58);
    tdc->type = type;
    if (type == kT_TDC) {
        tdc->ch   = (int)((data & 0x03f8'0000'0000'0000ULL) >> 51);
        const uint64_t tdcval = (data & 0x0000'0000'1fff'ffffULL);
        tdc->tdc4n = (int)(tdcval >> 12);
        return kT_TDC;
    }
    return type;
}

// TDC64L  (must match UnpackTdc.h TDC64L inline namespace v2)
__device__ __forceinline__ int unpack_tdc64l(uint64_t data, tdc_basic* tdc)
{
    const int type = (int)((data & 0xfc00'0000'0000'0000ULL) >> 58);
    tdc->type = type;
    if (type == kT_TDC) {   // leading edge only (CPU Mark checks == T_TDC)
        tdc->ch    = (int)((data & 0x03f8'0000'0000'0000ULL) >> 51);
        tdc->tdc4n = (int)((data & 0x0000'0007'fffc'0000ULL) >> 18);
        return kT_TDC;
    }
    return type;
}

// V3 HR: CPU Mark only marks leading edges (Unpack() == T_TDC == 0x0b).
// Trailing/threshold words are ignored, so accept ONLY the leading type here.
__device__ __forceinline__ int unpack_tdc64h_v3(uint64_t data, tdc_basic* tdc)
{
    const int type = (int)((data & 0xfc00'0000'0000'0000ULL) >> 58);
    tdc->type = type;
    if (type == kT_TDC) {   // T_TDC_L (leading) only
        tdc->ch   = (int)((data & 0x03f8'0000'0000'0000ULL) >> 51);
        const uint64_t tdcval = (data & 0x0000'0000'1fff'ffffULL);
        tdc->tdc4n = (int)(tdcval >> 12);
        return kT_TDC;
    }
    return type;
}

// V3 LR: 8-bit channel (>>50) and a different tdc4n field than HR/older LR.
// Must match UnpackTdc.h TDC64L_V3::Unpack exactly.
__device__ __forceinline__ int unpack_tdc64l_v3(uint64_t data, tdc_basic* tdc)
{
    const int type = (int)((data & 0xfc00'0000'0000'0000ULL) >> 58);
    tdc->type = type;
    if (type == kT_TDC) {   // T_TDC_L (leading) only
        tdc->ch    = (int)((data & 0x03fc'0000'0000'0000ULL) >> 50);
        tdc->tdc4n = (int)((data & 0x0000'0003'fffe'0000ULL) >> 17);
        return kT_TDC;
    }
    return type;
}

} // namespace dev_unpack

// -------- Kernel: unpack + mark (atomicOr scatter) --------
// One thread per word. After unpacking the channel, the thread walks the
// (small) per-FEM entry list and marks a window for EVERY matching entry, so
// the same channel registered with several bits/delays is handled just like the
// CPU loop "for each entry: if(tdc.ch==ch) ...".
__global__ void markUnpackKernel(uint32_t* timeRegion,
                                 uint32_t timeRegionSize,
                                 const uint64_t* words,
                                 uint32_t nWords,
                                 const int32_t*  entCh,
                                 const uint32_t* entBit,
                                 const int32_t*  entDelay,
                                 uint32_t nEnt,
                                 uint32_t femType,
                                 int halfLen)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= nWords) return;

    dev_unpack::tdc_basic tdc;
    tdc.type = 0; tdc.ch = -1; tdc.tdc4n = -1;

    const uint64_t w = words[idx];

    int rc = 0;
    if (femType == SubTimeFrame::TDC64H)         rc = dev_unpack::unpack_tdc64h(w, &tdc);
    else if (femType == SubTimeFrame::TDC64L)    rc = dev_unpack::unpack_tdc64l(w, &tdc);
    else if (femType == SubTimeFrame::TDC64H_V3) rc = dev_unpack::unpack_tdc64h_v3(w, &tdc);
    else if (femType == SubTimeFrame::TDC64L_V3) rc = dev_unpack::unpack_tdc64l_v3(w, &tdc);
    else return;

    if (rc != dev_unpack::kT_TDC) return;

    const int ch = tdc.ch;

    for (uint32_t e = 0; e < nEnt; e++) {
        if (entCh[e] != ch) continue;

        // CPU semantics: hit is uint32_t; drop the WHOLE hit unless
        //   0 <= hit < timeRegionSize - halfLen   (negative delay wraps -> drop)
        const int32_t hit_i = (int32_t)tdc.tdc4n + entDelay[e];
        if (hit_i < 0) continue;
        if ((uint32_t)hit_i >= timeRegionSize - (uint32_t)halfLen) continue;

        int start = hit_i - halfLen;
        if (start < 0) start = 0;               // low-edge clamp (matches CPU per-k check)
        const int end = hit_i + halfLen;        // guaranteed < timeRegionSize

        const uint32_t bit = entBit[e];
        for (int t = start; t <= end; t++) {
            atomicOr(&timeRegion[t], bit);
        }
    }
}

// -------- Kernel: scan rising edges using trigger LUT --------
__global__ void scanRisingKernel(const uint32_t* timeRegion,
                                 uint32_t timeRegionSize,
                                 const uint8_t* trigLut,
                                 uint32_t trigMapSize,
                                 uint32_t* outHits,
                                 uint32_t* outCount)
{
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= timeRegionSize - 1) return;

    const uint32_t a = timeRegion[i];
    const uint32_t b = timeRegion[i + 1];

    const uint8_t sa = (a < trigMapSize) ? trigLut[a] : 0;
    const uint8_t sb = (b < trigMapSize) ? trigLut[b] : 0;

    if ((sa == 0) && (sb != 0)) {
        // Rising edge at (i+1)
        const uint32_t idx = atomicAdd(outCount, 1u);
        outHits[idx] = i + 1;
    }
}

// -------- Batched kernels: one (B x regionSize) timeRegion, B independent HBFs --------
__device__ __forceinline__ int dev_unpack_dispatch(uint64_t w, uint32_t femType,
                                                    dev_unpack::tdc_basic* tdc)
{
    if (femType == SubTimeFrame::TDC64H)         return dev_unpack::unpack_tdc64h(w, tdc);
    else if (femType == SubTimeFrame::TDC64L)    return dev_unpack::unpack_tdc64l(w, tdc);
    else if (femType == SubTimeFrame::TDC64H_V3) return dev_unpack::unpack_tdc64h_v3(w, tdc);
    else if (femType == SubTimeFrame::TDC64L_V3) return dev_unpack::unpack_tdc64l_v3(w, tdc);
    return 0;
}

// One thread per word (over ALL segments of ALL HBFs in the batch). A binary
// search over segOff maps the word to its segment -> (hbf slice, entry slot).
__global__ void markBatchKernel(uint32_t* tr, uint32_t regionSize,
                                const uint64_t* words, uint32_t totalWords,
                                const int32_t* segOff, const int32_t* segHbf,
                                const int32_t* segSlot, int nSeg,
                                const uint32_t* femType, const int32_t* entOff,
                                const int32_t* entCnt, const int32_t* entCh,
                                const uint32_t* entBit, const int32_t* entDelay,
                                int halfLen)
{
    uint32_t w = blockIdx.x * blockDim.x + threadIdx.x;
    if (w >= totalWords) return;

    // largest s with segOff[s] <= w   (segOff has nSeg+1 entries, segOff[nSeg]=totalWords)
    int lo = 0, hi = nSeg;
    while (lo + 1 < hi) {
        int mid = (lo + hi) >> 1;
        if ((uint32_t)segOff[mid] <= w) lo = mid; else hi = mid;
    }
    const int seg  = lo;
    const int hbf  = segHbf[seg];
    const int slot = segSlot[seg];

    dev_unpack::tdc_basic tdc; tdc.type = 0; tdc.ch = -1; tdc.tdc4n = -1;
    const int rc = dev_unpack_dispatch(words[w], femType[slot], &tdc);
    if (rc != dev_unpack::kT_TDC) return;

    const int ch = tdc.ch;
    const int e0 = entOff[slot];
    const int e1 = e0 + entCnt[slot];
    const uint32_t base = (uint32_t)hbf * regionSize;

    for (int e = e0; e < e1; e++) {
        if (entCh[e] != ch) continue;
        const int32_t hit_i = (int32_t)tdc.tdc4n + entDelay[e];
        if (hit_i < 0) continue;
        if ((uint32_t)hit_i >= regionSize - (uint32_t)halfLen) continue;
        int start = hit_i - halfLen; if (start < 0) start = 0;
        const int end = hit_i + halfLen;
        const uint32_t bit = entBit[e];
        for (int t = start; t <= end; t++) atomicOr(&tr[base + t], bit);
    }
}

// One thread per (hbf,bin). Records rising edges as (time,hbf) pairs.
__global__ void scanBatchKernel(const uint32_t* tr, uint32_t regionSize, int B,
                                const uint8_t* lut, uint32_t mapSize,
                                uint32_t* outTime, int32_t* outHbf, uint32_t* outCount)
{
    uint64_t g = (uint64_t)blockIdx.x * blockDim.x + threadIdx.x;
    const uint64_t total = (uint64_t)regionSize * (uint64_t)B;
    if (g >= total) return;

    const uint32_t i = (uint32_t)(g % regionSize);
    if (i >= regionSize - 1) return;              // CPU scans i in [0, regionSize-2]
    const int hbf = (int)(g / regionSize);

    const uint32_t a = tr[g];
    const uint32_t b = tr[g + 1];                 // same slice: i+1 <= regionSize-1
    const uint8_t sa = (a < mapSize) ? lut[a] : 0;
    const uint8_t sb = (b < mapSize) ? lut[b] : 0;
    if ((sa == 0) && (sb != 0)) {
        const uint32_t idx = atomicAdd(outCount, 1u);
        outTime[idx] = i + 1;
        outHbf[idx]  = hbf;
    }
}

// -------- Host wrapper helpers --------
static inline void ck(cudaError_t e)
{
    if (e != cudaSuccess) throw std::runtime_error(cudaGetErrorString(e));
}

TriggerCuda::TriggerCuda(uint32_t timeRegionSize, uint32_t chMax)
    : fTimeRegionSize(timeRegionSize), fChMax(chMax)
{
    try {
        ck(cudaStreamCreate(&fStream));

        ck(cudaMalloc(&d_timeRegion, sizeof(uint32_t) * fTimeRegionSize));

        // scan buffers
        ensureHitCap(fTimeRegionSize);
        ck(cudaMalloc(&d_hitCount, sizeof(uint32_t)));

        ck(cudaStreamSynchronize(fStream));

        fOk = true;
    } catch (...) {
        fOk = false;
    }
}

TriggerCuda::~TriggerCuda()
{
    if (!fOk) return;
    if (d_hitCount) cudaFree(d_hitCount);
    if (d_hits) cudaFree(d_hits);
    if (d_words) cudaFree(d_words);
    if (d_trigLut) cudaFree(d_trigLut);
    if (d_entDelay) cudaFree(d_entDelay);
    if (d_entBit) cudaFree(d_entBit);
    if (d_entCh) cudaFree(d_entCh);
    if (d_timeRegion) cudaFree(d_timeRegion);
    // batch buffers
    if (d_trBatch)   cudaFree(d_trBatch);
    if (d_bWords)    cudaFree(d_bWords);
    if (d_segOff)    cudaFree(d_segOff);
    if (d_segHbf)    cudaFree(d_segHbf);
    if (d_segSlot)   cudaFree(d_segSlot);
    if (d_bFemType)  cudaFree(d_bFemType);
    if (d_bEntOff)   cudaFree(d_bEntOff);
    if (d_bEntCnt)   cudaFree(d_bEntCnt);
    if (d_bEntCh)    cudaFree(d_bEntCh);
    if (d_bEntBit)   cudaFree(d_bEntBit);
    if (d_bEntDelay) cudaFree(d_bEntDelay);
    if (d_bHitTime)  cudaFree(d_bHitTime);
    if (d_bHitHbf)   cudaFree(d_bHitHbf);
    if (d_bHitCount) cudaFree(d_bHitCount);
    if (fStream) cudaStreamDestroy(fStream);
}

void TriggerCuda::ensureWordCap(uint32_t nWords)
{
    if (nWords <= fWordCap) return;
    uint32_t newCap = std::max<uint32_t>(nWords, fWordCap * 2 + 4096);
    if (d_words) ck(cudaFree(d_words));
    ck(cudaMalloc(&d_words, sizeof(uint64_t) * newCap));
    fWordCap = newCap;
}

void TriggerCuda::ensureHitCap(uint32_t nHits)
{
    if (nHits <= fHitCap && d_hits != nullptr) return;
    if (d_hits) ck(cudaFree(d_hits));
    ck(cudaMalloc(&d_hits, sizeof(uint32_t) * nHits));
    fHitCap = nHits;
}

void TriggerCuda::ensureEntCap(uint32_t nEnt)
{
    if (nEnt <= fEntCap && d_entCh != nullptr) return;
    uint32_t newCap = std::max<uint32_t>(nEnt, fEntCap * 2 + 8);
    if (d_entCh)    ck(cudaFree(d_entCh));
    if (d_entBit)   ck(cudaFree(d_entBit));
    if (d_entDelay) ck(cudaFree(d_entDelay));
    ck(cudaMalloc(&d_entCh,    sizeof(int32_t)  * newCap));
    ck(cudaMalloc(&d_entBit,   sizeof(uint32_t) * newCap));
    ck(cudaMalloc(&d_entDelay, sizeof(int32_t)  * newCap));
    fEntCap = newCap;
}

void TriggerCuda::clearAsync()
{
    if (!fOk) return;
    ck(cudaMemsetAsync(d_timeRegion, 0, sizeof(uint32_t) * fTimeRegionSize, fStream));
}

void TriggerCuda::setChannelEntries(const std::vector<int32_t>&  chList,
                                    const std::vector<uint32_t>& bitList,
                                    const std::vector<int32_t>&  delayList)
{
    if (!fOk) return;
    if (chList.size() != bitList.size() || chList.size() != delayList.size()) {
        throw std::runtime_error("TriggerCuda::setChannelEntries: size mismatch");
    }

    fEntCount = (uint32_t)chList.size();
    if (fEntCount == 0) return; // nothing to mark for this FEM

    ensureEntCap(fEntCount);
    ck(cudaMemcpyAsync(d_entCh,    chList.data(),    sizeof(int32_t)  * fEntCount,
                       cudaMemcpyHostToDevice, fStream));
    ck(cudaMemcpyAsync(d_entBit,   bitList.data(),   sizeof(uint32_t) * fEntCount,
                       cudaMemcpyHostToDevice, fStream));
    ck(cudaMemcpyAsync(d_entDelay, delayList.data(), sizeof(int32_t)  * fEntCount,
                       cudaMemcpyHostToDevice, fStream));
    ck(cudaStreamSynchronize(fStream));
}

void TriggerCuda::setTriggerLUT(const std::vector<uint8_t>& trigLut)
{
    if (!fOk) return;
    if (trigLut.empty()) {
        throw std::runtime_error("TriggerCuda::setTriggerLUT: empty LUT");
    }

    fTrigMapSize = (uint32_t)trigLut.size();

    if (d_trigLut) {
        ck(cudaFree(d_trigLut));
        d_trigLut = nullptr;
    }

    ck(cudaMalloc(&d_trigLut, sizeof(uint8_t) * fTrigMapSize));
    ck(cudaMemcpyAsync(d_trigLut, trigLut.data(), sizeof(uint8_t) * fTrigMapSize,
                       cudaMemcpyHostToDevice, fStream));
    ck(cudaStreamSynchronize(fStream));
}

void TriggerCuda::markRawTdcWords(const unsigned char* pdata, int sizeBytes,
                                 uint32_t femType, int markLen)
{
    if (!fOk) return;
    if (pdata == nullptr || sizeBytes <= 0) return;

    const uint32_t nWords = (uint32_t)(sizeBytes / (int)sizeof(uint64_t));
    if (nWords == 0) return;
    if (fEntCount == 0) return; // no registered signals for the current FEM

    ensureWordCap(nWords);

    // Copy raw words to GPU
    ck(cudaMemcpyAsync(d_words, pdata, sizeof(uint64_t) * nWords,
                       cudaMemcpyHostToDevice, fStream));

    const int halfLen = markLen / 2;
    const int threads = 256;
    const int blocks = (int)((nWords + threads - 1) / threads);

    markUnpackKernel<<<blocks, threads, 0, fStream>>>(
        d_timeRegion, fTimeRegionSize,
        d_words, nWords,
        d_entCh, d_entBit, d_entDelay, fEntCount,
        femType, halfLen
    );
    ck(cudaGetLastError());
    // Async; synchronize at scan time.
}

void TriggerCuda::scanRisingEdges(std::vector<uint32_t>& hitsOut)
{
    hitsOut.clear();
    if (!fOk) return;
    if (d_trigLut == nullptr || fTrigMapSize == 0) {
        throw std::runtime_error("TriggerCuda::scanRisingEdges: trigger LUT not set");
    }

    // Reset counter
    ck(cudaMemsetAsync(d_hitCount, 0, sizeof(uint32_t), fStream));

    const int threads = 256;
    const int blocks = (int)((fTimeRegionSize + threads - 1) / threads);

    scanRisingKernel<<<blocks, threads, 0, fStream>>>(
        d_timeRegion, fTimeRegionSize,
        d_trigLut, fTrigMapSize,
        d_hits, d_hitCount
    );
    ck(cudaGetLastError());

    // Copy back count
    uint32_t count = 0;
    ck(cudaMemcpyAsync(&count, d_hitCount, sizeof(uint32_t), cudaMemcpyDeviceToHost, fStream));
    ck(cudaStreamSynchronize(fStream));

    if (count == 0) return;
    if (count > fHitCap) {
        // Should never happen because fHitCap == timeRegionSize.
        count = fHitCap;
    }

    hitsOut.resize(count);
    ck(cudaMemcpyAsync(hitsOut.data(), d_hits, sizeof(uint32_t) * count,
                       cudaMemcpyDeviceToHost, fStream));
    ck(cudaStreamSynchronize(fStream));

    // GPU threads append hits via atomicAdd -> unordered. CPU Scan emits them in
    // ascending bin order, so sort to make the two paths bit-for-bit identical.
    std::sort(hitsOut.begin(), hitsOut.end());
}

void TriggerCuda::downloadTimeRegion(uint32_t* hostOut)
{
    if (!fOk) return;
    ck(cudaMemcpyAsync(hostOut, d_timeRegion, sizeof(uint32_t) * fTimeRegionSize,
                       cudaMemcpyDeviceToHost, fStream));
    ck(cudaStreamSynchronize(fStream));
}

// -------- Batched path --------
void TriggerCuda::ensureBatchCap(uint32_t B, uint32_t totalWords, uint32_t nSeg,
                                 uint32_t nSlot, uint32_t nEnt)
{
    // timeRegion batch: B * regionSize
    const uint32_t trNeed = B * fTimeRegionSize;
    if (trNeed > fTrBatchCap || d_trBatch == nullptr) {
        uint32_t nc = std::max<uint32_t>(trNeed, fTrBatchCap * 2 + fTimeRegionSize);
        if (d_trBatch) ck(cudaFree(d_trBatch));
        ck(cudaMalloc(&d_trBatch, sizeof(uint32_t) * nc));
        fTrBatchCap = nc;
    }
    // words
    if (totalWords > fBWordCap || d_bWords == nullptr) {
        uint32_t nc = std::max<uint32_t>(totalWords, fBWordCap * 2 + 4096);
        if (d_bWords) ck(cudaFree(d_bWords));
        ck(cudaMalloc(&d_bWords, sizeof(uint64_t) * nc));
        fBWordCap = nc;
    }
    // segments (segOff has nSeg+1)
    if (nSeg + 1 > fSegCap || d_segOff == nullptr) {
        uint32_t nc = std::max<uint32_t>(nSeg + 1, fSegCap * 2 + 16);
        if (d_segOff)  ck(cudaFree(d_segOff));
        if (d_segHbf)  ck(cudaFree(d_segHbf));
        if (d_segSlot) ck(cudaFree(d_segSlot));
        ck(cudaMalloc(&d_segOff,  sizeof(int32_t) * nc));
        ck(cudaMalloc(&d_segHbf,  sizeof(int32_t) * nc));
        ck(cudaMalloc(&d_segSlot, sizeof(int32_t) * nc));
        fSegCap = nc;
    }
    // slots
    if (nSlot > fSlotCap || d_bFemType == nullptr) {
        uint32_t nc = std::max<uint32_t>(nSlot, fSlotCap * 2 + 8);
        if (d_bFemType) ck(cudaFree(d_bFemType));
        if (d_bEntOff)  ck(cudaFree(d_bEntOff));
        if (d_bEntCnt)  ck(cudaFree(d_bEntCnt));
        ck(cudaMalloc(&d_bFemType, sizeof(uint32_t) * nc));
        ck(cudaMalloc(&d_bEntOff,  sizeof(int32_t)  * nc));
        ck(cudaMalloc(&d_bEntCnt,  sizeof(int32_t)  * nc));
        fSlotCap = nc;
    }
    // entries
    if (nEnt > fBEntCap || d_bEntCh == nullptr) {
        uint32_t nc = std::max<uint32_t>(nEnt, fBEntCap * 2 + 8);
        if (d_bEntCh)    ck(cudaFree(d_bEntCh));
        if (d_bEntBit)   ck(cudaFree(d_bEntBit));
        if (d_bEntDelay) ck(cudaFree(d_bEntDelay));
        ck(cudaMalloc(&d_bEntCh,    sizeof(int32_t)  * nc));
        ck(cudaMalloc(&d_bEntBit,   sizeof(uint32_t) * nc));
        ck(cudaMalloc(&d_bEntDelay, sizeof(int32_t)  * nc));
        fBEntCap = nc;
    }
    // hit outputs: worst case one rising edge per bin over the whole batch
    const uint32_t hitNeed = trNeed;
    if (hitNeed > fBHitCap || d_bHitTime == nullptr) {
        uint32_t nc = std::max<uint32_t>(hitNeed, fBHitCap * 2 + fTimeRegionSize);
        if (d_bHitTime) ck(cudaFree(d_bHitTime));
        if (d_bHitHbf)  ck(cudaFree(d_bHitHbf));
        ck(cudaMalloc(&d_bHitTime, sizeof(uint32_t) * nc));
        ck(cudaMalloc(&d_bHitHbf,  sizeof(int32_t)  * nc));
        fBHitCap = nc;
    }
    if (d_bHitCount == nullptr) ck(cudaMalloc(&d_bHitCount, sizeof(uint32_t)));
}

void TriggerCuda::markScanBatch(int B,
    const std::vector<uint32_t>& femType,
    const std::vector<int32_t>&  entOff,
    const std::vector<int32_t>&  entCnt,
    const std::vector<int32_t>&  entCh,
    const std::vector<uint32_t>& entBit,
    const std::vector<int32_t>&  entDelay,
    const std::vector<const unsigned char*>& segData,
    const std::vector<int32_t>&  segNWords,
    const std::vector<int32_t>&  segHbf,
    const std::vector<int32_t>&  segSlot,
    int halfLen,
    std::vector<std::vector<uint32_t>>& hitsPerHbf)
{
    hitsPerHbf.assign(B, {});
    if (!fOk || B <= 0) return;
    if (d_trigLut == nullptr || fTrigMapSize == 0)
        throw std::runtime_error("TriggerCuda::markScanBatch: trigger LUT not set");

    const int nSeg  = (int)segData.size();
    const int nSlot = (int)femType.size();
    const int nEnt  = (int)entCh.size();

    // prefix offsets (in words) for each segment
    std::vector<int32_t> segOff(nSeg + 1, 0);
    for (int s = 0; s < nSeg; s++) segOff[s + 1] = segOff[s] + segNWords[s];
    const uint32_t totalWords = (uint32_t)segOff[nSeg];

    ensureBatchCap((uint32_t)B, totalWords, (uint32_t)nSeg,
                   (uint32_t)nSlot, (uint32_t)nEnt);

    // clear the whole (B x regionSize) timeRegion
    ck(cudaMemsetAsync(d_trBatch, 0, sizeof(uint32_t) * (uint32_t)B * fTimeRegionSize, fStream));

    // upload entry table
    if (nSlot > 0) {
        ck(cudaMemcpyAsync(d_bFemType, femType.data(), sizeof(uint32_t) * nSlot, cudaMemcpyHostToDevice, fStream));
        ck(cudaMemcpyAsync(d_bEntOff,  entOff.data(),  sizeof(int32_t)  * nSlot, cudaMemcpyHostToDevice, fStream));
        ck(cudaMemcpyAsync(d_bEntCnt,  entCnt.data(),  sizeof(int32_t)  * nSlot, cudaMemcpyHostToDevice, fStream));
    }
    if (nEnt > 0) {
        ck(cudaMemcpyAsync(d_bEntCh,    entCh.data(),    sizeof(int32_t)  * nEnt, cudaMemcpyHostToDevice, fStream));
        ck(cudaMemcpyAsync(d_bEntBit,   entBit.data(),   sizeof(uint32_t) * nEnt, cudaMemcpyHostToDevice, fStream));
        ck(cudaMemcpyAsync(d_bEntDelay, entDelay.data(), sizeof(int32_t)  * nEnt, cudaMemcpyHostToDevice, fStream));
    }

    // upload per-segment descriptors + raw words (one async copy per segment into offsets)
    if (nSeg > 0) {
        ck(cudaMemcpyAsync(d_segOff,  segOff.data(),  sizeof(int32_t) * (nSeg + 1), cudaMemcpyHostToDevice, fStream));
        ck(cudaMemcpyAsync(d_segHbf,  segHbf.data(),  sizeof(int32_t) * nSeg,       cudaMemcpyHostToDevice, fStream));
        ck(cudaMemcpyAsync(d_segSlot, segSlot.data(), sizeof(int32_t) * nSeg,       cudaMemcpyHostToDevice, fStream));
        for (int s = 0; s < nSeg; s++) {
            if (segNWords[s] <= 0) continue;
            ck(cudaMemcpyAsync(d_bWords + segOff[s], segData[s],
                               sizeof(uint64_t) * (size_t)segNWords[s],
                               cudaMemcpyHostToDevice, fStream));
        }
    }

    const int threads = 256;

    if (totalWords > 0 && nSeg > 0) {
        const int mblocks = (int)((totalWords + threads - 1) / threads);
        markBatchKernel<<<mblocks, threads, 0, fStream>>>(
            d_trBatch, fTimeRegionSize, d_bWords, totalWords,
            d_segOff, d_segHbf, d_segSlot, nSeg,
            d_bFemType, d_bEntOff, d_bEntCnt, d_bEntCh, d_bEntBit, d_bEntDelay,
            halfLen);
        ck(cudaGetLastError());
    }

    // scan over the whole batch
    ck(cudaMemsetAsync(d_bHitCount, 0, sizeof(uint32_t), fStream));
    const uint64_t total = (uint64_t)fTimeRegionSize * (uint64_t)B;
    const int sblocks = (int)((total + threads - 1) / threads);
    scanBatchKernel<<<sblocks, threads, 0, fStream>>>(
        d_trBatch, fTimeRegionSize, B, d_trigLut, fTrigMapSize,
        d_bHitTime, d_bHitHbf, d_bHitCount);
    ck(cudaGetLastError());

    uint32_t count = 0;
    ck(cudaMemcpyAsync(&count, d_bHitCount, sizeof(uint32_t), cudaMemcpyDeviceToHost, fStream));
    ck(cudaStreamSynchronize(fStream));
    if (count > fBHitCap) count = fBHitCap;

    if (count > 0) {
        std::vector<uint32_t> ht(count);
        std::vector<int32_t>  hh(count);
        ck(cudaMemcpyAsync(ht.data(), d_bHitTime, sizeof(uint32_t) * count, cudaMemcpyDeviceToHost, fStream));
        ck(cudaMemcpyAsync(hh.data(), d_bHitHbf,  sizeof(int32_t)  * count, cudaMemcpyDeviceToHost, fStream));
        ck(cudaStreamSynchronize(fStream));
        for (uint32_t k = 0; k < count; k++) {
            const int h = hh[k];
            if (h >= 0 && h < B) hitsPerHbf[h].push_back(ht[k]);
        }
        for (int h = 0; h < B; h++)
            std::sort(hitsPerHbf[h].begin(), hitsPerHbf[h].end());  // match CPU ascending order
    }
}

#else

// No CUDA build: provide empty stubs to keep linkage clean
TriggerCuda::TriggerCuda(uint32_t, uint32_t) {}
TriggerCuda::~TriggerCuda() {}
void TriggerCuda::clearAsync() {}
void TriggerCuda::setChannelEntries(const std::vector<int32_t>&, const std::vector<uint32_t>&, const std::vector<int32_t>&) {}
void TriggerCuda::setTriggerLUT(const std::vector<uint8_t>&) {}
void TriggerCuda::markRawTdcWords(const unsigned char*, int, uint32_t, int) {}
void TriggerCuda::scanRisingEdges(std::vector<uint32_t>&) {}
void TriggerCuda::downloadTimeRegion(uint32_t*) {}
void TriggerCuda::markScanBatch(int,
    const std::vector<uint32_t>&, const std::vector<int32_t>&, const std::vector<int32_t>&,
    const std::vector<int32_t>&, const std::vector<uint32_t>&, const std::vector<int32_t>&,
    const std::vector<const unsigned char*>&, const std::vector<int32_t>&,
    const std::vector<int32_t>&, const std::vector<int32_t>&, int,
    std::vector<std::vector<uint32_t>>& hitsPerHbf) { hitsPerHbf.clear(); }

#endif
