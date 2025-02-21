#include <vector>
#include <chrono>
#include <algorithm>
#include <iostream>

#include <fairmq/runDevice.h>

#include "utility/MessageUtil.h"
#include "DigitizerSampler.h"

namespace bpo = boost::program_options;

//______________________________________________________________________________
void
addCustomOptions(bpo::options_description& options)
{
    using opt = DigitizerSampler::OptionKey;
    options.add_options()
    (opt::NodeId.data(),            bpo::value<std::string>()->default_value("0"),   "Node Id of Digitizer in daisy chain")
    (opt::NodeNum.data(),           bpo::value<std::string>()->default_value("1"),   "# of Node of Digitizer in daisy chain")      
    (opt::BoardId.data(),           bpo::value<std::string>()->default_value("0"),   "Board Id of Digitizer in daisy chain")
    (opt::ConfigFile.data(),        bpo::value<std::string>()->default_value("config.txt"),  "Config file for Digitizer")            
    (opt::OutputChannelName.data(), bpo::value<std::string>()->default_value("out"), "Name of the output channel");
}

//______________________________________________________________________________
std::unique_ptr<fair::mq::Device> getDevice(FairMQProgOptions&)
{
    return std::make_unique<DigitizerSampler>();
}

//______________________________________________________________________________

DigitizerSampler::DigitizerSampler()
    : FairMQDevice()
{}

//______________________________________________________________________________
bool DigitizerSampler::ConditionalRun()
{
    using namespace std::chrono_literals;
    
    while(true){

      int ret = RunAcquisition();

      if( ret == 0 ){

	int nbytes = 0;
	nbytes = gDigitizer.GetBytesRead();

	if( nbytes > 0 ) {

	  char* buffer;	  
	  buffer = new char[nbytes] {};
      
	  gDigitizer.GetReadOutData(buffer, nbytes);
	  
	  FairMQMessagePtr msg(NewMessage((char*)buffer,
					nbytes,
					[](void* object, void*)
					{
					  delete [] static_cast<uint8_t*>(object);
					}
					)
			     );

	  Send(msg, fOutputChannelName);
	  
	  return true;
	}
      
      }else if( ret < 0 ){
	LOG(error) << "Readout Error";
	return false;
      }

      if(NewStatePending()){
	break;	
      }else{
	continue;
      }

    }// while(true)
    
    return true;
}


//______________________________________________________________________________
void DigitizerSampler::Init()
{
}

//______________________________________________________________________________
void DigitizerSampler::InitTask()
{

    using opt     = OptionKey;

    fBoardId      = std::stoi(fConfig->GetProperty<std::string>(opt::BoardId.data()));
    fNodeNum      = std::stoi(fConfig->GetProperty<std::string>(opt::NodeNum.data()));        
    fNodeId       = std::stoi(fConfig->GetProperty<std::string>(opt::NodeId.data()));    
    fConfigFile   = fConfig->GetProperty<std::string>(opt::ConfigFile.data());    

    LOG(info) << "Digitizer Board Id: " << fBoardId;
    LOG(info) << "fNodeNum          : " << fNodeId;
    LOG(info) << "fNodeId           : " << fNodeId; 
    LOG(info) << "fConfigFile       : " << fConfigFile;
    //    fTdcType = std::stoi(fConfig->GetProperty<std::string>("TdcType"));
			
    fOutputChannelName = fConfig->GetProperty<std::string>(opt::OutputChannelName.data());

    gDigitizer.RunInit();
    
    // Reading the configuration parameters from config.txt file.
    int ret = gDigitizer.ReadParameters(fConfigFile.data());    
    if (ret) {
      LOG(error) << "Error in Reading parameters for digitizer (board-id: "
		 << fBoardId << ") ";
    }
    sleep(1);

    // Open the Digitizer and get the gHandler for it.
    // Setting up the regitsters of Digitizer.
    ret = gDigitizer.OpenDigitizer();
    if (ret) {
      LOG(error) << "Error in OpenDigitizer2 (board-id: "
		 << fBoardId << "), Errorcode: " << ret;      
    }
    sleep(2);

    ret = gDigitizer.SetupParameters();
    if (ret) {
      LOG(error) << "Error during setting up parameters (board-id: "
		 << fBoardId << "), Errorcode: " << ret;      
    }
    sleep(2);

    ret = gDigitizer.AllocateMemory();

    fBoardParameters = gDigitizer.GetParameters();
    PrintParameters();

    LOG(info) << "Output channel    : " << fBoardId;
    
}

//______________________________________________________________________________
void DigitizerSampler::PrintParameters()
{
    LOG(info) << "LinkType           : " << fBoardParameters.LinkType ;
    LOG(info) << "LinkNum            : " << fBoardParameters.LinkNum ;
    LOG(info) << "ConetNode          : " << fBoardParameters.ConetNode;
    LOG(info) << "BaseAddress        : " << fBoardParameters.BaseAddress;
    LOG(info) << "Nch                : " << fBoardParameters.Nch;
    LOG(info) << "Nbit               : " << fBoardParameters.Nbit;
    LOG(info) << "Ts                 : " << fBoardParameters.Ts ;
    LOG(info) << "NumEvents          : " << fBoardParameters.NumEvents;
    LOG(info) << "RecordLength       : " << fBoardParameters.RecordLength;
    LOG(info) << "PostTrigger        : " << fBoardParameters.PostTrigger;
    LOG(info) << "InterruptNumEvents : " << fBoardParameters.InterruptNumEvents;
    LOG(info) << "TestPattern        : " << fBoardParameters.TestPattern;
    LOG(info) << "EnableMask         : " << std::hex << fBoardParameters.EnableMask;

    for(int i=0; i<MAX_SET / 2; i++){
      LOG(info) << "ChannelTriggerMode[" << i <<  "]   : " << std::dec << fBoardParameters.ChannelTriggerMode[i];
    }
    for(int i=0; i<MAX_SET / 2; i++){
      LOG(info) << "PulsePolarity[" << i <<  "]   : " << fBoardParameters.PulsePolarity[i];
    }
    for(int i=0; i<MAX_SET /2 ; i++){
      LOG(info) << "DCoffset[" << i <<  "]   : " << fBoardParameters.DCoffset[i];
    }
    for(int i=0; i<MAX_SET / 2; i++){
      LOG(info) << "Threshold[" << i <<  "]   : " << fBoardParameters.Threshold[i];
    }
    for(int i=0; i<MAX_SET / 2; i++){
      LOG(info) << "Version_used[" << i <<  "]   : " << fBoardParameters.Version_used[i];
    }
    for(int i=0; i<MAX_SET / 2; i++){
      LOG(info) << "GroupTrgEnableMask[" << i <<  "]   : " << fBoardParameters.GroupTrgEnableMask[i];
    }
    LOG(info) << "MaxGroupNumber  :" << fBoardParameters.MaxGroupNumber;
    for(int i=0; i<MAX_SET / 2; i++){
      LOG(info) << "GroupTrgEnableMask[" << i <<  "]   : " << fBoardParameters.GroupTrgEnableMask[i];
    }

    LOG(info) << "OutFileFlags       : " << fBoardParameters.OutFileFlags;
    
}

//______________________________________________________________________________
void DigitizerSampler::PreRun(){
    /* -----------------------------------------------------------------------
    ** Set Parameters (Read from configuration file)
    ** -----------------------------------------------------------------------
    */
    gDigitizer.RunStart();
    //     sleep(1);
    LOG(info) << "Start DAQ";
}

//______________________________________________________________________________
void DigitizerSampler::PostRun()
{
    gDigitizer.RunStop();
    //    sleep(1);
    LOG(info) << "End DAQ";
}

//______________________________________________________________________________
void DigitizerSampler::ResetTask()
{
    gDigitizer.RunEnd();
    sleep(1);
    LOG(info) << "Close Digitizer";    
}

// Event Cycle ------------------------------------------------------------
int DigitizerSampler::RunAcquisition()
{
    // data read ---------------------------------------------------------
  int ret = gDigitizer.RunAcquisition();  

  return ret;
}

