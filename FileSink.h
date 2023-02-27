#ifndef NESTDAQ_FILE_SINK_H_
#define NESTDAQ_FILE_SINK_H_

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

#include <fairmq/FairMQDevice.h>

#include "utility/RingBuffer.h"
#include "utility/FileUtil.h"
#include "FileSinkHeaderBlock.h"

namespace nestdaq {
class TaskProcessorMT;

class FileSink : public FairMQDevice {
public:
   const std::string fClassName;
   struct OptionKey {
      static constexpr std::string_view InputDataChannelName{"in-chan-name"};
      static constexpr std::string_view Multipart{"multipart"};
      static constexpr std::string_view MergeMessage{"merge-msg"};
      static constexpr std::string_view RunNumber{"run_number"};
      static constexpr std::string_view MQTimeout{"mq-timeout"};
      static constexpr std::string_view Discard{"discard"};
      static constexpr std::string_view NThreads{"n-threads"};
      static constexpr std::string_view InProcMQLength{"inproc-mq-length"};
      static constexpr std::string_view MaxIteration{"max-iteration"};
   };

   FileSink() : FairMQDevice(), fFile(), fClassName(__func__) {}
   FileSink(const FileSink &) = delete;
   FileSink &operator=(const FileSink &) = delete;
   ~FileSink() override = default;

private:
   bool CompressData(FairMQMessagePtr &rawMsg, int index);
   bool CompressMultipartData(FairMQParts &rawMsgParts, int index);
   bool HandleData(FairMQMessagePtr &msg, int index);
   bool HandleDataMT(FairMQMessagePtr &msg, int index);
   bool HandleMultipartData(FairMQParts &msgParts, int index);
   bool HandleMultipartDataMT(FairMQParts &msgParts, int index);
   void Init() override;
   void InitTask() override;
   void PreRun() override;
   void PostRun() override;
   void StartWorkers();
   bool WriteData(FairMQMessagePtr &msg, int index);
   bool WriteMultipartData(FairMQParts &msgParts, int index);

   std::size_t fMaxIteration;
   std::atomic<bool> fStopRequested;

   std::string fInputDataChannelName;
   std::unique_ptr<nestdaq::FileUtil> fFile;
   nestdaq::FileUtil::CompressFunc fCompress;
   std::atomic<std::size_t> fSize{0};
   std::atomic<std::size_t> fCompressedSize{0};
   std::size_t fNWrite{0};
   int fMQTimeoutMS;
   bool fDiscard;
   bool fMultipart;

   bool fMergeMessage{false};
   int64_t fRunNumber{0};

   int fInProcMQLength;
   std::unique_ptr<nestdaq::TaskProcessorMT> fWorker;
   std::size_t fNThreads{0};
   std::size_t fNReceived{0};
   std::string fFileExtension;
   std::string fFilePath;
   FileSinkHeaderBlock fFileSinkHeaderBlock;
};

} // namespace nestdaq

#endif
