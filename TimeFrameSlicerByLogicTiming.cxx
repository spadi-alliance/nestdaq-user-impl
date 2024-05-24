/*
 * @file TimeFrameSlicerByLogicTiming
 * @brief Slice Timeframe by Logic timing for NestDAQ
 * @date Created : 2024-05-04 12:31:55 JST
 *       Last Modified : 2024-05-24 13:55:21 JST
 *
 * @author Shinsuke OTA <ota@rcnp.osaka-u.ac.jp>
 *
 */
#include "TimeFrameSlicerByLogicTiming.h"
#include "fairmq/runDevice.h"

#include "utility/MessageUtil.h"
#include "UnpackTdc.h"

#define DEBUG 0

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
    //fOffset[0] = fConfig->GetValue<int>(opt::TimeOffsetBegin.data());
    //fOffset[1] = fConfig->GetValue<int>(opt::TimeOffsetEnd.data());
    fOffset[0] = std::stoi(fConfig->GetValue<std::string>(opt::TimeOffsetBegin.data()));
    fOffset[1] = std::stoi(fConfig->GetValue<std::string>(opt::TimeOffsetEnd.data()));
    LOG(info)
            << "InitTask : Input Channel  = " << fInputChannelName
            << "InitTask : Output Channel = " << fOutputChannelName;
    LOG(info) << "Time Window [" << fOffset[0] << " : " << fOffset[1] << "]";


    // identity
    fName = fConfig->GetProperty<std::string>("id");
    std::istringstream ss(fName.substr(fName.rfind("-") + 1));
    ss >> fId;


    fNumDestination = GetNumSubChannels(fOutputChannelName);
    fPollTimeoutMS  = std::stoi(fConfig->GetProperty<std::string>(opt::PollTimeout.data()));
    
}

void TimeFrameSlicerByLogicTiming::PreRun()
{
//   fTF = nullptr;
//   fSTF.resize(0);
//   fHBF.resize(0);
}

bool TimeFrameSlicerByLogicTiming::ConditionalRun()
{
   fEOM = false;
   fNextIdx = 0;
   fNumHBF = INT_MAX;
   fIdxHBF = 0;
   fDoCheck = fKTimer[0].Check();
#if DEBUG   
   if (fDoCheck) LOG(info) << "Let's build";
#endif   
   std::chrono::system_clock::time_point sw_start, sw_end;

   
   FairMQParts inParts;
   FairMQParts outParts;
   if (Receive(inParts, fInputChannelName,0,1) <= 0) return true;
   // inParts should have at least two messages

#if DEBUG   
   if (fDoCheck) {
      LOG(info) << "inParts : " << inParts.Size();
   }
#endif
   ParseMessages(inParts);

   using copyUnit = uint32_t;
   //----------------------------------------------------------------------
   // make slices
   //----------------------------------------------------------------------
   while (fIdxHBF < fNumHBF) {
      auto outdataptr = std::make_unique<std::vector<copyUnit>>();
      auto outdata = outdataptr.get();

      /***********************************************************************
       * Structure of timeframe slices
       * TFH (meta info)
       * FltH
       * TDCs
       * STFH
       * HBFH
       * HBDs
       * STFH
       * HBFH
       * HBDs
       * TFH (sliced data)
       * STFH
       * HBFH
       * TDCs
       * STFH
       * HBFH
       * TDCs
       ***********************************************************************/

      //----------------------------------------------------------------------
      // make summary block (meta)
      //----------------------------------------------------------------------
      
      // copy time frame header
      fTFHidx = 0;
      fTF.CopyHeaderTo<copyUnit>(outdata);
      // copy filter frame
      auto lfhidx = outdata->size();
      fLF.CopyHeaderTo<copyUnit>(outdata);

      // use only one frame
      fLF[fIdxHBF]->CopyHeaderTo<copyUnit>(outdata);
      fLF[fIdxHBF]->CopyDataTo<copyUnit>(outdata);

      auto lfh = (Filter::Header*) &((*outdata)[lfhidx]);
      lfh->length = (outdata->size() - lfhidx) * sizeof(copyUnit);
      lfh->numTrigs = fLF[fIdxHBF]->GetNumData();
      
      // copy heart beat delimiters
      for (uint32_t is = 0, ns = fTF.size(); is < ns; ++is) {
         auto stfhidx = outdata->size();
         fTF[is]->CopyHeaderTo<copyUnit>(outdata);
         auto hbfhidx = outdata->size();
         fTF[is]->at(fIdxHBF)->CopyHeaderTo<copyUnit>(outdata);
         // store hbds
         auto hbf = fTF[is]->at(fIdxHBF);
         hbf->CopyDataTo<copyUnit>(outdata,hbf->GetNumData()-2);
         hbf->CopyDataTo<copyUnit>(outdata,hbf->GetNumData()-1);
         // modify size of subtime frame
         auto stfh = (SubTimeFrame::Header*) &((*outdata)[stfhidx]);
         stfh->length = (outdata->size() - stfhidx) * sizeof(copyUnit);
         stfh->hLength = sizeof(struct SubTimeFrame::Header);
         // modify size of heartbeat frame
         auto hbfh = (HeartbeatFrame::Header*) &((*outdata)[hbfhidx]);
         hbfh->length = (outdata->size() - hbfhidx) * sizeof(copyUnit);
      }
      
      
      // modify header information
      auto tfh = (TimeFrame::Header*) &((*outdata)[0]);
      tfh->type = TimeFrame::META;
      tfh->length = outdata->size()*sizeof(copyUnit);

      /***********************************************************************
       * Prepare the slices 
       ***********************************************************************/
#if DEBUG      
      if (fDoCheck) {
         LOG(info) << "Num Sources = " << fTF.size() << " / " << fTF.GetHeader()->numSource;
         LOG(info) << " Num HB = " << fNumHBF << " / " << fTF[0]->GetHeader()->numMessages;
         LOG(info) << " Num LF = " << fLF.size();
         LOG(info) << " Num Data in " << fIdxHBF << " = " << fLF[fIdxHBF]->GetNumData();
         for (int i = 0,n = std::min(fLF[fIdxHBF]->GetNumData(),(uint64_t)4); i<n; ++i) {
            LOG(info) << " Trg[" << i << "] = " << fLF[fIdxHBF]->UncheckedAt(i);
         }
      }
#endif
      //----------------------------------------------------------------------
      // start analyzing tdc data
      //----------------------------------------------------------------------
      auto lf = fLF[fIdxHBF];
      auto nTrig = lf->GetNumData();
      std::vector<uint64_t> tdcidxs(fTF.size(),0);
      std::vector<THBF*> hbfs(fTF.size());
      std::vector<uint64_t> femtype(fTF.size());
      struct TDC64L_V3::tdc64 tdc64l;
      struct TDC64H_V3::tdc64 tdc64h;
      
      for (int is = 0, ns = fTF.size(); is < ns; ++is) {
         hbfs[is] = fTF[is]->at(fIdxHBF);
         femtype[is] = fTF[is]->GetHeader()->femType;
      }

      // check overlap of two neighboring search window
      for (uint32_t iTrig = 0; iTrig < nTrig; ++iTrig) {
         auto trig = lf->UncheckedAt(iTrig).time;
         auto trigBegin = trig + fOffset[0];
         auto trigEnd   = trig + fOffset[1];
         bool hasOverlapWithNextTrigger = false;
         if (iTrig < nTrig - 1) {
            hasOverlapWithNextTrigger = ((lf->UncheckedAt(iTrig+1).time + fOffset[0]) < trigEnd);
         }
            
#if DEBUG   
         if (fDoCheck) {
            LOG(info) << "trigger window [" << trigBegin << "," << trigEnd << "] " << trig;
         }
         if (hasOverlapWithNextTrigger) {
            LOG(info) << "Overlap in triggers" << (lf->UncheckedAt(iTrig+1).time + fOffset[0]) << "<" << trigEnd;
         }
#endif
         //----------------------------------------------------------------------
         // make timeframe for each slice
         //----------------------------------------------------------------------
         auto tfidx = outdata->size();
         fTF.CopyHeaderTo<copyUnit>(outdata);

         for (int is = 0, ns = fTF.size(); is < ns; ++is) {
            //----------------------------------------------------------------------
            // make subtime frame header
            //----------------------------------------------------------------------
            auto stfhidx = outdata->size();
            fTF[is]->CopyHeaderTo<copyUnit>(outdata);


            auto hbf = hbfs[is];
            auto fem = femtype[is];

            //----------------------------------------------------------------------
            // make heartbeat frame header
            //----------------------------------------------------------------------
            auto hbfhidx = outdata->size();
            hbf->CopyHeaderTo<copyUnit>(outdata);
            
            //----------------------------------------------------------------------
            // comparison of tdc 
            //----------------------------------------------------------------------
            auto iKeep = tdcidxs[is];
            // nt ignores HBD
            auto& it = tdcidxs[is], nt = hbf->GetNumData() - 2;            
            while ( it < nt) {
               int tdc4n = 0;
               int ch = 0;
               // decode data
               if (fem == SubTimeFrame::TDC64H_V3) {
                  TDC64H_V3::Unpack(hbf->UncheckedAt(it),&tdc64h);
                  tdc4n = tdc64h.tdc4n;
                  ch = tdc64h.ch;
               } else if (fem == SubTimeFrame::TDC64L_V3) {
                  TDC64L_V3::Unpack(hbf->UncheckedAt(it),&tdc64l);
                  tdc4n = tdc64l.tdc4n;
                  ch = tdc64l.ch;
               }
               // validate hits
               if (tdc4n < trigBegin) {
                  iKeep++;
                  it++;
                  continue;
               }
               if (tdc4n > trigEnd) {
                  break;
               }
               hbf->CopyDataTo<copyUnit>(outdata,it);
               it++;
            }
#if 0            
            if (hasOverlapWithNextTrigger) {
               // if the search window is overlap with next trigger,
               // the index should be rewineded to indicate the first hit in this trigger window.
               LOG(info) << "is = " << is << "Overlap at iTrig = " << iTrig << " " << tdcidxs[is] << " -> " << iKeep;
               tdcidxs[is] = iKeep;
            }
#endif            
            auto hbfh = (SubTimeFrame::Header*) &((*outdata)[hbfhidx]);
            hbfh->length = (outdata->size() - hbfhidx) * sizeof(copyUnit);
            auto stfh = (SubTimeFrame::Header*) &((*outdata)[stfhidx]);
            stfh->length = (outdata->size() - stfhidx) * sizeof(copyUnit);
            stfh->hLength = sizeof(struct SubTimeFrame::Header);
            stfh->numMessages = 2; // by definition the number of heartbeat frame is one and then num message is one
         }
         
         auto tfsh = (TimeFrame::Header*) &((*outdata)[tfidx]);
         tfsh->type = TimeFrame::SLICE;
         tfsh->length = (outdata->size() - tfidx) * sizeof(copyUnit);
      }

      //----------------------------------------------------------------------
      // store to output parts
      //----------------------------------------------------------------------
      FairMQMessagePtr msgall(MessageUtil::NewMessage(*this,std::move(outdataptr)));
      outParts.AddPart(std::move(msgall));
      // go to next
      fIdxHBF++;
   } // while 


    ////////////////////////////////////////////////////
    // Transfer the data to all of output channel
    ////////////////////////////////////////////////////
    auto poller = NewPoller(fOutputChannelName);
    while (!NewStatePending()) {
        auto direction = (fDirection++) % fNumDestination;
        poller->Poll(fPollTimeoutMS);
        if (poller->CheckOutput(fOutputChannelName, direction)) {
            if (Send(outParts, fOutputChannelName, direction) > 0) {
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
}


bool TimeFrameSlicerByLogicTiming::ParseMessages(FairMQParts& inParts)
{
#if DEBUG   
   
   if (fDoCheck) {
      LOG(info) << "GetNextHBF";
      LOG(info) << "nParts = " << inParts.Size();
   }
#endif   
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
      if (magic == TimeFrame::MAGIC) {
#if DEBUG
         if (fDoCheck) {
            LOG(info) << "analyze timeframe";
         }
#endif         
         stfIdx = -1; // reset stf index
         fTF.SetHeader(part.GetData());
         auto ns = fTF.GetHeader()->numSource;
#if DEBUG         
         if (fDoCheck) {
            LOG(info) << "timeframeid = " << fTF.GetHeader()->timeFrameId;
            LOG(info) << "numSource = " << ns;
         }
#endif         
         if (fTF.size() < ns) {
            fTF.resize(ns);
            for (decltype(ns) is = 0; is < ns; ++is) {
               if (fTF[is] == nullptr) {
                  fTF[is] = new TSTF;
               }
            }
         }
#if DEBUG         
         if (fDoCheck) {
            LOG(info) << "analyze timeframe done";
         }
#endif         
      } else if (magic == Filter::MAGIC) {
         fLF.SetHeader(part.GetData());
         auto nm = fLF.GetHeader()->numMessages;
#if DEBUG         
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
#endif         
         if (fLF.size() < nm - 1) {
            fLF.resize(nm);
            for (decltype(nm) im = 0; im < nm; ++im) {
               if (fLF[im] == nullptr) fLF[im] = new TTT;
            }
         }
      } else if (magic == Filter::TDC_MAGIC) {
#if DEBUG
         if (fDoCheck) {
            LOG(info) << "analyze filter tdc";
            LOG(info) << fLF.size();
            LOG(info) << *(uint64_t*)((char*)part.GetData());
            LOG(info) << *(uint32_t*)((char*)part.GetData()+8);
            LOG(info) << *(uint16_t*)((char*)part.GetData()+12);
            LOG(info) << *(uint16_t*)((char*)part.GetData()+14);
            LOG(info) << *(uint32_t*)((char*)part.GetData()+16);
            LOG(info) << *(uint32_t*)((char*)part.GetData()+20);
            LOG(info) << *(uint32_t*)((char*)part.GetData()+24);
            LOG(info) << *(uint32_t*)((char*)part.GetData()+28);
         }
#endif         
         auto& tdcf = *(fLF[fltIdx]);
         tdcf.Set(part.GetData());
         fltIdx++;
      } else if (magic == SubTimeFrame::MAGIC) {
         hbfIdx = 0; // reset hbf index
         stfIdx ++;
         auto& stf = *(fTF[stfIdx]);
         stf.SetHeader(part.GetData());
         uint32_t nh = stf.GetHeader()->numMessages;
#if DEBUG   
         if (fDoCheck) {
            LOG(info) << stf.size() << " " << nh;
         }
#endif         
         if (stf.size() < nh) {
            stf.resize(nh);
#if DEBUG            
            if (fDoCheck) {
               LOG(info) << stf.size() << " " << nh;
            }
#endif            
            for (decltype(nh) ih = 0; ih < nh; ++ih) {
               if (stf[ih] == nullptr) {
                  stf[ih] = new THBF;
               }
            }
         }
#if DEBUG         
         if (fDoCheck) {
            auto hdr = stf.GetHeader();
            LOG(info) << "stfIdx = " << stfIdx;
            LOG(info) << "femtype = " << hdr->femType;
            LOG(info) << "femId = " << hdr->femId;
            LOG(info) << "numMessages = " << hdr->numMessages;
         }
#endif         
      } else if (magic == HeartbeatFrame::MAGIC) {
         auto& hbf = *(fTF[stfIdx]->at(hbfIdx));
         hbf.Set(part.GetData());
         hbfIdx++;
      }
   }
   fNumHBF = hbfIdx;
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
      (opt::TimeOffsetBegin.data(),
       //bpo::value<int>()->default_value(-100),
       bpo::value<std::string>()->default_value("-100"),
       "offset where window begins")
      (opt::TimeOffsetEnd.data(),
       //bpo::value<int>()->default_value(100),
       bpo::value<std::string>()->default_value("100"),
       "offset where window ends")
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
    return std::make_unique<TimeFrameSlicerByLogicTiming>();
}

