#ifndef V1740DSampler_H_
#define V1740DSampler_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <thread>
#include <atomic>
#include <memory>

#include <fairmq/Device.h>

#include "V1740D.h"

class V1740DSampler : public fair::mq::Device
{
public:
    struct OptionKey {
        static constexpr std::string_view BoardId           {"board-id"};
        static constexpr std::string_view NodeId            {"node-id"};
        static constexpr std::string_view ConfigFile        {"cfg-file"};
        static constexpr std::string_view OutputChannelName {"out-chan-name"};
    };

    V1740DSampler();
    V1740DSampler(const V1740DSampler&)            = delete;
    V1740DSampler& operator=(const V1740DSampler&) = delete;
    ~V1740DSampler() = default;

    int  RunAcquisition();
    void PrintParameters();

private:

  
protected:
    bool ConditionalRun() override;
    void Init() override;
    void InitTask() override;
    void PreRun() override;
    void PostRun() override;
    void ResetTask() override;

    std::string BoardId {"0"};  
    std::string NodeId  {"0"};
    std::string fOutputChannelName;
    std::string fConfigFile;
  
    int fBoardId  {0};
    int fNodeId   {0};

    BoardParameters fBoardParameters;
    V1740D gV1740D;
};

#endif
