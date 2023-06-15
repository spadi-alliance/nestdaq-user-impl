#include <chrono>
#include <functional>
#include <thread>
#include <cassert>
#include <algorithm>
#include <numeric>
#include <sstream>

#include <fairmq/Poller.h>
#include <fairmq/runDevice.h>

#include "utility/HexDump.h"
#include "utility/MessageUtil.h"
#include "SubTimeFrameHeader.h"
#include "TimeFrameHeader.h"
#include "TimeFrameBuilder.h"

#include "AmQStrTdcData.h"

namespace bpo = boost::program_options;

//______________________________________________________________________________
void addCustomOptions(bpo::options_description& options)
{
    using opt = TimeFrameBuilder::OptionKey;
    options.add_options()
    (opt::BufferTimeoutInMs.data(), bpo::value<std::string>()->default_value("10000"), "Buffer timeout in milliseconds")
    (opt::InputChannelName.data(),  bpo::value<std::string>()->default_value("in"),    "Name of the input channel")
    (opt::OutputChannelName.data(), bpo::value<std::string>()->default_value("out"),   "Name of the output channel") 
    (opt::DQMChannelName.data(),    bpo::value<std::string>()->default_value("dqm"),   "Name of the data quality monitoring channel")      
    (opt::PollTimeout.data(),       bpo::value<std::string>()->default_value("0"),     "Timeout (in msec) of polling")
    ;
}

//______________________________________________________________________________
std::unique_ptr<fair::mq::Device> getDevice(FairMQProgOptions&)
{
    return std::make_unique<TimeFrameBuilder>();
}

//______________________________________________________________________________
TimeFrameBuilder::TimeFrameBuilder()
    : fair::mq::Device()
{
}

//______________________________________________________________________________
bool TimeFrameBuilder::ConditionalRun()
{
    namespace STF = SubTimeFrame;
    namespace TF  = TimeFrame;
        
    // receive
    FairMQParts inParts;
    if (Receive(inParts, fInputChannelName, 0, 1) > 0) {
        assert(inParts.Size() >= 2);

        LOG(debug4) << " received message parts size = " << inParts.Size() << std::endl;

        auto stfHeader = reinterpret_cast<STF::Header*>(inParts.At(0)->GetData());
        auto stfId     = stfHeader->timeFrameId;

        LOG(debug4) << "stfId: "<< stfId;
        LOG(debug4) << "msg size: " << inParts.Size();

        if (fTFBuffer.find(stfId) == fTFBuffer.end()) {
            fTFBuffer[stfId].reserve(fNumSource);
        }
        fTFBuffer[stfId].emplace_back(STFBuffer {std::move(inParts), std::chrono::steady_clock::now()});
    }

    // send
    if (!fTFBuffer.empty()) {

        bool dqmSocketExists = fChannels.count(fDQMChannelName);

        // find time frame in ready
        for (auto itr = fTFBuffer.begin(); itr!=fTFBuffer.end();) {
            auto stfId  = itr->first;
            auto& tfBuf = itr->second;

            if (tfBuf.size() == static_cast<long unsigned int>(fNumSource)) {

                LOG(debug4) << "All comes : " << tfBuf.size() << " stfId: "<< stfId ;

                // move ownership to complete time frame
                FairMQParts outParts;
                FairMQParts dqmParts;
                
                auto h = std::make_unique<TF::Header>();
                h->magic       = TF::Magic;
                h->timeFrameId = stfId;
                h->numSource   = fNumSource;
                h->length      = std::accumulate(tfBuf.begin(), tfBuf.end(), sizeof(TF::Header),
                [](auto init, auto& stfBuf) {
                    return init + std::accumulate(stfBuf.parts.begin(), stfBuf.parts.end(), 0,
                    [] (auto jinit, auto& m) {
                        return (!m) ? jinit : jinit + m->GetSize();
                    });
                });

                // for dqm
                if (dqmSocketExists){
                    for (auto& stfBuf: tfBuf) {
                      for (auto& m: stfBuf.parts) {
                        FairMQMessagePtr msgCopy(fTransportFactory->CreateMessage());
                        msgCopy->Copy(*m);
                        
                        dqmParts.AddPart(std::move(msgCopy));
                      }
                    }
                }
                // LOG(debug) << " length = " << h->length;
                outParts.AddPart(MessageUtil::NewMessage(*this, std::move(h)));
                for (auto& stfBuf: tfBuf) {
                    for (auto& m: stfBuf.parts) {
                        outParts.AddPart(std::move(m));
                    }
                }
                tfBuf.clear();

                if (dqmSocketExists) {
                  if (Send(dqmParts, fDQMChannelName) < 0) {
                    if (NewStatePending()) {
                      LOG(info) << "Device is not RUNNING";
                      return false;
                    }
                    LOG(error) << "Failed to enqueue TFB (DQM) ";
                  }
                }
                
                auto poller = NewPoller(fOutputChannelName);
                while (!NewStatePending()) {
                    poller->Poll(fPollTimeoutMS);
                    auto direction = fDirection % fNumDestination;
                    fDirection = direction + 1;
                    if (poller->CheckOutput(fOutputChannelName, direction)) {
                        // output ready

                        if (Send(outParts, fOutputChannelName, direction) > 0) {
                            // successfully sent
                            break;
                        } else {
                            LOG(warn) << "Failed to enqueue time frame : TF = " << h->timeFrameId;
                        }
                    }
                    if (fNumDestination==1) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                }
            } else {
                // discard incomplete time frame
                auto dt = std::chrono::steady_clock::now() - tfBuf.front().start;
                if (std::chrono::duration_cast<std::chrono::milliseconds>(dt).count() > fBufferTimeoutInMs) {
                    LOG(warn) << "Timeframe #" <<  std::hex << stfId << " incomplete after "
                            << std::dec << fBufferTimeoutInMs << " milliseconds, discarding";
                    //fDiscarded.insert(stfId);


                    ////// under debugging //////
                    std::vector<uint32_t> femid;
                    std::vector<uint32_t> expected = {
                        161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 
                                  173, 174, 175, 176, 177, 178, 179
                    };
                    std::vector<uint64_t> hb;
                    for (auto & stfBuf : tfBuf) {
                        auto & msg = stfBuf.parts[0];
                        SubTimeFrame::Header *stfheader
                            = reinterpret_cast<SubTimeFrame::Header *>(msg.GetData());
			//std::cout << " ID" << std::hex << stfheader->FEMId << std::dec;
                        femid.push_back(stfheader->FEMId);
			for (auto it = expected.begin() ; it != expected.end() ;) {
                            if (*it == (stfheader->FEMId && 0xff)) {
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
                    //std::cout << "#D lost FEMid :" << stfId << ":";
                    //for (auto & i : expected) std::cout << " " << (i & 0xff);
                    std::cout << "#D FEMid :" << stfId << ":";
                    for (auto & i : femid) std::cout << " " << (i & 0xff);
                    std::cout << std::endl;
                    std::cout << "#D HB :" << stfId << ":";
                    for (auto & i : hb) std::cout << " " << std::hex <<i;
                    std::cout << std::dec << std::endl;


                    /*
                    {// for debug-begin

                      for (auto& stfBuf: tfBuf) {
                        for (auto& m: stfBuf.parts) {
                          
                          std::for_each(reinterpret_cast<uint64_t*>(m->GetData()),
                                        reinterpret_cast<uint64_t*>(m->GetData() + m->GetSize()),
                                        ::HexDump{4});

                        }
                      }
                    }// for debug-end
                    */
                    
                    tfBuf.clear();
                    //LOG(warn) << "Number of discarded timeframes: " << fDiscarded.size();
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
        
    }
    return true;
}

//______________________________________________________________________________
void TimeFrameBuilder::Init()
{
}

//______________________________________________________________________________
void TimeFrameBuilder::InitTask()
{
    using opt = OptionKey;
    auto sBufferTimeoutInMs = fConfig->GetProperty<std::string>(opt::BufferTimeoutInMs.data());
    fBufferTimeoutInMs = std::stoi(sBufferTimeoutInMs);
    fInputChannelName  = fConfig->GetProperty<std::string>(opt::InputChannelName.data());
    fOutputChannelName = fConfig->GetProperty<std::string>(opt::OutputChannelName.data());
    auto numSubChannels = GetNumSubChannels(fInputChannelName);
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

}

//______________________________________________________________________________
void TimeFrameBuilder::PostRun()
{
    fTFBuffer.clear();
    //fDiscarded.clear();

    int nrecv=0;
    if (fChannels.count(fInputChannelName) > 0) {
        auto n = fChannels.count(fInputChannelName);

        for (auto i = 0u; i < n; ++i) {
            std::cout << " #i : "<< i << std::endl;
            while(true) {

                FairMQParts part;
                if (Receive(part, fInputChannelName, i, 1000) <= 0) {
                    LOG(debug) << __func__ << " no data received " << nrecv;
                    ++nrecv;
                    if (nrecv > 10) {
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                } else {
                    LOG(debug) << __func__ << " data comes..";
                }
            }
        }
    }// for clean up

}

//_____________________________________________________________________________
void TimeFrameBuilder::PreRun()
{
    fDirection    = 0;
}
