#ifndef FileSinkHeader_h
#define FileSinkHeader_h

#include <ctime>
#include <cstdint>

namespace FileSinkHeader {

namespace v0 {
constexpr uint64_t MAGIC = {0x444145482d534640}; /* "@FS-HEAD" */

struct Header { /* Total size: 304 bytes */
    uint64_t magic               {MAGIC}; /* 64 bits = 8 bytes */
    uint64_t size                {0};     /* 64 bits = 8 bytes */
    uint64_t fairMQDeviceType    {0};     /* 64 bits = 8 bytes */
    uint64_t runNumber           {0};     /* 64 bits = 8 bytes */
    time_t   startUnixtime       {0};     /* 64 bits = 8 bytes */
    time_t   stopUnixtime        {0};     /* 64 bits = 8 bytes */
    char     comments[256];               /* 8 bits x 256 = 256 bytes*/
};
} //namespace v0

inline namespace v1 {
constexpr uint64_t MAGIC = {0x004b4e53454c4946}; /* "FILESNK " */

struct Header { /* Total size: 304 bytes */
    uint64_t magic               {MAGIC}; /* 64 bits = 8 bytes */
    union {
        uint64_t size            {0};     /* 64 bits = 8 bytes */
        struct {
            uint32_t length         ;
            uint16_t hLength        ;
            uint16_t type           ;
        };
    };
    uint64_t fairMQDeviceType    {0};     /* 64 bits = 8 bytes */
    uint64_t runNumber           {0};     /* 64 bits = 8 bytes */
    time_t   startUnixtime       {0};     /* 64 bits = 8 bytes */
    time_t   stopUnixtime        {0};     /* 64 bits = 8 bytes */
    char     comments[256];               /* 8 bits x 256 = 256 bytes*/
};
} //namespace v0


} //namespace FileSinkHeader

#endif
