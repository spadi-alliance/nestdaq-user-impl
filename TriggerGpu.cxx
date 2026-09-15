/*
 * TriggerGpu.cxx
 *
 * GPU-capable Trigger used only by LogicFilterGpu.
 * Original Trigger.cxx is left unchanged for LogicFilter / fltcoin.
 * Without USE_CUDA this file is a CPU implementation (ExecBatch loops Exec).
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <map>
#include <memory>
#include <algorithm>
#include <string>
#include <cstring>
#include <cassert>

#include "UnpackTdc.h"
#include "SubTimeFrameHeader.h"
#include "TriggerMap.cxx"

#ifdef USE_CUDA
#include "TriggerCuda.h"
#endif

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
    Trigger();
    virtual ~Trigger();

    void InitParam();
    bool SetTimeRegion(int);
    void CleanUpTimeRegion();
    uint32_t* GetTimeRegion();
    uint32_t  GetTimeRegionSize();

    void Entry(uint32_t fem, int ch, int offset);
    void ClearEntry();
    bool CheckEntryFEM(uint32_t fem);

    void Mark(unsigned char* pdata, int len, int fem, uint32_t type);
    std::vector<uint32_t>* Scan();
    std::vector<uint32_t>* Exec(std::vector<struct HBFIndex>& hbf);

    // Batched execution of B HBFs in ONE set of GPU launches. Each HBF keeps its
    // own timeRegion slice and its own Scan, so hitsPerHbf[h] is bit-for-bit the
    // same as Exec(batch[h]); only kernel-launch / PCIe overhead is amortized.
    // Falls back to a per-HBF Exec() loop when CUDA is not active.
    void ExecBatch(std::vector<std::vector<struct HBFIndex>>& batch,
                   std::vector<std::vector<uint32_t>>& hitsPerHbf);

    void SetMarkLen(int val) { fMarkLen = val; }
    int  GetMarkLen() { return fMarkLen; }

    // Force the execution backend at runtime.
    //   SetUseCuda(true)  -> use GPU, but only if a CUDA context was actually
    //                        created in SetTimeRegion() (otherwise stays CPU).
    //   SetUseCuda(false) -> force the CPU fTimeRegion path for Mark/Scan/CleanUp.
    // Both backends share the same Entry()/MakeTable() setup, so this is the hook
    // used to run the identical LogicFilter trigger logic on CPU and GPU and
    // verify their outputs match bit-for-bit.
    void SetUseCuda(bool v) {
#ifdef USE_CUDA
        fUseCuda = v && (bool)fCuda && fCuda->ok();
#else
        (void)v;
#endif
    }
    bool GetUseCuda() const {
#ifdef USE_CUDA
        return fUseCuda;
#else
        return false;
#endif
    }

    void MakeTable(std::string& formula);

private:
    // per FEM definition (CPU-managed)
    std::map<uint32_t, std::vector<int>>      fEntryCh;
    std::map<uint32_t, std::vector<int>>      fEntryChDelay;
    std::map<uint32_t, std::vector<uint32_t>> fEntryChBit;
    int      fEntryCounts = 0;
    uint32_t fEntryMask   = 0;

    uint32_t  fTimeRegionSize = 0;
    uint32_t* fTimeRegion = nullptr; // CPU buffer kept for non-CUDA and for debugging

    std::vector<uint32_t> fHits;
    int fMarkLen = 5;

    TriggerMap fTMap;

#ifdef USE_CUDA
    bool fUseCuda = false;
    std::unique_ptr<TriggerCuda> fCuda;

    // Cached per-FEM trigger-signal entry list. Uploaded when FEM changes.
    // Mirrors fEntryCh/fEntryChBit/fEntryChDelay for the current FEM (a channel
    // may appear multiple times with different bit/delay).
    uint32_t fCudaLutFem = 0;
    bool     fCudaLutValid = false;
    static constexpr uint32_t kCudaChMax = 512;
    std::vector<int32_t>  fCudaEntCh;
    std::vector<uint32_t> fCudaEntBit;
    std::vector<int32_t>  fCudaEntDelay;

    // Cached trigger truth table LUT
    bool fCudaTrigLutValid = false;
    uint32_t fCudaTrigMapSize = 0;
#endif
};

Trigger::Trigger() {}

Trigger::~Trigger()
{
    if (fTimeRegion != nullptr) {
        delete[] fTimeRegion;
        fTimeRegion = nullptr;
    }
}

void Trigger::MakeTable(std::string& formula)
{
    // Build TriggerMap truth table on CPU (done rarely: at init or config changes)
    fTMap.MakeTable(formula);

#ifdef USE_CUDA
    // Upload truth table to GPU (done rarely, so overhead is negligible)
    if (fUseCuda && fCuda) {
        try {
            std::vector<uint8_t> lut;
            fTMap.ExportLut(lut);
            fCudaTrigMapSize = (uint32_t)lut.size();
            fCuda->setTriggerLUT(lut);
            fCudaTrigLutValid = true;
        } catch (...) {
            // If upload fails, keep CPU path.
            fUseCuda = false;
            fCuda.reset();
        }
    }
#endif
}

void Trigger::InitParam()
{
    fHits.clear();
    if (fTimeRegion != nullptr) {
        std::memset(fTimeRegion, 0, fTimeRegionSize * sizeof(uint32_t));
    }
}

bool Trigger::SetTimeRegion(int size)
{
    fTimeRegionSize = (uint32_t)size;

    if (fTimeRegion != nullptr) {
        delete[] fTimeRegion;
        fTimeRegion = nullptr;
    }
    fTimeRegion = new uint32_t[fTimeRegionSize];
    std::memset(fTimeRegion, 0, fTimeRegionSize * sizeof(uint32_t));

#ifdef USE_CUDA
    // Try to enable CUDA path. If no device is available, fall back to CPU automatically.
    fCuda = std::make_unique<TriggerCuda>(fTimeRegionSize, kCudaChMax);
    fUseCuda = (fCuda && fCuda->ok());

    fCudaLutValid = false;
    fCudaEntCh.clear();
    fCudaEntBit.clear();
    fCudaEntDelay.clear();

    fCudaTrigLutValid = false;
    fCudaTrigMapSize = 0;

    if (!fUseCuda) {
        fCuda.reset();
    }
#endif

    return true;
}

void Trigger::CleanUpTimeRegion()
{
#ifdef USE_CUDA
    if (fUseCuda && fCuda) {
        // v4: clear timeRegion directly on GPU.
        fCuda->clearAsync();
        return;
    }
#endif

    // [CPU-FALLBACK]
    if (fTimeRegion != nullptr) {
        std::memset(fTimeRegion, 0, fTimeRegionSize * sizeof(uint32_t));
    }
}

uint32_t* Trigger::GetTimeRegion()
{
    return fTimeRegion;
}

uint32_t Trigger::GetTimeRegionSize()
{
    return fTimeRegionSize;
}

void Trigger::Entry(uint32_t fem, int ch, int offset)
{
    fEntryCh[fem].emplace_back(ch);
    fEntryChDelay[fem].emplace_back(offset);
    fEntryChBit[fem].emplace_back(0x00000001u << fEntryCounts);
    fEntryMask |= 0x00000001u << fEntryCounts;
    fEntryCounts++;

    if ((unsigned)fEntryCounts > (sizeof(uint32_t) * 8)) {
        std::cerr << "Entry channels exceed " << (sizeof(uint32_t) * 8) << std::endl;
    }
    assert(fEntryCounts <= (int)(sizeof(uint32_t) * 8));
}

void Trigger::ClearEntry()
{
    fEntryCh.clear();
    fEntryChDelay.clear();
    fEntryChBit.clear();
    fEntryMask = 0;
    fEntryCounts = 0;

#ifdef USE_CUDA
    fCudaLutValid = false;
    fCudaLutFem = 0;
#endif
}

bool Trigger::CheckEntryFEM(uint32_t fem)
{
    return (fEntryCh.count(fem) >= 1);
}

void Trigger::Mark(unsigned char* pdata, int len, int fem, uint32_t type)
{
#ifdef USE_CUDA
    if (fUseCuda && fCuda) {
        // Make sure trigger LUT is available on GPU.
        if (!fCudaTrigLutValid) {
            // If MakeTable() was not called yet, we cannot scan on GPU.
            // Fall back to CPU (safe behavior).
            fUseCuda = false;
            fCuda.reset();
        }
    }

    if (fUseCuda && fCuda) {
        // Build and upload the per-FEM entry list on demand. Every registered
        // signal for this FEM becomes one entry (same channel may repeat with a
        // different bit/delay), exactly mirroring the CPU Mark loop below.
        if (!fCudaLutValid || fCudaLutFem != (uint32_t)fem) {
            fCudaEntCh.clear();
            fCudaEntBit.clear();
            fCudaEntDelay.clear();

            if (fEntryCh.count((uint32_t)fem) >= 1) {
                const auto& chs  = fEntryCh[(uint32_t)fem];
                const auto& dlys = fEntryChDelay[(uint32_t)fem];
                const auto& bits = fEntryChBit[(uint32_t)fem];

                const size_t n = chs.size();
                fCudaEntCh.reserve(n);
                fCudaEntBit.reserve(n);
                fCudaEntDelay.reserve(n);
                for (size_t i = 0; i < n; i++) {
                    fCudaEntCh.emplace_back((int32_t)chs[i]);
                    fCudaEntBit.emplace_back(bits[i]);
                    fCudaEntDelay.emplace_back((int32_t)dlys[i]);
                }
            }

            try {
                fCuda->setChannelEntries(fCudaEntCh, fCudaEntBit, fCudaEntDelay);
                fCudaLutFem = (uint32_t)fem;
                fCudaLutValid = true;
            } catch (...) {
                // LUT upload failed -> CPU fallback.
                fUseCuda = false;
                fCuda.reset();
            }
        }

        if (fUseCuda && fCuda) {
            // v4: Mark runs fully on GPU (including unpack).
            fCuda->markRawTdcWords(pdata, len, type, fMarkLen);
            return;
        }
    }
#endif

    // ===========================
    // [CPU-FALLBACK IMPLEMENTATION]
    // ===========================
    // NOTE: This block is kept for readability and as a reference.
    // When CUDA is enabled and healthy, code returns earlier.

    if (fEntryCh.count((uint32_t)fem) >= 1) {
        uint64_t* tdcval = reinterpret_cast<uint64_t*>(pdata);

        for (unsigned int i = 0; i < fEntryCh[(uint32_t)fem].size(); i++) {
            int ch = fEntryCh[(uint32_t)fem][i];
            int delay = fEntryChDelay[(uint32_t)fem][i];
            uint32_t markbit = fEntryChBit[(uint32_t)fem][i];

            for (unsigned int j = 0; j < (unsigned)(len / (int)sizeof(uint64_t)); j++) {

                if (type == SubTimeFrame::TDC64H) {
                    struct TDC64H::tdc64 tdc;
                    if (TDC64H::Unpack(tdcval[j], &tdc) == TDC64H::T_TDC) {
                        if (tdc.ch == ch) {
                            uint32_t hit = tdc.tdc4n + delay;
                            if (hit < fTimeRegionSize - (fMarkLen / 2)) {
                                for (int k = -1 * (fMarkLen / 2); k < ((fMarkLen / 2) + 1); k++) {
                                    if ((int)hit + k >= 0 && (hit + (uint32_t)k) < fTimeRegionSize) {
                                        fTimeRegion[hit + k] |= markbit;
                                    }
                                }
                            }
                        }
                    }
                } else if (type == SubTimeFrame::TDC64L) {
                    struct TDC64L::tdc64 tdc;
                    if (TDC64L::Unpack(tdcval[j], &tdc) == TDC64L::T_TDC) {
                        if (tdc.ch == ch) {
                            uint32_t hit = tdc.tdc4n + delay;
                            if (hit < fTimeRegionSize - (fMarkLen / 2)) {
                                for (int k = -1 * (fMarkLen / 2); k < ((fMarkLen / 2) + 1); k++) {
                                    if ((int)hit + k >= 0 && (hit + (uint32_t)k) < fTimeRegionSize) {
                                        fTimeRegion[hit + k] |= markbit;
                                    }
                                }
                            }
                        }
                    }
                } else if (type == SubTimeFrame::TDC64H_V3) {
                    struct TDC64H_V3::tdc64 tdc;
                    if (TDC64H_V3::Unpack(tdcval[j], &tdc) == TDC64H_V3::T_TDC) {
                        if (tdc.ch == ch) {
                            uint32_t hit = tdc.tdc4n + delay;
                            if (hit < fTimeRegionSize - (fMarkLen / 2)) {
                                for (int k = -1 * (fMarkLen / 2); k < ((fMarkLen / 2) + 1); k++) {
                                    if ((int)hit + k >= 0 && (hit + (uint32_t)k) < fTimeRegionSize) {
                                        fTimeRegion[hit + k] |= markbit;
                                    }
                                }
                            }
                        }
                    }
                } else if (type == SubTimeFrame::TDC64L_V3) {
                    struct TDC64L_V3::tdc64 tdc;
                    if (TDC64L_V3::Unpack(tdcval[j], &tdc) == TDC64L_V3::T_TDC) {
                        if (tdc.ch == ch) {
                            uint32_t hit = tdc.tdc4n + delay;
                            if (hit < fTimeRegionSize - (fMarkLen / 2)) {
                                for (int k = -1 * (fMarkLen / 2); k < ((fMarkLen / 2) + 1); k++) {
                                    if ((int)hit + k >= 0 && (hit + (uint32_t)k) < fTimeRegionSize) {
                                        fTimeRegion[hit + k] |= markbit;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

std::vector<uint32_t>* Trigger::Scan()
{
    // ==============================
    // [CPU IMPLEMENTATION (REFERENCE)]
    // ==============================
    // In v4 CUDA path, Scan is executed on GPU (see Exec()).

    fHits.clear();

    for (uint32_t i = 0; i < fTimeRegionSize - 1; i++) {
        if ((!fTMap.LookUp(fTimeRegion[i])) && (fTMap.LookUp(fTimeRegion[i + 1]))) {
            fHits.emplace_back(i + 1);
        }
    }

    return &fHits;
}

std::vector<uint32_t>* Trigger::Exec(std::vector<struct HBFIndex>& hbf)
{
    // 1) Clear timeRegion (GPU if enabled)
    Trigger::CleanUpTimeRegion();

    // 2) Mark for each segment
    for (auto& seg : hbf) {
        Trigger::Mark(seg.data, seg.size, (int)seg.femId, seg.femType);
    }

#ifdef USE_CUDA
    if (fUseCuda && fCuda) {
        // 3) Scan on GPU and get rising-edge hit times (small output)
        try {
            fCuda->scanRisingEdges(fHits);
            return &fHits;
        } catch (...) {
            // If scan fails, fall back to CPU.
            fUseCuda = false;
            fCuda.reset();
        }
    }
#endif

    // CPU fallback path: Scan uses fTimeRegion.
    // NOTE: If CUDA Mark was used but Scan failed, fTimeRegion is not updated.
    // Therefore we only reach here when CUDA is disabled or fully failed early.
    return Trigger::Scan();
}

void Trigger::ExecBatch(std::vector<std::vector<struct HBFIndex>>& batch,
                        std::vector<std::vector<uint32_t>>& hitsPerHbf)
{
    const int B = (int)batch.size();
    hitsPerHbf.assign(B, {});

#ifdef USE_CUDA
    if (fUseCuda && fCuda) {
        // Build the multi-FEM entry table from the signals that (a) are
        // registered in the config and (b) actually appear in this batch.
        std::map<uint32_t, int> slotOfFem;
        std::vector<uint32_t> femType;
        std::vector<int32_t>  entOff, entCnt, entCh, entDelay;
        std::vector<uint32_t> entBit;
        std::vector<const unsigned char*> segData;
        std::vector<int32_t> segNWords, segHbf, segSlot;

        for (int h = 0; h < B; h++) {
            for (auto& seg : batch[h]) {
                const uint32_t fem = seg.femId;
                auto it = fEntryCh.find(fem);
                if (it == fEntryCh.end() || it->second.empty())
                    continue;  // FEM has no registered signals -> CPU skips it too

                int slot;
                auto sit = slotOfFem.find(fem);
                if (sit == slotOfFem.end()) {
                    slot = (int)femType.size();
                    slotOfFem[fem] = slot;
                    femType.push_back(seg.femType);
                    entOff.push_back((int32_t)entCh.size());
                    const auto& chs = it->second;
                    const auto& dls = fEntryChDelay[fem];
                    const auto& bts = fEntryChBit[fem];
                    for (size_t i = 0; i < chs.size(); i++) {
                        entCh.push_back((int32_t)chs[i]);
                        entDelay.push_back((int32_t)dls[i]);
                        entBit.push_back(bts[i]);
                    }
                    entCnt.push_back((int32_t)chs.size());
                } else {
                    slot = sit->second;
                }

                segData.push_back(seg.data);
                segNWords.push_back(seg.size / (int)sizeof(uint64_t));
                segHbf.push_back(h);
                segSlot.push_back(slot);
            }
        }

        const int halfLen = fMarkLen / 2;
        try {
            fCuda->markScanBatch(B, femType, entOff, entCnt, entCh, entBit, entDelay,
                                 segData, segNWords, segHbf, segSlot, halfLen, hitsPerHbf);
            return;
        } catch (...) {
            // On any GPU error, fall through to the CPU per-HBF path below.
            fUseCuda = false;
            fCuda.reset();
        }
    }
#endif

    // CPU (or fallback) path: identical per-HBF Exec loop.
    for (int h = 0; h < B; h++) {
        std::vector<uint32_t>* hits = Trigger::Exec(batch[h]);
        hitsPerHbf[h] = *hits;
    }
}

// ============================================================================
// [ORIGINAL CPU-ONLY REFERENCE] (commented out)
// The following blocks show the pre-v4 CPU-only logic, kept for easy review.
// ============================================================================
#if 0

void Trigger::CleanUpTimeRegion()
{
    // Original: always clear on CPU
    if (fTimeRegion != nullptr) {
        std::memset(fTimeRegion, 0, fTimeRegionSize * sizeof(uint32_t));
    }
}

std::vector<uint32_t>* Trigger::Scan()
{
    fHits.clear();
    for (uint32_t i = 0; i < fTimeRegionSize - 1; i++) {
        if ((!fTMap.LookUp(fTimeRegion[i])) && (fTMap.LookUp(fTimeRegion[i + 1]))) {
            fHits.emplace_back(i + 1);
        }
    }
    return &fHits;
}

std::vector<uint32_t>* Trigger::Exec(std::vector<struct HBFIndex>& hbf)
{
    Trigger::CleanUpTimeRegion();

    for (auto& seg : hbf) {
        Trigger::Mark(seg.data, seg.size, (int)seg.femId, seg.femType);
    }

    return Trigger::Scan();
}

#endif
