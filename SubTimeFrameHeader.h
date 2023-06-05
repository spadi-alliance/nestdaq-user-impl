#ifndef SubTimeFrameHeader_h
#define SubTimeFrameHeader_h

#include <cstdint>

namespace SubTimeFrame {

// "DAEH-FTS" : little endian of "STF-HEAD"
constexpr uint64_t Magic {0x444145482d465453};
//constexpr uint32_t TDC64H {0x48434454};
//constexpr uint32_t TDC64L {0x4c434454};
constexpr uint32_t TDC64H {2};
constexpr uint32_t TDC64L {1};

struct Header {
    uint64_t magic        {Magic};
    uint32_t timeFrameId  {0};
    uint32_t reserved     {0};
    uint32_t FEMType      {0}; // module type
    uint32_t FEMId        {0}; // ip address
    uint32_t length       {0};
    uint32_t numMessages  {0};
    uint64_t time_sec     {0};
    uint64_t time_usec    {0};
};

} //

#endif
