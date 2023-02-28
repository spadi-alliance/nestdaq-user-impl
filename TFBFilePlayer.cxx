#include <fairmq/runDevice.h>

#include "TFBFilePlayer.h"

namespace bpo = boost::program_options;

//_____________________________________________________________________________
void addCustomOptions(bpo::options_description& options)
{
  using opt = TFBFilePlayer::OptionKey;
  options.add_options()
    (opt::InputFileName.data(), bpo::value<std::string>(), "path to input data file")
    //
    (opt::OutputChannelName.data(), bpo::value<std::string>()->default_value("out"), "Name of the data output channel");
}

//_____________________________________________________________________________
std::unique_ptr<fair::mq::Device> getDevice(fair::mq::ProgOptions& /*config*/)
{
  return std::make_unique<TFBFilePlayer>(); 
}

//_____________________________________________________________________________
TFBFilePlayer::TFBFilePlayer()
  : fair::mq::Device()
{}

//_____________________________________________________________________________
bool TFBFilePlayer::ConditionalRun()
{
  return true;
}

//_____________________________________________________________________________
void TFBFilePlayer::InitTask()
{
  using opt = OptionKey;
  fInputFileName = fConfig->GetProperty<std::string>(opt::InputFileName.data());
  fOutputChannelName = fConfig->GetProperty<std::string>(opt::OutputChannelName.data());
}