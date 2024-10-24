#ifndef ScalerSQ_h
#define ScalerSQ_h

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
#include <Slowdashify.h>
#include <RedisDataStore.h>

#include "sqlite/sqlite3.h"

// forward declaration

std::array<std::string, 3> tables = {"Scaler_", "AmQFlag_", "HBF_"};
std::array<std::string, 3> columns = {"(timestamp INTEGER, channel STRING, count REAL)",
				      "(timestamp INTEGER, iBit STRING, bitSum INTEGER)",
				      "(timestamp INTEGER, nHBF REAL)"  };
std::array<std::string, 3> values = { "(timestamp, channel, count) VALUES(?,?,?)",
				      "(timestamp, iBit, bitSum) VALUES(?,?,?);",
				      "(timestamp, nHBF) VALUES(?,?)" };

class ScalerSQ : public fair::mq::Device
{
public:
    struct OptionKey {
        static constexpr std::string_view NumSource         {"num-source"};
        static constexpr std::string_view BufferTimeoutInMs {"buffer-timeout"};
        static constexpr std::string_view InputChannelName  {"in-chan-name"};
        static constexpr std::string_view OutputChannelName {"out-chan-name"};      
        static constexpr std::string_view RunNumber         {"run_number"};
        /*static constexpr std::string_view Http              {"http"};*/
        static constexpr std::string_view UpdateInterval    {"update-ms"};
        static constexpr std::string_view DataBaseDir       {"db-dir"};
    };

    enum Status {
        OK,
        Mismatch,
        Discarded
    };

    ScalerSQ();
    ScalerSQ(const ScalerSQ&)            = delete;
    ScalerSQ& operator=(const ScalerSQ&) = delete;
    ~ScalerSQ() = default;
    
private:

    bool HandleData(FairMQParts& parts, int index);
    void Init() override;
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
    std::string fOutputChannelName;  
    int32_t fFEMId;
    std::vector<uint16_t> fHbc;
    int fUpdateIntervalInMs {1000};
    std::chrono::steady_clock::time_point fPrevUpdate;
    std::string fDataBaseDir;
    
    std::unique_ptr<nestdaq::FileUtil> fFile;
    std::string fFileExtension;
  
    std::mutex fMutex;
    std::string fSeparator;
    std::string fServiceName;
    std::string fTopPrefix;
    std::string fdevId;

    uint32_t    fpreFlagSum[10]; 
  
    uint32_t    FlagSum[10];
    uint32_t    tsHeartbeatFlag {0};
    uint64_t    tsHeartbeatCounter {0};
    uint64_t    fPrevHeartbeatCounter {0};

    UH1Book *hScaler {0};
    UH1Book *hScalerPrev {0};
    UH1Book *hCountRate {0};
    UH1Book *hFlag {0};
    UH1Book *hFlagPrev {0};

    sqlite3  *SqltDB;
    sqlite3_stmt *st;
  
    std::string fNumId;
};

//_____________________________________________________________________________
inline std::string join(const std::vector<std::string> &v, std::string_view separator)
{
    return boost::join(v, separator.data());
}

#endif
