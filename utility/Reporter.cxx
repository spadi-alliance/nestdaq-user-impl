#include <iostream>

#include <fairmq/FairMQParts.h>
#include <fairmq/FairMQLogger.h>
#include <fairmq/options/FairMQProgOptions.h>

#include "MessageUtil.h"
#include "Reporter.h"

//_______________________________________________________________________________
uint64_t Reporter::AddInputMessageSize(uint64_t n)
{
    auto& r = Instance();
    r.fMsgIn += n;
    if (r.fConfig && r.fConfig->Count("msg-in-bytes-total")) {
        r.fConfig->SetValue("msg-in-bytes-total", r.fMsgIn);
    }

    ++(r.fNumIn);
    if (r.fConfig && r.fConfig->Count("num-msg-in-total")) {
        r.fConfig->SetValue("num-msg-in-total", r.fNumIn);
    }
    return r.fMsgIn;
}

//_______________________________________________________________________________
uint64_t Reporter::AddInputMessageSize(const FairMQParts& parts)
{
    return AddInputMessageSize(MessageUtil::TotalLength(parts));
}

//_______________________________________________________________________________
uint64_t Reporter::AddOutputMessageSize(uint64_t n)
{
    auto& r = Instance();
    r.fMsgOut += n;
    if (r.fConfig && r.fConfig->Count("msg-out-bytes-total")) {
        r.fConfig->SetValue("msg-out-bytes-total", r.fMsgOut);
    }

    ++(r.fNumOut);
    if (r.fConfig && r.fConfig->Count("num-msg-out-total")) {
        r.fConfig->SetValue("num-msg-out-total", r.fNumOut);
    }
    return r.fMsgOut;
}

//_______________________________________________________________________________
uint64_t Reporter::AddOutputMessageSize(const FairMQParts& parts)
{
    return AddOutputMessageSize(MessageUtil::TotalLength(parts));
}

//_______________________________________________________________________________
Reporter& Reporter::Instance(FairMQProgOptions* config)
{
    static Reporter gInstance;
    if (!gInstance.fConfig && config) {
        gInstance.fConfig = config;
        fair::Logger::AddCustomSink("Reporter", "info", [](const std::string& content, const fair::LogMetaData& metaData) {
            if (metaData.severity != fair::Severity::state) Set(content);
        });
        LOG(info) << "Register custom sink of Reporter to FairLogger";
    }

    return gInstance;
}

//_______________________________________________________________________________
void Reporter::Reset()
{
    auto& r   = Instance();
    r.fNumIn  = 0;
    r.fNumOut = 0;
    r.fMsgIn  = 0;
    r.fMsgOut = 0;
}

//_______________________________________________________________________________
void Reporter::Set(const std::string& content)
{
    auto& r = Instance();
    if (r.fConfig && r.fConfig->Count("report")) {
        r.fConfig->SetValue("report", content);
    }
}
