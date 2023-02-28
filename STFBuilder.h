#ifndef STFBuilder_H
#define STFBuilder_H

#include <string>
#include <string_view>
#include <cstdint>
#include <deque>
#include <memory>
#include <queue>

#include <fairmq/Device.h>

#include "AmQStrTdcData.h"

struct FEMInfo {
  uint64_t magic    {0};
  uint32_t FEMType  {0};
  uint32_t FEMId    {0};
  uint64_t reserved {0};
};


class AmQStrTdcSTFBuilder : public fair::mq::Device
{
public:
  using WorkBuffer = std::unique_ptr<std::vector<FairMQMessagePtr>>;
  using RecvBuffer = std::vector<AmQStrTdc::Data::Word>; //64 bits
  using SendBuffer = std::queue<WorkBuffer>;

  struct OptionKey {
    static constexpr std::string_view FEMId             {"fem-id"};
    static constexpr std::string_view InputChannelName  {"in-chan-name"};
    static constexpr std::string_view OutputChannelName {"out-chan-name"};
    static constexpr std::string_view DQMChannelName    {"dqm-chan-name"};
    static constexpr std::string_view StripHBF          {"strip-hbf"};
    static constexpr std::string_view MaxHBF            {"max-hbf"};
    static constexpr std::string_view SplitMethod       {"split"};
  };

  AmQStrTdcSTFBuilder();
  AmQStrTdcSTFBuilder(const AmQStrTdcSTFBuilder&)            = delete;
  AmQStrTdcSTFBuilder& operator=(const AmQStrTdcSTFBuilder&) = delete;
  ~AmQStrTdcSTFBuilder() = default;

private:
  void BuildFrame(FairMQMessagePtr& msg, int index);
  void FillData(AmQStrTdc::Data::Word* first, 
                AmQStrTdc::Data::Word* last, 
                bool isSpillEnd);
  void FinalizeSTF();
  bool HandleData(FairMQMessagePtr&, int index);
  void Init() override;
  void InitTask() override;
  void NewData();
  void PostRun() override;

  uint64_t fFEMId   {0};
  uint64_t fFEMType {0};
  int         fNumDestination {0};
  std::string fInputChannelName;
  std::string fOutputChannelName;
  std::string fDQMChannelName;
  int fMaxHBF {1};
  int fHBFCounter {0};
  uint64_t fSTFId {0};
  int fSplitMethod {0};
  uint8_t fLastHeader {0};
  // int fH_flag {0};

  bool mdebug;
  RecvBuffer fInputPayloads;
  WorkBuffer fWorkingPayloads;
  SendBuffer fOutputPayloads;
};

#endif
