#ifndef Examples_Sampler_h
#define Exapmles_Sampler_h

#include <string>

#if __has_include(<fairmq/Device.h>)
#include <fairmq/Device.h>  // since v1.4.34
#else 
#include <fairmq/FairMQDevice.h>
#endif

#include "emulator/AmQTdcData.h"
#include "emulator/AmQTdc.h"


//
constexpr uint64_t Magic {0x4f464e492d4d4546};

struct FEMInfo {
  uint64_t magic    {Magic};
  uint32_t FEMId    {0};
  uint32_t FEMType  {0};
  uint64_t reserved {0};
};

class Sampler : public FairMQDevice
{
  public: 
  Sampler();
  ~Sampler() override = default;

  protected:
    std::string fText;
    uint64_t fMaxIterations;
    uint64_t fNumIterations;

    void Init() override;
    void InitTask() override;
    bool ConditionalRun() override;
    void PostRun() override;
    void PreRun() override;
    void Run() override;

    FEMInfo  fem_info_;
    AmQTdc   amqTdc;
    int      tdc_type;
    unsigned int max_cycle_count;
    int GeneCycle(uint8_t* buffer);
    const int fnByte  {8};
    //    int fnWordCount {16384*10};
    int fnWordCount {256};
    int HBrate {512};
};

#endif
