#include <vector>
#include <chrono>
#include <algorithm>
#include <iostream>

#include "utility/MessageUtil.h"
#include "utility/Reporter.h"
#include "AmQStrTdcSampler.h"

#include "utility/network.hh"
#include "utility/rbcp.h"
#include "utility/UDPRBCP.hh"

//______________________________________________________________________________

AmQStrTdcSampler::AmQStrTdcSampler()
  : FairMQDevice()
{}

//______________________________________________________________________________
bool AmQStrTdcSampler::ConditionalRun()
{
  using namespace std::chrono_literals;

  int n_word = 0;
  uint8_t* buffer = new uint8_t[fnByte*fnWordPerCycle]{};

  while( -1 == ( n_word = Event_Cycle(buffer))) continue;

  if(n_word == -4){
    for(int i = 0; i<fnWordPerCycle; ++i){

      //      if((buffer[fnByte*i+4] & 0xff) == 0x50){
      if((buffer[fnByte*i+7] & 0xff) == 0x50){
        printf("\n#D : Spill End is detected\n");
        n_word = i+1; 
        break;
      }
    }// For(i)
  }
  if (n_word<=0) return true;

  FairMQMessagePtr msg(NewMessage((char*)buffer,
                                  //fnByte*fnWordPerCycle, 
                                  fnByte*n_word,
                                  [](void* object, void*)
                                  {delete [] static_cast<uint8_t*>(object);}
                                  )
                       );

  
  //    while (Send(msg, "data") == -2);

  Reporter::AddOutputMessageSize(msg->GetSize());
  Send(msg, fOutputChannelName);

  //     ++fNumIterations;
  //     if ((fMaxIterations >0) && (fNumIterations >= fMaxIterations)) {
  // 	  LOG(info) << "Configured maximum number of iterations reached. Leaving RUNNING state.";
  // 	  return false;
  //     }
  // } 
  // else {
  //     LOG(warn) << "failed to send a message.";
  // }

  //    LOG(warn) << "You should swim!";
  //    std::this_thread::sleep_for(std::chrono::seconds(1));

  return true;
}

//______________________________________________________________________________
void AmQStrTdcSampler::Init()
{
  Reporter::Instance(fConfig);
}
//_____________________________________________________________________________
void AmQStrTdcSampler::SendFEMInfo()
{
  //  int mtype = std::stoi(fConfig->GetProperty<std::string>("TdcType"));
  int tdc_type = 1;
  std::cout << "mtype: " << tdc_type << std::endl;
  //  tdc_type = 1;
  
  {
    uint64_t fFEMId = 0;
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
  
  // bit set-up for module information
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

    if (Send(initmsg, fOutputChannelName) < 0) {
      LOG(warn) << " Fail to send FEMInfo :  " << count;
      count++;
    }else{
      LOG(debug) << " Send FEMInfo :  "  << count;
      break;
    } 
  }

}

//______________________________________________________________________________
void AmQStrTdcSampler::InitTask()
{

  using opt     = OptionKey;
  fIpSiTCP           = fConfig->GetValue<std::string>("ModuleTcpIp");
  LOG(debug) << "TPC IP: " << fIpSiTCP;
  //  fIpSiTCP           = fConfig->GetValue<std::string>(opt::IpSiTCP.data());
  fOutputChannelName = fConfig->GetValue<std::string>(opt::OutputChannelName.data()); 

  LOG(info) << fIpSiTCP;
  SendFEMInfo();

  /*
  rbcp_header rbcpHeader;
  rbcpHeader.type = UDPRBCP::rbcp_ver_;
  rbcpHeader.id   = 0;
    
  FPGAModule fModule(fIpSiTCP.c_str(), 4660, &rbcpHeader, 0);
  LOG(info) << std::hex << fModule.ReadModule(hul_strtdc::BCT::addr_Version, 4) << std::dec;
  */
  Reporter::Reset();
}

//______________________________________________________________________________
void AmQStrTdcSampler::PreRun()
{

  if(-1 == (fAmqSocket = ConnectSocket(fIpSiTCP.c_str()))) return;
  LOG(info) << "TCP connected";

  /*
  rbcp_header rbcpHeader;
  rbcpHeader.type = UDPRBCP::rbcp_ver_;
  rbcpHeader.id   = 0;
    
  FPGAModule fModule(fIpSiTCP.c_str(), 4660, &rbcpHeader, 0);
  fModule.WriteModule(ODP::addr_en_filter,       fTotFilterEn,  1);
  fModule.WriteModule(DCT::addr_gate,  1, 1);
  */
  LOG(info) << "Start DAQ";
}

//______________________________________________________________________________
void AmQStrTdcSampler::PostRun()
{
  /*
  using namespace hul_strtdc;

  rbcp_header rbcpHeader;
  rbcpHeader.type = UDPRBCP::rbcp_ver_;
  rbcpHeader.id   = 0;
    
  FPGAModule fModule(fIpSiTCP.c_str(), 4660, &rbcpHeader, 0);
  fModule.WriteModule(DCT::addr_gate,  0, 1);
  */
  LOG(info) << "End DAQ";
}

//______________________________________________________________________________
void AmQStrTdcSampler::ResetTask()
{
  int n_word = 0;
  uint8_t buffer[fnByte*fnWordPerCycle];
  while( -1 == ( n_word = Event_Cycle(buffer))) continue;

  close(fAmqSocket);

  LOG(info) << "Socket close";

}

//______________________________________________________________________________
int AmQStrTdcSampler::ConnectSocket(const char* ip)
{
  struct sockaddr_in SiTCP_ADDR;
  unsigned int port = 24;

  int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  SiTCP_ADDR.sin_family      = AF_INET;
  SiTCP_ADDR.sin_port        = htons((unsigned short int) port);
  SiTCP_ADDR.sin_addr.s_addr = inet_addr(ip);

  struct timeval tv;
  tv.tv_sec  = 0;
  tv.tv_usec = 250000;
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));

  int flag = 1;
  setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));

  if(0 > connect(sock, (struct sockaddr*)&SiTCP_ADDR, sizeof(SiTCP_ADDR))){
    LOG(error) << "TCP connection error";
    close(sock);
    return -1;
  }

  return sock;
}

// Event Cycle ------------------------------------------------------------
int AmQStrTdcSampler::Event_Cycle(uint8_t* buffer)
{
  // data read ---------------------------------------------------------
  static const unsigned int sizeData = fnByte*fnWordPerCycle*sizeof(uint8_t);
  int ret = receive(fAmqSocket, (char*)buffer, sizeData);
  if(ret < 0) return ret;

  return fnWordPerCycle;
}

// receive ----------------------------------------------------------------
int AmQStrTdcSampler::receive(int sock, char* data_buf, unsigned int length)
{
  unsigned int revd_size = 0;
  int tmp_ret            = 0;

  while(revd_size < length){
    tmp_ret = recv(sock, data_buf + revd_size, length -revd_size, 0);

    if(tmp_ret == 0) break;
    if(tmp_ret < 0){
      int errbuf = errno;
      perror("TCP receive");
      if(errbuf == EAGAIN){
	// this is time out
	std::cout << "#D : TCP recv time out" << std::endl;
	return -4;
      }else{
	// something wrong
	std::cerr << "TCP error : " << errbuf << std::endl;
      }

      revd_size = tmp_ret;
      break;
    }

    revd_size += tmp_ret;
  }

  return revd_size;
}

