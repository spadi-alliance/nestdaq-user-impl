#ifndef AmQStrTdcSampler_h
#define AmQStrTdcSampler_h

#include <cstdint>
#include <string>
#include <string_view>
#include <thread>
#include <atomic>
#include <memory>

#include <fairmq/Device.h>

//                          O F N I - M E F
constexpr uint64_t Magic {0x4f464e492d4d4546};
constexpr int kOutBufByte           {8};
//  const int fnWordPerCycle {16384*10};
//constexpr int fnWordPerCycle {16384};
constexpr int fnWordPerCycle {1024};

struct FEMInfo {
    uint64_t magic    {Magic};
    uint32_t FEMId    {0};
    uint32_t FEMType  {0};
    uint64_t reserved {0};
};

struct lrWord {
    uint8_t d[5];
};

class AmQStrTdcSampler : public fair::mq::Device
{
public:
    struct OptionKey {
        static constexpr std::string_view IpSiTCP           {"sitcp-ip"};
        static constexpr std::string_view TdcType           {"TdcType"};
        static constexpr std::string_view OutputChannelName {"out-chan-name"};
    };

    AmQStrTdcSampler();
    AmQStrTdcSampler(const AmQStrTdcSampler&)            = delete;
    AmQStrTdcSampler& operator=(const AmQStrTdcSampler&) = delete;
    ~AmQStrTdcSampler() = default;

    int ConnectSocket(const char* ip);
    int Event_Cycle(uint8_t* buffer, int& num_recieved_bytes);
    int receive(int sock, char* data_buf, unsigned int length, int& num_recived_bytes);

    void SendFEMInfo();

protected:
    bool ConditionalRun() override;
    void Init() override;
    void InitTask() override;
    void PreRun() override;
    void PostRun() override;
    void ResetTask() override;

    int fAmqSocket {0};
    std::string fIpSiTCP {"0"};
    std::string fOutputChannelName;




    FEMInfo  fem_info_;
    int fTdcType {2};
    int header_pos {0};
    int optnByte   {0};

    int remain {0};
};

#endif
