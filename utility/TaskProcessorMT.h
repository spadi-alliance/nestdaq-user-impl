#ifndef NESTDAQ_MQ_TaskProcessorMT_h
#define NESTDAQ_MQ_TaskProcessorMT_h

// A task processor with multi-threads

/*
//                         Splitter     MQ (address)              WorkerIn                Worker WorkerOut     MQ
//                         (address)            Merger
//                    +--> (bind)   --> [inproc://splitter-0] --> (connect) ... HandleInput -> HandleOutput ...
(connect)  -- [inproc://merger-0] --> (bind) --+
//                    | |
// input -> Splitter -+--> (bind)   --> [inproc://splitter-1] --> (connect) ... HandleInput -> HandleOutput ...
(connect)  -- [inproc://merger-1] --> (bind) --+-> merger --> output
//                    | |
//                    +--> (bind)   --> [inproc://splitter-2] --> (connect) ... HandleInput -> HandleOutput ...
(connect)  -- [inproc://merger-2] --> (bind) --+
//
//
*/

// ToDo: implement proper termination sequence.

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <fairmq/FairMQDevice.h>
#include <fairmq/FairMQTransportFactory.h>

namespace nestdaq {

class TaskProcessorMT {
public:
    static constexpr std::string_view SplitterChannelAddressPrefix{"inproc://splitter"};
    static constexpr std::string_view SplitterChannelName{"splitter"};
    static constexpr std::string_view WorkerInputChannelName{"worker-in"};

    static constexpr std::string_view WorkerOutputChannelName{"worker-out"};
    static constexpr std::string_view MergerChannelName{"merger"};
    static constexpr std::string_view MergerChannelAddressPrefix{"inproc://merger"};

    explicit TaskProcessorMT(FairMQDevice &device) : fDevice(device), fClassName(__func__) {}
    TaskProcessorMT(const TaskProcessorMT &) = delete;
    TaskProcessorMT &operator=(const TaskProcessorMT &) = delete;
    ~TaskProcessorMT() = default;

    // called by multi-threads (workers)
    std::function<bool(FairMQMessagePtr & /* msg */, int /* index */)> fHandleInput;
    std::function<bool(FairMQParts & /* msgParts */, int /* index */)> fHandleInputMultipart;

    // called by a single thread (merger)
    std::function<bool(FairMQMessagePtr & /* msg*/, int /* index*/)> fHandleOutput;
    std::function<bool(FairMQParts & /* msgParts */, int /* index */)> fHandleOutputMultipart;

    FairMQDevice &fDevice;
    const std::string fClassName;

    void InitChannels(int nThreads, int bufSize, int timeoutMS);
    void Join();
    template <typename... Args>
    FairMQMessagePtr NewMessage(Args &&...args)
    {
        return fTransportFactory->CreateMessage(std::forward<Args>(args)...);
    }
    void Run();

    std::vector<std::unique_ptr<FairMQChannel>> fSplitterChannels;
    std::vector<std::unique_ptr<FairMQChannel>> fWorkerInputChannels;
    std::vector<std::unique_ptr<FairMQChannel>> fWorkerOutputChannels;
    std::vector<std::unique_ptr<FairMQChannel>> fMergerChannels;

private:
    std::unique_ptr<FairMQChannel>
    AttachChannel(const std::string &name, const std::string &method, const std::string &addressPrefix, int index);

    void RunMerger();
    void RunWorkers();

    int fNThreads{0};
    int fMQTimeoutMS;
    std::atomic<std::size_t> fNInput{0};
    std::vector<std::size_t> fNProcessed;
    std::size_t fNOutput{0};

    std::shared_ptr<FairMQTransportFactory> fTransportFactory;
    std::vector<std::thread> fWorkers;
    std::thread fMerger;
    int fBufSize;
};

} // namespace nestdaq

#endif
