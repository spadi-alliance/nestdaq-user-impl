#ifndef FileSinkTrailer_h
#define FileSinkTrailer_h

#include <ctime>
#include <cstdint>

namespace FileSinkTrailer {

namespace v0 {
constexpr uint64_t MAGIC = {0x494152542d534640}; /* "@FS-TRAI" */

struct Trailer { /* Total size: 304 bytes */
    uint64_t magic               {MAGIC}; /* 64 bits = 8 bytes */
    uint64_t size                {0};     /* 64 bits = 8 bytes */
    uint64_t fairMQDeviceType    {0};     /* 64 bits = 8 bytes */
    uint64_t runNumber           {0};     /* 64 bits = 8 bytes */
    time_t   startUnixtime       {0};     /* 64 bits = 8 bytes */
    time_t   stopUnixtime        {0};     /* 64 bits = 8 bytes */
    char     comments[256];               /* 8 bits x 256 = 256 bytes*/
};
}

inline namespace v1 {
constexpr uint64_t MAGIC = {0x004c5254454c4946}; /* "FILETRL " */

struct Trailer { /* Total size: 304 bytes */
    uint64_t magic               {MAGIC}; /* 64 bits = 8 bytes */
    union {
        uint64_t size               ;     /* 64 bits = 8 bytes */
        struct {
            uint32_t length      {0x130}; /* 32 bits = 8 bytes */
            uint16_t hLength     {0x130}; /* 32 bits = 8 bytes */
            uint16_t type        {0};     /* 32 bits = 8 bytes */
        };
    };
    uint64_t fairMQDeviceType    {0};     /* 64 bits = 8 bytes */
    uint64_t runNumber           {0};     /* 64 bits = 8 bytes */
    time_t   startUnixtime       {0};     /* 64 bits = 8 bytes */
    time_t   stopUnixtime        {0};     /* 64 bits = 8 bytes */
    char     comments[256];               /* 8 bits x 256 = 256 bytes*/
};
}

} // namespace FileSinkTrailer

#endif
