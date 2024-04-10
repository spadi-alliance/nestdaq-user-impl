#ifndef TimeFrameHeader_h
#define TimeFrameHeader_h

#include <cstdint>

#pragma pack(4)
namespace TimeFrame {

// This format is temporary and should be updated.
inline namespace v0 {
// "DAEH-FT@" : little endian of "@TF-HEAD"
constexpr uint64_t MAGIC {0x444145482d465440};
struct Header {
    uint64_t magic       {MAGIC};
    uint32_t timeFrameId {0};
    uint32_t numSource   {0};
    uint64_t length      {0};
};

} // namespace v0

namespace v1 {
// " MRFEMIT" : little endian of "TIMEFRM "
constexpr uint64_t MAGIC {0x004d5246454d4954};
struct Header {
    uint64_t magic       {MAGIC};
    uint32_t length      {0};
    uint16_t hLength     {24};
    uint16_t type        {0};
    uint32_t timeFrameId {0};
    uint32_t numSource   {0};
};

} // namespace v1

} // namespace TimeFrame
#pragma pack()

#endif
