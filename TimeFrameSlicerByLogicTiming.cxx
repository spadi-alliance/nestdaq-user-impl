/*
 * @file TimeFrameSlicerByLogicTiming
 * @brief Slice Timeframe by Logic timing for NestDAQ
 * @date Created : 2024-05-04 12:31:55 JST
 *       Last Modified : 2024-05-04 17:43:21 JST
 *
 * @author Shinsuke OTA <ota@rcnp.osaka-u.ac.jp>
 *
 */
#include "TimeFrameSlicerByLogicTiming.h"
#include "fairmq/runDevice.h"

#include "utility/MessageUtil.h"
#include "UnpackTdc.h"


using nestdaq::TimeFrameSlicerByLogicTiming;
namespace bpo = boost::program_options;


TimeFrameSlicerByLogicTiming::TimeFrameSlicerByLogicTiming()
   : fKTimer(1,KTimer(1000))
{
}


void TimeFrameSlicerByLogicTiming::InitTask()
{
    using opt = OptionKey;

    fInputChannelName  = fConfig->GetValue<std::string>(opt::InputChannelName.data());
    fOutputChannelName = fConfig->GetValue<std::string>(opt::OutputChannelName.data());
    LOG(info)
            << "InitTask : Input Channel  = " << fInputChannelName
            << "InitTask : Output Channel = " << fOutputChannelName;

    // identity
    fName = fConfig->GetProperty<std::string>("id");
    std::istringstream ss(fName.substr(fName.rfind("-") + 1));
    ss >> fId;
}

void TimeFrameSlicerByLogicTiming::PreRun()
{
//   fTF = nullptr;
//   fSTF.resize(0);
//   fHBF.resize(0);
   fEOM = false;
   fNextIdx = 0;
   fDoCheck = fKTimer[0].Check();
}

bool TimeFrameSlicerByLogicTiming::ConditionalRun()
{
   if (fDoCheck) LOG(info) << "Let's build";
   std::chrono::system_clock::time_point sw_start, sw_end;

   
   FairMQParts inParts;
   FairMQParts outParts;
   if (Receive(inParts, fInputChannelName,0,1) <= 0) return true;
   // inParts should have at least two messages

   if (fDoCheck) {
      LOG(info) << "inParts : " << inParts.Size();
   }

   GetNextHBF(inParts);
//   for (uint32_t ipt = 0, npt = inParts.Size(); ipt < npt; ++ipt) {
//      auto& part = inParts[ipt];
//      auto magic = *reinterpret_cast<uint64_t*>(part.GetData());
//      if (fDoCheck) {
//         if (magic == TimeFrame::MAGIC) {
//            LOG(info) << "part[" << std::setw(3) << ipt << "] = " << std::hex << magic;
//            auto tfbhdr = reinterpret_cast<struct TimeFrame::Header*>(part.GetData());
//            tfbhdr->Print();
//         } else if (magic == Filter::MAGIC) {
//            LOG(info) << "part[" << std::setw(3) << ipt << "] = " << std::hex << magic;
//            auto hdr = reinterpret_cast<struct Filter::Header*>(part.GetData());
//            LOG(info) << "length  = " << hdr->length;
//            LOG(info) << "hLength = " << hdr->hLength;
//            LOG(info) << "nTiming = " << (hdr->length - hdr->hLength)/sizeof(uint32_t);
//            LOG(info) << "data ptr = " << (hdr + 1) << " ?? " << reinterpret_cast<uint8_t*>(part.GetData()) + hdr->hLength << " ?? " << part.GetData();
//         }
//      }
//   }
   

   return true;
}

void TimeFrameSlicerByLogicTiming::PostRun()
{
   fDoCheck = 0;
}


bool TimeFrameSlicerByLogicTiming::GetNextHBF(FairMQParts& inParts)
{
   LOG(info) << "GetNextHBF";
   LOG(info) << "nParts = " << inParts.Size();
   if (fNextIdx >= inParts.Size()) {
      LOG(info) << "no data is available";
      return !(fEOM = true);
   }
   int stfIdx = 0; // array index for stf
   int hbfIdx = 0; // array index for hbf
   int fltIdx = 0; // array index for flt
   for (auto& i = fNextIdx, n = inParts.Size(); i < n; ++i) {
      auto& part = inParts[i];
      auto magic = *reinterpret_cast<uint64_t*>(part.GetData());
      LOG(info) << (char*)&magic;
      if (magic == TimeFrame::MAGIC) {
         if (fDoCheck) {
            LOG(info) << "analyze timeframe";
         }
         stfIdx = 0; // reset stf index
         fTF.SetHeader(part.GetData());
         auto ns = fTF.GetHeader()->numSource;
         if (fDoCheck) {
            LOG(info) << "numSource = " << ns;
         }
         if (fTF.size() < ns) {
            fTF.resize(ns);
            for (decltype(ns) is = 0; is < ns; ++is) {
               if (fTF[is] == nullptr) {
                  fTF[is] = new TSTF;
               }
            }
         }
         if (fDoCheck) {
            LOG(info) << "analyze timeframe done";
         }
      } else if (magic == Filter::MAGIC) {
         fLF.SetHeader(part.GetData());
         auto nm = fLF.GetHeader()->numMessages;
         if (fDoCheck) {
            auto hdr = fLF.GetHeader();
            LOG(info) << "analyze filter tdc";
            LOG(info) << nm;
            LOG(info) << hdr->length;
            LOG(info) << hdr->hLength;
            LOG(info) << hdr->numTrigs;
            LOG(info) << hdr->workerId;
            LOG(info) << hdr->numMessages;
            LOG(info) << hdr->elapseTime;
         }
         
         if (fLF.size() < nm) {
            fLF.resize(nm);
            for (decltype(nm) im = 0; im < nm; ++im) {
               if (fLF[im] == nullptr) fLF[im] = new TTT;
            }
         }
      } else if (magic == Filter::TDC_MAGIC) {
         if (fDoCheck) {
            LOG(info) << "analyze filter tdc";
            LOG(info) << fLF.size();
         }
         
         auto& tdcf = *(fLF[fltIdx]);
         tdcf.Set(part.GetData());
         fltIdx++;
      } else if (magic == SubTimeFrame::MAGIC) {
         hbfIdx = 0; // reset hbf index
         auto& stf = *(fTF[stfIdx]);
         stf.SetHeader(part.GetData());
         auto nh = stf.GetHeader()->numMessages;
         if (stf.size() < nh) {
            stf.resize(nh);
            for (decltype(nh) ih = 0; ih < nh; ++ih) {
               if (stf[ih] == nullptr) {
                  stf[ih] = new THBF;
               }
            }
         }
         if (fDoCheck) {
            auto hdr = stf.GetHeader();
            LOG(info) << "stfIdx = " << stfIdx;
            LOG(info) << "femtype = " << hdr->femType;
            LOG(info) << "femId = " << hdr->femId;
            LOG(info) << "numMessages = " << hdr->numMessages;
         }
         stfIdx++;
      } else if (magic == HeartbeatFrame::MAGIC) {
         auto& hbf = *(fTF[stfIdx]->at(hbfIdx));
         hbf.Set(part.GetData());
         hbfIdx++;
      }
   }
   return true;
}


////////////////////////////////////////////////////
// override runDevice
////////////////////////////////////////////////////

void addCustomOptions(bpo::options_description& options)
{
   using opt = TimeFrameSlicerByLogicTiming::OptionKey;

   options.add_options()
      (opt::InputChannelName.data(),
       bpo::value<std::string>()->default_value("in"),
       "Name of the input channel")
      (opt::OutputChannelName.data(),
       bpo::value<std::string>()->default_value("out"),
       "Name of the output channel")
      (opt::DQMChannelName.data(),
       bpo::value<std::string>()->default_value("dqm"),
       "Name of the data quality monitoring channel")
      ;
   
}


std::unique_ptr<fair::mq::Device> getDevice(fair::mq::ProgOptions& /*config*/)
{
    return std::make_unique<TimeFrameSlicerByLogicTiming>();
}

