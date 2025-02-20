#include <vector>
#include <chrono>
#include <algorithm>
#include <iostream>

#include <fairmq/runDevice.h>

#include "utility/MessageUtil.h"
#include "AmQStrTdcSampler.h"

#include "utility/network.hh"
#include "utility/rbcp.h"
#include "utility/UDPRBCP.hh"

namespace bpo = boost::program_options;

//______________________________________________________________________________
void
addCustomOptions(bpo::options_description& options)
{
    using opt = AmQStrTdcSampler::OptionKey;
    options.add_options()
    (opt::IpSiTCP.data(),           bpo::value<std::string>()->default_value("0"),   "SiTCP IP (xxx.yyy.zzz.aaa)")
    (opt::OutputChannelName.data(), bpo::value<std::string>()->default_value("out"), "Name of the output channel")
    (opt::TdcType.data(), bpo::value<std::string>()->default_value("2"), "TDC type: HR=2, LR=1");
}

//______________________________________________________________________________
std::unique_ptr<fair::mq::Device> getDevice(FairMQProgOptions&)
{
    return std::make_unique<AmQStrTdcSampler>();
}

//______________________________________________________________________________

AmQStrTdcSampler::AmQStrTdcSampler()
    : FairMQDevice()
{}

//______________________________________________________________________________
bool AmQStrTdcSampler::ConditionalRun()
{
    using namespace std::chrono_literals;

    //    int n_word = 0;
    int recv_status = 0;
    int num_recieved_bytes = 0;
    int n_word = 0;
    uint8_t* buffer = new uint8_t[kOutBufByte*fnWordPerCycle] {};

    while( -1 == ( recv_status = Event_Cycle(buffer, num_recieved_bytes))) {
        if (NewStatePending()) {
            break;
	} else {
            continue;
	}
    }

    n_word = num_recieved_bytes/optnByte;

#if 0
    if(recv_status == -4) {
      for(int i = 0; i<fnWordPerCycle; ++i) {

	//      if((buffer[fnByte*i+4] & 0xff) == 0x50){
	//if((buffer[optnByte*i + header_pos] & 0xff) == 0x50) {
	//	if((buffer[optnByte*i + header_pos] & 0xf0) == 0x50) {
	if(true 
	   &&(buffer[optnByte*i + 0] & 0xff) == 0x0
	   &&(buffer[optnByte*i + 1] & 0xff) == 0x0
	   &&(buffer[optnByte*i + 2] & 0xff) == 0x0
	   &&(buffer[optnByte*i + 3] & 0xff) == 0x0
	   &&(buffer[optnByte*i + 4] & 0xff) == 0x0
	   &&(buffer[optnByte*i + 5] & 0xff) == 0x0
	   &&(buffer[optnByte*i + 6] & 0xff) == 0x0
	   &&(buffer[optnByte*i + 7] & 0xff) == 0x0
	   ) {
	  LOG(debug) << "#D: Empty data: ";
	  LOG(debug) << " n_word: " << n_word << ", array index:" << i;

	  //        n_word = i+1;
	  //	  n_word = i+2;
	  break;
	}
      }// For(i)
    }
#endif

    //  if(n_word == -4){
    //    n_word = remain;
    //    remain = 0;
    //  }

    if (recv_status <= 0 && recv_status != -4) {
      delete[] buffer;
      return true;
    }

#if 0
    if(n_word == -4) {
        for(int i = 0; i<fnWordPerCycle; ++i) {

            //      if((buffer[fnByte*i+4] & 0xff) == 0x50){
            //if((buffer[optnByte*i + header_pos] & 0xff) == 0x50) {
            if((buffer[optnByte*i + header_pos] & 0xf0) == 0x50) {
                printf("\n#D : Spill End is detected\n");
                //        n_word = i+1;
                n_word = i+2;
                break;
            }
        }// For(i)
    }

    if (n_word<=0) {
        delete[] buffer;
        return true;
    }
#endif
    
    FairMQMessagePtr msg(NewMessage((char*)buffer,
                                    //fnByte*fnWordPerCycle,
				    kOutBufByte*n_word,
                                    [](void* object, void*)
    {
        delete [] static_cast<uint8_t*>(object);
    }
                                   )
                        );

    //    while (Send(msg, fOutputChannelName) == -2);

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
}
//_____________________________________________________________________________
void AmQStrTdcSampler::SendFEMInfo()
{

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
        fem_info_.FEMType = fTdcType;
    }


    // bit set-up for module information
    unsigned char* fbuf = new uint8_t[sizeof(fem_info_)];

    uint8_t buf[8] = {0};
    for(int i = 0; i < 8; i++) {
        buf[7-i] = (fem_info_.magic >> 8*(7-i) ) & 0xff;
    }
    memcpy(fbuf, &buf, sizeof(char)*8);

    for(int i = 0; i < 8; i++) {
        if(i < 4) {
            buf[7-i] = (fem_info_.FEMId >> 8*(3-i)) & 0xff;
        } else {
            buf[7-i] = (fem_info_.FEMType >> 8*(7-i)) & 0xff;
        }
    }
    memcpy(&fbuf[8], &buf, sizeof(char)*8);

    uint8_t resv[8] = {0};
    memcpy(&fbuf[16], &resv, sizeof(char)*8);
    //memcpy(fbuf, &fem_info_, sizeof(fem_info_));

    FairMQMessagePtr initmsg(
        NewMessage((char*)fbuf,
            kOutBufByte*3,
            [](void* object, void*) {
            delete [] static_cast<uint8_t*>(object);
            }
        )
    );

    LOG(info) << "Sending FEMInfo \"" << sizeof(fem_info_) << "\"";


    int count=0;
    while(true) {

        if (Send(initmsg, fOutputChannelName) < 0) {
            LOG(warn) << " Fail to send FEMInfo :  " << count;
            count++;
        } else {
            LOG(debug) << " Send FEMInfo :  "  << count;
            break;
        }
    }

}

//______________________________________________________________________________
void AmQStrTdcSampler::InitTask()
{

    using opt     = OptionKey;

    fIpSiTCP           = fConfig->GetProperty<std::string>("msiTcpIp");
    // fIpSiTCP           = fConfig->GetProperty<std::string>(opt::IpSiTCP.data());
    LOG(info) << "TPC IP: " << fIpSiTCP;
    //  fIpSiTCP           = fConfig->GetProperty<std::string>(opt::IpSiTCP.data());
    fOutputChannelName = fConfig->GetProperty<std::string>(opt::OutputChannelName.data());

    fTdcType = std::stoi(fConfig->GetProperty<std::string>(opt::TdcType.data()));
    LOG(info) << "TDC Type: " << fTdcType << std::endl;

    LOG(info) << fIpSiTCP;
    SendFEMInfo();

    header_pos = 7;
    optnByte = 8;


    /*
    rbcp_header rbcpHeader;
    rbcpHeader.type = UDPRBCP::rbcp_ver_;
    rbcpHeader.id   = 0;

    FPGAModule fModule(fIpSiTCP.c_str(), 4660, &rbcpHeader, 0);
    LOG(info) << std::hex << fModule.ReadModule(hul_strtdc::BCT::addr_Version, 4) << std::dec;
    */
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

    close(fAmqSocket);
    LOG(info) << "Socket close";
    LOG(info) << "End DAQ";
}

//______________________________________________________________________________
void AmQStrTdcSampler::ResetTask()
{
    //  close(fAmqSocket);

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
    //tv.tv_sec  = 0;
    //tv.tv_usec = 250000;
    tv.tv_sec = 1;
    //    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));

    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));

    if(0 > connect(sock, (struct sockaddr*)&SiTCP_ADDR, sizeof(SiTCP_ADDR))) {
        LOG(error) << "TCP connection error";
        close(sock);
        return -1;
    }

    return sock;
}

// Event Cycle ------------------------------------------------------------
int AmQStrTdcSampler::Event_Cycle(uint8_t* buffer, int& num_recieved_bytes)
{
    // data read ---------------------------------------------------------
    //  static const unsigned int sizeData = fnByte*fnWordPerCycle*sizeof(uint8_t);
    static const unsigned int sizeData = optnByte*fnWordPerCycle*sizeof(uint8_t);
    int ret = receive(fAmqSocket, (char*)buffer, sizeData, num_recieved_bytes);

    return ret;
    //    if(ret <= 0) return ret;
    //    return fnWordPerCycle;
}

// receive ----------------------------------------------------------------
int AmQStrTdcSampler::receive(int sock, char* data_buf, unsigned int length, int& num_recieved_bytes)
{
    unsigned int revd_size = 0;
    int tmp_ret            = 0;

    while(revd_size < length) {
        tmp_ret = recv(sock, data_buf + revd_size, length -revd_size, 0);
	//	LOG(debug) << "tmp_ret: " << tmp_ret;

        if(tmp_ret == 0) break;
        if(tmp_ret < 0) {
            int errbuf = errno;
            perror("TCP receive");
            if(errbuf == EAGAIN) {
              // this is time out
	      //	      std::cout << "#D : TCP recv time out"  << std::endl;
              //	remain = revd_size;
	      LOG(debug) << "#D: recv time out, recieved byte size: " << revd_size;
	      num_recieved_bytes = revd_size;
	      return -4;
            } else {
                // something wrong
                std::cerr << "TCP error : " << errbuf << std::endl;
            }

            revd_size = tmp_ret;
            break;
        }

        revd_size += tmp_ret;
    }

    num_recieved_bytes = revd_size;
    return revd_size;
}

