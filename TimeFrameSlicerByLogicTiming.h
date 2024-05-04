/**
 * @file EventBuilder.h 
 * @brief EventBuilder for NestDAQ
 * @date Created : 2024-05-04 12:27:57 JST
 *       Last Modified : 2024-05-04 17:34:31 JST
 *
 * @author Shinsuke OTA <ota@rcnp.osaka-u.ac.jp>
 *
 */


#ifndef NESTDAQ_TIMEFRAMESLICERBYLOGICTIMING_H
#define NESTDAQ_TIMEFRAMESLICERBYLOGICTIMING_H

#include "fairmq/Device.h"
#include "KTimer.cxx"
#include "SubTimeFrameHeader.h"
#include "TimeFrameHeader.h"
#include "FilterHeader.h"
#include "HeartbeatFrameHeader.h"

namespace nestdaq {
   class TimeFrameSlicerByLogicTiming;
}


class nestdaq::TimeFrameSlicerByLogicTiming : public fair::mq::Device {
public:
   TimeFrameSlicerByLogicTiming();
   virtual ~TimeFrameSlicerByLogicTiming() override = default;

   void PreRun() override;
   void InitTask() override;
   bool ConditionalRun() override;
   void PostRun() override;


   struct OptionKey {
      static constexpr std::string_view InputChannelName {"in-chan-name"};
      static constexpr std::string_view OutputChannelName {"out-chan-name"};
      static constexpr std::string_view DQMChannelName {"out-chan-name"};
   };

protected:

   bool GetNextHBF(FairMQParts& inParts);
   
   std::string fInputChannelName;
   std::string fOutputChannelName;

   std::string fName;
   uint32_t fID;


   std::vector<KTimer> fKTimer;
   bool fDoCheck;
   

   template <typename frametype> class Header {
   public:
      void SetHeader(void* data) { fHeader = reinterpret_cast<frametype*>(data); }
      frametype* GetHeader() { return fHeader; }
   protected:
      frametype *fHeader;
   };

   template <class frametype, class datatype> class DataFrame : public Header<frametype>  {
   public:
      void Set(void* data) {
         this->SetHeader(data);
         fData = reinterpret_cast<datatype*>(data) + sizeof(frametype);
         fNumData = (this->GetHeader()->length - this->GetHeader()->hLength)/sizeof(datatype);
      }
   protected:
      datatype* fData;
      uint64_t fNumData;
   };

   
   template <class frametype, class child> class ContainerFrame : public Header<frametype>, public std::vector<child*>  {
   }  ;

   
   class THBF : public DataFrame<struct HeartbeatFrame::Header,uint64_t> {
   };

   using TTT = DataFrame<struct Filter::TrgTimeHeader,uint32_t>;
   class TLF : public ContainerFrame<struct Filter::Header,TTT> {
   };

   class TSTF : public ContainerFrame<struct SubTimeFrame::Header,THBF> {
   };

   class TTF : public ContainerFrame<struct TimeFrame::Header,TSTF> {
   };

   TTF fTF;
   TLF fLF; // logic filter
   
   int fNextIdx;
   bool fEOM;
};


#endif  // NESTDAQ_TIMEFRAMESLICERBYLOGICTIMING_H
