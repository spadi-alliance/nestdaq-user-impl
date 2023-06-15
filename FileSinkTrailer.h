#ifndef FileSinkTrailer_h
#define FileSinkTrailer_h

#include <ctime>
#include <cstdint>

namespace FileSinkTrailer {

/* "@FS-TRAI" */
constexpr uint64_t Magic = {0x494152542d534640};

struct Trailer { /* Total size: 304 bytes */
    uint64_t magic               {Magic}; /* 64 bits = 8 bytes */
    uint64_t size                {0};     /* 64 bits = 8 bytes */
    uint64_t fairMQDeviceType    {0};     /* 64 bits = 8 bytes */
    uint64_t runNumber           {0};     /* 64 bits = 8 bytes */
    time_t   startUnixtime       {0};     /* 64 bits = 8 bytes */
    time_t   stopUnixtime        {0};     /* 64 bits = 8 bytes */
    char     comments[256];               /* 8 bits x 256 = 256 bytes*/
};

} // namespace FileSinkTrailer

#endif
