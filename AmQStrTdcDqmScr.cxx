#include <chrono>
#include <thread>
#include <future>
#include <map>
#include <iostream>
#include <boost/format.hpp>
#include <cmath>

#include <fairmq/runDevice.h>

#include "TimeFrameHeader.h"
#include "SubTimeFrameHeader.h"
#include "STFBuilder.h"
#include "AmQStrTdcData.h"
#include "utility/Reporter.h"
#include "utility/HexDump.h"
#include "utility/MessageUtil.h"

#include <sw/redis++/redis++.h>
#include <sw/redis++/patterns/redlock.h>
#include <sw/redis++/errors.h>

#include "AmQStrTdcDqmScr.h"
#include "ScalerData.h"

namespace bpo = boost::program_options;

//______________________________________________________________________________

void addCustomOptions(bpo::options_description& options)
{
  {
    using opt = AmQStrTdcDqmScr::OptionKey;
  options.add_options()
    (opt::NumSource.data(),          bpo::value<std::string>()->default_value("1"), "Number of source endpoint")
    (opt::SourceType.data(),         bpo::value<std::string>()->default_value("stf"), "Type of source endpoint")    
    (opt::BufferTimeoutInMs.data(),  bpo::value<std::string>()->default_value("100000"), "Buffer timeout in milliseconds")
    (opt::RunNumber.data(),          bpo::value<std::string>()->default_value("1"), "Run number (integer) given by DAQ plugin")
    (opt::InputChannelName.data(),   bpo::value<std::string>()->default_value("in"), "Name of the input channel")
    (opt::OutputChannelName.data(),   bpo::value<std::string>()->default_value("out"), "Name of the output channel")    
    (opt::UpdateInterval.data(),     bpo::value<std::string>()->default_value("1000"), "Canvas update rate in milliseconds")
    (opt::ServerUri.data(),          bpo::value<std::string>(),                        "Redis server URI (if empty, the same URI of the service registry is used.)")
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
std::unique_ptr<fair::mq::Device> getDevice(fair::mq::ProgOptions& /*config*/)
{
   return std::make_unique<AmQStrTdcDqmScr>();
}

//______________________________________________________________________________
AmQStrTdcDqmScr::AmQStrTdcDqmScr()
    : FairMQDevice()
{
    
}

//______________________________________________________________________________
void AmQStrTdcDqmScr::Check(std::vector<STFBuffer>& stfs)
{

  namespace STF = SubTimeFrame;
  namespace Data = AmQStrTdc::Data;

  using Bits     = Data::Bits;

  // [time-stamp, femId-list]
  std::unordered_map<uint16_t, std::unordered_set<uint32_t>> heartbeatCnt;
  std::unordered_map<uint16_t, std::unordered_set<uint32_t>> heartbeatNum;
  std::unordered_set<uint32_t> spillEnd;
  std::unordered_set<uint32_t> spillOn;

  std::unordered_map<uint16_t, std::unordered_set<uint32_t>> timeFrameId;
  std::unordered_map<uint16_t, std::unordered_set<uint32_t>> length;

  std::unordered_map<uint16_t, std::unordered_set<uint32_t>> heartbeatFlag;

  std::unordered_map<uint16_t, std::unordered_set<uint32_t>> lrtdcCnt;
  std::unordered_map<uint16_t, std::unordered_set<uint32_t>> hrtdcCnt;
  
  if(fDebug){
    LOG(debug) << "===========================";
    LOG(debug) << "fNumSource: " << fNumSource;
    LOG(debug) << "===========================";
  }
  for (int istf=0; istf<fNumSource; ++istf) {
    auto& [stf, t] = stfs[istf];
    auto h = reinterpret_cast<STF::Header*>(stf.At(0)->GetData());
    auto nmsg  = stf.Size();

    if(fDebug){
      LOG(debug) << "===========================";      
      LOG(debug) << "nmsg: " << nmsg;      
      LOG(debug) << "magic: " << std::hex << h->magic << std::dec;
      LOG(debug) << "timeFrameId: " << h->timeFrameId;
      LOG(debug) << "FEMType: " << h->FEMType;
      LOG(debug) << "FEMTId: " << std::hex << h->FEMId << std::dec;
      LOG(debug) << "length: " << h->length;
      LOG(debug) << "time_sec: " << h->time_sec;
      LOG(debug) << "time_usec: " << h->time_usec;
    }


    if (!fFEMId.count(h->FEMId)) {
      LOG(debug) << "FEMTId: " << std::hex << h->FEMId << std::dec;
      fFEMId[h->FEMId] = fId;
      fModuleIp[fId] = h->FEMId & 0xff;
      LOG(debug) << "fId: "<< fId <<  " mIp: " << fModuleIp[fId];
      
      fId++;
    }

    //auto len   = h->length - sizeof(STF::Header); // data size including tdc measurement
    //auto nword = len/sizeof(Word);
    //auto nhit  = nword - nmsg;
    auto femIdx = fFEMId[h->FEMId];
    //    auto femIdx = (h->FEMId & 0x0f) - 1;

    timeFrameId[h->timeFrameId].insert(femIdx);
    length[h->length].insert(femIdx);

    int numHB = 0;
    for (int imsg=1; imsg<nmsg; ++imsg) {
      
      const auto& msg = stf.At(imsg);
      auto wb =  reinterpret_cast<Bits*>(msg->GetData());
      if(fDebug){
	LOG(debug) << " word =" << std::hex << wb->raw << std::dec;
	LOG(debug) << " head =" << std::hex << wb->head << std::dec;
      }
      
      std::stringstream ss;
      switch (wb->head) {
      case Data::Heartbeat:
	ss << "== Data::Heartbeat --> " << std::endl
	   << "hbframe: 0x" << std::hex << wb->hbframe << std::dec << std::endl
	   << "hbspill#: 0x" << std::hex << wb->hbspilln << std::dec << std::endl
	   << "hbfalg: 0x" << std::hex << wb->hbflag << std::dec << std::endl
	   << "header: 0x" << std::hex << wb->htype << std::dec << std::endl
	   << "femIdx: 0x" << std::hex << femIdx << std::dec << std::endl;
	if(fDebug){
	  LOG(debug) << "== Data::Heartbeat --> ";
	  LOG(debug) << "hbframe: " << std::hex << wb->hbframe << std::dec;
	  LOG(debug) << "hbspill#: " << std::hex << wb->hbspilln << std::dec;
	  LOG(debug) << "hbfalg: " << std::hex << wb->hbflag << std::dec;
	  LOG(debug) << "header: " << std::hex << wb->htype << std::dec;
	  LOG(debug) << "femIdx: " << std::hex << femIdx << std::dec;
	  LOG(debug) << "head =" << std::hex << wb->head << std::dec;
	  LOG(debug) << "== scaler --> ";
	  for (auto& [id, chpair] : scaler) {
	    for (auto& [ch, val] : chpair) {
	      LOG(debug) << "id, ch, val: " << id << ", " << ch << ", " << val;
	    }
	  }
	  LOG(debug) << "--> scaler ==";
	  LOG(debug) << "--> Data::Heartbeat ==";
	}
	
        if (heartbeatCnt.count(wb->hbframe) && heartbeatCnt.at(wb->hbframe).count(femIdx)) {
          LOG(error) << " double count of heartbeat " << wb->hbframe << " " << std::hex << h->FEMId << std::dec << " " << femIdx;
        }
	
        heartbeatCnt[wb->hbframe].insert(femIdx);
	
	if(fDebug){
	  LOG(debug) << "============================";
	  LOG(debug) << "femIdx: "  << femIdx;
	  LOG(debug) << "HB frame : " << wb->hbframe;		
	  LOG(debug) << "timeFrameId : " << h->timeFrameId;	
	  LOG(debug) << "# of HB: " << wb->hbframe - h->timeFrameId;
	  LOG(debug) << "hbflag: "  << wb->hbflag;
	}
	
	//	heartbeatNum[(wb->hbframe+1)/(h->timeFrameId+1)].insert(femIdx);
	
	if(wb->hbframe >= (h->timeFrameId & 0xFFFF) )
	  heartbeatNum[ wb->hbframe - (h->timeFrameId & 0xFFFF) + 1 ].insert(femIdx);
	
	heartbeatFlag[wb->hbflag].insert(femIdx);
	
	tsHeartbeatCounter++;
	for (int i = 0; i < 10; i++) {
	  int t = 1 << i;
	  if (wb->hbflag & t) {
	    tsHeartbeatFlag[i]++;
	  }
	}
	
	ss << "== scaler -->" << std::endl;
	for (auto& [id, chpair] : scaler) {
	  for (auto& [ch, val] : chpair) {
	    ss << "id, ch, val: " << id << ", " << ch << ", " << val << std::endl;
	  }
	}
	ss << " --> scaler ==" << std::endl;
	if(fDebug){
	  LOG(debug) << ss.str().c_str();
	}
	fFile->Write(reinterpret_cast<const char *>(ss.str().c_str()), ss.str().length());
	
	ss.str("");
	ss.clear(std::stringstream::goodbit);
	numHB++;
	
        break;
      case Data::SpillOn: 
        if (spillOn.count(femIdx)) {
          LOG(error) << " double count of spill end in TF " << h->timeFrameId << " " << std::hex << h->FEMId << std::dec << " " << femIdx;
        }

        spillOn.insert(femIdx);

        break;
      case Data::SpillEnd: 
        if (spillEnd.count(femIdx)) {
          LOG(error) << " double count of spill end in TF " << h->timeFrameId << " " << std::hex << h->FEMId << std::dec << " " << femIdx;
        }

        spillEnd.insert(femIdx);

        break;
      case Data::Data:
	{
	  if(fDebug){
	    LOG(debug) << "FEMtype: "<< h->FEMType;
	    LOG(debug) << "femIdx: " << std::hex << femIdx << std::dec;	  
	    LOG(debug) << "AmQStrTdc : " << std::hex << h->FEMId << std::dec << " " << femIdx
		       << " receives Head of tdc data : " << std::hex << wb->head << std::dec;
	  }

	
	  auto msgBegin = reinterpret_cast<Data::Word*>(msg->GetData());	
	  auto msgSize  = msg->GetSize();
	  auto nWord    = msgSize / sizeof(uint64_t);
	
	  for(long unsigned int i = 0; i < nWord; ++i){	  
	    auto iwb = reinterpret_cast<Bits*>(msgBegin+i);	

	    if(fDebug){
	      if( (h->FEMType==1) || (h->FEMType==3) )
		LOG(debug) << "hrtdc: "   << std::hex << iwb->hrtdc << std::dec;
	      if(h->FEMType==2)	  
		LOG(debug) << "hrtdcCh: " << std::hex << iwb->hrch << std::dec;
	    }
	
	    if( (h->FEMType==1) || (h->FEMType==3) ) {
	      lrtdcCnt[iwb->ch].insert(femIdx);
	      tsScaler[iwb->ch]++;

	      if (!scaler.count(h->FEMId)){
		scaler[h->FEMId][iwb->ch] = 1;
	      }else if(!scaler[h->FEMId].count(iwb->ch)){
		scaler[h->FEMId][iwb->ch] = 1;
	      }else{
		scaler[h->FEMId][iwb->ch]++;
	      }
	      
#if 0	      
	      if (!scaler.count(femIdx)){
		scaler[femIdx][iwb->ch] = 1;
	      }else if(!scaler[femIdx].count(iwb->ch)){
		scaler[femIdx][iwb->ch] = 1;
	      }else{
		scaler[femIdx][iwb->ch]++;
	      }
#endif
	      
	    }
	    if(h->FEMType==2) {
	      hrtdcCnt[iwb->hrch].insert(femIdx);
	      tsScaler[iwb->hrch]++;
	      if (fDebug) {
		LOG(debug) << "iwb->hrch:" << iwb->hrch;
		LOG(debug) << "femIdx:"   << femIdx;
	      }

	      if (!scaler.count(h->FEMId)){
		scaler[h->FEMId][iwb->hrch] = 1;
	      }else if(!scaler[h->FEMId].count(iwb->hrch)){
		scaler[h->FEMId][iwb->hrch] = 1;
	      }else{
		scaler[h->FEMId][iwb->hrch]++;
	      }
	      
#if 0
	      if (!scaler.count(femIdx)){
		scaler[femIdx][iwb->hrch] = 1;
	      }else if(!scaler[femIdx].count(iwb->hrch)){
		scaler[femIdx][iwb->hrch] = 1;
	      }else{
		scaler[femIdx][iwb->hrch]++;
	      }
#endif	      
	    }
	    
	  }//
	}
        break;	
      default:
        LOG(error) << "AmQStrTdc : " << std::hex << h->FEMId << std::dec  << " " << femIdx
                   << " unknown Head : " << std::hex << wb->head << std::dec;
        break;
      }
    }
  }
}
//______________________________________________________________________________
bool AmQStrTdcDqmScr::HandleDataTFB(FairMQParts& parts, int index)
{
  namespace STF = SubTimeFrame;
  namespace Data = AmQStrTdc::Data;

  using Bits     = Data::Bits;

  (void)index;
  assert(parts.Size()>=2);
  Reporter::AddInputMessageSize(parts);
  {
    auto dt = std::chrono::steady_clock::now() - fPrevUpdate;
    if (std::chrono::duration_cast<std::chrono::milliseconds>(dt).count() > fUpdateIntervalInMs) {
      fPrevUpdate = std::chrono::steady_clock::now();
    }
  }
  
  // [time-stamp, femId-list]
  
  std::unordered_map<uint16_t, std::unordered_set<uint32_t>> heartbeatCnt;
  std::unordered_map<uint16_t, std::unordered_set<uint32_t>> heartbeatNum;
  std::unordered_set<uint32_t> spillEnd;
  std::unordered_set<uint32_t> spillOn;

  std::unordered_map<uint16_t, std::unordered_set<uint32_t>> timeFrameId;
  std::unordered_map<uint16_t, std::unordered_set<uint32_t>> length;

  std::unordered_map<uint16_t, std::unordered_set<uint32_t>> heartbeatFlag;

  std::unordered_map<uint16_t, std::unordered_set<uint32_t>> lrtdcCnt;
  std::unordered_map<uint16_t, std::unordered_set<uint32_t>> hrtdcCnt;

  /*
  { // for debug-begin

    std::cout << " parts size = " << parts.Size() << std::endl;
    for (int i=0; i<parts.Size(); ++i){
      const auto& msg = parts.At(i);

      LOG(debug) << " part " << i << " " << msg->GetSize() << " "
		 << std::showbase << std::hex <<  msg->GetSize() << std::noshowbase<< std::dec << std::endl;
      auto n = msg->GetSize()/sizeof(Data::Word);

      std::for_each(reinterpret_cast<Data::Word*>(msg->GetData()),
		    reinterpret_cast<Data::Word*>(msg->GetData()) + n,
		    ::HexDump{4});

    }
  } // for debug-end
  */
  
  int nmsg = 0;
  
  while(nmsg < parts.Size()){
    
    const auto& stfmsg = parts.At(nmsg);

    auto stfh    = reinterpret_cast<STF::Header*>(stfmsg->GetData());
    auto msgSize = stfh->numMessages;
    nmsg++;
    //    LOG(debug) << " STF msg size : " << msgSize ;

    if (!fFEMId.count(stfh->FEMId)) {
      LOG(debug) << "FEMTId: " << std::hex << stfh->FEMId << std::dec;
      fFEMId[stfh->FEMId] = fId;
      fModuleIp[fId] = stfh->FEMId & 0xff;
      LOG(debug) << "fId: "<< fId <<  " mIp: " << fModuleIp[fId];
      
      fId++;
    }

    auto femIdx = fFEMId[stfh->FEMId];

    timeFrameId[stfh->timeFrameId].insert(femIdx);
    length[stfh->length].insert(femIdx);
    
    // STF part
    for(unsigned int imsg = nmsg; imsg < nmsg + msgSize - 1; ++imsg){

      const auto& tmsg = parts.At(imsg);
      auto wb = reinterpret_cast<Bits*>(tmsg->GetData());

      switch (wb->head) {

      case Data::Heartbeat:

        if (heartbeatCnt.count(wb->hbframe) && heartbeatCnt.at(wb->hbframe).count(femIdx)) {
          LOG(error) << " double count of heartbeat " << wb->hbframe << " " << std::hex << stfh->FEMId << std::dec << " " << femIdx;
        }
	
        heartbeatCnt[wb->hbframe].insert(femIdx);

	if(wb->hbframe >= (stfh->timeFrameId & 0xFFFF) )
	  heartbeatNum[ wb->hbframe - (stfh->timeFrameId & 0xFFFF) + 1 ].insert(femIdx);

	heartbeatFlag[wb->hbflag].insert(femIdx);	
        break;
      case Data::SpillOn: 
	if (spillOn.count(femIdx)) {
          LOG(error) << " double count of spill end in TF " << stfh->timeFrameId << " " << std::hex << stfh->FEMId
		     << std::dec << " " << femIdx;
        }

        spillOn.insert(femIdx);
	
        break;
      case Data::SpillEnd: 
        if (spillEnd.count(femIdx)) {
          LOG(error) << " double count of spill end in TF " << stfh->timeFrameId << " " << std::hex << stfh->FEMId
		     << std::dec << " " << femIdx;
        }

        spillEnd.insert(femIdx);

        break;
      case Data::Data:
	
	if( (stfh->FEMType==1) || (stfh->FEMType==3) ) lrtdcCnt[wb->ch].insert(femIdx);
	if(stfh->FEMType==2) hrtdcCnt[wb->hrch].insert(femIdx);

        break;
      default:
        LOG(error) << "AmQStrTdc : " << std::hex << stfh->FEMId << std::dec  << " " << femIdx
                   << " unknown Head : " << std::hex << wb->head << std::dec
		   << " word = " << std::hex << wb->raw << std::dec;
        break;
      }
    }
      
    nmsg = nmsg + msgSize - 1;
    
  }
  
  return true;
}

//______________________________________________________________________________
bool AmQStrTdcDqmScr::HandleData(FairMQParts& parts, int index)
{
  namespace STF = SubTimeFrame;
  namespace Data = AmQStrTdc::Data;
  
  fSeparator = "_";
  auto scHeader = std::make_unique<Scaler::Header>();

  auto outdata = std::make_unique<std::vector<uint64_t>>();
  
  (void)index;
  assert(parts.Size()>=2);
  Reporter::AddInputMessageSize(parts);
  {
    auto dt = std::chrono::steady_clock::now() - fPrevUpdate;
    if (std::chrono::duration_cast<std::chrono::milliseconds>(dt).count() > fUpdateIntervalInMs) {
	  LOG(debug) << "== scaler --> ";
	  for (auto& [id, chpair] : scaler) {
	    
	    for (auto& [ch, val] : chpair) {
	      LOG(debug) << "id, ch, val: " << id << ", " << ch << ", " << val;

	      Scaler::Bits abit;
	      abit.id = id;
	      abit.ch = ch;
	      
	      uint64_t idch = abit.word;	      
	      //	      LOG(debug) << "IDCH: " << std::hex << idch << " word: " << abit.word;
	      
	      outdata->insert(outdata->end(), idch);
	      outdata->insert(outdata->end(), val);
	    }
	  }
	  LOG(debug) << "--> scaler ==";

      try {
	if (fPipe) {
          fTsHeartbeatCounterKey = join({"ts", fdevId, "heartbeatCounter"}, fSeparator);
          fPipe->command("ts.add", fTsHeartbeatCounterKey, "*", std::to_string(tsHeartbeatCounter));
          for (int i = 0; i < 10; i++) {
            fTsHeartbeatFlagKey[i] = join({"ts", fdevId, "heartbeatFlag", std::to_string(i)}, fSeparator);
            fPipe->command("ts.add", fTsHeartbeatFlagKey[i], "*", std::to_string(tsHeartbeatFlag[i]));
          }
          fTsHeartbeatCounterKey = join({"ts", fdevId, "heartbeatCounter"}, fSeparator);
          for (auto  itr = tsScaler.begin(); itr != tsScaler.end(); itr++) {
            fTsScalerKey = join({"ts", fdevId, "Scaler", std::to_string(itr->first)}, fSeparator);
            fPipe->command("ts.add", fTsScalerKey, "*", std::to_string(itr->second));
          }
	}
	LOG(info) << "tsHeartbeatCounter" << tsHeartbeatCounter;
	
      } catch (const std::exception& e) {
      	LOG(error) << "AmQStrTdcDqmScr " << __FUNCTION__ << " exception : what() " << e.what();
      } catch (...) {
      	LOG(error) << "AmQStrTdcDqmScr " << __FUNCTION__ << " exception : unknown ";
      }
      fPipe->exec();

      /*send data to Filesink*/
      FairMQParts outParts;
      
      scHeader->length = outdata->size()*sizeof(uint64_t) + sizeof(Scaler::Header);

      FairMQMessagePtr tmsg = MessageUtil::NewMessage(*this, std::move(scHeader));
      //

#if 0
      auto n = tmsg->GetSize()/sizeof(uint64_t);      
      LOG(debug) << "tmsg: " << n << "  " << tmsg->GetSize();      
      std::for_each(reinterpret_cast<uint64_t*>(tmsg->GetData()),
		    reinterpret_cast<uint64_t*>(tmsg->GetData()) + n,
		    ::HexDump{4});
#endif
      
      outParts.AddPart(std::move(tmsg));
      
      FairMQMessagePtr smsg; 
      
      if(outdata->size() > 0){

	smsg = MessageUtil::NewMessage(*this, std::move(outdata));

#if 0
	auto m = smsg->GetSize()/sizeof(uint64_t);	
	LOG(debug) << "smsg: " << m << "  " << smsg->GetSize();	
	std::for_each(reinterpret_cast<uint64_t*>(smsg->GetData()),
		      reinterpret_cast<uint64_t*>(smsg->GetData()) + m,
		      ::HexDump{4});
#endif
	
	outParts.AddPart(std::move(smsg));	
      }	

      
      while(Send(outParts, fOutputChannelName) < 0){
	if (NewStatePending()) {
	  LOG(info) << "Device is not RUNNING";
	  return false;
	}
	LOG(error) << "Failed to enqueue OutputChannel"; 
      }

      /*gSystem->ProcessEvents();*/
      fPrevUpdate = std::chrono::steady_clock::now();
    }
  }

  auto stfHeader = reinterpret_cast<STF::Header*>(parts.At(0)->GetData());
  auto stfId    = stfHeader->timeFrameId;

  //LOG(debug) << "**************";
  //LOG(debug) << "stdId: " << stfId;
  //LOG(debug) << "**************";

  
  // insert new STF
  if (!fDiscarded.count(stfId)) {
    // accumulate sub time frame with same STF ID
    if (!fTFBuffer.count(stfId)) {
      fTFBuffer[stfId].reserve(fNumSource);
    }
    fTFBuffer[stfId].emplace_back(STFBuffer {std::move(parts), std::chrono::steady_clock::now()});
  }
  else {
    // if received ID has been previously discarded.
    LOG(warn) << "Received part from an already discarded timeframe with id " << std::hex << stfId << std::dec;
  }
  
  // check TF-build completion
  if (!fTFBuffer.empty()) {
    // find time frame in ready
    
//    LOG(debug) << "**************";
//    LOG(debug) << "fTFBuffer.size(): " << fTFBuffer.size();
//    LOG(debug) << "**************";
//    for (auto itr = fTFBuffer.begin(); itr!=fTFBuffer.end();) {
//      auto l_stfId  = itr->first;
//      LOG(debug) << "l_stfId: " << l_stfId;
//      ++itr;
//    }
    for (auto itr = fTFBuffer.begin(); itr!=fTFBuffer.end();) {
      auto l_stfId  = itr->first;
      //LOG(debug) << "**************";
      //LOG(debug) << "In the loop, fTFBuffer.size(): " << fTFBuffer.size();
      //LOG(debug) << "l_stfId: " << l_stfId;
      //LOG(debug) << "**************";
      auto& stfs  = itr->second;
      if (static_cast<int>(stfs.size()) != fNumSource) {
	// discard incomplete time frame
	auto dt = std::chrono::steady_clock::now() - stfs.front().start;
	if (std::chrono::duration_cast<std::chrono::milliseconds>(dt).count() > fBufferTimeoutInMs) {
	  LOG(warn) << "Timeframe #" << l_stfId << " incomplete after " << fBufferTimeoutInMs << " milliseconds, discarding";
	  fDiscarded.insert(l_stfId);
	  stfs.clear();
	  LOG(warn) << "Number of discarded timeframes: " << fDiscarded.size();
	  /* HF1(fH1Map, 0, Discarded); */
	}
      } else {
	if (fFEMId.empty()) {
	  for (auto& [id, buf] : fTFBuffer) {
	    fFEMId[id] = fFEMId.size();
	  }
	}
	Check(stfs);
	stfs.clear();	
      }

      
      // remove empty buffer
      if (stfs.empty()) {
	//	LOG(debug) << "clear: "<< stfs.empty() ;
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
void AmQStrTdcDqmScr::Init()
{
    Reporter::Instance(fConfig);
}

//______________________________________________________________________________
void AmQStrTdcDqmScr::InitTask()
{
    using opt = OptionKey;
    
    fBufferTimeoutInMs  = std::stoi(fConfig->GetProperty<std::string>(opt::BufferTimeoutInMs.data()));
    LOG(debug) << "fBufferTimeoutInMs: "<< fBufferTimeoutInMs ;    

    fInputChannelName    = fConfig->GetProperty<std::string>(opt::InputChannelName.data());
    fOutputChannelName   = fConfig->GetProperty<std::string>(opt::OutputChannelName.data());    
    
    //    fNumSource         = std::stoi(fConfig->GetProperty<std::string>(opt::NumSource.data()));
    //    assert(fNumSource>=1);
    auto numSubChannels = GetNumSubChannels(fInputChannelName);
    fNumSource = 0;
    for (auto i=0u; i<numSubChannels; ++i) {
      fNumSource += GetNumberOfConnectedPeers(fInputChannelName, i);
    }
    LOG(debug) << "fNumSource: "<< fNumSource;

    LOG(debug) << " input channel : name = " << fInputChannelName
               << " num = " << GetNumSubChannels(fInputChannelName)
               << " num peer = " << GetNumberOfConnectedPeers(fInputChannelName,0);

    LOG(debug) << " output channel : name = " << fOutputChannelName;
    
    LOG(debug) << " number of source = " << fNumSource;

    fSourceType = fConfig->GetProperty<std::string>(opt::SourceType.data());    
    LOG(debug) << " type of source = " << fSourceType;

    /*
    auto server         = fConfig->GetProperty<std::string>(opt::Http.data());
    LOG(debug) << "Http Server Name: "<< server ;                
    */
    fUpdateIntervalInMs = std::stoi(fConfig->GetProperty<std::string>(opt::UpdateInterval.data()));
    LOG(debug) << "fUpdateIntervalInMs: "<< fUpdateIntervalInMs ;      

    fHbc.resize(fNumSource);

    if(fSourceType == "stf") {
      fBins = fNumSource;
      OnData(fInputChannelName, &AmQStrTdcDqmScr::HandleData);
    }else if(fSourceType == "tfb") {
      fBins = 5;      
      OnData(fInputChannelName, &AmQStrTdcDqmScr::HandleDataTFB);      
    }else {
      LOG(error) << " invalid source-type "<< std::endl;
    }

    using opt = AmQStrTdcDqmScr::OptionKey;
    std::string serverUri;
    serverUri = fConfig->GetProperty<std::string>(opt::ServerUri.data());
    if (!serverUri.empty()) {
        fClient = std::make_shared<sw::redis::Redis>(serverUri);
    }
    LOG(debug) << "serverUri: " << serverUri;
    fPipe = std::make_unique<sw::redis::Pipeline>(std::move(fClient->pipeline()));
    fdevId        = fConfig->GetProperty<std::string>("id");
    LOG(debug) << "fdevId: " << fdevId;
    fTopPrefix   = "dqm";
    //    fSeparator   = fConfig->GetProperty<std::string>("separator");
    fSeparator   = "_";
    LOG(debug) << "fSeparator: " << fSeparator;
    const auto fCreatedTimeKey = join({fTopPrefix, opt::CreatedTimePrefix.data()},   fSeparator);
    const auto fHostNameKey    = join({fTopPrefix, opt::HostnamePrefix.data()},      fSeparator);
    const auto fIpAddressKey   = join({fTopPrefix, opt::HostIpAddressPrefix.data()}, fSeparator);
    fPipe->hset(fCreatedTimeKey, fdevId, "test")
      .hset(fHostNameKey,    fdevId, fConfig->GetProperty<std::string>("hostname"))
      .hset(fIpAddressKey,   fdevId, fConfig->GetProperty<std::string>("host-ip"))
      .exec();

    Reporter::Reset();

    fFile = std::make_unique<nestdaq::FileUtil>();
    fFile->Init(fConfig->GetVarMap());
    fFile->Print();
    fFileExtension = fFile->GetExtension();
    
}

//______________________________________________________________________________
void AmQStrTdcDqmScr::PreRun()
{
    using opt = AmQStrTdcDqmScr::OptionKey;
    if (fConfig->Count(opt::RunNumber.data())) {
        fRunNumber = std::stoll(fConfig->GetProperty<std::string>(opt::RunNumber.data()));
        LOG(debug) << " RUN number = " << fRunNumber;
    }
    fFile->SetRunNumber(fRunNumber);
    fFile->ClearBranch();
    fFile->Open();
}
//______________________________________________________________________________
void AmQStrTdcDqmScr::PostRun()
{
    fDiscarded.clear();
    fTFBuffer.clear();

    int nrecv=0;
    if (fChannels.count(fInputChannelName) > 0) {
        auto n = fChannels.count(fInputChannelName);

        for (auto i = 0u; i < n; ++i) {
	    if (fDebug) {
	        LOG(debug) << " #i : "<< i;
	    }
            while(true) {

                FairMQParts part;
                if (Receive(part, fInputChannelName, i, 1000) <= 0) {
		  //                    LOG(debug) << __func__ << " no data received " << nrecv;
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

    std::stringstream ss;
    ss << "Scaler data at PostRun()" << std::endl;
    ss << "== scaler -->" << std::endl;
    for (auto& [id, chpair] : scaler) {
      for (auto& [ch, val] : chpair) {
	ss << "id, ch, val: " << id << ", " << ch << ", " << val << std::endl;
      }
    }
    ss << " --> scaler ==" << std::endl;
    fFile->Write(reinterpret_cast<const char *>(ss.str().c_str()), ss.str().length());
    ss.str("");
    ss.clear(std::stringstream::goodbit);
    fFile->Close();
    fFile->ClearBranch();

    LOG(debug) << __func__ << " done";
}

