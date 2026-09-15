#pragma once
// CUDA helper for Trigger.
// v4 design ("Plan 1"): keep the whole trigger decision path on GPU per HBF:
//   - clear timeRegion on GPU
//   - Mark(): unpack raw TDC words and atomicOr markbits into timeRegion
//   - Scan(): apply TriggerMap LUT on GPU and extract rising-edge hit times
// CPU only receives the (usually small) hit-time list.

#include <cstdint>
#include <vector>

#ifdef USE_CUDA
#include <cuda_runtime.h>
#endif

class TriggerCuda
{
public:
    explicit TriggerCuda(uint32_t timeRegionSize, uint32_t chMax = 512);
    ~TriggerCuda();

    TriggerCuda(const TriggerCuda&) = delete;
    TriggerCuda& operator=(const TriggerCuda&) = delete;

    bool ok() const { return fOk; }

    // Clear timeRegion on GPU (asynchronous). Call once per HBF.
    void clearAsync();

    // Upload the trigger-signal ENTRY list for the CURRENT FEM.
    // Each entry i is one registered (channel -> bit/delay) signal, mirroring
    // Trigger::fEntryCh/fEntryChBit/fEntryChDelay for that FEM. The three
    // vectors must have identical length. The SAME channel may appear multiple
    // times (different bit/delay), exactly like the CPU path.
    void setChannelEntries(const std::vector<int32_t>&  chList,
                           const std::vector<uint32_t>& bitList,
                           const std::vector<int32_t>&  delayList);

    // Upload TriggerMap LUT (truth table). value is 0/1.
    // Call when trigger formula is set/changed.
    void setTriggerLUT(const std::vector<uint8_t>& trigLut);

    // Mark using raw words (pdata is bytes, sizeBytes bytes). femType is SubTimeFrame::TDC* constant.
    void markRawTdcWords(const unsigned char* pdata, int sizeBytes,
                         uint32_t femType, int markLen);

    // Scan rising edges of TriggerMap(LUT(timeRegion[t])) and return hit times.
    // Output: times in "4ns bins" (same as CPU Scan). hitsOut is overwritten.
    void scanRisingEdges(std::vector<uint32_t>& hitsOut);

    // (Debug/CPU fallback helper)
    void downloadTimeRegion(uint32_t* hostOut);

    // ------------------------------------------------------------------
    // Batched path: clear + mark + scan for B HBFs in ONE set of launches.
    //
    // Each HBF h owns its own regionSize slice of a (B x regionSize) device
    // timeRegion, so its Mark/Scan are completely independent -> per-HBF hit
    // lists are bit-for-bit identical to the per-HBF path. Only the kernel
    // launch count and PCIe round-trips are amortized across the batch.
    //
    // Entry table (per "slot" = one FEM that has registered signals):
    //   femType[s]          : SubTimeFrame::TDC* constant for slot s
    //   entOff[s],entCnt[s]  : range of slot s in the concatenated ent* arrays
    //   entCh/entBit/entDelay: concatenated per-signal (channel,bit,delay)
    // Segments (one per FEM per HBF that has signals):
    //   segData[k]   : pointer to the raw HBF message bytes for segment k
    //   segNWords[k] : number of 64-bit words in that segment
    //   segHbf[k]    : which HBF slice (0..B-1) this segment marks into
    //   segSlot[k]   : which entry-table slot (FEM) this segment uses
    // Output: hitsPerHbf[h] = ascending rising-edge times for HBF h.
    void markScanBatch(int B,
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
        std::vector<std::vector<uint32_t>>& hitsPerHbf);

private:
    void ensureWordCap(uint32_t nWords);
    void ensureHitCap(uint32_t nHits);
    void ensureEntCap(uint32_t nEnt);
#ifdef USE_CUDA
    void ensureBatchCap(uint32_t B, uint32_t totalWords, uint32_t nSeg,
                        uint32_t nSlot, uint32_t nEnt);
#endif

private:
    bool fOk{false};
    uint32_t fTimeRegionSize{0};
    uint32_t fChMax{0};

    // Trigger LUT
    uint32_t fTrigMapSize{0};

#ifdef USE_CUDA
    cudaStream_t fStream{nullptr};

    uint32_t* d_timeRegion{nullptr};

    // per-FEM trigger-signal entry list (channel/bit/delay), one slot per entry
    int32_t*  d_entCh{nullptr};
    uint32_t* d_entBit{nullptr};
    int32_t*  d_entDelay{nullptr};
    uint32_t  fEntCap{0};
    uint32_t  fEntCount{0};

    // trigger truth table LUT (0/1)
    uint8_t*  d_trigLut{nullptr};

    // raw input buffer
    uint64_t* d_words{nullptr};
    uint32_t fWordCap{0};

    // scan output buffers
    uint32_t* d_hits{nullptr};
    uint32_t  fHitCap{0};
    uint32_t* d_hitCount{nullptr};

    // ---- batched-path buffers ----
    uint32_t* d_trBatch{nullptr};   uint32_t fTrBatchCap{0};   // B * regionSize
    uint64_t* d_bWords{nullptr};    uint32_t fBWordCap{0};     // concatenated words
    int32_t*  d_segOff{nullptr};                               // nSeg+1 prefix (words)
    int32_t*  d_segHbf{nullptr};
    int32_t*  d_segSlot{nullptr};   uint32_t fSegCap{0};
    uint32_t* d_bFemType{nullptr};
    int32_t*  d_bEntOff{nullptr};
    int32_t*  d_bEntCnt{nullptr};   uint32_t fSlotCap{0};
    int32_t*  d_bEntCh{nullptr};
    uint32_t* d_bEntBit{nullptr};
    int32_t*  d_bEntDelay{nullptr}; uint32_t fBEntCap{0};
    uint32_t* d_bHitTime{nullptr};
    int32_t*  d_bHitHbf{nullptr};   uint32_t fBHitCap{0};
    uint32_t* d_bHitCount{nullptr};
#endif
};
