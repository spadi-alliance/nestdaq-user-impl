/**
 * @file TimeFrameSlicerByLogicTiming.h
 * @brief TimeFrameSlicerByLogicTiming
 * @date Created : 2024-05-04 12:27:57 JST
 *       Last Modified : 2024-06-29 13:09:15 JST
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
      static constexpr std::string_view DQMChannelName {"dqm-chan-name"};
      static constexpr std::string_view TimeOffsetBegin {"time-offset-begin"};
      static constexpr std::string_view TimeOffsetEnd {"time-offset-end"};
      static constexpr std::string_view PollTimeout        {"poll-timeout"};
      static constexpr std::string_view SplitMethod        {"split"};
   };

protected:

   bool ParseMessages(FairMQParts& inParts);
   
   std::string fInputChannelName;
   std::string fOutputChannelName;
   std::string fDQMChannelName;

   std::string fName;
   uint32_t fID;
   int fOffset[2];


   std::vector<KTimer> fKTimer;
   bool fDoCheck;
   


   TTF fTF;
   TLF fLF; // logic filter

   uint32_t fIdxHBF;
   uint32_t fNumHBF;
   
   int fNextIdx;
   bool fEOM;

   // for the output
   uint64_t fTFHidx;


   // output
   int fNumDestination {0};
   uint32_t fDirection {0};
   int fPollTimeoutMS  {0};
   int fSplitMethod    {0};
   

};


#endif  // NESTDAQ_TIMEFRAMESLICERBYLOGICTIMING_H
