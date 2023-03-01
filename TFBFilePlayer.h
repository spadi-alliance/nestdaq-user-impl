#ifndef TFBFilePlayer_h
#define TFBFilePlayer_h

// File replay for TimeFrameBuilder output data

#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>

#include <fairmq/Device.h>

class TFBFilePlayer : public fair::mq::Device 
{
public: 
  struct OptionKey {
    static constexpr std::string_view InputFileName{"in-file"};
    static constexpr std::string_view OutputChannelName{"out-chan-name"};
    static constexpr std::string_view MaxIterations{"max-iterations"};
  };
  TFBFilePlayer();
  TFBFilePlayer(const TFBFilePlayer&) = delete;
  TFBFilePlayer& operator=(const TFBFilePlayer&) = delete;
  ~TFBFilePlayer() = default;

protected:
  bool ConditionalRun() override;
  void InitTask() override;
  void PostRun() override;
  void PreRun() override;

private: 
  std::string fOutputChannelName;
  std::string fInputFileName;
  std::ifstream fInputFile;
  int64_t fNumIteration{0};
  int64_t fMaxIterations{0};
};

#endif