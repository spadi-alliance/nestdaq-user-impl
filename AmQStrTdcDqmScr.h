#ifndef AmQStrTdcDqmScr_h
#define AmQStrTdcDqmScr_h

#include <chrono>
#include <string>
#include <string_view>
#include <memory>
#include <cstdint>
#include <map>
#include <unordered_map>
#include <unordered_set>

#include <fairmq/Device.h>

#include "utility/RingBuffer.h"
#include "utility/FileUtil.h"

#include "AmQStrTdcData.h"

#include <boost/algorithm/string.hpp>

#include <sw/redis++/redis++.h>
#include <sw/redis++/patterns/redlock.h>
#include <sw/redis++/errors.h>
// forward declaration
namespace sw::redis {
class Redis;
template <typename Impl> class QueuedRedis;
class PipelineImpl;
using Pipeline = QueuedRedis<PipelineImpl>;
}

class AmQStrTdcDqmScr : public fair::mq::Device
{
public:
    struct OptionKey {
        static constexpr std::string_view NumSource         {"num-source"};
        static constexpr std::string_view SourceType        {"source-type"};      
        static constexpr std::string_view BufferTimeoutInMs {"buffer-timeout"};
        static constexpr std::string_view InputChannelName  {"in-chan-name"};
        static constexpr std::string_view RunNumber{"run_number"};
      /*static constexpr std::string_view Http              {"http"};*/
        static constexpr std::string_view UpdateInterval    {"update-ms"};
        static constexpr std::string_view ServerUri         {"dqm-uri"};
        static constexpr std::string_view CreatedTimePrefix {"created-time"};
        static constexpr std::string_view HostnamePrefix    {"hostname"};
        static constexpr std::string_view HostIpAddressPrefix{"host-ip"};
    };

    struct STFBuffer {
        FairMQParts parts;
        std::chrono::steady_clock::time_point start;
    };

    enum Status {
        OK,
        Mismatch,
        Discarded
    };

    AmQStrTdcDqmScr();
    AmQStrTdcDqmScr(const AmQStrTdcDqmScr&)            = delete;
    AmQStrTdcDqmScr& operator=(const AmQStrTdcDqmScr&) = delete;
    ~AmQStrTdcDqmScr() = default;

private:
    void Check(std::vector<STFBuffer>&& stfs);
    bool HandleData(FairMQParts& parts, int index);
    bool HandleDataTFB(FairMQParts& parts, int index);
    void Init() override;
    void InitServer(std::string_view server);
    void InitTask() override;
    void PreRun() override;
    void PostRun() override;

    bool fDebug  {false};
  
    int64_t fRunNumber{0};
    int fNumSource {1};
    int fBins {1};  
    int fBufferTimeoutInMs {100000};
    int fId {0};
    std::string fInputChannelName;
    std::string fSourceType;  
    std::map<uint64_t, int> fFEMId;
    std::map<int, uint64_t> fModuleIp;
    std::vector<uint16_t> fHbc;
    int fUpdateIntervalInMs {1000};
    std::chrono::steady_clock::time_point fPrevUpdate;
    std::unique_ptr<nestdaq::FileUtil> fFile;
    std::string fFileExtension;
    
    std::unordered_map<uint32_t, std::vector<STFBuffer>> fTFBuffer;
    std::unordered_set<uint64_t> fDiscarded;

    std::mutex fMutex;
    std::shared_ptr<sw::redis::Redis> fClient;
    std::unique_ptr<sw::redis::Pipeline> fPipe;
    std::string fSeparator;
    std::string fServiceName;
    std::string fTopPrefix;
    std::string fdevId;
    std::string fCreatedTimeKey;
    std::string fHostNameKey;
    std::string fIpAddressKey;

    uint32_t tsHeartbeatFlag[10];
    uint32_t tsHeartbeatCounter;
    std::string fTsHeartbeatFlagKey[10];
    std::string fTsHeartbeatCounterKey;
    
    std::map<uint32_t, std::map<uint32_t, uint64_t> > scaler;
    std::unordered_map<uint32_t, uint64_t> tsScaler;
    std::string fTsScalerKey;
};

//_____________________________________________________________________________
inline std::string join(const std::vector<std::string> &v, std::string_view separator)
{
    return boost::join(v, separator.data());
}

#endif
