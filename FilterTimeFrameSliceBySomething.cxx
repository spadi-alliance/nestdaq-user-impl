/*
 * @file FilterTimeFrameSliceBySomething
 * @brief Slice Timeframe by Logic timing for NestDAQ
 * @date Created : 2024-05-04 12:31:55 JST
 *       Last Modified : 2024-05-23 07:12:22 JST
 *
 * @author Shinsuke OTA <ota@rcnp.osaka-u.ac.jp>
 *
 */
#include "FilterTimeFrameSliceBySomething.h"
#include "FilterTimeFrameSliceABC.icxx"
#include "fairmq/runDevice.h"

#include "utility/MessageUtil.h"
#include "UnpackTdc.h"

#define DEBUG 0

using nestdaq::FilterTimeFrameSliceBySomething;
namespace bpo = boost::program_options;




FilterTimeFrameSliceBySomething::FilterTimeFrameSliceBySomething()
{
}

bool FilterTimeFrameSliceBySomething::ProcessSlice(TTF& tf)
{
#if 0
   int doKeep = false;

   if (tf[0]->GetHeader()->femid == /* some module */) {
      auto& hbf = tf[0]->at(0)); // 
      uint64_t nData = hbf->GetNumData();
      for (int i = 0; i < nData; ++i) {
         // unpack data
         // Unpack(hbf->UncheckedAt(i),tdc);
         //
      }
      // judge
      doKeep = true;
   }

   if (!doKeep) {
      return false;
   }
#endif   
   return true;
}



////////////////////////////////////////////////////
// override runDevice
////////////////////////////////////////////////////

void addCustomOptions(bpo::options_description& options)
{
   using opt = FilterTimeFrameSliceBySomething::OptionKey;

   options.add_options()
      (opt::InputChannelName.data(),
       bpo::value<std::string>()->default_value("in"),
       "Name of the input channel")
      (opt::OutputChannelName.data(),
       bpo::value<std::string>()->default_value("out"),
       "Name of the output channel")
      (opt::DQMChannelName.data(),
       bpo::value<std::string>()->default_value("dqm"),
       "Name of the data quality monitoring channel")
      (opt::PollTimeout.data(),
       bpo::value<std::string>()->default_value("1"),
       "Timeout of polling (in msec)")
      (opt::SplitMethod.data(),
       bpo::value<std::string>()->default_value("1"),
       "STF split method")
      ;
   
}



std::unique_ptr<fair::mq::Device> getDevice(fair::mq::ProgOptions& /*config*/)
{
    return std::make_unique<FilterTimeFrameSliceBySomething>();
}

