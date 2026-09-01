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
        static constexpr std::string_view InputFileName     {"in-file"};
        static constexpr std::string_view OutputChannelName {"out-chan-name"};
        static constexpr std::string_view MaxIterations     {"max-iterations"};
        static constexpr std::string_view PollTimeout       {"poll-timeout"};
        static constexpr std::string_view IterationWait     {"wait"};
        static constexpr std::string_view SplitMethod       {"split"};
        static constexpr std::string_view EnableRecbe       {"recbe"};

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
    int64_t fNumIteration  {0};
    int64_t fMaxIterations {0};
    int fDirection         {0};
    int fNumDestination    {0};
    int fPollTimeoutMS     {0};
    int fWait              {0};
    int fSplitMethod       {0};

    bool fEnableRecbe      {false};
};

#endif
