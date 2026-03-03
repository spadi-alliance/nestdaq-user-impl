#include <functional>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>

#include <boost/algorithm/string/case_conv.hpp>

#include <fairmq/runDevice.h>

#include "utility/CompressHelper.h"
#include "utility/Compressor.h"
#include "utility/HexDump.h"
#include "utility/program_options.h"
#include "utility/MessageUtil.h"
#include "utility/TaskProcessorMT.h"
#include "EDM4hepSink.h"
#include "AmQStrTdcData.h"

#include <edm4hep/SimTrackerHitCollection.h>
#include <podio/Frame.h>
#include <podio/ROOTWriter.h>

#include <memory>

using namespace nestdaq;
using namespace std::string_literals;

namespace bpo = boost::program_options;
using opt = nestdaq::EDM4hepSink::OptionKey;

//=============================================================================
void addCustomOptions(bpo::options_description &options)
{
    {   // EDM4hepSink's options ------------------------------
        options.add_options()
        //
        (opt::InputDataChannelName.data(),
         bpo::value<std::string>()->default_value("in"),
         "Name of input channel")
        //
        (opt::Multipart.data(), bpo::value<std::string>()->default_value("true"), "Handle multipart message")
        //
        (opt::MergeMessage.data(), bpo::value<std::string>()->default_value("true"),
         "Merge multipart message to a single message before write")
        //
        (opt::RunNumber.data(), bpo::value<std::string>(), "Run number (integer) given by DAQ plugin")
        //
        (opt::MQTimeout.data(), bpo::value<std::string>()->default_value("1000"),
         "Timeout of receive messages in milliseconds")
        //
        (opt::Discard.data(), bpo::value<std::string>()->default_value("true"), "Discard data received in PostRun")
        //
        (opt::NThreads.data(), bpo::value<std::string>()->default_value("0"),
         "Number of threads for data compression. (if zero or negative, a value obtained by "
         "std::thread::hardware_concurrency() will be used.")
        //
        (opt::InProcMQLength.data(), bpo::value<std::string>()->default_value("1"), "in-process message queue length.")
        //
        (opt::MaxIteration.data(), bpo::value<std::string>()->default_value("0"),
         "Number of iterations (if zero, no limitation is set)")
        (opt::WriteSleepInMilliSec.data(), bpo::value<std::string>()->default_value("0"),
         "Write Sleep in msec")
	;
    }

    {   // FileUtil's options ------------------------------
        using fu = nestdaq::FileUtil;
        //using fopt = fu::OptionKey;
        //using sopt = fu::SplitOption;
        //using oopt = fu::OpenmodeOption;
        auto &desc = fu::GetDescriptions();

        fu::AddOptions(options);
    }
}

//______________________________________________________________________________
std::unique_ptr<fair::mq::Device> getDevice(FairMQProgOptions &config)
{
    (void)config;

    return std::make_unique<nestdaq::EDM4hepSink>();
}

//=============================================================================
namespace nestdaq {

//______________________________________________________________________________
bool EDM4hepSink::CompressData(FairMQMessagePtr &rawMsg, int index)
{
    if (fStopRequested) {
        return false;
    }
    // LOG(debug) << __LINE__ << ":" << __func__ << " index = " << index;
    auto first = reinterpret_cast<char *>(rawMsg->GetData());
    fSize += rawMsg->GetSize();
    auto compressed = std::make_unique<std::vector<char>>(fCompress(first, rawMsg->GetSize()));
    auto compressedMsg = MessageUtil::NewMessage(*fWorker, std::move(compressed));
    fCompressedSize += compressedMsg->GetSize();
    fWorker->fWorkerOutputChannels[index]->Send(compressedMsg, fMQTimeoutMS);
    return true;
}

//______________________________________________________________________________
bool EDM4hepSink::CompressMultipartData(FairMQParts &rawMsgParts, int index)
{
    if (fStopRequested) {
        return false;
    }
    FairMQParts compressedMsgParts;
    if (fMergeMessage) {
        auto len = MessageUtil::TotalLength(rawMsgParts);
        auto buf = fWorker->NewMessage(len);
        auto bufFirst = reinterpret_cast<char *>(buf->GetData());
        auto bufLast = bufFirst + len;
        auto itr = bufFirst;

        for (const auto &m : rawMsgParts) {
            auto first = reinterpret_cast<char *>(m->GetData());
            auto last = first + m->GetSize();
            // std::for_each(first, last, HexDump());
            std::copy(first, last, itr);
            itr += m->GetSize();
        }
        if (bufLast != itr) {
            LOG(error) << __LINE__ << ":" << __func__ << " index = " << index << ": failed to copy buffer";
        }
        auto compressed = std::make_unique<std::vector<char>>(fCompress(bufFirst, len));
        fSize += len;
        fCompressedSize += compressed->size();
        compressedMsgParts.AddPart(MessageUtil::NewMessage(*fWorker, std::move(compressed)));
    } else {
        for (const auto &m : rawMsgParts) {
            fSize += m->GetSize();
            auto first = reinterpret_cast<char *>(m->GetData());
            auto compressed = std::make_unique<std::vector<char>>(fCompress(first, m->GetSize()));
            fCompressedSize += compressed->size();
            compressedMsgParts.AddPart(MessageUtil::NewMessage(*fWorker, std::move(compressed)));
        }
    }

    // LOG(debug) << __LINE__ << ":" << __func__ << " index = " << index << " worker output channels send";
    fWorker->fWorkerOutputChannels[index]->Send(compressedMsgParts, fMQTimeoutMS);
    // LOG(debug) << __LINE__ << ":" << __func__ << " index = " << index << " worker output channels send done";
    return true;
}

//______________________________________________________________________________
bool EDM4hepSink::HandleData(FairMQMessagePtr &msg, int index)
{
    if (fStopRequested) {
        return false;
    }
    // LOG(info) << __LINE__ << ":" << __func__ << " index = " << index << " n-received = " << fNReceived << ", n-write =
    // " << fNWrite;
    ++fNReceived;
    return WriteData(msg, index);
}

//______________________________________________________________________________
bool EDM4hepSink::HandleDataMT(FairMQMessagePtr &msg, int index)
{
    (void)index;

    if (fStopRequested) {
        return false;
    }
    auto i = fNReceived % fNThreads;
    // LOG(info) << __LINE__ << ":" << __func__ << " index = " << index << " i = " << i << " n-received = " << fNReceived
    // << ", n-write = " << fNWrite;
    ++fNReceived;
    while (fWorker->fSplitterChannels[i]->Send(msg, fMQTimeoutMS) < 0) {
        LOG(warn) << __func__ << " (local mq) failed to send";
        if (fStopRequested || NewStatePending()) {
            return false;
        }
    }

    return true;
}

//______________________________________________________________________________
bool EDM4hepSink::HandleMultipartData(FairMQParts &msgParts, int index)
{
    if (fStopRequested) {
        return false;
    }
    // LOG(info) << __LINE__ << ":" << __func__ << " index = " << index << " n-received = " << fNReceived << ", n-write =
    // " << fNWrite;
    ++fNReceived;
    return WriteMultipartData(msgParts, index);
}

//______________________________________________________________________________
bool EDM4hepSink::HandleMultipartDataMT(FairMQParts &msgParts, int index)
{
    (void)index;

    if (fStopRequested) {
        return false;
    }
    auto i = fNReceived % fNThreads;
    // LOG(info) << __LINE__ << ":" << __func__ << " index = " << index <<  " i = " << i << " n-received = " <<
    // fNReceived << ", n-write = " << fNWrite;
    ++fNReceived;
    while (fWorker->fSplitterChannels[i]->Send(msgParts, fMQTimeoutMS) < 0) {
        LOG(warn) << __func__ << " (local mq) failed to send";
    }

    return true;
}

//______________________________________________________________________________
void EDM4hepSink::Init() {}

//______________________________________________________________________________
void EDM4hepSink::InitTask()
{
    auto get = [this](auto name) -> std::string {
        if (fConfig->Count(name.data()) < 1) {
            LOG(debug) << " variable: " << name << " not found";
            return "";
        }
        return fConfig->GetProperty<std::string>(name.data());
    };

    auto checkFlag = [this, &get](auto name) {
        std::string s = get(name);
        s = boost::to_lower_copy(s);
        return (s == "1") || (s == "true") || (s == "yes");
    };

    fInputDataChannelName = get(opt::InputDataChannelName);

    fNThreads = std::stoi(get(opt::NThreads));
    if (fNThreads < 1) {
        fNThreads = std::thread::hardware_concurrency();
    }
    LOG(debug) << __func__ << " n threads = " << fNThreads;
    fInProcMQLength = std::stoi(get(opt::InProcMQLength));
    fMultipart = checkFlag(opt::Multipart);
    fWriteSleepInMilliSec = std::stoi(get(opt::WriteSleepInMilliSec));
    LOG(info) << __func__ << " fWriteSleepInMilliSec = " << fWriteSleepInMilliSec;

    //fFile = std::make_unique<FileUtil>();
    //fFile->Init(fConfig->GetVarMap());
    //fFile->Print();
    //fFileExtension = fFile->GetExtension();
    //auto compressFormat = Compressor::ExtToFormat(fFileExtension);
    //LOG(debug) << " compress format = " << fFileExtension << " " << static_cast<int>(compressFormat);
    //fCompress = [this, compressFormat](const char *src, int n) -> std::vector<char> {
    //    return std::move(Compressor::Compress(src, n, compressFormat, fFile->GetCompBufSize()));
    //};
    fWorker.reset();
    if (fMultipart) {
      //LOG(debug) << " register data handler for multipart message";
        //if ((fNThreads > 1) && (compressFormat != Compressor::Format::none)) {
        //    OnData(fInputDataChannelName, &EDM4hepSink::HandleMultipartDataMT);
        //    fWorker = std::make_unique<TaskProcessorMT>(*this);
        //    LOG(debug) << " use multi-thread for compression";
        //} else {
            OnData(fInputDataChannelName, &EDM4hepSink::HandleMultipartData);
            LOG(debug) << " single thread";
	    //}
    } else {
      //LOG(debug) << " register data handler for single message";
        //if ((fNThreads > 1) && (compressFormat != Compressor::Format::none)) {
        //    OnData(fInputDataChannelName, &EDM4hepSink::HandleDataMT);
        //    fWorker = std::make_unique<TaskProcessorMT>(*this);
        //    LOG(debug) << " use multi-thread for compression";
        //} else {
            OnData(fInputDataChannelName, &EDM4hepSink::HandleData);
            LOG(debug) << " single thread";
	    //}
    }

    fMaxIteration = std::stoull(get(opt::MaxIteration));

    fMQTimeoutMS = std::stoi(get(opt::MQTimeout));
    fDiscard = checkFlag(opt::Discard);
    fMergeMessage = checkFlag(opt::MergeMessage);
}

//______________________________________________________________________________
void EDM4hepSink::PostRun()
{
    LOG(info) << __LINE__ << ":" << __func__;
    if (fWorker) {
        fWorker->Join();
    }
    if (fChannels.count(fInputDataChannelName) > 0) {
        auto n = fChannels.count(fInputDataChannelName);
        for (auto i = 0u; i < n; ++i) {
            for (auto itry = 0; itry < 10; ++itry) {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                if (fMultipart) {
                    FairMQParts part;
                    if (Receive(part, fInputDataChannelName, i, fMQTimeoutMS) > 0) {
                        // if (!fDiscard) {
                        //   HandleMultipartData(part, i);
                        //}
                    } else {
                        break;
                    }
                } else {
                    FairMQMessagePtr msg(NewMessage());
                    if (Receive(msg, fInputDataChannelName, i, fMQTimeoutMS) > 0) {
                        // if (!fDiscard) {
                        //   HandleData(msg, i);
                        //}
                    } else {
                        break;
                    }
                }
            }
        }
    }

    //std::string str1 = "aa\n";
    //fFile->Write(str1.c_str(), str1.size());
    //fFile->Write(reinterpret_cast<char *>(&fEDM4hepSinkTrailer), sizeof(EDM4hepSinkTrailer::Trailer));

    //fFile->Close();
    writer->finish();
    //int64_t n = -1;
    //std::ostringstream ss;
    //ss << " nwrite = " << fNWrite << "\n";
    //if (fFilePath.find("/dev/null") == std::string::npos) {
    //    const auto &files = fFile->GetBranchFilePath();
    //    const auto &rawSize = fFile->GetBranchRawSize();
    //    const auto &compressedSize = fFile->GetBranchCompressedSize();
    //    const auto &numIteration = fFile->GetBranchNumIteration();
    //
    //    n = 0;
    //    if (!fWorker) {
    //        fSize = 0;
    //        fCompressedSize = 0;
    //    }
    //    for (auto i = 0u; i < files.size(); ++i) {
    //        ss << " file: " << files[i]                   //
    //           << ", n = " << numIteration[i]             //
    //           << ", compressed = " << compressedSize[i]; //
    //
    //        if (!fWorker) {
    //            ss << ", raw = " << rawSize[i];
    //            fSize += rawSize[i];
    //            fCompressedSize += compressedSize[i];
    //        }
    //
    //        ss << "\n";
    //        n += stdfs::file_size(files[i]);
    //    }
    //}
    //double r = (fSize > 0) ? static_cast<double>(fCompressedSize) / fSize : 0.0;
    // double realMB = n/1e6;
    // double rawMB = fSize/1e6;
    // ss << " raw size = " << fSize << " Bytes, compressed size = " << fCompressedSize << " Bytes processed. r = " << r;
    //ss << " comperssion ratio (after/before) = " << r;
    //ss << "\n file size (sum) = " << n;
    //LOG(info) << " run : " << fFile->GetRunNumber() << " stopped." << ss.str();
    //
    //fFile->ClearBranch();
    //LOG(info) << __LINE__ << ":" << __func__ << " done";
}
//______________________________________________________________________________
void EDM4hepSink::PreRun()
{
    LOG(info) << __LINE__ << ":" << __func__;
    fNReceived = 0;
    fNWrite = 0;
    fStopRequested = false;
    fSize = 0;
    fCompressedSize = 0;

    if (fConfig->Count(opt::RunNumber.data())) {
        fRunNumber = std::stoll(fConfig->GetProperty<std::string>(opt::RunNumber.data()));
        LOG(debug) << " Run number: " << fRunNumber;
    }
    if (fConfig->Count("registry-uri")) {
        std::string registryUri = fConfig->GetProperty<std::string>("registry-uri");
        LOG(debug) << "registryUri: " << registryUri;
        if (!registryUri.empty()) {
            fClient = std::make_shared<sw::redis::Redis>(registryUri);
            std::string key;
            fClient->keys("run_info:run_comment", &key);
            if (key.length()>0) {
                auto run_comment_ptr = fClient->get("run_info:run_comment");
                run_comment = *run_comment_ptr;
	        LOG(debug) << "run comment: " << run_comment;
            }else{
                LOG(debug) << "There is no key run_info:run_comment";
            }
        }
    }
    if (run_comment.length() > 255) {
        run_comment.resize(255); // The max bytes of the comment are 256 bytes including a null charactor of 1 bytes.
    }

    
    //fFile->SetRunNumber(fRunNumber);
    //fFile->ClearBranch();
    //fFile->Open();
    writer = std::make_unique<podio::ROOTWriter>("output.root");
    //fFilePath = fFile->GetFilePath();
    if (fWorker) {
        // FileUtil writes data compressed by worker threads of EDM4hepSink.
        //fFile->SetCompressFunc([this](const char *src, int n) -> std::vector<char> {
        //    return std::move(Compressor::Compress(src, n, Compressor::Format::none, fFile->GetCompBufSize()));
        //});
        //LOG(debug) << " setup threads: n = " << fNThreads;
        //LOG(debug) << " file extension = " << fFileExtension << ", !compress = " << (!fCompress);
        fWorker->InitChannels(fNThreads, fInProcMQLength, fMQTimeoutMS);

        if (!fWorker->fHandleInput && !fMultipart) {
            LOG(debug) << " set single message handler for input data";
            fWorker->fHandleInput = [this](auto &msg, auto index) {
                return CompressData(msg, index);
            };
        }
        if (!fWorker->fHandleInputMultipart) {
            LOG(debug) << " set multipart message handler for input data";
            fWorker->fHandleInputMultipart = [this](auto &msgParts, auto index) {
                return CompressMultipartData(msgParts, index);
            };
        }
        if (!fWorker->fHandleOutput && !fMultipart) {
            LOG(debug) << " set single message handler for output data";
            fWorker->fHandleOutput = [this](auto &msg, auto index) {
                return WriteData(msg, index);
            };
        }
        if (!fWorker->fHandleOutputMultipart) {
            LOG(debug) << " set multipart message handler for output data";
            fWorker->fHandleOutputMultipart = [this](auto &msgParts, auto index) {
                return WriteMultipartData(msgParts, index);
            };
        }
        fWorker->Run();
    }

    
    //fFile->Write(reinterpret_cast<char *>(&fEDM4hepSinkHeader), sizeof(EDM4hepSinkHeader::Header));
    //std::string str1 = "aa\n";
    //fFile->Write(str1.c_str(), str1.size());
}

//______________________________________________________________________________
bool EDM4hepSink::WriteData(FairMQMessagePtr &msg, int index)
{
    (void)index;

    if (fStopRequested) {
        return false;
    }
    // LOG(info) << __LINE__ << ":" << __func__ << " index = " << index << " n-received = " << fNReceived << ", n-write =
    // " << fNWrite;
    //fFile->Write(reinterpret_cast<char *>(msg->GetData()), msg->GetSize());

    ++fNWrite;
    // LOG(info) << __LINE__ << ":" << __func__ << " : done : n-received = " << fNReceived << ", n-write = " << fNWrite;
    if (fWriteSleepInMilliSec > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(fWriteSleepInMilliSec));
      LOG(info) << __LINE__ << ":" << __func__ << " : fWriteSleepInMilliSec = " << fWriteSleepInMilliSec << " msec";
    }
    
    if ((fMaxIteration > 0) && (fNWrite == fMaxIteration)) {
        LOG(info) << " number of WriteData() reached the max iteration. n-write = " << fNWrite
                  << " max iteration = " << fMaxIteration << ". state transition : stop";
        // ChangeState(fair::mq::Transition::Stop);
        fStopRequested = true;
    }

    return !fStopRequested;
}

//______________________________________________________________________________
bool EDM4hepSink::WriteMultipartData(FairMQParts &msgParts, int index)
{
    (void)index;

    if (fStopRequested) {
        return false;
    }
    auto len = MessageUtil::TotalLength(msgParts);
    // LOG(info) << __LINE__ << ":" << __func__
    //           << " index = " << index
    //           << " n-received = " << fNReceived
    //           << " parts.Sze() = " << msgParts.Size()
    //           << " total length = " << len << " bytes"
    //           << ", n-write = " << fNWrite;
    
#if 0
    for (const auto &m : msgParts) {
      auto first = reinterpret_cast<const char *>(m->GetData());
      auto last = first + m->GetSize();
      LOG(debug) << "message nbytes = " << m->GetSize();
      std::for_each(first, last, HexDump());
    }
#endif
        
    if (fMergeMessage) {
        std::vector<char> v;
        v.reserve(len);
        for (auto &msg : msgParts) {
            auto pbegin = reinterpret_cast<char *>(msg->GetData());
            auto pend = pbegin + msg->GetSize();
            v.insert(v.end(), std::make_move_iterator(pbegin), std::make_move_iterator(pend));
        }
 	std::stringstream ss;
	AmQStrTdc::Data::Bits idata;
	//ss << "v.size()" << std::dec << v.size() << std::endl;
	uint64_t par_int;


	for (int i = 0; i < v.size()/8; i++) {
	  memcpy(&idata,&v.at(i*8),8);
	  if (idata.head == AmQStrTdc::Data::Heartbeat) {
	    //ss << "   HBF "
	    //   << ", HBF num: 0x" << std::hex << idata.hbframe << std::dec
	    //   << ", Delimiter flag: (0x" << std::hex << std::setw(4) << std::setfill('0') << idata.hbflag << std::setfill(' ') << std::dec << "";
	    //ss << ", active bits:";
	    //for (int i = 0; i < 16; i++){
	    //  if ((idata.hbflag >> i) & 0x1) {
	    //	ss << " bit" << i+1;
	    //  }
	    //}
	    //if (idata.hbflag == 0){
	    //  ss << " none";
	    //}
	    //ss << ")" << std::endl;
	    //hbcounter +=1;
	  }else if (idata.head == AmQStrTdc::Data::Data || idata.head == AmQStrTdc::Data::Trailer ||
		    idata.head == AmQStrTdc::Data::ThrottlingT1Start || idata.head == AmQStrTdc::Data::ThrottlingT1End ||
		    idata.head == AmQStrTdc::Data::ThrottlingT2Start || idata.head == AmQStrTdc::Data::ThrottlingT2End){
	    //if ( stfHeader.femType == 2 || stfHeader.femType == 5 ) { //HRTDC

	    auto out_hits = std::make_unique<edm4hep::SimTrackerHitCollection>();
            auto cellID     = (std::uint64_t)idata.hrch;
            auto eDep       = (float)idata.hrtot;
            auto time       = (float)idata.hrtdc;
            auto pathLength = (float)0.0;
            auto quality    = (std::uint64_t)0;
            auto pos        = edm4hep::Vector3d(0.0, 0.0, 0.0);
            auto mom        = edm4hep::Vector3f(0.0, 0.0, 0.0);
	    out_hits->create(
			     cellID,
			     eDep,
			     time,
			     pathLength,
			     quality,
			     pos,
			     mom
			     );
	    podio::Frame frame;
	    frame.put(std::move(out_hits), "SimTrackerHits");
	    writer->writeFrame(frame, "events");
	    
	    //}else if ( stfHeader.femType == 3 || stfHeader.femType == 6 ) { //LRTDC
	    //  ss << ", ch: "<< std::setw(3) << idata.ch;		
	    //  ss << ", tdc: "<< idata.tdc;
	    //  ss << ", tot: "<< idata.tot << std::endl;
	    //}
	  }
	  //std::cout << "here" << std::endl;
	}
	
	//fFile->Write(ss.str().c_str(), ss.str().size());

        ++fNWrite;
        if (fWriteSleepInMilliSec > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(fWriteSleepInMilliSec));
            LOG(info) << __LINE__ << ":" << __func__ << " : fWriteSleepInMilliSec = " << fWriteSleepInMilliSec << " msec";
        }
    } else {
        for (auto &msg : msgParts) {
	    //fFile->Write(reinterpret_cast<char *>(msg->GetData()), msg->GetSize());
	    ++fNWrite;
            if (fWriteSleepInMilliSec > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(fWriteSleepInMilliSec));
                LOG(info) << __LINE__ << ":" << __func__ << " : fWriteSleepInMilliSec = " << fWriteSleepInMilliSec << " msec";
            }
        }
    }

    if ((fMaxIteration > 0) && (fNWrite == fMaxIteration)) {
        LOG(info) << " number of WriteData() reached the max iteration. n-write = " << fNWrite
                  << " max iteration = " << fMaxIteration << ". state transition : stop";
        // ChangeState(fair::mq::Transition::Stop);
        fStopRequested = true;
    }

    
    return !fStopRequested;
}
} // namespace nestdaq
