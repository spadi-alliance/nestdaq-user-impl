#ifndef DigitizerSampler_H_
#define DigitizerSampler_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <thread>
#include <atomic>
#include <memory>

#include <fairmq/Device.h>

#include "Digitizer.h"
#include "DigitizerStruct.h"

class DigitizerSampler : public fair::mq::Device
{
public:
    struct OptionKey {
        static constexpr std::string_view BoardId           {"board-id"};
        static constexpr std::string_view NodeNum           {"node-num"};      
        static constexpr std::string_view NodeId            {"node-id"};
        static constexpr std::string_view ConfigFile        {"cfg-file"};
        static constexpr std::string_view OutputChannelName {"out-chan-name"};
    };

    DigitizerSampler();
    DigitizerSampler(const DigitizerSampler&)            = delete;
    DigitizerSampler& operator=(const DigitizerSampler&) = delete;
    ~DigitizerSampler() = default;

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
    int fNodeNum  {0};
    int fNodeId   {0};

    WaveDumpConfig_t fBoardParameters;
    Digitizer gDigitizer;
};

#endif
