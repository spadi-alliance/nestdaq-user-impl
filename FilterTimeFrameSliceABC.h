/**
 * @file FilterTimeFrameSliceABC.h
 * @brief Abstract base class for filtering time frame slice
 * @date Created : 2024-05-04 12:27:57 JST
 *       Last Modified : 2024-05-21 17:18:34 JST
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
#include "FrameContainer.h"

namespace nestdaq {
   class FilterTimeFrameSliceABC;
}


class nestdaq::FilterTimeFrameSliceABC : public fair::mq::Device {
public:
   FilterTimeFrameSliceABC();
   virtual ~FilterTimeFrameSliceABC() override = default;

   void PreRun() override;
   void InitTask() override;
   bool ConditionalRun() override;
   void PostRun() override;


   struct OptionKey {
      static constexpr std::string_view InputChannelName {"in-chan-name"};
      static constexpr std::string_view OutputChannelName {"out-chan-name"};
      static constexpr std::string_view DQMChannelName {"out-chan-name"};
      static constexpr std::string_view PollTimeout        {"poll-timeout"};
      static constexpr std::string_view SplitMethod        {"split"};
   };

protected:

   bool ParseMessages(FairMQParts& inParts);

   bool ProcessSlice(TTF& ) { return true; }
   
   std::string fInputChannelName;
   std::string fOutputChannelName;
   std::string fName;
   uint32_t fID;


   // control
   uint32_t fNextIdx;

   std::vector<KTimer> fKTimer;
   bool fDoCheck;

   std::vector<TTF> fTFs; // time frame
//   std::vector<TLF> fLFs; // logic filter



   // output
   int fNumDestination {0};
   uint32_t fDirection {0};
   int fPollTimeoutMS  {0};
   int fSplitMethod    {0};
   

};


#endif  // NESTDAQ_TIMEFRAMESLICERBYLOGICTIMING_H
