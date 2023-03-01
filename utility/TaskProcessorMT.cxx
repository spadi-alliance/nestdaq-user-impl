#include <chrono>
#include <thread>

#include "utility/TaskProcessorMT.h"

#include <fairmq/FairMQLogger.h>

namespace nestdaq {
//_____________________________________________________________________________
std::unique_ptr<FairMQChannel> TaskProcessorMT::AttachChannel(const std::string &name, const std::string &method,
        const std::string &addressPrefix, int index)
{
    std::string idx = std::to_string(index);
    auto subChannel = std::make_unique<FairMQChannel>(name + "[" + idx + "]",    //
                      "pair",                    //
                      method,                    //
                      addressPrefix + "-" + idx, //
                      fTransportFactory);
    subChannel->UpdateSndBufSize(fBufSize);
    subChannel->UpdateRcvBufSize(fBufSize);
    bool status{false};
    if (subChannel->Validate()) {
        std::string address = subChannel->GetAddress();
        if (subChannel->GetMethod() == "bind") {
            status = subChannel->BindEndpoint(address);
        } else {
            status = subChannel->ConnectEndpoint(address);
        }
    }
    if (!status) {
        LOG(warn) << " failed to attach channel: " << name << " [" << index << "]"
                  << " " << addressPrefix;
    }
    return subChannel;
}

//_____________________________________________________________________________
void TaskProcessorMT::InitChannels(int nThreads, int bufSize, int timeoutMS)
{
    fSplitterChannels.clear();
    fWorkerInputChannels.clear();
    fWorkerOutputChannels.clear();
    fMergerChannels.clear();

    fNThreads = nThreads;
    fBufSize = bufSize;
    fMQTimeoutMS = timeoutMS;

    fNProcessed.resize(fNThreads, 0);
    fNInput = 0;
    fNOutput = 0;

    fTransportFactory = FairMQTransportFactory::CreateTransportFactory(
                            fair::mq::TransportNames.at(fair::mq::Transport::ZMQ), fDevice.fConfig->GetProperty<std::string>("id"));

    LOG(debug) << ":" << __func__ << " attach binding channels for worker threads";
    for (auto i = 0; i < fNThreads; ++i) {
        fSplitterChannels.push_back(
            AttachChannel(SplitterChannelName.data(), "bind", SplitterChannelAddressPrefix.data(), i));
        fMergerChannels.push_back(AttachChannel(MergerChannelName.data(), "bind", MergerChannelAddressPrefix.data(), i));
    }

    LOG(debug) << ":" << __func__ << " connecting channels for worker threads";
    for (auto i = 0; i < fNThreads; ++i) {
        fWorkerInputChannels.push_back(
            AttachChannel(WorkerInputChannelName.data(), "connect", SplitterChannelAddressPrefix.data(), i));
        fWorkerOutputChannels.push_back(
            AttachChannel(WorkerOutputChannelName.data(), "connect", MergerChannelAddressPrefix.data(), i));
    }

    /*
    for (auto i = 0; i < fNThreads; ++i) {
       LOG(debug) << " splitetr i = " << i << " " << fSplitterChannels[i].get();
    }
    for (auto i = 0; i < fNThreads; ++i) {
       LOG(debug) << " merger i = " << i << " " << fMergerChannels[i].get();
    }
    for (auto i = 0; i < fNThreads; ++i) {
       LOG(debug) << " worker in i = " << i << " " << fWorkerInputChannels[i].get();
    }
    for (auto i = 0; i < fNThreads; ++i) {
       LOG(debug) << " worker out i = " << i << " " << fWorkerOutputChannels[i].get();
    }
    */
}

//_____________________________________________________________________________
void TaskProcessorMT::Join()
{
    for (auto &t : fWorkers) {
        if (t.joinable()) {
            t.join();
        }
    }

    if (fMerger.joinable()) {
        fMerger.join();
    }
}

//_____________________________________________________________________________
void TaskProcessorMT::Run()
{
    RunMerger();
    RunWorkers();
}

//_____________________________________________________________________________
void TaskProcessorMT::RunMerger()
{
    fMerger = std::thread([this] {
        while (!fDevice.NewStatePending()) {
            auto i = fNOutput % fNThreads;

            if (fHandleOutputMultipart) {
                // LOG(debug) << fClassName << ":" << __LINE__ << " Merge channel receive i = " << i << " input = " <<
                // fNInput << " output = " << fNOutput;
                FairMQParts payload;
                if (fMergerChannels[i]->Receive(payload, fMQTimeoutMS) >= 0) {
                    fHandleOutputMultipart(payload, i);
                    ++fNOutput;
                    ++fNProcessed[i];
                } else {
                    // LOG(debug) << fClassName << ":" << __LINE__ << " Merge channel receive failed i = " << i << " input =
                    // " << fNInput << " output = " << fNOutput;
                    std::this_thread::yield();
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            } else if (fHandleOutput) {
                // LOG(debug) << fClassName << ":" << __LINE__ << " Merge channel receive i = " << i << " input = " <<
                // fNInput << " output = " << fNOutput;
                auto payload = NewMessage();
                if (fMergerChannels[i]->Receive(payload, fMQTimeoutMS) >= 0) {
                    fHandleOutput(payload, i);
                    ++fNOutput;
                    ++fNProcessed[i];
                } else {
                    // LOG(debug) << fClassName << ":" << __LINE__ << " Merge channel receive failed i = " << i << " input =
                    // " << fNInput << " output = " << fNOutput;
                    std::this_thread::yield();
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            } else {
                LOG(debug) << fClassName << ":" << __func__ << " no handler is set";
            }
        }
    });
}

//_____________________________________________________________________________
void TaskProcessorMT::RunWorkers()
{
    for (auto i = 0; i < fNThreads; ++i) {
        fWorkers.emplace_back([i, this] {
            while (!fDevice.NewStatePending()) {
                if (fHandleInputMultipart) {
                    // LOG(debug) << fClassName << ":" << __LINE__ << " worker channel receive i = " << i << " input = " <<
                    // fNInput << " output = " << fNOutput;
                    FairMQParts payloadIn;
                    if (fWorkerInputChannels[i]->Receive(payloadIn, fMQTimeoutMS) >= 0) {
                        ++fNInput;
                        fHandleInputMultipart(payloadIn, i);
                    } else {
                        // LOG(warn) << fClassName << ":" << __LINE__ << " worker channel receive failed i = " << i << " input
                        // = " << fNInput << " output = " << fNOutput;
                        std::this_thread::yield();
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                } else if (fHandleInput) {
                    // LOG(debug) << fClassName << ":" << __LINE__ << " worker channel receive i = " << i << " input = " <<
                    // fNInput << " output = " << fNOutput;
                    auto payloadIn = NewMessage();
                    if (fWorkerInputChannels[i]->Receive(payloadIn, fMQTimeoutMS) >= 0) {
                        ++fNInput;
                        fHandleInput(payloadIn, i);
                    } else {
                        // LOG(warn) << fClassName << ":" << __LINE__ << " worker channel receive failed i = " << i << " input
                        // = " << fNInput << " output = " << fNOutput;
                        std::this_thread::yield();
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                } else {
                    LOG(debug) << fClassName << ":" << __func__ << " no handler is set";
                }
            }
        });
    }
}

} // namespace nestdaq::daq
