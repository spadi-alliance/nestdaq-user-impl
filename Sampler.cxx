#include <sstream>
#include <string>
#include <iostream>
#include <fairmq/runFairMQDevice.h>

#include "Sampler.h"

namespace bpo = boost::program_options;

//_____________________________________________________________________________
void addCustomOptions(bpo::options_description& options)
{
  using opt = Sampler::OptionKey;
  options.add_options()
    (opt::Text.data(),           bpo::value<std::string>()->default_value("AMANEQ Emulator"), "Text to send out")
    (opt::IpSiTCP.data(),        bpo::value<std::string>()->default_value("0"),   "Ip Address for FEMId")
    (opt::TdcType.data(),        bpo::value<std::string>()->default_value("0"),   "TDC type: HR=2, LR=1")
    (opt::MaxNumberHBF.data(),   bpo::value<std::string>()->default_value("50"), "# of HeartBeat Frame")
    (opt::SendInfo.data(),  bpo::value<std::string>()->default_value("0"),   "Flag to send FEM Info")
    ("max-iterations",      bpo::value<std::string>()->default_value("0"), "Maximum number of iterations of Run/ConditionalRun/OnData (0 - infinite)");    

} 

//_____________________________________________________________________________
FairMQDevicePtr getDevice(const fair::mq::ProgOptions& /*config*/)
{
  return new Sampler(); 
}

//_____________________________________________________________________________
void PrintConfig(const fair::mq::ProgOptions* config, std::string_view name, std::string_view funcname)
{
  auto c = config->GetPropertiesAsStringStartingWith(name.data());
  std::ostringstream ss;
  ss << funcname << "\n\t " << name << "\n";
  for (const auto &[k, v] : c) {
    ss << "\t key = " << k << ", value = " << v << "\n";
  }
  LOG(debug) << ss.str();
} 


//_____________________________________________________________________________
Sampler::Sampler()
  : fText()
  , fMaxIterations(0)
  , fNumIterations(0)
{
  LOG(debug) << "Sampler : AmQ Emulator";
  
}

//_____________________________________________________________________________
//Sampler::~Sampler()
//{
  // unsubscribe to property change  
//  fConfig->UnsubscribeAsString("Sampler");
//  LOG(debug) << "Sampler : bye";
//}

//_____________________________________________________________________________
void Sampler::Init()
{
  // subscribe to property change  
//  fConfig->SubscribeAsString("Sampler", [](const std::string& key, std::string value){
//    LOG(debug) << "Sampler (subscribe) : key = " << key << ", value = " << value;
//  });
  PrintConfig(fConfig, "channel-config", __PRETTY_FUNCTION__);
  PrintConfig(fConfig, "chans.", __PRETTY_FUNCTION__);
}

//----------------------------------------------------------------------------
void Sampler::SendFEMInfo() {
  
  {
    uint64_t fFEMId = 0;
    //    std::string sFEMId("192.168.10.16");
    std::string sFEMId(fIpSiTCP);
    std::istringstream istrst(sFEMId);
    std::string token;
    int ik = 3;
    while(std::getline(istrst, token, '.')) {
      if(ik < 0) break;

      uint32_t ipv = (std::stoul(token) & 0xff) << (8*ik);
      fFEMId |= ipv;
      --ik;
    }
    LOG(debug) << "FEM ID    " << std::hex << fFEMId << std::dec;
    LOG(debug) << "FEM Magic " << std::hex << fem_info_.magic << std::dec;

    fem_info_.FEMId = fFEMId;
    fem_info_.FEMType = tdc_type;
  }
  
  
  unsigned char* fbuf = new uint8_t[sizeof(fem_info_)];
  uint8_t buf[8] = {0};
  
  for(int i = 0; i < 8; i++){
    buf[7-i] = (fem_info_.magic >> 8*(7-i) ) & 0xff;
  }
  memcpy(fbuf, &buf, sizeof(char)*8);

  for(int i = 0; i < 8; i++){
    if(i < 4){
      buf[7-i] = (fem_info_.FEMId >> 8*(3-i)) & 0xff;
    }else{
      buf[7-i] = (fem_info_.FEMType >> 8*(7-i)) & 0xff;
    }
  }
  memcpy(&fbuf[8], &buf, sizeof(char)*8);

  uint8_t resv[8] = {0};
  memcpy(&fbuf[16], &resv, sizeof(char)*8);

  //memcpy(fbuf, &fem_info_, sizeof(fem_info_));  

  FairMQMessagePtr initmsg( NewMessage((char*)fbuf,
				   fnByte*3,
				   [](void* object, void*)
				   {delete [] static_cast<uint8_t*>(object);}
				   )
			);

  LOG(info) << "Sending FEMInfo \"" << sizeof(fem_info_) << "\"";

  int count=0;
  while(true){

    if (Send(initmsg, "data") < 0) {
      LOG(warn) << " fail to send FEMInfo :  " << count;
      count++;
    }else{
      LOG(debug) << " send FEMInfo :  "  << count;
      break;
    } 
  }

}
//_____________________________________________________________________________
void Sampler::InitTask()
{ 
  using opt = OptionKey;

  // for TDC Type for module (LR, HR) (1, 2)
  auto type = fConfig->GetProperty<std::string>(opt::TdcType.data());
  int mtype = stoi(type);

  if( mtype > 0 ){
    tdc_type = mtype;
    LOG(info) << "tdc type: " << tdc_type ;
  }else{
    tdc_type = 1;
    LOG(info) << "can not find param. for tdc type." << mtype ; 
  }

  // for Modue IP Address
  fIpSiTCP = fConfig->GetValue<std::string>(opt::IpSiTCP.data());
  LOG(info) << "TPC IP: " << fIpSiTCP;

  // A flag for Sending the FEM info or not
  auto flag = fConfig->GetValue<std::string>(opt::SendInfo.data());
  int nflag = stoi(flag);
  LOG(info) << "nflag: " << nflag;

  if(nflag>0)
    SendFEMInfo();

  // channel info
  PrintConfig(fConfig, "channel-config", __PRETTY_FUNCTION__);
  PrintConfig(fConfig, "chans.", __PRETTY_FUNCTION__);

  fText = fConfig->GetProperty<std::string>(opt::Text.data());
  fMaxIterations = std::stoull(fConfig->GetProperty<std::string>("max-iterations"));
 
  amqTdc.set_WordCount(fnWordCount);
  LOG(info) << "Word Counts: "<< amqTdc.get_WCount() ; 

  // for # of Heartbeat frame per one cycle 
  auto maxHBF = fConfig->GetProperty<std::string>(opt::MaxNumberHBF.data());
  int fMaxNumberHBF = stoi(maxHBF);

  if( fMaxNumberHBF > 0 ){
    fMaxHBF = fMaxNumberHBF;
    LOG(info) << "MaxHBF from param: "<< fMaxNumberHBF ;     
  }else{
    LOG(info) << "no param for MaxHBF: "<< fMaxNumberHBF ;     
  }  
  amqTdc.set_HBrate(fMaxHBF);
  LOG(info) << "Heartbeat Rate: "<< amqTdc.get_HBrate(); 

  LOG(info) << "SEQ: "<< amqTdc.get_nseq(); 
}

int Sampler::GeneCycle(uint8_t* buffer){
  //==== data generator ===== 
  int ByteSize = amqTdc.packet_generator(tdc_type,buffer);

  if(ByteSize == fnWordCount*fnByte)
    return ByteSize;
  else
    return -2;
}

//_____________________________________________________________________________
bool Sampler::ConditionalRun()
{
  bool fShow = false;

  int nByteSize = 0;
  //  int8_t* buffer = new uint8_t[fnByte*fnWordCount];
  unsigned char* buffer = new uint8_t[fnByte*fnWordCount];

  while( -2 == ( nByteSize = GeneCycle(buffer) )){ 
    
    //LOG(info) << "Spill off: "<< cycle_count; 
    continue;
  }

  if(fShow){
    std::cout << "Out of cycle "<< std::endl;
    for(int ia = 0; ia < fnWordCount*fnByte; ia++){
      printf("%02x ", buffer[ia]);
      if( ((ia+1)%8) == 0 ){
	printf("\n");
      }
    }
  }

  sleep(1);

  FairMQMessagePtr msg( NewMessage((char*)buffer,
				   //				  fnByte*nword
				  nByteSize,
				  [](void* object, void*)
				  {delete [] static_cast<uint8_t*>(object);}
				  )
		       );
  

  LOG(info) << "Sending \"" << nByteSize << "\"";

  if (Send(msg, "data") < 0) {
    LOG(warn) << " event:  " << fNumIterations;
    return false;
  } else { 
    ++fNumIterations;
    if (fMaxIterations > 0 && fNumIterations >= fMaxIterations) {
      LOG(info) << "Configured maximum number of iterations reached. Leaving RUNNING state. " << fNumIterations << " / " << fMaxIterations;
      return false;
    }
  }
  LOG(info) << " processed events:  " << fNumIterations;
  

  return true;
}

//_____________________________________________________________________________
void Sampler::PostRun()
{
  LOG(debug) << __FUNCTION__;
  fNumIterations = 0;
  amqTdc.Delete();
}

//_____________________________________________________________________________
void Sampler::PreRun()
{

  LOG(debug) << __FUNCTION__;
  amqTdc.Init();
  std::cout << "NSEQ: "<< amqTdc.get_nseq() << std::endl;
  
}

//_____________________________________________________________________________
void Sampler::Run()
{
  LOG(debug) << __FUNCTION__;
}
