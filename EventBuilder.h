/**
 * @file EventBuilder.h 
 * @brief EventBuilder for NestDAQ
 * @date Created :
 *       Last Modified : 2023/07/18 04:38:13
 *
 * @author Shinsuke OTA <ota@rcnp.osaka-u.ac.jp>
 *
 */


 #ifndef NESTDAQ_EVENTBUILDER_H
 #define NESTDAQ_EVENTBUILDER_H

 #include <fairmq/Device.h>

namespace nestdaq {
   class EventBuilder;
}


class nestdaq::EventBuilder : public fair::mq::Device {
public:

struct OptionKey {
		static constexpr std::string_view InputChannelName   {"in-chan-name"};
		static constexpr std::string_view OutputChannelName  {"out-chan-name"};
		static constexpr std::string_view DQMChannelName     {"dqm-chan-name"};
		static constexpr std::string_view DataSuppress       {"data-suppress"};
		static constexpr std::string_view PollTimeout        {"poll-timeout"};
		static constexpr std::string_view SplitMethod        {"split"};
	};      


   EventBuilder(); 
   virtual ~EventBuilder() override = default; 

   void InitTask() override;
   bool ConditionalRun() override;
   void PostRun() override;

   static bool IsEndDelimiter(uint64_t data);
   

protected:
   std::string fInputChannelName; // name of input channel 
   std::string fOutputChannelName; // name of output channel
   
   std::string fName; // name of this process
   uint32_t fID; // id of this process
   
	int fNumDestination {0};
	uint32_t fDirection {0};
	int fPollTimeoutMS  {0};
	int fSplitMethod    {0};

   std::vector<uint64_t> fTref;  // store reference tbin
   std::vector<std::vector<std::vector<uint64_t>>> fTDCData; // store tdc data in the array of tbin / stfb / nhits
   //std::vector<std::vector<uint64_t>> fTDCData;

   std::vector<int64_t> fTDCIdx; // idx
};


bool nestdaq::EventBuilder::IsEndDelimiter(uint64_t data)
{
   return ((data & 0xf000'0000'0000'0000) == 0x7000'0000'0000'0000
         || (data & 0xf000'0000'0000'0000) == 0x5000'0000'0000'0000 );
}

 #endif // NESTDAQ_EVENTBUILDER_H