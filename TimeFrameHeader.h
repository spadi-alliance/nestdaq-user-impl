#ifndef TimeFrameHeader_h
#define TimeFrameHeader_h

#include <cstdint>

namespace TimeFrame {

// This format is temporary and should be updated.
inline namespace v0 {

// "DAEH-FT@" : little endian of "@TF-HEAD"
constexpr uint64_t Magic {0x444145482d465440};

struct Header {
    uint64_t magic       {Magic};
    uint32_t timeFrameId {0};
    uint32_t numSource   {0};
    uint64_t length      {0};
};

} // namespace v0

} // namespace TimeFrame

#endif
