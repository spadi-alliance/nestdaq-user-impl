#include <thread>
#include <future>
#include <TROOT.h>
#include <TSystem.h>
#include <TStyle.h>
#include <TDirectory.h>
#include <TCanvas.h>
#include <TH1.h>
#include <TH2.h>
#include <TProfile.h>
#include <TString.h>
#include <iostream>
#include <boost/format.hpp>
#include <cmath>

#include <fairmq/runDevice.h>

#include "SubTimeFrameHeader.h"
#include "TimeFrameHeader.h"
#include "STFBuilder.h"
#include "AmQStrTdcData.h"
#include "utility/Reporter.h"
#include "utility/HexDump.h"
#include "utility/MessageUtil.h"

#include "AmQStrTdcDqm.h"


namespace bpo = boost::program_options;

//______________________________________________________________________________

void addCustomOptions(bpo::options_description& options)
{
  using opt = AmQStrTdcDqm::OptionKey;
  options.add_options()
    (opt::NumSource.data(),          bpo::value<std::string>()->default_value("1"), "Number of source endpoint")
    (opt::BufferTimeoutInMs.data(),  bpo::value<std::string>()->default_value("100000"), "Buffer timeout in milliseconds")
    (opt::InputChannelName.data(),   bpo::value<std::string>()->default_value("in"), "Name of the input channel")
    (opt::Http.data(),               bpo::value<std::string>()->default_value("http:5999"), "http engine and port, etc.")
    (opt::UpdateInterval.data(),     bpo::value<std::string>()->default_value("1000"), "Canvas update rate in milliseconds")
    ;
}


//______________________________________________________________________________
std::unique_ptr<fair::mq::Device> getDevice(fair::mq::ProgOptions& /*config*/)
{
   return std::make_unique<AmQStrTdcDqm>();
}


TH1* HF1(std::unordered_map<std::string, TH1*>& h1Map, 
         int id, double x, double w=1.0)
{
  //  auto name = TString::Form("h%04d", id);
  boost::format name("h%04d");
  name % id;
  auto h    = h1Map.at(name.str());
  if (h)  h->Fill(x, w);
  

  return h;
}


//______________________________________________________________________________
AmQStrTdcDqm::AmQStrTdcDqm()
    : FairMQDevice()
{
    gROOT->SetStyle("Plain");
    int fontid = 132;

    gStyle->SetStatFont(fontid);
    gStyle->SetLabelFont(fontid, "XYZ");
    gStyle->SetTitleFont(fontid, "XYZ");
    gStyle->SetTextFont(fontid);
    gStyle->SetOptStat("ouirmen");
    gStyle->SetOptFit(true);

    gStyle->SetPadGridX(true);
    gStyle->SetPadGridY(true);

}

//______________________________________________________________________________
void AmQStrTdcDqm::Check(std::vector<STFBuffer>&& stfs)
{
  namespace TF   = TimeFrame;  
  namespace STF  = SubTimeFrame;
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

  
  for (int istf=0; istf<fNumSource; ++istf) {
    
    auto& [stf, t] = stfs[istf];
    auto h = reinterpret_cast<STF::Header*>(stf.At(0)->GetData());
    auto nmsg  = stf.Size();

    if(fDebug){
      LOG(debug) << "nmsg: " << nmsg;      
      LOG(debug) << "magic: " << std::hex << h->magic << std::dec;
      LOG(debug) << "timeFrameId: " << h->timeFrameId;
      LOG(debug) << "FEMType: " << h->FEMType;
      LOG(debug) << "FEMTId: " << std::hex << h->FEMId << std::dec;
      LOG(debug) << "length: " << h->length;
      LOG(debug) << "time_sec: " << h->time_sec;
      LOG(debug) << "time_sec: " << h->time_usec;
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

    int nspillOn  = 0;
    int nspillOff = 0;
    
    for (int imsg=1; imsg<nmsg; ++imsg) {
      const auto& msg = stf.At(imsg);
      auto wb =  reinterpret_cast<Bits*>(msg->GetData());
      if(fDebug){
	LOG(debug) << " word =" << std::hex << wb->raw << std::dec;
	LOG(debug) << " head =" << std::hex << wb->head << std::dec;
      }
      
      switch (wb->head) {
      case Data::Heartbeat:
	if(fDebug){
	  LOG(debug) << "hbframe: " << std::hex << wb->hbframe << std::dec;
	  LOG(debug) << "hbspill#: " << std::hex << wb->hbspilln << std::dec;
	  LOG(debug) << "hbfalg: " << std::hex << wb->hbflag << std::dec;
	  LOG(debug) << "header: " << std::hex << wb->htype << std::dec;
	  LOG(debug) << "femIdx: " << std::hex << femIdx << std::dec;
	}

        if (heartbeatCnt.count(wb->hbframe) && heartbeatCnt.at(wb->hbframe).count(femIdx)) {
          LOG(error) << " double count of heartbeat " << wb->hbframe << " " << std::hex << h->FEMId << std::dec << " " << femIdx;
        }
	
        heartbeatCnt[wb->hbframe].insert(femIdx);

	if(false){
	  LOG(debug) << "============================";
	  LOG(debug) << "femIdx: "  << femIdx;
	  LOG(debug) << "HB frame : " << wb->hbframe;		
	  LOG(debug) << "timeFrameId : " << h->timeFrameId;	
	  LOG(debug) << "# of HB: " << wb->hbframe - h->timeFrameId;
	  //	  LOG(debug) << "hbflag: "  << wb->hbflag;
	}

	//	heartbeatNum[(wb->hbframe+1)/(h->timeFrameId+1)].insert(femIdx);

	if(wb->hbframe >= h->timeFrameId)
	  heartbeatNum[ wb->hbframe - h->timeFrameId + 1 ].insert(femIdx);

	heartbeatFlag[wb->hbflag].insert(femIdx);
	

        break;
      case Data::SpillOn: 
        if (spillOn.count(femIdx) && (nspillOn > 1) ) {
          LOG(error) << " double count of spill end in TF " << h->timeFrameId << " " << std::hex << h->FEMId << std::dec << " " << femIdx;
        }else if (spillOn.count(femIdx) && nspillOn == 1) {
	  nspillOn = 0;
	}

        spillOn.insert(femIdx);
	nspillOn++;
	
        break;
      case Data::SpillEnd: 
        if (spillEnd.count(femIdx) && (nspillOff > 1) ) {
          LOG(error) << " double count of spill end in TF " << h->timeFrameId << " " << std::hex << h->FEMId << std::dec << " " << femIdx;
        }else if(spillEnd.count(femIdx) && (nspillOff == 1) ) {
	  nspillOff = 0;
	}

        spillEnd.insert(femIdx);
	nspillOff++;
	
        break;
      case Data::Data:
	
	if(fDebug){
	  LOG(debug) << "FEMtype: "<< h->FEMType;
	  LOG(debug) << "femIdx: " << std::hex << femIdx << std::dec;	  
	  LOG(debug) << "AmQStrTdc : " << std::hex << h->FEMId << std::dec << " " << femIdx
		     << " receives Head of tdc data : " << std::hex << wb->head << std::dec;
	}

	if(fDebug){
	  if(h->FEMType==1)
	    LOG(debug) << "hrtdc: "   << std::hex << wb->hrtdc << std::dec;

	  if(h->FEMType==2)	  
	    LOG(debug) << "hrtdcCh: " << std::hex << wb->hrch << std::dec;
	}

	
	if(h->FEMType==1) lrtdcCnt[wb->ch].insert(femIdx);
	if(h->FEMType==2) hrtdcCnt[wb->hrch].insert(femIdx);
	
        break;
      default:
        LOG(error) << "AmQStrTdc : " << std::hex << h->FEMId << std::dec  << " " << femIdx
                   << " unknown Head : " << std::hex << wb->head << std::dec;
        break;
      }
    }
  }

  for (const auto& [t, fems] : heartbeatCnt) {
    for (auto femId : fems) {
      //      HF1(fH1Map, 1, femId, fModuleIp[femId])
      HF1(fH1Map, 1, femId);
      HF1(fH1Map, 100+femId, t);
    }
  }

  for (const auto femId : spillOn) {
    HF1(fH1Map, 2, femId);
  }

  for (const auto femId : spillEnd) {
    HF1(fH1Map, 3, femId);
  }

  for (const auto& [t, fems] : heartbeatFlag) {
    for (auto femId : fems) {
      HF1(fH1Map, 4, femId, t);
    }
  }

  for (const auto& [t, fems] : heartbeatNum) {

    for (auto femId : fems) {
      HF1(fH1Map, 150+femId, t);
    }
  }

  for (const auto& [t, fems] : timeFrameId) {
    for (auto femId : fems) {
      HF1(fH1Map, 200+femId, t);
    }
  }

  for (const auto& [t, fems] : length) {
    for (auto femId : fems) {
      HF1(fH1Map, 300+femId, t);
    }
  }
  
  
  for (const auto& [t, fems] : lrtdcCnt) {
    for (auto femId : fems) {
      HF1(fH1Map, 2000+femId, t);
    }
  }

  for (const auto& [t, fems] : hrtdcCnt) {
    for (auto femId : fems) {
      HF1(fH1Map, 2100+femId, t);
    }
  }
 

  /*
  bool mismatch=false;
  for (const auto& [t, fems] : hb_or_err) {
    for (auto femId : fems) {
      HF1(fH1Map, 3, femId);
      HF1(fH1Map, 300+femId, t);
  }
  std::cout << "out of switch3"<< std::endl;
  
  for (const auto femId : spillEnd) {
    HF1(fH1Map, 4, femId);
  }
  std::cout << "out of switch4"<< std::endl;
  if (mismatch) { 
    HF1(fH1Map, 0, Mismatch);
    return;
  }
  std::cout << "out of switch5"<< std::endl;  
  */

  //  LOG(debug) << __FUNCTION__ << " : " << __LINE__;

  HF1(fH1Map, 0, OK);
}

//______________________________________________________________________________
bool AmQStrTdcDqm::HandleData(FairMQParts& parts, int index)
{

  namespace TF  = TimeFrame;
  namespace STF = SubTimeFrame;
  namespace Data = AmQStrTdc::Data;  

  (void)index;
  assert(parts.Size()>=2);
  Reporter::AddInputMessageSize(parts);
  {
    auto dt = std::chrono::steady_clock::now() - fPrevUpdate;
    if (std::chrono::duration_cast<std::chrono::milliseconds>(dt).count() > fUpdateIntervalInMs) {

      gSystem->ProcessEvents();
      fPrevUpdate = std::chrono::steady_clock::now();
    }
  }
        
  auto stfHeader = reinterpret_cast<STF::Header*>(parts.At(0)->GetData());
  auto stfId    = stfHeader->timeFrameId;

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
    for (auto itr = fTFBuffer.begin(); itr!=fTFBuffer.end();) {
      auto l_stfId  = itr->first;
      auto& stfs  = itr->second;
      if (static_cast<int>(stfs.size()) != fNumSource) {
	// discard incomplete time frame
	auto dt = std::chrono::steady_clock::now() - stfs.front().start;
	if (std::chrono::duration_cast<std::chrono::milliseconds>(dt).count() > fBufferTimeoutInMs) {
	  LOG(warn) << "Timeframe #" << l_stfId << " incomplete after " << fBufferTimeoutInMs << " milliseconds, discarding";
	  fDiscarded.insert(l_stfId);
	  stfs.clear();
	  LOG(warn) << "Number of discarded timeframes: " << fDiscarded.size();
	  HF1(fH1Map, 0, Discarded);
	}
      } else {
	if (fFEMId.empty()) {
	  for (auto& [id, buf] : fTFBuffer) {
	    fFEMId[id] = fFEMId.size();
	  }
	}
	Check(std::move(stfs));
      }
      
      // remove empty buffer
      if (stfs.empty()) {
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
void AmQStrTdcDqm::Init()
{
    Reporter::Instance(fConfig);
}

//______________________________________________________________________________
void AmQStrTdcDqm::InitServer(std::string_view server)
{

  if (!fServer) {
    LOG(warn) << "THttpServer = " << server;
    fServer = std::make_unique<THttpServer>(server.data());
    if(!fServer)
      LOG(debug) << "fServer failed..";
  }

    auto HB1 = [this](int id, std::string_view title, int nbin, double xmin, double xmax, std::string_view folder) -> TH1* {
        //auto name = TString::Form("h%04d", id);

        boost::format name("h%04d");
        name % id;
        std::string hist_name = boost::str(name);

        if (gDirectory->Get(hist_name.c_str())) return nullptr;

        auto h = new TH1F(hist_name.c_str(), title.data(), nbin, xmin, xmax);
        h->SetDirectory(0);
        fH1Map.emplace(hist_name, h);


    if (folder.empty()) {
      fServer->Register("/amqdqm",  h);
    } else {
      boost::format dirname("/amqdqm/%s");
      dirname % folder.data();
      std::string dir_name = boost::str(dirname);
      fServer->Register(dir_name.c_str(), h);
    }

    LOG(debug) << " create histogram " << name;
    return h;
    };

#if 0
    auto createCanvas = [](std::string_view name, std::string_view title, int nx, int ny) -> TCanvas* {
        auto l = gROOT->GetListOfCanvases();
        if (dynamic_cast<TCanvas*>(l->FindObject(name.data()))) return nullptr;

        LOG(debug) << " create canvas " << name;
        auto c = new TCanvas(name.data(), title.data());
        c->Divide(nx, ny);
        return c;
    };
#endif
    
    HB1(0, "status",                 3, -0.5, 3-0.5, "");
    HB1(1, "# Heatbeat",             fNumSource+2, -1, fNumSource+1, "");
    HB1(2, "# spill On",             fNumSource+2, -1, fNumSource+1, "");
    HB1(3, "# spill end",            fNumSource+2, -1, fNumSource+1, "");
    HB1(4, "# HB Delimiter Flag",    fNumSource+2, -1, fNumSource+1, "");
    //    HB1(5, "# Spill Delimiter",      fNumSource+2, -1, fNumSource+1, "");


    for (int i=0; i<fNumSource; ++i) {
      //    for (int i=0; i<10; ++i) {
        boost::format name("heartbeat_%d");
        name % i;
        std::string hname = boost::str(name);
        HB1(100+i, hname.c_str(), 500, -0.5, 500-0.5, "Heartbeat");
    }

    for (int i=0; i<fNumSource; ++i) {
        boost::format name("number_of_hb_%d");
        name % i;
        std::string hname = boost::str(name);
        HB1(150+i, hname.c_str(), 10, 0, 10, "#OfHB_STF");
    }
    
    for (int i=0; i<fNumSource; ++i) {
        boost::format name("timeFrameId_%d");
        name % i;
        std::string hname = boost::str(name);
        HB1(200+i, hname.c_str(), 500, -0.5, 500-0.5, "TimeFrameId");
    }

    for (int i=0; i<fNumSource; ++i) {
        boost::format name("length_%d");
        name % i;
        std::string hname = boost::str(name);
        HB1(300+i, hname.c_str(), 1000, -0.5, 2000-0.5, "Length");
    }
    

    for (int i=0; i<fNumSource; ++i) {
    //    for (int i=0; i<10; ++i) {
        boost::format name("lrtdcCnt_%d");
        name % i;
        std::string hname = boost::str(name);
        HB1(2000+i, hname.c_str(), 131, -1.5, 130-0.5, "LR-TDC Counts");
    }

    for (int i=0; i<fNumSource; ++i) {
    //    for (int i=0; i<10; ++i) {
        boost::format name("hrtdcCnt_%d");
        name % i;
        std::string hname = boost::str(name);
        HB1(2100+i, hname.c_str(), 131, -1.5, 130-0.5, "HR-TDC Counts");
    }
    
    gSystem->ProcessEvents();
    fPrevUpdate = std::chrono::steady_clock::now();
}

//______________________________________________________________________________
void AmQStrTdcDqm::InitTask()
{
    using opt = OptionKey;
    
    fBufferTimeoutInMs  = std::stoi(fConfig->GetProperty<std::string>(opt::BufferTimeoutInMs.data()));
    LOG(debug) << "fBufferTimeoutInMs: "<< fBufferTimeoutInMs ;    

    fInputChannelName   = fConfig->GetProperty<std::string>(opt::InputChannelName.data());

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

    LOG(debug) << " number of source = " << fNumSource;

    
    auto server         = fConfig->GetProperty<std::string>(opt::Http.data());
    LOG(debug) << "Http Server Name: "<< server ;                
    fUpdateIntervalInMs = std::stoi(fConfig->GetProperty<std::string>(opt::UpdateInterval.data()));
    LOG(debug) << "fUpdateIntervalInMs: "<< fUpdateIntervalInMs ;      

    fHbc.resize(fNumSource);

    OnData(fInputChannelName, &AmQStrTdcDqm::HandleData);

    InitServer(server);

    Reporter::Reset();
}

//______________________________________________________________________________
void AmQStrTdcDqm::PostRun()
{
    fDiscarded.clear();

    int nrecv = 0;

    while(true) {
        FairMQMessagePtr msg(NewMessage());

        if (Receive(msg, fInputChannelName) <= 0) {
            LOG(debug) << __func__ << " no data received " << nrecv;
            ++nrecv;
            if (nrecv>10) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        } else {
            LOG(debug) << __func__ << " print data";
            //      HandleData(msg, 0);
        }
    }

    LOG(debug) << __func__ << " done";

}

