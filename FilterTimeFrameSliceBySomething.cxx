/*
 * @file FilterTimeFrameSliceBySomething
 * @brief Slice Timeframe by Logic timing for NestDAQ
 * @date Created : 2024-05-04 12:31:55 JST
 *       Last Modified : 2024-05-21 17:30:38 JST
 *
 * @author Shinsuke OTA <ota@rcnp.osaka-u.ac.jp>
 *
 */
#include "FilterTimeFrameSliceBySomething.h"
#include "fairmq/runDevice.h"

#include "utility/MessageUtil.h"
#include "UnpackTdc.h"

#define DEBUG 0

using nestdaq::FilterTimeFrameSliceBySomething;
namespace bpo = boost::program_options;


FilterTimeFrameSliceBySomething::FilterTimeFrameSliceBySomething()
{
}

bool FilterTimeFrameSliceBySomething::ProcessSlice(TTF& tf) {

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
   
   return true;
}



////////////////////////////////////////////////////
// override runDevice
////////////////////////////////////////////////////

void addCustomOptions(bpo::options_description& options)
{
   using opt = FilterTimeFrameSliceBySomething::OptionKey;
// option can be defined   
}


std::unique_ptr<fair::mq::Device> getDevice(fair::mq::ProgOptions& /*config*/)
{
    return std::make_unique<FilterTimeFrameSliceBySomething>();
}

