/**
 * @file InputRegister.h
 * @brief Software Input Register for NestDAQ
 * @date Created : 2024-05-04 12:27:57 JST
 *       Last Modified : 2024-07-04 20:35:54 JST
 *
 * @author Shinsuke OTA <ota@rcnp.osaka-u.ac.jp>
 *
 */

#ifndef INPUTREGISTER_H
#define INPUTREGISTER_H

#include "fairmq/Device.h"
#include "KTimer.cxx"
#include "SubTimeFrameHeader.h"
#include "TimeFrameHeader.h"
#include "FilterHeader.h"
#include "HeartbeatFrameHeader.h"
#include "FrameContainer.h"

namespace nestdaq {
   class InputRegister;


   struct hbd1 {
      uint64_t type;
      uint64_t flag;
      uint64_t offset;
      uint64_t hbfn;
   };
}



class nestdaq::InputRegister : public fair::mq::Device {

public:
   static constexpr uint32_t kID {SubTimeFrame::INPUT_REG}; 
   InputRegister();
   virtual ~InputRegister() override = default;

   void InitTask() override;
   bool ConditionalRun() override;


   struct OptionKey {
      static constexpr std::string_view InputChannelName {"in-chan-name"};
      static constexpr std::string_view OutputChannelName {"out-chan-name"};
      static constexpr std::string_view RegisterChannels {"register-channels"};
      static constexpr std::string_view FEMID {"output-fem-id"};
      static constexpr std::string_view FEMType {"output-fem-type"};
      static constexpr std::string_view PollTimeout        {"poll-timeout"};
      static constexpr std::string_view SplitMethod        {"split"};
   };


   uint64_t GenerateStatus(uint64_t ch, uint64_t bit, uint64_t offset, uint64_t tdc8n, uint64_t hbfn) {
      uint64_t status = ((ch & 0xff) << 48);
      status |= ((bit & 0x3) << 46);
      status |= ((((offset>>13)+tdc8n) & 0xffff) << 24);
      status |= (hbfn & 0xffffff);
      return status;
   }


   hbd1 DecodeHBD1(uint64_t data) {
      hbd1 hbd;
      hbd.type = ((data >> 58) & 0x3f);
      hbd.flag = ((data >> 40) & 0xffff);
      hbd.offset = ((data >> 24) & 0xffff);
      hbd.hbfn = ((data) & 0xffffff);
      return hbd;
   }
      
      

protected:
   bool ParseMessages(FairMQParts& inParts);

   TSTF fSubtimeFrame;
   TSTF fOutputSubtimeFrame;


   std::string fInputChannelName;
   std::string fOutputChannelName;

   std::vector<uint32_t> fRegisterChannels;
   
   // output control
   int fNumDestination {0};
   uint32_t fDirection {0};
   int fPollTimeoutMS  {0};
   int fSplitMethod    {0};

   std::vector<uint64_t> fStatus;   // keep status
   KTimer fKT;
   bool fDoCheck;
};


#endif
