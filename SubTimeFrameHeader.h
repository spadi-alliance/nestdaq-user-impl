#ifndef SubTimeFrameHeader_h
#define SubTimeFrameHeader_h

#include <cstdint>

namespace SubTimeFrame {

// This format is temporary and should be updated.
namespace v0 {

// "DAEH-FTS" : little endian of "STF-HEAD"
constexpr uint64_t MAGIC  {0x444145482d465453};
//constexpr uint32_t TDC64H {0x48434454};
//constexpr uint32_t TDC64L {0x4c434454};
constexpr uint32_t TDC64H    {2};
constexpr uint32_t TDC64H_V1 {2};
constexpr uint32_t TDC64L    {3};
constexpr uint32_t TDC64L_V1 {1};
constexpr uint32_t TDC64L_V2 {3};

constexpr uint32_t NULDEV    {0};

struct Header {
    uint64_t magic        {MAGIC};
    uint32_t timeFrameId  {0}; 
    uint32_t reserved     {0};
    uint32_t FEMType      {0};
    uint32_t FEMId        {0};
    uint32_t length       {0};
    uint32_t numMessages  {0};
    uint64_t time_sec     {0};
    uint64_t time_usec    {0};
};

} // namespace v0

inline namespace v1 {

// " EMITBUS" : little endian of "SUBTIME "
constexpr uint64_t MAGIC  {0x00454d4954425553};
constexpr uint32_t TDC64H    {2};
constexpr uint32_t TDC64H_V1 {2};
constexpr uint32_t TDC64L    {3};
constexpr uint32_t TDC64L_V1 {1};
constexpr uint32_t TDC64L_V2 {3};
constexpr uint32_t FLT_TDC   {0x0000'1000};

constexpr uint32_t NULDEV    {0};


struct Header {
    uint64_t magic        {MAGIC};
    uint32_t length       {0};
    uint16_t hLength      {40};
    uint16_t type         {0};
    uint32_t timeFrameId  {0}; 
    uint32_t femType      {0};
    uint32_t femId        {0};
    uint32_t numMessages  {0};
    uint64_t timeSec     {0};
    uint64_t timeUSec    {0};
};

} // namespace v1

} // namespace SubTimeFrame

#endif
