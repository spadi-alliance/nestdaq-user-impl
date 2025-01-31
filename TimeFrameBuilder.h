#ifndef TimeFrameBuilder_h
#define TimeFrameBuilder_h

#include <string>
#include <string_view>
#include <cstdint>
#include <chrono>

#include <vector>
#include <unordered_map>
#include <unordered_set>

#include <fairmq/Device.h>


class TimeFrameBuilder : public fair::mq::Device
{
public:
    struct OptionKey {
        static constexpr std::string_view NumSource            {"num-source"};
        static constexpr std::string_view BufferTimeoutInMs    {"buffer-timeout"};
        static constexpr std::string_view BufferDepthLimit     {"buffer-depth-limit"};
        static constexpr std::string_view InputChannelName     {"in-chan-name"};
        static constexpr std::string_view OutputChannelName    {"out-chan-name"};
        static constexpr std::string_view DQMChannelName       {"dqm-chan-name"};      
        static constexpr std::string_view DecimatorChannelName {"decimator-chan-name"};
        static constexpr std::string_view PollTimeout          {"poll-timeout"};
        static constexpr std::string_view DecimationFactor     {"decimation-factor"};
        static constexpr std::string_view DecimationOffset     {"decimation-offset"};
    };

    struct STFBuffer {
        FairMQParts parts;
        std::chrono::steady_clock::time_point start;
    };

    TimeFrameBuilder();
    TimeFrameBuilder(const TimeFrameBuilder&)            = delete;
    TimeFrameBuilder& operator=(const TimeFrameBuilder&) = delete;
    ~TimeFrameBuilder() = default;

protected:
    bool ConditionalRun() override;
    void Init() override;
    void InitTask() override;
    void PostRun() override;
    void PreRun() override;

private:
    int fNumSource {0};
    int fBufferTimeoutInMs {10000};
    int fBufferDepthLimit {1000};
    std::string fInputChannelName;
    std::string fOutputChannelName;
    std::string fDQMChannelName;  
    std::string fDecimatorChannelName;
    int fNumDestination   {0};
    uint32_t fDirection   {0}; // used to determine the sub-socket index
    uint32_t fDecimationFactor {0};
    uint32_t fDecimationOffset {0};
    int fDecimatorNumberOfConnectedPeers {0};
    int fPollTimeoutMS    {0};
    uint64_t fNumSend {0};

    std::unordered_map<uint32_t, std::vector<STFBuffer>> fTFBuffer;
    //std::unordered_set<uint64_t> fDiscarded;

};

#endif
