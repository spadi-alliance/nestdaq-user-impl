#ifndef HBFHeader_H
#define HBFHeader_H

namespace HeartbeatFrame
{
// "HRTBEAT\0" -> "\0TAEBTRH"
constexpr uint64_t MAGIC = 0x0054414542545248;
#pragma pack(4)
struct Header {
    uint64_t magic      {MAGIC};
    uint32_t length     {0};
    uint16_t hLength    {16};
    uint16_t type       {0};
};
#pragma pack()
}

#endif
