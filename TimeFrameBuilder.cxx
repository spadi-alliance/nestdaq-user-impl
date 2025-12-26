#include <chrono>
#include <functional>
#include <thread>
#include <cassert>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <ctime>

#include <fairmq/Poller.h>
#include <fairmq/runDevice.h>

#include "utility/HexDump.h"
#include "utility/MessageUtil.h"

#include "RedisDataStore.h"

#include "SubTimeFrameHeader.h"
#include "TimeFrameHeader.h"
#include "TimeFrameBuilder.h"


//#include "AmQStrTdcData.h"

namespace bpo = boost::program_options;

//______________________________________________________________________________
void addCustomOptions(bpo::options_description& options)
{
    using opt = TimeFrameBuilder::OptionKey;
    options.add_options()
    (opt::BufferTimeoutInMs.data(),    bpo::value<std::string>()->default_value("10000"),     "Buffer timeout in milliseconds")
    (opt::BufferDepthLimit.data(),     bpo::value<std::string>()->default_value("10000"),     "Buffer depth limit")
    (opt::InputChannelName.data(),     bpo::value<std::string>()->default_value("in"),        "Name of the input channel")
    (opt::OutputChannelName.data(),    bpo::value<std::string>()->default_value("out"),       "Name of the output channel")
    (opt::DQMChannelName.data(),       bpo::value<std::string>()->default_value("dqm"),       "Name of the data quality monitoring channel")
    (opt::DecimatorChannelName.data(), bpo::value<std::string>()->default_value("decimator"), "Name of the decimated output channel")
    (opt::PollTimeout.data(),          bpo::value<std::string>()->default_value("0"),         "Timeout (in msec) of polling")
    (opt::DecimationFactor.data(),     bpo::value<std::string>()->default_value("0"),         "Decimation factor for decimated output channel")
    (opt::DecimationOffset.data(),     bpo::value<std::string>()->default_value("0"),         "Decimation offset for decimated output channel")
    (opt::DiscardOutput.data(),        bpo::value<std::string>()->default_value("false"),     "Discard output option to eliminate the back pressure for upstream FairMQ device. true: discard, false (default): no discard causing back pressure")
    (opt::OutputIncompleteTF.data(),   bpo::value<std::string>()->default_value("false"),     "Output incomplete Time Frame")
    (opt::ObjectDbNumber.data(),       bpo::value<std::string>()->default_value("3"),         "DB number for DQM objects")
    ;
}


//______________________________________________________________________________
std::unique_ptr<fair::mq::Device> getDevice(FairMQProgOptions&)
{
    return std::make_unique<TimeFrameBuilder>();
}


//______________________________________________________________________________
TimeFrameBuilder::TimeFrameBuilder()
    : fair::mq::Device(), fKt(1000)
{
}


void TimeFrameBuilder::TFBFailDump(std::vector<STFBuffer> &tfBuf, uint32_t stfId)
{

    std::vector<uint32_t> femid;
    std::vector<uint32_t> expected = {
        160, 161, 162, 163, 164, 165, 166, 167, 168, 169,
        170, 171, 172, 173, 174, 175, 176, 177, 178, 179
    };
    std::vector<uint64_t> hb;
    for (auto & stfBuf : tfBuf) {
        auto & msg = stfBuf.parts[0];
        SubTimeFrame::Header *stfheader
            = reinterpret_cast<SubTimeFrame::Header *>(msg.GetData());
        //std::cout << " ID" << std::hex << stfheader->femId << std::dec;
        femid.push_back(stfheader->femId);
        for (auto it = expected.begin() ; it != expected.end() ;) {
            if (*it == (stfheader->femId && 0xff)) {
                it = expected.erase(it);
            } else {
                it++;
            }
        }
        //std::for_each(reinterpret_cast<uint64_t*>(msg.GetData()),
        //    reinterpret_cast<uint64_t*>(msg.GetData() + msg.GetSize()),
        //    ::HexDump{4});

        if (stfBuf.parts.Size() > 2) {
            auto & hb0 = stfBuf.parts[2];
            uint64_t hb00 = (reinterpret_cast<uint64_t *>(hb0.GetData()))[0];
            hb.push_back(hb00);
        }
    }

#if 0
    std::cout << "#D TFid :" << stfId << " Lost FEid :";
    for (auto & i : expected) std::cout << " " << (i & 0xff);
    std::cout << std::endl;
#else
    std::cout << "#D TFB Fail. TFid: " << stfId << "(0x" << std::hex << stfId << "), "
        << std::dec << "N: " << femid.size() << "/" << fNumSource << ", FEid:";
    for (auto & i : femid) std::cout << " " << (i & 0xff);
    std::cout << std::endl;
#endif

#if 0
    std::cout << "#D HB :" << stfId << ":";
    for (auto & i : hb) std::cout << " " << std::hex <<i;
    std::cout << std::dec << std::endl;
#endif

    return;
}


int TimeFrameBuilder::CheckHBFDelimitor(fair::mq::Parts &inParts, uint32_t stfId)
{
    auto nmsg = inParts.Size();
    const auto& msg =inParts.At(nmsg -1);

    auto msgSize  = msg->GetSize();
    auto nWord    = msgSize / sizeof(uint64_t);
    auto msgBegin = reinterpret_cast<uint64_t *>(msg->GetData());

    auto firstHBF  = reinterpret_cast<uint64_t *>(msgBegin + (nWord - 2));
    //auto secondHBF = reinterpret_cast<uint64_t *>(msgBegin + (nWord - 1));

    auto fem     = reinterpret_cast<SubTimeFrame::Header*>(inParts.At(0)->GetData())->femId;
    auto lastmsg = reinterpret_cast<uint64_t *>(inParts.At(inParts.Size() - 1)->GetData());
    unsigned int type = (firstHBF[0] & 0xfc00'0000'0000'0000) >> 58;
    //if ((type == 0x1c) || (type == 0x18) || (type == 0x14) || (type == 0x1e)) {
    if ((type == 0x1c) || (type == 0x1e)) {
    } else {
        LOG(warn) << "BAD delimitor " << std::hex << lastmsg[0]
                  << " FEM: " << std::dec << (fem & 0xff)
                  << " TFid: " << stfId;
        return -1;
    }

    return 0;
}


void TimeFrameBuilder::MakeOutPartsFromSTFBuffers(
    std::vector<STFBuffer>& tfBuf,
    uint32_t stfId,
    uint16_t type,
    fair::mq::Parts& outParts)
{
    auto h = std::make_unique<TimeFrame::Header>();
    h->magic       = TimeFrame::MAGIC;
#if 0
    h->timeFrameId = reinterpret_cast<SubTimeFrame::Header*>(tfBuf[0].parts.At(0)->GetData())->timeFrameId;
#else
    h->timeFrameId = stfId;
#endif
    h->numSource   = fNumSource;
    h->type        = type;
    h->length      = std::accumulate(tfBuf.begin(), tfBuf.end(), sizeof(TimeFrame::Header),
        [](auto init, auto& stfBuf) {
            return init + std::accumulate(stfBuf.parts.begin(), stfBuf.parts.end(), 0,
            [] (auto jinit, auto& m) {
                return (!m) ? jinit : jinit + m->GetSize();
            });
        });

    outParts.AddPart(MessageUtil::NewMessage(*this, std::move(h)));
    for (auto& stfBuf: tfBuf) {
        for (auto& m: stfBuf.parts) {
            outParts.AddPart(std::move(m));
        }
    }

    return;
}


void TimeFrameBuilder::SendTimeFrameForDQM(
    fair::mq::Parts& outParts)
{
    auto poller = NewPoller(fDQMChannelName);
    poller->Poll(fPollTimeoutMS);
    int numDestDQM = GetNumSubChannels(fDQMChannelName);
    for (int i = 0 ; i < numDestDQM ; i++) {
        if (poller->CheckOutput(fDQMChannelName, i)) {
            fair::mq::Parts dqmParts = MessageUtil::Copy(*this, outParts);
            if (Send(dqmParts, fDQMChannelName, i) < 0) {
                if (NewStatePending()) {
                    LOG(info) << "Device is not RUNNING";
                } else {
                    LOG(error) << "Failed to enqueue TFB (DQM) : " << i;
                }
            }
        }
    }

    #if 0
    if (trySend) {
        fair::mq::Parts dqmParts = MessageUtil::Copy(*this, outParts);
        if (Send(dqmParts, fDQMChannelName) < 0) {
            if (NewStatePending()) {
                LOG(info) << "Device is not RUNNING";
            } else {
                LOG(error) << "Failed to enqueue TFB (DQM) ";
            }
        }
    }
    #endif

    return;
}


void TimeFrameBuilder::SendTimeFrameForDecimator(
    fair::mq::Parts& outParts)
{

    if ((fDecimatorNumberOfConnectedPeers > 0)
        && (fDecimationFactor > 0)
        && (fNumSend % fDecimationFactor == fDecimationOffset)) {

        unsigned int decimatorNumSubChannels = 0;
        decimatorNumSubChannels = GetNumSubChannels(fDecimatorChannelName);

        auto poller = NewPoller(fDecimatorChannelName);
        poller->Poll(fPollTimeoutMS);
        for (auto iSubChannel=0u; iSubChannel<decimatorNumSubChannels; ++iSubChannel) {
            while (!NewStatePending()) {
                if (poller->CheckOutput(fDecimatorChannelName, iSubChannel)) {
                    fair::mq::Parts decimatorParts = MessageUtil::Copy(*this, outParts);
                    if (Send(decimatorParts, fDecimatorChannelName, iSubChannel) > 0) {
                        // LOG(debug) << " successfully send to decimator "
                        // << iSubChannel << " " << fNumSend << " " << fDecimationFactor;
                        break;
                    } else {
                        auto h = reinterpret_cast<TimeFrame::Header*>(decimatorParts.At(0)->GetData());
                        LOG(warn) << "Failed to enqueue to decimator output iSubChannel = "
                            << iSubChannel << " : TF = " << h->timeFrameId;
                    }
                }
            }
        }
    }

    return;
}


void TimeFrameBuilder::SendTimeFrame(
    fair::mq::Parts& outParts
)
{
    // send for Output
    auto poller = NewPoller(fOutputChannelName);
    int attempts = 0;
    while (!NewStatePending()) {
        poller->Poll(fPollTimeoutMS);
        auto direction = fDirection % fNumDestination;
        fDirection = direction + 1;
        if (poller->CheckOutput(fOutputChannelName, direction)) {
            // output ready
            if (Send(outParts, fOutputChannelName, direction) > 0) {
                ++fNumSend;
                break;
            } else {
                auto h = reinterpret_cast<TimeFrame::Header*>(outParts.At(0)->GetData());
                LOG(warn) << "Failed to enqueue time frame : TF = " << h->timeFrameId;
            }
        }
        if (fDiscardOutput) {
            attempts++;
            if (attempts >= fNumDestination) {
                LOG(error) << "Failed to send time frame after checking all destinations."
                    << "Discarding data. Direction = " << direction;
                break;
            }
        }
        if (fNumDestination==1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    return;
}


void TimeFrameBuilder::SendTimeFrameToEvery(
    fair::mq::Parts& outParts)
{

    // for DQM
    bool dqmSocketExists = fChannels.count(fDQMChannelName);
    if (dqmSocketExists) {
        SendTimeFrameForDQM(outParts);
    }

    // for Decimator
    if (fDecimatorNumberOfConnectedPeers > 0) {
        SendTimeFrameForDecimator(outParts);
    }

    // for out ; outParts will be moved to ZeroMQ.
    SendTimeFrame(outParts);
    fNumSccesssfulTFB++;

    return;
}


//______________________________________________________________________________
bool TimeFrameBuilder::ConditionalRun()
{
    namespace STF = SubTimeFrame;
    namespace TF  = TimeFrame;

    // receive
    fair::mq::Parts inParts;
    if (Receive(inParts, fInputChannelName, 0, 1) > 0) {
        assert(inParts.Size() >= 2);

        auto stfHeader = reinterpret_cast<STF::Header*>(inParts.At(0)->GetData());
        auto stfId     = stfHeader->timeFrameId;

#if 1
        LOG(debug4) << "msg size: " << inParts.Size()
            << ", TFid: "<< stfId
            << " FEid: 0x" << std::hex << stfHeader->femId
            << " Type: " << std::dec << stfHeader->type;
#endif

#if 1
        CheckHBFDelimitor(inParts, stfId);
#endif

        if (fTFBuffer.find(stfId) == fTFBuffer.end()) {
            fTFBuffer[stfId].reserve(fNumSource);
        }
        fTFBuffer[stfId].emplace_back(STFBuffer {std::move(inParts), std::chrono::steady_clock::now()});

#if 0
        LOG(debug4) << "TFid: " << stfId
            << ", Fragments: " << fTFBuffer[stfId].size() << "/" << fNumSource;
#endif

    }

    // send
    if (!fTFBuffer.empty()) {

        // find time frame in ready
        for (auto itr = fTFBuffer.begin(); itr != fTFBuffer.end();) {
            auto stfId  = itr->first;
            auto& tfBuf = itr->second;

            if (tfBuf.size() == static_cast<long unsigned int>(fNumSource)) {
                LOG(debug4) << "All comes : " << tfBuf.size() << " stfId: "<< stfId ;

                fair::mq::Parts outParts;

                MakeOutPartsFromSTFBuffers(tfBuf, stfId, TimeFrame::TF_COMPLETE, outParts);
                tfBuf.clear();

                SendTimeFrameToEvery(outParts);

            } else {
                // discard incomplete time frame
                auto dt = std::chrono::steady_clock::now() - tfBuf.front().start;
                if (std::chrono::duration_cast<std::chrono::milliseconds>(dt).count() > fBufferTimeoutInMs) {
                    // output debug info
                    TFBFailDump(tfBuf, stfId);

                    // output incomplete TF
                    if (fOutputIncompleteTF) {

                        std::cout << "#D Output incomplete TF: " << stfId << std::endl;

                        fair::mq::Parts outParts;
                        MakeOutPartsFromSTFBuffers(tfBuf, stfId,
                            TimeFrame::TF_INCOMPLETE | TimeFrame::TF_TIMEOUT,
                            outParts);
                        SendTimeFrameToEvery(outParts);
                    }

                    fNumFailedTFB++;
                    tfBuf.clear();
                }
            }

            // remove empty buffer
            if (tfBuf.empty()) {
                itr = fTFBuffer.erase(itr);
            }
            else {
                ++itr;
            }
        }
    } // send

    // erase a depth limint exceeded TF.
    if (fBufferDepthLimit > 0) {
        if (fTFBuffer.size() > fBufferDepthLimit) {

#if 1
            std::cout << "#D Buffer Size: " << fTFBuffer.size()
                << ": Top TFN: " << fTFBuffer.begin()->first
                << " Size: " << fTFBuffer.begin()->second.size();
            auto& tfBuf = fTFBuffer.begin()->second;

            for (auto & stfBuf : tfBuf) {
                auto & msg = stfBuf.parts[0];
                SubTimeFrame::Header *stfheader
                    = reinterpret_cast<SubTimeFrame::Header *>(msg.GetData());
                std::cout << " " << std::dec << (stfheader->femId & 0xff);
            }
            std::cout << std::endl;
#endif

            if (fOutputIncompleteTF) {
                uint32_t stfId = fTFBuffer.begin()->first;
                auto & tfBuf = fTFBuffer.begin()->second;
                fair::mq::Parts outParts;
                MakeOutPartsFromSTFBuffers(tfBuf,
                    stfId, TimeFrame::TF_INCOMPLETE | TimeFrame::TF_DEPTH_LIMIT,
                    outParts);
                SendTimeFrameToEvery(outParts);
            }

            fNumFailedTFB++;
            fTFBuffer.begin()->second.clear();
            fTFBuffer.erase(fTFBuffer.begin());
        }
    }

    if (fKt.Check()) {
        static int lcounts = 0;
        LOG(debug) << "TimeFrameBuilding Successful: " << fNumSccesssfulTFB
                  << ", Failed: " << fNumFailedTFB;
        if ((lcounts++ % 5) == 0) {

            double successfulRatio = 0;
            if (fNumSccesssfulTFB + fNumFailedTFB > 0) {
                successfulRatio
                    = fNumSccesssfulTFB / static_cast<double>(fNumSccesssfulTFB + fNumFailedTFB)
                    * 100.0;
            }
            #if 0
            time_t now = std::time(nullptr);
            std::cout << "#D "
                << fKeyPrefixMetric + "SucessfulRatio "
                << now
                << " TFB Successful Ratio: "
                << std::to_string(successfulRatio) << std::endl;
            #endif
            fDbMetric->ts_add(
                fKeyPrefixMetric + "SuccessfulRatio",
                std::to_string(std::time(nullptr) * 1000),
                std::to_string(successfulRatio));

            fNumSccesssfulTFB = 0;
            fNumFailedTFB     = 0;
        }
    }

    return true;
}


//______________________________________________________________________________
void TimeFrameBuilder::Init()
{
}

void TimeFrameBuilder::SetKeyPrefix()
{
    std::string serverUri;
    serverUri = fConfig->GetProperty<std::string>("metrics-uri");
    if (serverUri.empty()) {
        serverUri = fConfig->GetProperty<std::string>("registry-uri");
    }
    if (serverUri.empty()) {
        LOG(error) << "DB server URI for metrics is not specified.";
        throw std::runtime_error("DB server URI for metrics is not specified.");
    }
    
    std::string serviceRegistryUri = fConfig->GetProperty<std::string>("registry-uri");
    std::cout << "#D DB Server URI: " << serverUri << std::endl;
    std::cout << "#D DB serviceRegistoryUri: " << serviceRegistryUri << std::endl;

    std::string service_name = fConfig->GetProperty<std::string>("service-name");
    std::string separator   = fConfig->GetProperty<std::string>("separator");
    fKeyPrefixMetric = "ts" + separator + fId + separator;
    fKeyPrefixObjects = "dqm" + separator + fId + separator;


    if (fDbMetric == nullptr) {
        fDbUriMetric = serverUri;
        fDbMetric = std::make_unique<RedisDataStore>(fDbUriMetric);
    }

    if (fDbObjects == nullptr) {
        fDbUriObjects = serverUri.substr(0, serverUri.rfind("/") + 1)
            + fConfig->GetProperty<std::string>(OptionKey::ObjectDbNumber.data());
        fDbObjects = std::make_unique<RedisDataStore>(fDbUriObjects);
    }

    LOG(info) << "Metric DB: " << fDbUriMetric << " Prefix:" << fKeyPrefixMetric;
    LOG(info) << "DQM DB: " << fDbUriObjects << " Prefix:" << fKeyPrefixObjects;

    /*
    if (!serverUri.empty()) {
        fClient = std::make_shared<sw::redis::Redis>(serverUri);
    }
    */

    return;
}


//______________________________________________________________________________
void TimeFrameBuilder::InitTask()
{
    using opt = OptionKey;
    auto sBufferTimeoutInMs = fConfig->GetProperty<std::string>(opt::BufferTimeoutInMs.data());
    fBufferTimeoutInMs = std::stoi(sBufferTimeoutInMs);

    auto sBufferDepthLimit = fConfig->GetProperty<std::string>(opt::BufferDepthLimit.data());
    fBufferDepthLimit = std::stoi(sBufferDepthLimit);

    fInputChannelName  = fConfig->GetProperty<std::string>(opt::InputChannelName.data());
    fOutputChannelName = fConfig->GetProperty<std::string>(opt::OutputChannelName.data());
    fDecimatorChannelName = fConfig->GetProperty<std::string>(opt::DecimatorChannelName.data());

    auto numSubChannels = GetNumSubChannels(fInputChannelName);
    LOG(debug) << " Nsubchnnael[" << fInputChannelName << "]: " << numSubChannels;

    fNumSource = 0;
    for (auto i=0u; i<numSubChannels; ++i) {
        fNumSource += GetNumberOfConnectedPeers(fInputChannelName, i);
    }

    fDQMChannelName    = fConfig->GetProperty<std::string>(opt::DQMChannelName.data());

    LOG(debug) << " input channel : name = " << fInputChannelName
               << " num = " << GetNumSubChannels(fInputChannelName)
               << " num peer = " << GetNumberOfConnectedPeers(fInputChannelName,0);

    LOG(debug) << " number of source = " << fNumSource;

    fNumDestination = GetNumSubChannels(fOutputChannelName);
    fPollTimeoutMS  = std::stoi(fConfig->GetProperty<std::string>(opt::PollTimeout.data()));
    fDecimationFactor = std::stoi(fConfig->GetProperty<std::string>(opt::DecimationFactor.data()));
    fDecimationOffset = std::stoi(fConfig->GetProperty<std::string>(opt::DecimationOffset.data()));
    if ((fDecimationFactor > 0)  && (fDecimationOffset >= fDecimationFactor)) {
        LOG(warn) << "invalid decimation-offset = " << fDecimationOffset
            << " (< decimation-factor).  Set to 0";
        fDecimationOffset = 0;
    }
    LOG(debug) << " decimation-factor = " << fDecimationFactor
        << " decimation-offset = " << fDecimationOffset;

    fDecimatorNumberOfConnectedPeers = 0;
    if (fChannels.count(fDecimatorChannelName) > 0) {
        auto nsub = GetNumSubChannels(fDecimatorChannelName);
        for (auto i = 0u; i<nsub; ++i) {
            fDecimatorNumberOfConnectedPeers += GetNumberOfConnectedPeers(fDecimatorChannelName,i);
        }
    }

    auto sDiscardOutput = fConfig->GetProperty<std::string>(opt::DiscardOutput.data());
    fDiscardOutput = ((sDiscardOutput == "1") || (sDiscardOutput == "true") || (sDiscardOutput == "yes"));
    LOG(debug) << " discard-output = " << fDiscardOutput;

    std::string sOutputIncompleteTF = fConfig->GetProperty<std::string>(opt::OutputIncompleteTF.data());
    fOutputIncompleteTF = ((sOutputIncompleteTF == "1") || (sOutputIncompleteTF == "true") || (sOutputIncompleteTF == "yes"));
    LOG(debug) << " output-incomplete-tf = " << fOutputIncompleteTF;

    SetKeyPrefix();

}


//______________________________________________________________________________
void TimeFrameBuilder::PostRun()
{
    fTFBuffer.clear();

    int nrecv=0;
    if (fChannels.count(fInputChannelName) > 0) {
        auto n = fChannels.count(fInputChannelName);

        for (auto i = 0u; i < n; ++i) {
            std::cout << "#D SubChannel : "<< i << std::endl;
            while(true) {
                fair::mq::Parts part;
                if (Receive(part, fInputChannelName, i, 1000) <= 0) {
                    LOG(debug) << __func__ << " no data received " << nrecv;
                    ++nrecv;
                    if (nrecv > 10) {
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                } else {
//                    LOG(debug) << __func__ << " data comes..";
                }
            }
        }
    }// for clean up
}


//_____________________________________________________________________________
void TimeFrameBuilder::PreRun()
{
    fDirection    = 0;
    fNumSend = 0;
}
