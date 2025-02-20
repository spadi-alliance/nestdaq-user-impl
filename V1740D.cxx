#include <iostream>
#include <cstring>
#include <inttypes.h>

#include "V1740D.h"

void V1740D::SetFlag(){

   gAcquisitionBufferAllocated = 0;
   gLoops= 0;
   gBytesRead = 0;

   gAcqBuffer = NULL;
}

void V1740D::SetDefaultParameters(BoardParameters *params){

    int i;

    params->RecordLength = 200;	     /* Number of samples in the acquisition window (waveform mode only)    */
    params->PreTrigger = 100;  	     /* PreTrigger is in number of samples                                  */
    params->ActiveChannel = 0;	     /* Channel used for the data analysis (plot, histograms, etc...)       */
    params->NevAggr = 1;             /* Number of events per aggregate (buffer). 0=automatic */
    params->AcqMode = CAEN_DGTZ_DPP_ACQ_MODE_List;  /* Acquisition Mode (LIST or MIXED)                     */
    params->DefaultTriggerThr = 10;  /* Default threshold for trigger                        */
    params->FixedBaseline = 2100;    /* fixed baseline (used when BaselineMode = 0)          */
    params->PreGate = 20;	     /* Position of the gate respect to the trigger (num of samples before) */
    params->BaselineMode = 2;	     /* Baseline: 0=Fixed, 1=4samples, 2=16 samples, 3=64samples            */
    params->TrgHoldOff = 10;         /*Trigger HoldOff */
    params->TrgMode = 0;             /*Trigger Mode: 0 Normal, 1 Paired */
    params->DisSelfTrigger = 0;      /*Disable self trigger: if disabled it can be propagated to TRGOUT, but not used to acquire*/
    params->TrgSmoothing = 0;        /*Input smoothing factor: 0=disabled, 1=2samples, 2=4samples ... 5=64samples  */
    params->ChargeSensitivity = 2;   /* Charge sesnitivity (0=max, 7=min) */
    params->PulsePol = 1;            /* Pulse Polarity (1=negative, 0=positive)              */
    params->EnChargePed = 0;         /* Enable Fixed Charge Pedestal in firmware (0=off, 1=on (1024 pedestal)) */
    params->DisTrigHist = 0;         /* 0 = Trigger Histeresys on; 1 = Trigger Histeresys off */


    for (i = 0; i < MAX_X740_GROUP_SIZE; ++i) {
      params->DPPParams.GateWidth[i] = 40;	   /* Gate Width in samples        */
      params->DPPParams.DisTrigHist[i] = params->DisTrigHist;
      params->DPPParams.DisSelfTrigger[i] = params->DisSelfTrigger;
      params->DPPParams.BaselineMode[i] = params->BaselineMode;
      params->DPPParams.TrgMode[i] = params->TrgMode;
      params->DPPParams.ChargeSensitivity[i] = params->ChargeSensitivity;
      params->DPPParams.PulsePol[i] = params->PulsePol;
      params->DPPParams.EnChargePed[i] = params->EnChargePed;
      params->DPPParams.TestPulsesRate[i] = params->TestPulsesRate;
      params->DPPParams.EnTestPulses[i] = params->EnTestPulses;
      params->DPPParams.InputSmoothing[i] = params->TrgSmoothing;
      params->DPPParams.BaselineMode[i] = params->BaselineMode;
      params->DPPParams.InputSmoothing[i] = params->TrgSmoothing;
      params->DPPParams.trgho[i] = params->TrgHoldOff;
      params->DPPParams.FixedBaseline[i] = params->FixedBaseline;
    }

    for (i = 0; i < MAX_X740_GROUP_SIZE; ++i)
      params->DCoffset[i] = 0x8000;            /* DC offset adjust in DAC counts (0x8000 = mid scale)  */

    for (i = 0; i < MAX_DPP_QDC_CHANNEL_SIZE; ++i) {
      params->TriggerThreshold[i] = params->DefaultTriggerThr;
    }

}


/*
** Read config file and assign new values to the parameters
** Returns 0 on success; -1 in case of any error detected.
*/
int V1740D::LoadConfigurationFile(char * fname, BoardParameters *params) {
    FILE *parameters_file;

    params->ConnectionType = CONNECTION_TYPE_AUTO;

    parameters_file = fopen(fname, "r");

    if (parameters_file != NULL) {

      while (!feof(parameters_file)) {
	char str[100];
	fscanf(parameters_file, "%s", str);
	if (str[0] == '#') {
	  fgets(str, 100, parameters_file);
	  continue;
	}

	if (strcmp(str, "AcquisitionMode") == 0) {
	  char str1[100];
	  fscanf(parameters_file, "%s", str1);
	  if (strcmp(str1, "MIXED") == 0)
	    params->AcqMode = CAEN_DGTZ_DPP_ACQ_MODE_Mixed;
	  if (strcmp(str1, "LIST") == 0)
	    params->AcqMode = CAEN_DGTZ_DPP_ACQ_MODE_List;
	}

	if (strcmp(str, "ConnectionType") == 0) {
	  char str1[100];
	  fscanf(parameters_file, "%s", str1);
	  if (strcmp(str1, "USB") == 0)
	    params->ConnectionType = CONNECTION_TYPE_USB;
	  if (strcmp(str1, "OPT") == 0)
	    params->ConnectionType = CONNECTION_TYPE_OPT;
	  if (strcmp(str1, "A4818") == 0)
	    params->ConnectionType = CONNECTION_TYPE_USB_A4818;
	  
	}


	if (strcmp(str, "ConnectionLinkNum") == 0)
	  fscanf(parameters_file, "%d", &params->ConnectionLinkNum);
	if (strcmp(str, "ConnectionConetNode") == 0)
	  fscanf(parameters_file, "%d", &params->ConnectionConetNode);
	if (strcmp(str, "ConnectionVmeBaseAddress") == 0)
	  fscanf(parameters_file, "%lx", (unsigned long *)&params->ConnectionVMEBaseAddress);
	

	if (strcmp(str, "TriggerThreshold") == 0) {
	  int ch;
	  fscanf(parameters_file, "%d", &ch);
	  fscanf(parameters_file, "%d", &params->TriggerThreshold[ch]);
	}

	if (strcmp(str, "RecordLength") == 0)
	  fscanf(parameters_file, "%d", &params->RecordLength);
	if (strcmp(str, "PreTrigger") == 0)
	  fscanf(parameters_file, "%d", &params->PreTrigger);
	if (strcmp(str, "ActiveChannel") == 0)
	  fscanf(parameters_file, "%d", &params->ActiveChannel);
	if (strcmp(str, "BaselineMode") == 0)
				fscanf(parameters_file, "%d", &params->BaselineMode);
	if (strcmp(str, "TrgMode") == 0)
	  fscanf(parameters_file, "%d", &params->TrgMode);
	if (strcmp(str, "TrgSmoothing") == 0)
	  fscanf(parameters_file, "%d", &params->TrgSmoothing);
	if (strcmp(str, "TrgHoldOff") == 0)
	  fscanf(parameters_file, "%d", &params->TrgHoldOff);
	if (strcmp(str, "FixedBaseline") == 0)
	  fscanf(parameters_file, "%d", &params->FixedBaseline);
	if (strcmp(str, "PreGate") == 0) {
	  int gr;
	  fscanf(parameters_file, "%d", &gr);
	  fscanf(parameters_file, "%d", &params->DPPParams.PreGate[gr]);
	}
	if (strcmp(str, "GateWidth") == 0) {
	  int gr;
	  fscanf(parameters_file, "%d", &gr);
	  fscanf(parameters_file, "%d", &params->DPPParams.GateWidth[gr]);
	}
	if (strcmp(str, "DCoffset") == 0) {
	  int ch;
	  fscanf(parameters_file, "%d", &ch);
	  fscanf(parameters_file, "%d", &params->DCoffset[ch]);
	}
	if (strcmp(str, "ChargeSensitivity") == 0)
	  fscanf(parameters_file, "%d", &params->ChargeSensitivity);
	if (strcmp(str, "NevAggr") == 0)
	  fscanf(parameters_file, "%d", &params->NevAggr);
	if (strcmp(str, "ChannelTriggerMask") == 0)
	  //	  fscanf(parameters_file, "%llx", &params->ChannelTriggerMask);
	  fscanf(parameters_file, "%" SCNx64, &params->ChannelTriggerMask);
	if (strcmp(str, "PulsePolarity") == 0)
	  fscanf(parameters_file, "%d", &params->PulsePol);
	if (strcmp(str, "EnableChargePedestal") == 0)
	  fscanf(parameters_file, "%d", &params->EnChargePed);
	if (strcmp(str, "DisableTriggerHysteresis") == 0)
	  fscanf(parameters_file, "%d", &params->DisTrigHist);
	if (strcmp(str, "DisableSelfTrigger") == 0)
	  fscanf(parameters_file, "%d", &params->DisSelfTrigger);
	if (strcmp(str, "EnableTestPulses") == 0)
	  fscanf(parameters_file, "%d", &params->EnTestPulses);
	if (strcmp(str, "TestPulsesRate") == 0)
	  fscanf(parameters_file, "%d", &params->TestPulsesRate);
	if (strcmp(str, "DefaultTriggerThr") == 0)
	  fscanf(parameters_file, "%d", &params->DefaultTriggerThr);
	if (strcmp(str, "EnableExtendedTimeStamp") == 0)
	  fscanf(parameters_file, "%d", &params->DPPParams.EnableExtendedTimeStamp);
      }
      fclose(parameters_file);

      /* copy values into DPPParams */
      int g = 0;
      for (g = 0; g < MAX_X740_GROUP_SIZE; g++) {
	params->DPPParams.DisTrigHist[g] = params->DisTrigHist;
	params->DPPParams.DisSelfTrigger[g] = params->DisSelfTrigger;
	params->DPPParams.BaselineMode[g] = params->BaselineMode;
	params->DPPParams.TrgMode[g] = params->TrgMode;
	params->DPPParams.ChargeSensitivity[g] = params->ChargeSensitivity;
	params->DPPParams.PulsePol[g] = params->PulsePol;
	params->DPPParams.EnChargePed[g] = params->EnChargePed;
	params->DPPParams.TestPulsesRate[g] = params->TestPulsesRate;
	params->DPPParams.EnTestPulses[g] = params->EnTestPulses;
	params->DPPParams.InputSmoothing[g] = params->TrgSmoothing;
	params->DPPParams.BaselineMode[g] = params->BaselineMode;
	params->DPPParams.InputSmoothing[g] = params->TrgSmoothing;
	params->DPPParams.trgho[g] = params->TrgHoldOff;
	params->DPPParams.FixedBaseline[g] = params->FixedBaseline;
      }

      printf("End of Config file\n");
      return 0;
    }
    return -1;
}


/* Set parameters for the acquisition */
int V1740D::SetupParameters(BoardParameters *params, char *fname) {
    int ret;
    SetDefaultParameters(params);

    ret = LoadConfigurationFile(fname, params);
    return ret;
}

int V1740D::ConfigureDigitizer(int handle, int EquippedGroups, BoardParameters *params) {

    int ret = 0;
    int i;
    //uint32_t DppCtrl1;
    uint32_t GroupMask = 0;

    /* Reset Digitizer */
    ret |= CAEN_DGTZ_Reset(handle);

    /* Build Group enablemask from channel trigger mask */
    for (i = 0; i < EquippedGroups; i++) {
      uint8_t mask = (params->ChannelTriggerMask >> (i * 8)) & 0xFF;
      ret |= CAEN_DGTZ_SetChannelGroupMask(handle, i, (uint32_t)mask);
      if (mask)
	GroupMask |= (1 << i);
    }
    /* Set Group enable mask */
    ret |= CAEN_DGTZ_SetGroupEnableMask(handle, GroupMask);


    /*
    ** Set selfTrigger threshold
    ** Check if the module has 64 (VME) or 32 channels available (Desktop NIM)
    */
    if ((gBoardInfo.FormFactor == CAEN_DGTZ_VME64_FORM_FACTOR) || (gBoardInfo.FormFactor == CAEN_DGTZ_VME64X_FORM_FACTOR))
      for (i = 0; i < 64; i++)
	ret |= CAEN_DGTZ_SetChannelTriggerThreshold(handle, i, params->TriggerThreshold[i]);
    else
      for (i = 0; i < 32; i++)
	ret |= CAEN_DGTZ_SetChannelTriggerThreshold(handle, i, params->TriggerThreshold[i]);


    /* Disable Group self trigger for the acquisition (mask = 0) */
    ret |= CAEN_DGTZ_SetGroupSelfTrigger(handle, CAEN_DGTZ_TRGMODE_ACQ_ONLY, 0x00);
    /* Set the behaviour when a SW tirgger arrives */
    ret |= CAEN_DGTZ_SetSWTriggerMode(handle, CAEN_DGTZ_TRGMODE_ACQ_ONLY);
    /* Set the max number of events/aggregates to transfer in a sigle readout */
    ret |= CAEN_DGTZ_SetMaxNumAggregatesBLT(handle, MAX_AGGR_NUM_PER_BLOCK_TRANSFER);
    /* Set the start/stop acquisition control */
    ret |= CAEN_DGTZ_SetAcquisitionMode(handle, CAEN_DGTZ_SW_CONTROLLED);

    /*Set DPP Acquisition mode*/
    // CAEN_DGTZ_DPP_SAVE_PARAM_ChargeAndTime    Both charge and time are returned
    ret |= CAEN_DGTZ_SetDPPAcquisitionMode(handle, params->AcqMode, CAEN_DGTZ_DPP_SAVE_PARAM_ChargeAndTime);

    // CTIN: This part of code doesn't work?
    if (params->TrgMode == 2) {
      uint32_t d32;
      params->DisSelfTrigger = 1;  
      ret |= CAEN_DGTZ_ReadRegister(handle, 0x8000, &d32);
      d32 = d32 | (0 & 0x3 << 20); 
      ret |= CAEN_DGTZ_WriteRegister(handle, 0x8000, d32);
    }
    else
      params->DisSelfTrigger = 0;

    /* Set Pre Trigger (in samples) */
    ret |= CAEN_DGTZ_SetDPPPreTriggerSize(handle, -1, params->PreTrigger);

    /* Set DPP-PSD parameters */
    ret |= CAEN_DGTZ_SetDPPParameters(handle, GroupMask, &params->DPPParams);

    /* Set the waveform lenght (in samples) */
    ret |= CAEN_DGTZ_SetRecordLength(handle, params->RecordLength);

    /* Set DC offset */
    for (i = 0; i < EquippedGroups; i++) {
      ret |= CAEN_DGTZ_SetGroupDCOffset(handle, i, params->DCoffset[i]);
      if (ret != CAEN_DGTZ_Success) {
	printf("Errors setting dco %d.\n", i);
      }
    }

    /* Set number of events per memory buffer */
    ret |= CAEN_DGTZ_SetDPPEventAggregation(handle, params->NevAggr, 0);

    /* enable test pulses on TRGOUT/GPO */
    if (ENABLE_TEST_PULSE) {
      uint32_t d32;
      ret |= CAEN_DGTZ_ReadRegister(handle, 0x811C, &d32);
      ret |= CAEN_DGTZ_WriteRegister(handle, 0x811C, d32 | (1 << 15));
      ret |= CAEN_DGTZ_WriteRegister(handle, 0x8168, 2);
    }

    /* Set TTL I/O electrical standard */
    ret |= CAEN_DGTZ_SetIOLevel(handle, CAEN_DGTZ_IOLevel_NIM);
    
    /* Set run synchronization mode */
    ret |= CAEN_DGTZ_SetRunSynchronizationMode(handle, CAEN_DGTZ_RUN_SYNC_Disabled);
    
    /* Check errors */
    if (ret != CAEN_DGTZ_Success) {
      printf("Errors during Digitizer Configuration.\n");
      return -1;
    }

    return 0;
}



int V1740D::RunStop(){
    std::cout << "Stoping Digitizer..." << std::endl;;
    
    CAEN_DGTZ_SWStopAcquisition(gHandle);

    std::cout << "Stoped Digitizer...;" << std::endl;

    return 0;
}


/*
** Cleanup memory
** Returns 0 on success; -1 in case of any error detected.
*/

int V1740D::CleanUp() {

    int ret;

    printf("Clear buffer...\n");
    /* Free the buffers and close the digitizer */
    ret = CAEN_DGTZ_FreeReadoutBuffer(&gAcqBuffer);
  
    if (gWaveforms)
      ret = CAEN_DGTZ_FreeDPPWaveforms(gHandle, &gWaveforms);
  
    ret |= CAEN_DGTZ_FreeDPPEvents(gHandle, (void**)&gEvent);
    ret |= CAEN_DGTZ_CloseDigitizer(gHandle);

    return ret;
}

/*
** Setup acquisition.
**   - setup board according to configuration file
**   - open a connection with the module
**   - allocates memory for acquisition buffers and structures
** Returns 0 on success; -1 in case of any error detected.
*/

int V1740D::SetupAcquisition(const BoardParameters& params) {

    int ret;
    //    unsigned int i;
    //    uint32_t     size = 0;
    uint32_t AllocatedSize = 0;
    uint32_t rom_version;
    uint32_t rom_board_id;

    gParams = params;

    /* ---------------------------------------------------------------------------------------
    // Open the digitizer and read board information
    // ---------------------------------------------------------------------------------------
    */

    //    CAEN_DGTZ_CloseDigitizer(gHandle);

    /* Open a conection with module and gets library handle */
    if (gParams.ConnectionType == CONNECTION_TYPE_AUTO) {
      /* Try USB connection first */
      ret = CAEN_DGTZ_OpenDigitizer(CAEN_DGTZ_USB, gParams.ConnectionLinkNum, gParams.ConnectionConetNode, gParams.ConnectionVMEBaseAddress, &gHandle);
      if (ret != CAEN_DGTZ_Success) {
	/* .. if not successful try opticallink connection then ...*/
	ret = CAEN_DGTZ_OpenDigitizer(CAEN_DGTZ_OpticalLink, gParams.ConnectionLinkNum, gParams.ConnectionConetNode, gParams.ConnectionVMEBaseAddress, &gHandle);
	if (ret != CAEN_DGTZ_Success) {
	  printf("Can't open digitizer\n");
	  return -1;
	}
      }
    }
    else {
      if (gParams.ConnectionType == CONNECTION_TYPE_USB){
	ret = CAEN_DGTZ_OpenDigitizer(CAEN_DGTZ_USB, gParams.ConnectionLinkNum, gParams.ConnectionConetNode, gParams.ConnectionVMEBaseAddress, &gHandle);

	if (ret != CAEN_DGTZ_Success) {
	  printf("Can't open digitizer\n");
	  return -1;
	}
      }

      if (gParams.ConnectionType == CONNECTION_TYPE_OPT){ 
	ret = CAEN_DGTZ_OpenDigitizer(CAEN_DGTZ_OpticalLink, gParams.ConnectionLinkNum, gParams.ConnectionConetNode, gParams.ConnectionVMEBaseAddress, &gHandle);
	if (ret != CAEN_DGTZ_Success) {
	  printf("Can't open digitizer\n");
	  return -1;
	}
	
      }

      if (gParams.ConnectionType == CONNECTION_TYPE_USB_A4818){
	ret = CAEN_DGTZ_OpenDigitizer(CAEN_DGTZ_USB_A4818, 21151, gParams.ConnectionConetNode, gParams.ConnectionVMEBaseAddress, &gHandle);

	if (ret != CAEN_DGTZ_Success) {
	  printf("Can't open digitizer: CONNECTION_TYPE_USB_A4818 \n");
	  return -1;
	}
      }
      
    }

    /* Check board type */
    ret = CAEN_DGTZ_GetInfo(gHandle, &gBoardInfo);
    
    /* Check board ModelName */
    CAEN_DGTZ_ReadRegister(gHandle, 0xF030, &rom_version);
    if ((rom_version & 0xFF) == 0x54) ///0x54 for bigger flash  ---> 0x50 small flash
      {
	CAEN_DGTZ_ReadRegister(gHandle, 0xF034, &rom_board_id);
	switch (rom_board_id)
	  {
	  case 0:
	    sprintf(gBoardInfo.ModelName, "V1740D");
	    break;
	  case 1:
	    sprintf(gBoardInfo.ModelName, "VX1740D");
	    break;
	  case 2:
	    sprintf(gBoardInfo.ModelName, "DT5740D");
	    break;
	  case 3:
	    sprintf(gBoardInfo.ModelName, "N6740D");
	    break;
	  default:
	    break;
	  }
      }
    else
      {
	printf("This software is not appropriate for module %s\n", gBoardInfo.ModelName);
	printf("Press any key to exit!\n");
	exit(0);
      }


    printf("\nConnected to CAEN Digitizer Model %s\n", gBoardInfo.ModelName);
    printf("ROC FPGA Release is %s\n", gBoardInfo.ROC_FirmwareRel);
    printf("AMC FPGA Release is %s\n\n", gBoardInfo.AMC_FirmwareRel);
    
    gEquippedChannels = gBoardInfo.Channels * 8; //gBoardInfo is DigitizerTable.Info --> N.B: Channels means Groups in case of x740
    gEquippedGroups = gEquippedChannels / 8;

    printf("gEquippedChannels is %d\n\n", gEquippedChannels);
    printf("gActiveChannel is %d\n\n", gParams.ActiveChannel);	

	
    gActiveChannel = gParams.ActiveChannel;
    if (gActiveChannel >= gEquippedChannels)
      gActiveChannel = gEquippedChannels - 1;
    
    ret = CAEN_DGTZ_SWStopAcquisition(gHandle);

    ret = ConfigureDigitizer(gHandle, gEquippedGroups, &gParams);
    if (ret) {
      printf("Error during digitizer configuration\n");
      return -1;
    }

    /* ---------------------------------------------------------------------------------------
    // Memory allocation and Initialization of counters, histograms, etc...
    */
    memset(gExtendedTimeTag, 0, MAX_CHANNELS * sizeof(uint64_t));
    memset(gETT, 0, MAX_CHANNELS * sizeof(uint64_t));
    memset(gCurrTimeTag, 0, MAX_CHANNELS*sizeof(uint64_t));
    memset(gPrevTimeTag, 0, MAX_CHANNELS * sizeof(uint64_t));
    memset(gPrevTimeTag, 0, MAX_CHANNELS * sizeof(uint64_t));

    if (!gAcquisitionBufferAllocated) {
      gAcquisitionBufferAllocated = 1;

      /* Malloc Readout Buffer.
	 NOTE: The mallocs must be done AFTER digitizer's configuration! */
      uint32_t size1;
      ret = CAEN_DGTZ_MallocReadoutBuffer(gHandle, &gAcqBuffer, &size1);
      if (ret) { printf("Cannot allocate %d bytes for the acquisition buffer! Exiting...", size1); exit(-1); }
      printf("Allocated %d bytes for readout buffer \n", size1);

      /* Allocate memory for channel events */
      unsigned int allocated_size;
      ret = CAEN_DGTZ_MallocDPPEvents(gHandle, (void**)gEvent, &allocated_size);
      if (ret) { printf("Cannot allocate memory for waveforms\n. Exiting...."); exit(-1); };


      /* allocate memory buffer for the data readout */
      ret = CAEN_DGTZ_MallocDPPWaveforms(gHandle, (void**)&gWaveforms, &AllocatedSize);
      if (ret) { printf("Cannot allocate memory for waveforms \n. Exiting...."); exit(-1); };
      
    }

    printf("Setup the Acq.\n");
    return 0;
}

int V1740D::RunStart(){
    CAEN_DGTZ_SWStartAcquisition(gHandle);
    printf("Acquisition started\n");
    return 0;
}

int V1740D::GetBytesRead(){

  return gBytesRead;
}

int V1740D::GetReadOutData(char* destBuf, uint32_t byteSize){

    if(byteSize > 0){
      std::memcpy(destBuf, gAcqBuffer, byteSize);
    }else{
      return -1;
    }

    return 0;
}

int V1740D::RunAcquisition(){
  
    int ret;

    uint32_t     bsize = 0;
    uint32_t     NumEvents[MAX_CHANNELS];
    //    CAEN_DGTZ_EventInfo_t* eventInfo;
    //    char *EventPtr = NULL;

    //    printf("+++++++++++start acquisiton++++++++\n");
    /* Send a SW Trigger if requested by user */
    if (gSWTrigger) {
        if (gSWTrigger == 1)    gSWTrigger = 0;
        ret = CAEN_DGTZ_SendSWtrigger(gHandle);
        if (ret != CAEN_DGTZ_Success) {
            printf("Error sending software trigger (ret = %d): exiting ....\n", ret);
            exit(-1);
        }
    }

    memset(NumEvents, 0, MAX_CHANNELS * sizeof(NumEvents[0]));
    /* Read a block of data from the digitizer */
    /* Read the buffer from the digitizer */
    ret = CAEN_DGTZ_ReadData(gHandle, CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT, gAcqBuffer, &bsize); 
    gLoops += 1;
    if (ret) {
	printf("Readout Error: %d\n", ret);
	return ret;
    }
    
    if ( mDecode ){
      /* Decode and analyze Events */
      if ((ret = CAEN_DGTZ_GetDPPEvents(gHandle, gAcqBuffer, bsize, (void **)gEvent, NumEvents)) != CAEN_DGTZ_Success) {
        printf("Error during _CAEN_DGTZ_GetDPPEvents() call (ret = %d): exiting ....\n", ret);
        exit(-1);
      }
    }

    gBytesRead = bsize;
    //    printf("gByteRead: %d\n", gBytesRead);        

    return 0;
}
