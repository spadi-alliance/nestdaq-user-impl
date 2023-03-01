#ifndef NESTDAQ_FILE_SINK_HEADER_BLOCK_H_
#define NESTDAQ_FILE_SINK_HEADER_BLOCK_H_

#include <ctime>
#include <cstdint>

namespace nestdaq {

class FileSinkHeaderBlock {
public:
   constexpr static uint64_t kMagic = 0x444145482d534640ull; // "@FS-HEAD"
   std::uint64_t fFileSinkHeaderBlockSize; /* 64 bits */
   std::uint64_t fMagic;                   /* 64 bits */
   std::uint64_t fFairMQDeviceType;        /* 64 bits */
   std::uint64_t fRunNumber;               /* 64 bits */
   std::time_t   fStartUnixtime;           /* 64 bits */
   std::time_t   fStopUnixtime;            /* 64 bits */
   char          fComments[256];           /* 8 bits x 256 */
   FileSinkHeaderBlock(){
     fFileSinkHeaderBlockSize = 0;
     fMagic                   = 0;
     fFairMQDeviceType        = 0;
     fRunNumber               = 0;
     fStartUnixtime           = 0;
     fStopUnixtime            = 0;
     strcpy(fComments, "");
   }
  ~FileSinkHeaderBlock(){}
};

} // namespace nestdaq

#endif
