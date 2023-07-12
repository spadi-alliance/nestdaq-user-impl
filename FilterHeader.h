#ifndef FilterHeader_h
#define FilterHeader_h

#include <cstdint>
#include <sys/time.h>

namespace Filter {

// "NIOC-TLF" : little endian of "FLT-COIN"
constexpr uint64_t Magic {0x4e494f43'2d544c46};

struct Header {
    uint64_t magic       {Magic};
    uint64_t length      {0};
    uint32_t numTrigs    {0};
    uint32_t workerId    {0};
    uint32_t elapseTime  {0};
    struct timeval processTime {0, 0};
};

} // namespace Filter

#endif
