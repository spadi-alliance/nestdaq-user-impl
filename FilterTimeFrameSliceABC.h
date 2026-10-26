/*
 * @file FilterTimeFrameSliceABC.icxx
 * @brief Slice Timeframe by Logic timing for NestDAQ
 * @date Created : 2024-05-04 12:31:55 JST
 *       Last Modified : 2024-07-21 02:23:44 JST (furukawa)
 *
 * @author Shinsuke OTA <ota@rcnp.osaka-u.ac.jp>
 * @comment Added geoToIndex function for VDC channel map loading
 */
#ifndef NESTDAQ_FILTERTIMEFRAMESLICEABC_H
#define NESTDAQ_FILTERTIMEFRAMESLICEABC_H

#include "fairmq/Device.h"
#include "KTimer.cxx"
#include "SubTimeFrameHeader.h"
#include "TimeFrameHeader.h"
#include "FilterHeader.h"
#include "HeartbeatFrameHeader.h"
#include "FrameContainer.h"

namespace nestdaq {
   class FilterTimeFrameSliceABC;
}

struct Wire_map {
    int catid = -1;
    int id = -1;
    int sh = -1;
    int fp = -1;
    int det = -1;
    uint64_t geo = -1;
    int ch = -1;
};

class nestdaq::FilterTimeFrameSliceABC : public fair::mq::Device {
public:
   FilterTimeFrameSliceABC();
   virtual ~FilterTimeFrameSliceABC() override = default;
   
   virtual void PreRun() override;
   virtual void InitTask() override;
   virtual bool ConditionalRun() override;
   virtual void PostRun() override;

   struct OptionKey {
      static constexpr std::string_view InputChannelName {"in-chan-name"};
      static constexpr std::string_view OutputChannelName {"out-chan-name"};
      static constexpr std::string_view DQMChannelName {"out-chan-name"};
      static constexpr std::string_view PollTimeout        {"poll-timeout"};
      static constexpr std::string_view SplitMethod        {"split"};
   };

protected:
   virtual bool ParseMessages(FairMQParts& inParts);
   virtual bool ProcessSlice(TTF& ) { return true; }
   
   std::string fInputChannelName;
   std::string fOutputChannelName;
   std::string fName;
   uint32_t fId;

   // control
   uint32_t fNextIdx;

   std::vector<KTimer> fKTimer;
   bool fDoCheck;

   // time frame
   std::vector<TTF> fTFs; 

   // Maximum number of wires on each plane 
   static const int maxCh = 112; 

   // Number of amaneq units = 8
   std::array<std::array<Wire_map, maxCh + 1>, 8> wireMapArray;


   int geoToIndex(uint64_t geo);

   // output
   int fNumDestination {0};  
   uint32_t fDirection {0};  
   int fPollTimeoutMS  {0}; 
   int fSplitMethod    {0};
};

#endif  // NESTDAQ_FILTERTIMEFRAMESLICEABC_H
