#ifndef TFBFilePlayer_h
#define TFBFilePlayer_h

// File replay for TimeFrameBuilder output data

#include <cstdint>
#include <string>
#include <string_view>

#include <fairmq/Device.h>

class TFBFilePlayer : public fair::mq::Device 
{
public: 
  struct OptionKey {
    static constexpr std::string_view InputFileName{"in-file"};
    static constexpr std::string_view OutputChannelName{"out-chan-name"};
  };
  TFBFilePlayer();
  TFBFilePlayer(const TFBFilePlayer&) = delete;
  TFBFilePlayer& operator=(const TFBFilePlayer&) = delete;
  ~TFBFilePlayer() = default;

protected:
  bool ConditionalRun() override;
  void InitTask() override;

private: 
  std::string fInputFileName;
  std::string fOutputChannelName;
};

#endif