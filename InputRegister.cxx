/*
 * @file InputRegister.cxx
 * @brief InputRegister for NestDAQ
 * @date Created : 2024-06-29 15:45:01 JST
 *       Last Modified : 2024-07-04 22:03:40 JST
 *
 * @author Shinsuke OTA <ota@rcnp.osaka-u.ac.jp>
 *
 */

#include "InputRegister.h"
#include "fairmq/runDevice.h"

#include "utility/MessageUtil.h"
#include "UnpackTdc.h"

#include <typeinfo>

using nestdaq::InputRegister;
namespace bpo = boost::program_options;


namespace {
   struct tdcdata {
      int ch;
      int tdc4n;
   };
   tdcdata Unpack(uint64_t data, uint32_t femtype) {
      if (femtype == SubTimeFrame::TDC64H_V3) {
         struct TDC64H_V3::tdc64 tdc;
         TDC64H_V3::Unpack(data,&tdc);
         return tdcdata{tdc.ch, tdc.tdc4n};
      } else if (femtype == SubTimeFrame::TDC64L_V3) {
         struct TDC64L_V3::tdc64 tdc;
         TDC64L_V3::Unpack(data,&tdc);
         return tdcdata{tdc.ch, tdc.tdc4n};
      } else {
         return tdcdata{0,0};
      }
   }
}      


InputRegister::InputRegister()
   : fKT(1000)
{
   // tdc_type.emplace_back(5,typeid(struct TDC64H_V3::tdc64));
}


void InputRegister::InitTask()
{
   using opt = OptionKey;


   fInputChannelName  = fConfig->GetValue<std::string>(opt::InputChannelName.data());
   fOutputChannelName = fConfig->GetValue<std::string>(opt::OutputChannelName.data());
   std::stringstream ss(fConfig->GetProperty<std::string>(opt::RegisterChannels.data()));
   std::istream_iterator<int> it(ss); // 入力ストリームオブジェクトへの参照を渡す
   std::istream_iterator<int> last;   // デフォルト構築。終端値として使用する
   while (it != last) {
      fRegisterChannels.push_back(*it);
      LOG(info) << "InitTask: reg-ch = " <<  *it;
      it++;
   }

   fNumDestination = GetNumSubChannels(fOutputChannelName);
   fPollTimeoutMS  = std::stoi(fConfig->GetProperty<std::string>(opt::PollTimeout.data()));
   
}


bool InputRegister::ConditionalRun()
{
   std::chrono::system_clock::time_point sw_star, sw_end;

   fDoCheck = fKT.Check();

   FairMQParts inParts;
   FairMQParts outParts;
   if (Receive(inParts, fInputChannelName,0,1) <= 0) return true;
   // LOG(info) << " numSize = " << inParts.Size();

   ParseMessages(inParts);
   bool isCleared = false;

   if (1 || fDoCheck) {
      auto femtype = fSubtimeFrame.GetHeader()->femType;
      // LOG(info) << "FEMType:" << femtype << " femid: " << std::hex << fSubtimeFrame.GetHeader()->femId;
      for (int ihbf = 0, nhbf = fSubtimeFrame.size(); ihbf < nhbf; ++ihbf) {
         auto& hbf = *fSubtimeFrame[ihbf];
         auto hbd = DecodeHBD1(hbf.GetData()[hbf.GetNumData() - 2]);
         // LOG(info) << hbd.hbfn;
          // decltype(tdc_type[fSubtimeFrame.GetHeader()->femType]) tdc;
         for (int idata = 0, ndata = hbf.GetNumData(); idata < ndata; ++idata) {
            auto tdcdata = Unpack(hbf.GetData()[idata],femtype);
            if (std::find(fRegisterChannels.begin(),fRegisterChannels.end(),tdcdata.ch) == fRegisterChannels.end()) continue;
            auto status = GenerateStatus(tdcdata.ch,1,hbd.offset,(tdcdata.tdc4n >> 1), hbd.hbfn);
            if (fStatus.size() > 0 && !isCleared) {
               fStatus.clear();
               isCleared = true;
            }
            fStatus.push_back(status);
            LOG(info) << " Found " << tdcdata.ch << " = " << fSubtimeFrame.GetHeader()->timeFrameId << ":" <<  std::hex << status;
         }
      }
   }

//   if (fSubtimeFrame.size() > fOutputSubtimeFrame.size()) {
//      fOutputSubtimeFrame.resize(fSubtimeFrame.size(), new THBF);
//   }

#if 1
   // LOG(info) << "prepare";
   
   using copyUnit = uint32_t;
   uint64_t totalSize = 0;
   {
      // prepare subtime frame header
      auto outdataptr = std::make_unique<std::vector<copyUnit>>();
      auto outdata = outdataptr.get();

      fSubtimeFrame.CopyHeaderTo<copyUnit>(outdata);
      FairMQMessagePtr msgall(MessageUtil::NewMessage(*this,std::move(outdataptr)));
      outParts.AddPart(std::move(msgall));
      totalSize = fSubtimeFrame.GetHeader()->hLength;
   }
   for (int ihbf = 0, nhbf = fSubtimeFrame.size(); ihbf < nhbf; ++ihbf) {
      auto& hbf = *fSubtimeFrame[ihbf];
      auto outdataptr = std::make_unique<std::vector<copyUnit>>();
      auto outdata = outdataptr.get();
      auto hbd = hbf.GetData()[hbf.GetNumData() -2];
      uint32_t nst = 0;
      // add data size if available here
      hbf.CopyHeaderTo<copyUnit>(outdata);
      // copy data here if available
      // copy heartbeat delimiter
      LOG(info) << "HBFN " << (hbd & 0xffffff);
      for (int i = 0, n = fStatus.size(); i < n; ++i) {
         auto st = fStatus[i];
         if ((st &0xffffff) <= (hbd & 0xffffff)) {
            for (int j = 0, nj = sizeof(st)/sizeof(copyUnit); j < nj; ++j) {
               outdata->push_back(*((copyUnit*)&st+j));
            }
         } 
      }
      // remove status other than last
      // LOG(info) << "erase";
      if (fStatus.size() > 0) {
         auto st = fStatus[fStatus.size()-1];
         fStatus.clear();
         fStatus.push_back(st);
      }
      // LOG(info) << "done";
      hbf.CopyDataTo<copyUnit>(outdata,hbf.GetNumData()-2);
      hbf.CopyDataTo<copyUnit>(outdata,hbf.GetNumData()-1);
      uint32_t size = outdata->size();
      FairMQMessagePtr msgall(MessageUtil::NewMessage(*this,std::move(outdataptr)));
      outParts.AddPart(std::move(msgall));
      auto hbfh = reinterpret_cast<HeartbeatFrame::Header*>(outParts[outParts.Size()-1].GetData());
      hbfh->length = size * sizeof(copyUnit);
      LOG(info) << "length = " << hbfh->length;
      
      for (int i = 0; i < outParts.Size(); i++) {
         LOG(info) << i << ":" << outParts[i].GetSize();
      }
      //totalSize += hbfh->length;
   }


   // set length
   auto stfh = reinterpret_cast<SubTimeFrame::Header*>(outParts[0].GetData());
   stfh->length = totalSize;
   stfh->femType = kID;
   stfh->type = SubTimeFrame::META;
   for (int i = 0; i < outParts.Size(); i++) {
      LOG(info) << i << ":" << outParts[i].GetSize();
   }
      
#endif   
   ////////////////////////////////////////////////////
   // Transfer the data to all of output channel
   ////////////////////////////////////////////////////
   auto poller = NewPoller(fOutputChannelName);
   while (!NewStatePending()) {
      auto direction = (fDirection++) % fNumDestination;
      poller->Poll(fPollTimeoutMS);
      if (poller->CheckOutput(fOutputChannelName, direction)) {
         if (Send(outParts, fOutputChannelName, direction) > 0) {
            LOG(info) << "data sent" << " size " << outParts.Size() << " direction = " << direction;
            // successfully sent
            break;
         } else {
            LOG(error) << "Failed to queue output-channel";
         }
      }
      if (fNumDestination==1) {
         std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
   }

   return true;
}


bool InputRegister::ParseMessages(FairMQParts& inParts)
{
   // assuming input with subtime frame headers
   uint32_t hbfidx = 0;
      
   for (int ip = 0, np = inParts.Size(); ip < np; ++ip) {
      auto& part = inParts[ip];
      if (fDoCheck) {
         auto magic_str = reinterpret_cast<char*>(part.GetData());
         // LOG(info) << magic_str;
      }

      auto magic = *reinterpret_cast<uint64_t*>(part.GetData());
      if (magic == SubTimeFrame::MAGIC) {
         hbfidx = 0;
         auto &stf = fSubtimeFrame;
         stf.SetHeader(part.GetData());
         auto nh = stf.GetHeader()->numMessages;
         if (fDoCheck) {
            LOG(info) << nh;
         }
         if (stf.size() < nh) {
            stf.resize(nh);
            for (decltype(nh) ih = 0; ih < nh; ++ih) {
               if (stf[ih] == nullptr) stf[ih] = new THBF;
            }
         }
      } else if (magic == HeartbeatFrame::MAGIC) {
         auto& hbf = *(fSubtimeFrame[hbfidx]);
         hbf.Set(part.GetData());
         hbfidx++;
      }
   }
   

   return true;
}
void addCustomOptions(bpo::options_description& options)
{
   using opt = InputRegister::OptionKey;

   options.add_options()
      (opt::InputChannelName.data(),
       bpo::value<std::string>()->default_value("in"),
       "Name of the input channel")
      (opt::OutputChannelName.data(),
       bpo::value<std::string>()->default_value("out"),
       "Name of the output channel")
      (opt::RegisterChannels.data(),
       bpo::value<std::string>()->default_value("reg-chs"),
       "List of register channels (space separated)")
      (opt::PollTimeout.data(),
       bpo::value<std::string>()->default_value("1"),
       "Timeout of polling (in msec)")
      (opt::SplitMethod.data(),
       bpo::value<std::string>()->default_value("1"),
       "STF split method")
      ;
}

std::unique_ptr<fair::mq::Device> getDevice(fair::mq::ProgOptions& /*config*/)
{
   return std::make_unique<InputRegister>();
}

