#include <vector>
#include <chrono>
#include <algorithm>
#include <iostream>

#include <fairmq/runDevice.h>

#include "utility/MessageUtil.h"
#include "V1740DSampler.h"

namespace bpo = boost::program_options;

//______________________________________________________________________________
void
addCustomOptions(bpo::options_description& options)
{
    using opt = V1740DSampler::OptionKey;
    options.add_options()
    (opt::NodeId.data(),            bpo::value<std::string>()->default_value("0"),   "Node Id of Digitizer in daisy chain")
    (opt::BoardId.data(),           bpo::value<std::string>()->default_value("0"),  "Board Id of Digitizer in daisy chain")
    (opt::ConfigFile.data(),        bpo::value<std::string>()->default_value("config.txt"),  "Config file for Digitizer")            
    (opt::OutputChannelName.data(), bpo::value<std::string>()->default_value("out"), "Name of the output channel");
}

//______________________________________________________________________________
std::unique_ptr<fair::mq::Device> getDevice(FairMQProgOptions&)
{
    return std::make_unique<V1740DSampler>();
}

//______________________________________________________________________________

V1740DSampler::V1740DSampler()
    : FairMQDevice()
{}

//______________________________________________________________________________
bool V1740DSampler::ConditionalRun()
{
    using namespace std::chrono_literals;
    
    while(true){

      int ret = RunAcquisition();

      if( ret == 0 ){

	int nbytes = 0;
	nbytes = gV1740D.GetBytesRead();

	//	LOG(debug) << "nbytes: " << nbytes ;

	if( nbytes > 0 ) {

	  char* buffer;	  
	  buffer = new char[nbytes] {};
      
	  gV1740D.GetReadOutData(buffer, nbytes);
	  
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
void V1740DSampler::Init()
{
}

//______________________________________________________________________________
void V1740DSampler::InitTask()
{

    using opt     = OptionKey;

    fBoardId      = std::stoi(fConfig->GetProperty<std::string>(opt::BoardId.data()));
    fNodeId       = std::stoi(fConfig->GetProperty<std::string>(opt::NodeId.data()));    
    fConfigFile   = fConfig->GetProperty<std::string>(opt::ConfigFile.data());    

    LOG(info) << "Digitizer Board Id: " << fBoardId;
    LOG(info) << "fNodeId           : " << fNodeId; 
    LOG(info) << "fConfigFile       : " << fConfigFile;
    //    fTdcType = std::stoi(fConfig->GetProperty<std::string>("TdcType"));
			
    fOutputChannelName = fConfig->GetProperty<std::string>(opt::OutputChannelName.data());


    // Set the flags
    gV1740D.SetFlag();
    
    // Reading the configuration parameters from config.txt file.
    int ret = gV1740D.SetupParameters(&fBoardParameters, fConfigFile.data());
    if (ret) {
      LOG(error) << "Error in setting parameters for digitizer (board-id: "
		 << fBoardId << ") ";
      
    }
    sleep(1);

    // Open the Digitizer and get the gHandler for it.
    // Setting up the regitsters of Digitizer.
    ret = gV1740D.SetupAcquisition(fBoardParameters);
    if (ret) {
      LOG(error) << "Error during acquisition setup (board-id: "
		 << fBoardId << "), Errorcode: " << ret;
      
    }
    sleep(2);

    PrintParameters();

    LOG(info) << "Output channel    : " << fBoardId;
    
}

//______________________________________________________________________________
void V1740DSampler::PrintParameters()
{
    LOG(info) << "ConnectionType     : " << fBoardParameters.ConnectionType ;
    LOG(info) << "ConnectionLinkNum  : " << fBoardParameters.ConnectionLinkNum ;
    LOG(info) << "ConnectionLinkNum  : " << fBoardParameters.ConnectionConetNode ;
    LOG(info) << "RecordLength       : " << fBoardParameters.RecordLength ;
    LOG(info) << "PreTrigger         : " << fBoardParameters.PreTrigger ;
    LOG(info) << "ActiveChannel      : " << fBoardParameters.ActiveChannel ;
    LOG(info) << "PreGate            : " << fBoardParameters.PreGate ;
    LOG(info) << "ChargeSensitivity  : " << fBoardParameters.ChargeSensitivity ;
    LOG(info) << "FixedBaseline      : " << fBoardParameters.FixedBaseline ;
    LOG(info) << "BaselineMode       : " << fBoardParameters.BaselineMode ;
    LOG(info) << "TrgMode            : " << fBoardParameters.TrgMode ;
    LOG(info) << "TrgSmoothing       : " << fBoardParameters.TrgSmoothing;
    LOG(info) << "TrgHoldOff         : " << fBoardParameters.TrgHoldOff;

    for(int i=0; i<32; i++){
      LOG(info) << "TriggerThreshold[" << i <<  "]   : " << fBoardParameters.TriggerThreshold[i];
    }
    for(int i=0; i<8; i++){
      LOG(info) << "DCoffset["<< i << "]   : " << fBoardParameters.DCoffset[i];
    }
    LOG(info) << "NevAggr            : " << fBoardParameters.NevAggr;
    LOG(info) << "PulsePol           : " << fBoardParameters.PulsePol;
    LOG(info) << "EnChargePed        : " << fBoardParameters.EnChargePed;  
    LOG(info) << "DisTrigHist        : " << fBoardParameters.DisTrigHist;
    LOG(info) << "DisSelfTrigger     : " << fBoardParameters.DisSelfTrigger;
    LOG(info) << "EnTestPulses       : " << fBoardParameters.EnTestPulses;
    LOG(info) << "TestPulsesRate     : " << fBoardParameters.TestPulsesRate;
    LOG(info) << "DefaultTriggerThr  : " << fBoardParameters.DefaultTriggerThr;
    LOG(info) << "ChannelTriggerMask : " << fBoardParameters.ChannelTriggerMask;
}

//______________________________________________________________________________
void V1740DSampler::PreRun()
{
    /* -----------------------------------------------------------------------
    ** Set Parameters (Read from configuration file)
    ** -----------------------------------------------------------------------
    */
    gV1740D.RunStart();
    //     sleep(1);
    LOG(info) << "Start DAQ";
}

//______________________________________________________________________________
void V1740DSampler::PostRun()
{
    gV1740D.RunStop();
    //    sleep(1);
    LOG(info) << "End DAQ";
}

//______________________________________________________________________________
void V1740DSampler::ResetTask()
{
    gV1740D.CleanUp();
    sleep(1);
    LOG(info) << "Clear Memory";    
}

// Event Cycle ------------------------------------------------------------
int V1740DSampler::RunAcquisition()
{
    // data read ---------------------------------------------------------
  int ret = gV1740D.RunAcquisition();  

  return ret;
}

