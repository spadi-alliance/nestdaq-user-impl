#include <sstream>
#include <string>

#include <fairmq/runFairMQDevice.h>

#include "Sampler.h"

namespace bpo = boost::program_options;

//_____________________________________________________________________________
void addCustomOptions(bpo::options_description& options)
{
  options.add_options()
  ("text", bpo::value<std::string>()->default_value("uuuuueeeee"), "Text to send out")
  ("max-iterations", bpo::value<std::string>()->default_value("0"), "Maximum number of iterations of Run/ConditionalRun/OnData (0 - infinite)");

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
  LOG(debug) << "AmQ : " << AmqTdc.get_HBrate();
  
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


//_____________________________________________________________________________
void Sampler::InitTask()
{
  {
    uint64_t fFEMId = 0;
    //  auto sFEMId = fConfig->GetValue<std::string>(opt::FEMId.data());
    std::string sFEMId("192.168.10.16");
    std::istringstream istrst(sFEMId);
    std::string token;
    int ik = 3;
    while(std::getline(istrst, token, '.')) {
      if(ik < 0) break;

      uint32_t ipv = (std::stoul(token) & 0xff) << (8*ik);
      fFEMId |= ipv;
      --ik;
    }
    LOG(debug) << "FEM ID    " << std::hex << fFEMId << std::dec << std::endl;
    LOG(debug) << "FEM Magic " << std::hex << fem_info_.magic << std::dec << std::endl;

    fem_info_.FEMId = fFEMId;
    fem_info_.FEMType = 0;
  }
  
  
  unsigned char* fbuf = new uint8_t[sizeof(fem_info_)];
  memcpy(fbuf, &fem_info_, sizeof(fem_info_));

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

 
  PrintConfig(fConfig, "channel-config", __PRETTY_FUNCTION__);
  PrintConfig(fConfig, "chans.", __PRETTY_FUNCTION__);

  fText = fConfig->GetProperty<std::string>("text");
  fMaxIterations = std::stoull(fConfig->GetProperty<std::string>("max-iterations"));

  AmqTdc.set_WordCount(fnWordCount);
  
  LOG(info) << "Word Counts: "<< AmqTdc.get_WCount() ; 
  max_cycle_count = AmqTdc.get_HBrate();

  LOG(info) << "Heartbeat Rate: "<< max_cycle_count ;
  
}


int Sampler::GeneCycle(uint8_t* buffer){
  //==== data generator ===== 
  int ByteSize = AmqTdc.packet_generator(buffer);

  //  std::cout << "cycle count: " << cycle_count << std::endl;
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
}

//_____________________________________________________________________________
void Sampler::PreRun()
{

  LOG(debug) << __FUNCTION__;
}

//_____________________________________________________________________________
void Sampler::Run()
{
  LOG(debug) << __FUNCTION__;
}
