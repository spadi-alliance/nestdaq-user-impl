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

#include <fairmq/runFairMQDevice.h>

#include "SubTimeFrameHeader.h"
#include "STFBuilder.h"
#include "AmQStrTdcData.h"
#include "utility/Reporter.h"
#include "AmQStrTdcDqm.h"

TH1* HF1(std::unordered_map<std::string, TH1*>& h1Map, 
         int id, double x, double w=1.0)
{
  //  auto name = TString::Form("h%04d", id);
  boost::format name("h%06d");
  name % id;
  auto h    = h1Map.at(name.str());
  if (h) h->Fill(x, w);

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
  namespace STF = SubTimeFrame;

  // LOG(debug) << "Check"; 
  namespace Data = AmQStrTdc::Data;
  //using Word     = Data::Word;
  using Bits     = Data::Bits;

  // [time-stamp, femId-list]
  std::unordered_map<uint16_t, std::unordered_set<uint32_t>> heartbeatCnt;
  std::unordered_map<uint16_t, std::unordered_set<uint32_t>> errorRecoveryCnt;
  std::unordered_map<uint16_t, std::unordered_set<uint32_t>> hb_or_err;
  std::unordered_set<uint32_t> spillEnd;

  for (int istf=0; istf<fNumSource; ++istf) {
    auto& [stf, t] = stfs[istf];
    auto h = reinterpret_cast<STF::Header*>(stf.At(0)->GetData());
    auto nmsg  = stf.Size();
    //auto len   = h->length - sizeof(STF::Header); // data size including tdc measurement
    //auto nword = len/sizeof(Word);
    //auto nhit  = nword - nmsg;
    auto femIdx = fFEMId[h->FEMId];

    for (int imsg=1; imsg<nmsg; ++imsg) {
      const auto& msg = stf.At(imsg);
      auto wb =  reinterpret_cast<Bits*>(msg->GetData());
      // LOG(debug) << " word =" << std::hex << wb->raw << std::dec;
      switch (wb->head) {
      case Data::Heartbeat:
        if (heartbeatCnt.count(wb->hbframe) && heartbeatCnt.at(wb->hbframe).count(femIdx)) {
          LOG(error) << " double count of heartbeat " << wb->hbframe << " " << std::hex << h->FEMId << std::dec << " " << femIdx;
        }
        heartbeatCnt[wb->hbframe].insert(femIdx);
        hb_or_err[wb->hbframe].insert(femIdx);
        break;
      case Data::SpillEnd: 
        if (spillEnd.count(femIdx)) {
          LOG(error) << " double count of spill end in TF " << h->timeFrameId << " " << std::hex << h->FEMId << std::dec << " " << femIdx;
        }
        spillEnd.insert(femIdx);
        break;
      case Data::Data: 
        LOG(error) << "AmQStrTdc : " << std::hex << h->FEMId << std::dec << " " << femIdx
                   << " receives Head of tdc data : " << std::hex << wb->head << std::dec;
        break;
      default:
        LOG(error) << "AmQStrTdc : " << std::hex << h->FEMId << std::dec  << " " << femIdx
                   << " unknown Head : " << std::hex << wb->head << std::dec;
        break;
      }
    }
  }

  for (const auto& [t, fems] :  heartbeatCnt) {
    for (auto femId : fems) {
      HF1(fH1Map, 1, femId);
      HF1(fH1Map, 100+femId, t);
    }
  }

  for (const auto& [t, fems] : errorRecoveryCnt) {
    for (auto femId : fems) {
      HF1(fH1Map, 2, femId);
      HF1(fH1Map, 200+femId, t);
    }
  }

  bool mismatch=false;
  for (const auto& [t, fems] : hb_or_err) {
    for (auto femId : fems) {
      HF1(fH1Map, 3, femId);
      HF1(fH1Map, 300+femId, t);
    }
    if (static_cast<int>(fems.size()) != fNumSource) {
      mismatch = true;
    }
  }

  for (const auto femId : spillEnd) {
    HF1(fH1Map, 4, femId);
  }

  if (mismatch) { 
    HF1(fH1Map, 0, Mismatch);
    return;
  }
  
        LOG(debug) << __FUNCTION__ << " : " << __LINE__;
  HF1(fH1Map, 0, OK);
}

//______________________________________________________________________________
bool AmQStrTdcDqm::HandleData(FairMQParts& parts, int index)
{
  namespace STF = SubTimeFrame;

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

  LOG(debug) << "Received data n msgs = " << parts.Size();

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
      fServer->Register("/huldqm",  h);
    } else {
      boost::format dirname("/huldqm/%s");
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

  HB1(0, "status",           3,          -0.5, 3-0.5, "");
  HB1(1, "# Heatbeat",       fNumSource, -0.5, fNumSource-0.5, "");
  HB1(2, "# HB + ERR",       fNumSource, -0.5, fNumSource-0.5, "");
  HB1(3, "# spill end",      fNumSource, -0.5, fNumSource-0.5, "");

  for (int i=0; i<fNumSource; ++i) {
    boost::format name("heartbeat_%d");
    name % i;
    std::string hname = boost::str(name);

    HB1(100+i, hname.c_str(), 21000, -0.5, 21000-0.5, "Heartbeat");
  }

  for (int i=0; i<fNumSource; ++i) {
    boost::format name("HB + ERR %d");
    name % i;
    std::string hname = boost::str(name);

    HB1(200+i, hname.c_str(), 21000, -0.5, 21000-0.5, "HB+ERR");
  }

  gSystem->ProcessEvents();
  fPrevUpdate = std::chrono::steady_clock::now();
}

//______________________________________________________________________________
void AmQStrTdcDqm::InitTask()
{
  using opt = OptionKey;
  fNumSource         = fConfig->GetProperty<int>(opt::NumSource.data());
  assert(fNumSource>=1);

  fBufferTimeoutInMs  = fConfig->GetProperty<int>(opt::BufferTimeoutInMs.data());
  fInputChannelName   = fConfig->GetProperty<std::string>(opt::InputChannelName.data());
  auto server         = fConfig->GetProperty<std::string>(opt::Http.data());
  fUpdateIntervalInMs = fConfig->GetProperty<int>(opt::UpdateInterval.data());

  fHbc.resize(fNumSource);

  OnData(fInputChannelName, &AmQStrTdcDqm::HandleData);

  InitServer(server);

  Reporter::Reset();
}

//______________________________________________________________________________
void AmQStrTdcDqm::PostRun()
{
  fDiscarded.clear();
}


//______________________________________________________________________________
namespace bpo = boost::program_options;

void addCustomOptions(bpo::options_description& options)
{
  using opt = AmQStrTdcDqm::OptionKey;
  options.add_options()
    (opt::NumSource.data(),          bpo::value<int>()->default_value(1),          \
     "Number of source endpoint")
    (opt::BufferTimeoutInMs.data(),  bpo::value<int>()->default_value(100000),     \
     "Buffer timeout in milliseconds")
    (opt::InputChannelName.data(),   bpo::value<std::string>()->default_value("in")\
     ,                       "Name of the input channel")
    (opt::Http.data(),               bpo::value<std::string>()->default_value("http\
:8888?monitoring=500"), "http engine and port, etc.")
    (opt::UpdateInterval.data(),     bpo::value<int>()->default_value(1000),       \
     "Canvas update rate in milliseconds")
    ;
}

//______________________________________________________________________________
FairMQDevicePtr getDevice(const FairMQProgOptions&)
{
   return new AmQStrTdcDqm;
}

