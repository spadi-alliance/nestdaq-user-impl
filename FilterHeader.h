#ifndef FilterHeader_h
#define FilterHeader_h

#include <cstdint>
#include <sys/time.h>

namespace Filter {

namespace v0 {
// "NIOC-TLF" : little endian of "FLT-COIN"
constexpr uint64_t MAGIC {0x4e494f43'2d544c46};

struct Header {
    uint64_t magic       {MAGIC};
    uint64_t length      {0};
    uint32_t numTrigs    {0};
    uint32_t workerId    {0};
    uint32_t elapseTime  {0};
    struct timeval processTime {0, 0};
};
} //v0

inline namespace v1 {
// " IGOLTLF" : little endian of "FLTLOGI "
constexpr uint64_t MAGIC {0x0049474f'4c544c46};

struct Header {
    uint64_t magic       {MAGIC};
    uint32_t length      {0};
    uint16_t hLength     {36};
    uint16_t type        {0};
    uint32_t numTrigs    {0};
    uint32_t workerId    {0};
    uint32_t elapseTime  {0};
    struct timeval processTime {0, 0};
};
} //namespace v1

} // namespace Filter

#endif
