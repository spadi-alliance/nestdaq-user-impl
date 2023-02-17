#include <fairmq/runFairMQDevice.h>

#include "AmQStrTdcSampler.h"

namespace bpo = boost::program_options;

//______________________________________________________________________________
void
addCustomOptions(bpo::options_description& options)
{
    using opt = AmQStrTdcSampler::OptionKey;
    options.add_options()
      (opt::IpSiTCP.data(),           bpo::value<std::string>()->default_value("0"),   "SiTCP IP (xxx.yyy.zzz.aaa)")
      (opt::OutputChannelName.data(), bpo::value<std::string>()->default_value("data"), "Name of the output channel");
      //      ("TdcType", bpo::value<std::string>()->default_value("2"), "TDC type: HR=2, LR=1");
}

//______________________________________________________________________________
FairMQDevicePtr getDevice(const FairMQProgOptions&)
{
  return new AmQStrTdcSampler;
}
