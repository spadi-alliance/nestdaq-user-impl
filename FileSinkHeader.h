#ifndef FileSinkHeader_h
#define FileSinkHeader_h

#include <ctime>
#include <cstdint>

namespace FileSinkHeader {

/* "@FS-HEAD" */
constexpr uint64_t Magic = {0x444145482d534640};

struct Header { /* Total size: 304 bytes */
    uint64_t magic               {Magic}; /* 64 bits = 8 bytes */
    uint64_t size                {0};     /* 64 bits = 8 bytes */
    uint64_t fairMQDeviceType    {0};     /* 64 bits = 8 bytes */
    uint64_t runNumber           {0};     /* 64 bits = 8 bytes */
    time_t   startUnixtime       {0};     /* 64 bits = 8 bytes */
    time_t   stopUnixtime        {0};     /* 64 bits = 8 bytes */
    char     comments[256];               /* 8 bits x 256 = 256 bytes*/
};

} //namespace FileSinkHeader

#endif
