#ifndef TimeFrameHeader_h
#define TimeFrameHeader_h

#include <cstdint>

namespace TimeFrame {
   
// This format is temporary and should be updated.
   namespace v0 {
// "DAEH-FT@" : little endian of "@TF-HEAD"
      constexpr uint64_t MAGIC {0x444145482d465440};
      struct Header {
         uint64_t magic       {MAGIC};
         uint32_t timeFrameId {0};
         uint32_t numSource   {0};
         uint64_t length      {0};
      };
      
   } // namespace v0
   
   inline
   namespace v1 {
      // " MRFEMIT" : little endian of "TIMEFRM "
      constexpr uint64_t MAGIC {0x004d5246454d4954};
      // TYPE 
      constexpr uint16_t META          {1};
      constexpr uint16_t SLICE         {2};
      constexpr uint16_t COMPLETE_TF   {0x00};
      constexpr uint16_t INCOMPLETE_TF {0x10};
      
      #pragma pack(2)
      struct Header {
         uint64_t magic       {MAGIC};
         uint32_t length      {0};
         uint16_t hLength     {24};
         uint16_t type        {0};
         uint32_t timeFrameId {0};
         uint32_t numSource   {0};
         

         void Print() {
            printf("TimeFrameHeader\n");
            printf("Length        = %d\n",length);
            printf("Header Lenght = %d\n",hLength);
            printf("Type          = %d\n",type);
            printf("timeframeid   = %d\n",timeFrameId);
            printf("numSource     = %d\n",numSource);
         }
      };
      
   } // namespace v1
   
} // namespace TimeFrame

#endif
