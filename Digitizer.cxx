#include <iostream>
#include <cstring>
#include <inttypes.h>

#include "Digitizer.h"

void Digitizer::RunInit(){

    handle = -1;

    Event16  = nullptr;  Event8   = nullptr;  Event742 = nullptr;
    buffer   = nullptr;  EventPtr = nullptr;

    ErrCode= ERR_NONE;
    ReloadCfgStatus = 0x7FFFFFFF;

    nCycles = 0;    
    BufferSize = 0;  AllocatedSize = 0;
    fNb = 0; fNe = 0;

    CurrentTime  = 0;
    PrevRateTime = 0;
    ElapsedTime  = 0;

    WDrun.AcqRun = 0;    
    
    memset(&WDrun, 0, sizeof(WDrun));
    memset(&WDcfg, 0, sizeof(WDcfg));

}

//PreRun
int Digitizer::RunStart(){

    WDrun.AcqRun = 1;
    CAEN_DGTZ_SWStartAcquisition(handle);

    return 0;
}

int Digitizer::RunStop(){

    CAEN_DGTZ_SWStopAcquisition(handle);
    CAEN_DGTZ_FreeReadoutBuffer(&buffer);
    
    if(WDcfg.Nbit == 8) {
      CAEN_DGTZ_FreeEvent(handle, (void**)&Event8);
    }else{
      if (BoardInfo.FamilyCode != CAEN_DGTZ_XX742_FAMILY_CODE) {
	CAEN_DGTZ_FreeEvent(handle, (void**)&Event16);
      }
      else {
	CAEN_DGTZ_FreeEvent(handle, (void**)&Event742);
      }
    }

    /* close the output files and free histograms*/
    for (int ch = 0; ch < WDcfg.Nch; ch++) {
        if (WDrun.fout[ch])
	  fclose(WDrun.fout[ch]);
    }

    /* Need to check....*/
    WDrun.AcqRun = 0;
    SetupParameters();
    AllocateMemory();        


    return 0;
}

int Digitizer::RunEnd(){

    CAEN_DGTZ_FreeReadoutBuffer(&buffer);

    if(WDcfg.Nbit == 8) {
      CAEN_DGTZ_FreeEvent(handle, (void**)&Event8);
    }else{
      if (BoardInfo.FamilyCode != CAEN_DGTZ_XX742_FAMILY_CODE) {
	CAEN_DGTZ_FreeEvent(handle, (void**)&Event16);
      }
      else {
	CAEN_DGTZ_FreeEvent(handle, (void**)&Event742);
      }
    }
   
    CAEN_DGTZ_CloseDigitizer(handle);   

    std::cout << "Close Digitizer. "<< std::endl;

    return 0;
}

int Digitizer::ReadParameters(char *cfgFileName){

    memset(&WDrun, 0, sizeof(WDrun));
    memset(&WDcfg, 0, sizeof(WDcfg));

    FILE *cfgFile;
    cfgFile = fopen(cfgFileName, "r");
    if (cfgFile == nullptr){
        ErrCode = ERR_CONF_FILE_NOT_FOUND;
	if (ErrCode){
	  printf("\a%s\n", ErrMsg[ErrCode]);
	}
    }
    digitCfg.ParseConfigFile(cfgFile, &WDcfg);
    fclose(cfgFile);
    
    return 0;
}


int Digitizer::OpenDigitizer(){

    /* Open the digitizer and read the board information */
    isVMEDevice = WDcfg.BaseAddress ? 1 : 0;

    int ret = CAEN_DGTZ_OpenDigitizer2((CAEN_DGTZ_ConnectionType)WDcfg.LinkType,
				   (WDcfg.LinkType == CAEN_DGTZ_ETH_V4718) ? WDcfg.ipAddress:(void *)&(WDcfg.LinkNum),
				   WDcfg.ConetNode, WDcfg.BaseAddress, &handle);
    if (ret) {
        ErrCode = ERR_DGZ_OPEN;
	if (ErrCode) {
	  printf("\a%s\n", ErrMsg[ErrCode]);
	}
	return ret;
    }
    
    ret = CAEN_DGTZ_GetInfo(handle, &BoardInfo);
    if (ret) {
        ErrCode = ERR_BOARD_INFO_READ;
	if (ErrCode) {
	  printf("\a%s\n", ErrMsg[ErrCode]);
	}
	return ret;
    }
    printf("Connected to CAEN Digitizer Model %s\n", BoardInfo.ModelName);
    printf("ROC FPGA Release is %s\n", BoardInfo.ROC_FirmwareRel);
    printf("AMC FPGA Release is %s\n", BoardInfo.AMC_FirmwareRel);

    // Check firmware rivision (DPP firmwares cannot be used.
    sscanf(BoardInfo.AMC_FirmwareRel, "%d", &MajorNumber);
    if (MajorNumber >= 128) {
        printf("This digitizer has a DPP firmware\n");
        ErrCode = ERR_INVALID_BOARD_TYPE;
	if (ErrCode) {
	  printf("\a%s\n", ErrMsg[ErrCode]);
	}
	return -1;
    }


    // Get Number of Channels, Number of bits, Number of Groups of the board */
    ret = digitFunc.GetMoreBoardInfo(handle, BoardInfo, &WDcfg);
    if (ret) {
        ErrCode = ERR_INVALID_BOARD_TYPE;
	if (ErrCode) {
	  printf("\a%s\n", ErrMsg[ErrCode]);
	}
	return ret;
    }

    //Check if model x742 and x740 is in use --> Use its specific configuration file
    if (BoardInfo.FamilyCode == CAEN_DGTZ_XX742_FAMILY_CODE) {		
      std::cout << " == Configuration file for Board model x742 is necessary. " << std::endl;
      
    }else if (BoardInfo.FamilyCode == CAEN_DGTZ_XX740_FAMILY_CODE) {
      std::cout << " == Configuration file for Board model x740 is necessary. " << std::endl;      
    }

    //Check for possible board internal errors
    ret = digitFunc.CheckBoardFailureStatus(handle);
    if (ret) {
      ErrCode = ERR_BOARD_FAILURE;
      if (ErrCode) {
	printf("\a%s\n", ErrMsg[ErrCode]);
      }
      return ret;      
    }

    //set default DAC calibration coefficients
    for (int i = 0; i < MAX_SET; i++) {
      WDcfg.DAC_Calib.cal[i] = 1;
      WDcfg.DAC_Calib.offset[i] = 0;
    }
    //load DAC calibration data (if present in flash)
    if (BoardInfo.FamilyCode != CAEN_DGTZ_XX742_FAMILY_CODE)//XX742 not considered
      ret = digitCfg.Load_DAC_Calibration_From_Flash(handle, &WDcfg, BoardInfo);

    if(ret==1){
        if (BoardInfo.FamilyCode == CAEN_DGTZ_XX740_FAMILY_CODE)//XX740 specific
	    digitFunc.CalibrateXX740DCOffset(handle, &WDcfg, BoardInfo);
	else if (BoardInfo.FamilyCode != CAEN_DGTZ_XX742_FAMILY_CODE)//XX742 not considered
	    digitFunc.CalibrateDCOffset(handle, &WDcfg, BoardInfo);

	int i = 0;
	CAEN_DGTZ_ErrorCode err;
	//set new dco values using calibration data
	for (i = 0; i < (int)BoardInfo.Channels; i++) {
	    if (WDcfg.EnableMask & (1 << i)) {
	        if(WDcfg.Version_used[i] == 1)
		    digitFunc.SetCalibratedDCO(handle, i, &WDcfg, BoardInfo);
		else {
		    err = CAEN_DGTZ_SetChannelDCOffset(handle, (uint32_t)i, WDcfg.DCoffset[i]);
		    if (err)
		        printf("Error setting channel %d offset\n", i);
		}
	    }
	}      
    }
    
    // HACK
    for (int ch = 0; ch < (int)BoardInfo.Channels; ch++) {
      WDcfg.DAC_Calib.cal[ch] = 1.0;
      WDcfg.DAC_Calib.offset[ch] = 0.0;
    }

    // Perform calibration (if needed, X740 No Need). 
    if (WDcfg.StartupCalibration)
      digitFunc.Calibrate(handle, &WDrun, BoardInfo);
    
    return ret;
}

int Digitizer::SetupParameters(){

    // mask the channels not available for this model
    if ((BoardInfo.FamilyCode != CAEN_DGTZ_XX740_FAMILY_CODE) &&
	(BoardInfo.FamilyCode != CAEN_DGTZ_XX742_FAMILY_CODE)){
      
        WDcfg.EnableMask &= (1<<WDcfg.Nch)-1;
    } else {
        WDcfg.EnableMask &= (1<<(WDcfg.Nch/8))-1;
    }
    if ((BoardInfo.FamilyCode == CAEN_DGTZ_XX751_FAMILY_CODE) && WDcfg.DesMode) {
        WDcfg.EnableMask &= 0xAA;
    }
    if ((BoardInfo.FamilyCode == CAEN_DGTZ_XX731_FAMILY_CODE) && WDcfg.DesMode) {
        WDcfg.EnableMask &= 0x55;
    }

    /* ***************************************************************************** */
    /* program the digitizer                                                         */
    /* ***************************************************************************** */
    int ret = digitFunc.ProgramDigitizer(handle, WDcfg, BoardInfo);
    if (ret) {
        ErrCode = ERR_DGZ_PROGRAM;
	if (ErrCode) {
	  printf("\a%s\n", ErrMsg[ErrCode]);
	}
	return ret;
    }

    Sleep(300);

    //check for possible failures after programming the digitizer
    ret = digitFunc.CheckBoardFailureStatus(handle);
    if (ret) {
        ErrCode = ERR_BOARD_FAILURE;
	if (ErrCode) {
	  printf("\a%s\n", ErrMsg[ErrCode]);
	}
	return ret;
    }

    return ret;
}

int Digitizer::AllocateMemory(){

    int ret;

    // Read again the board infos, just in case some of them were changed by the programming
    // (like, for example, the TSample and the number of channels if DES mode is changed)
    if(ReloadCfgStatus > 0) {
        ret = CAEN_DGTZ_GetInfo(handle, &BoardInfo);
        if (ret) {
            ErrCode = ERR_BOARD_INFO_READ;
	    if (ErrCode) {
	        printf("\a%s\n", ErrMsg[ErrCode]);
	    }
	    return ret;
        }
        ret = digitFunc.GetMoreBoardInfo(handle,BoardInfo, &WDcfg);
        if (ret) {
            ErrCode = ERR_INVALID_BOARD_TYPE;
	    if (ErrCode) {
	        printf("\a%s\n", ErrMsg[ErrCode]);
	    }
	    return ret;
        }

        // Reload Correction Tables if changed
        if(BoardInfo.FamilyCode == CAEN_DGTZ_XX742_FAMILY_CODE && (ReloadCfgStatus & (0x1 << CFGRELOAD_CORRTABLES_BIT)) ) {
            if(WDcfg.useCorrections != -1) { // Use Manual Corrections
                uint32_t GroupMask = 0;

                // Disable Automatic Corrections
                if ((ret = CAEN_DGTZ_DisableDRS4Correction(handle)) != CAEN_DGTZ_Success)
  		    return -1;

                // Load the Correction Tables from the Digitizer flash
                if ((ret = CAEN_DGTZ_GetCorrectionTables(handle, WDcfg.DRS4Frequency, (void*)X742Tables)) != CAEN_DGTZ_Success)
		    return -1;

                if(WDcfg.UseManualTables != -1) { // The user wants to use some custom tables
                    uint32_t gr;
		    int32_t clret;
					
                    GroupMask = WDcfg.UseManualTables;

                    for(gr = 0; gr < WDcfg.MaxGroupNumber; gr++) {
                        if (((GroupMask>>gr)&0x1) == 0)
                            continue;
                        if ((clret = LoadCorrectionTable(WDcfg.TablesFilenames[gr], &(X742Tables[gr]))) != 0)
                            printf("Error [%d] loading custom table from file '%s' for group [%u].\n", clret, WDcfg.TablesFilenames[gr], gr);
                    }
                }
                // Save to file the Tables read from flash
                GroupMask = (~GroupMask) & ((0x1<<WDcfg.MaxGroupNumber)-1);
                SaveCorrectionTables("X742Table", GroupMask, X742Tables);
            }
            else { // Use Automatic Corrections
                if ((ret = CAEN_DGTZ_LoadDRS4CorrectionData(handle, WDcfg.DRS4Frequency)) != CAEN_DGTZ_Success)
		    return -1;
                if ((ret = CAEN_DGTZ_EnableDRS4Correction(handle)) != CAEN_DGTZ_Success)
          	    return -1;
            }
        }
    }
    
    
    // Allocate memory for the event data and readout buffer
    if(WDcfg.Nbit == 8){
        ret = CAEN_DGTZ_AllocateEvent(handle, (void**)&Event8);
    }else {
        if (BoardInfo.FamilyCode != CAEN_DGTZ_XX742_FAMILY_CODE) {
            ret = CAEN_DGTZ_AllocateEvent(handle, (void**)&Event16);
        }
        else {
            ret = CAEN_DGTZ_AllocateEvent(handle, (void**)&Event742);
        }
    }
    if (ret != CAEN_DGTZ_Success) {
        ErrCode = ERR_MALLOC;
	if (ErrCode) {
	  printf("\a%s\n", ErrMsg[ErrCode]);
	}
	return ret;
    }

    /* WARNING: This malloc must be done after the digitizer programming */
    ret = CAEN_DGTZ_MallocReadoutBuffer(handle, &buffer,&AllocatedSize); 
    if (ret) {
        ErrCode = ERR_MALLOC;
	if (ErrCode) {
	  printf("\a%s\n", ErrMsg[ErrCode]);
	}
	return ret;
    }
    
    std::cout << "CAEN Digitizer is Ready. "<< std::endl;

    return ret;
}

long Digitizer::GetTime(){
    long time_ms;
    struct timeval t1;
    struct timezone tz;
    gettimeofday(&t1, &tz);
    time_ms = (t1.tv_sec) * 1000 + t1.tv_usec / 1000;

    return time_ms;
}

int Digitizer::InterruptTimeout(){

    int ret = 0;
    /* Calculate throughput and trigger rate (every second) */
    fNb += BufferSize;
    fNe += NumEvents;
    CurrentTime = GetTime();
    ElapsedTime = CurrentTime - PrevRateTime;

    nCycles++;
    if (ElapsedTime > 1000) {
      if (fNb == 0)
	if (ret == CAEN_DGTZ_Timeout) printf ("Timeout...\n"); else printf("No data...\n");
      else
	printf("Reading at %.2f MB/s (Trg Rate: %.2f Hz)\n",
	       (float)fNb/((float)ElapsedTime*1048.576f), (float)fNe*1000.0f/(float)ElapsedTime);
      nCycles= 0;
      fNb = 0;
      fNe = 0;
      PrevRateTime = CurrentTime;
    }

    return ret;
}

int Digitizer::RunAcquisition(){

    int ret;
    /* Send a software trigger */
    if (WDrun.ContinuousTrigger) {
        CAEN_DGTZ_SendSWtrigger(handle);
    }

    /* Wait for interrupt (if enabled) */
    if (WDcfg.InterruptNumEvents > 0) {
        int32_t boardId;
	int VMEHandle = -1;
	int InterruptMask = (1 << VME_INTERRUPT_LEVEL);

	BufferSize = 0;
	NumEvents = 0;
	// Interrupt handling
	if (isVMEDevice) {
	    ret = CAEN_DGTZ_VMEIRQWait ((CAEN_DGTZ_ConnectionType)WDcfg.LinkType,
					WDcfg.LinkNum, WDcfg.ConetNode, (uint8_t)InterruptMask,
					INTERRUPT_TIMEOUT, &VMEHandle);
	}else{
	    ret = CAEN_DGTZ_IRQWait(handle, INTERRUPT_TIMEOUT);
	}
	
	if (ret == CAEN_DGTZ_Timeout)  // No active interrupt requests
               InterruptTimeout();

	if (ret != CAEN_DGTZ_Success)  {
	  ErrCode = ERR_INTERRUPT;
	  if (ErrCode) {
	    printf("\a%s\n", ErrMsg[ErrCode]);
	  }
	  return ret;
	}

	// Interrupt Ack
	if (isVMEDevice) {
	    ret = CAEN_DGTZ_VMEIACKCycle(VMEHandle, VME_INTERRUPT_LEVEL, &boardId);

	    if ((ret != CAEN_DGTZ_Success) || (boardId != VME_INTERRUPT_STATUS_ID)) {
	      InterruptTimeout();
	    } else {
	      if (INTERRUPT_MODE == CAEN_DGTZ_IRQ_MODE_ROAK)
		ret = CAEN_DGTZ_RearmInterrupt(handle);
	    }
	}
    }

    /* Read data from the board */
    ret = CAEN_DGTZ_ReadData(handle, CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT, buffer, &BufferSize);
    if (ret) {
      
        ErrCode = ERR_READOUT;
	if (ErrCode) {
	  printf("\a%s\n", ErrMsg[ErrCode]);
	}
	return ret;
    }
    std::cout << "BufferSize: " << BufferSize << std::endl;
    
    NumEvents = 0;

    if (BufferSize != 0) {
        ret = CAEN_DGTZ_GetNumEvents(handle, buffer, BufferSize, &NumEvents);
            if (ret) {
                ErrCode = ERR_READOUT;
		if (ErrCode) {
		  printf("\a%s\n", ErrMsg[ErrCode]);
		}
		return ret;
            }
	    
    } else {
        uint32_t lstatus;
	ret = CAEN_DGTZ_ReadRegister(handle, CAEN_DGTZ_ACQ_STATUS_ADD, &lstatus);
	if (ret) {
	  printf("Warning: Failure reading reg:%x (%d)\n", CAEN_DGTZ_ACQ_STATUS_ADD, ret);

	}else {
	  if (lstatus & (0x1 << 19)) {
	    ErrCode = ERR_OVERTEMP;
	    if (ErrCode) {
	        printf("\a%s\n", ErrMsg[ErrCode]);	      
	    }
	    return -1;
	  }
	}
    }
    std::cout << "NumEvents: " << NumEvents << std::endl;
    
    /* Calculate throughput and trigger rate (every second) */
    fNb += BufferSize;
    fNe += NumEvents;
    CurrentTime = GetTime();
    ElapsedTime = CurrentTime - PrevRateTime;

    nCycles++;
    if (ElapsedTime > 1000) {
      if (fNb == 0)
	if (ret == CAEN_DGTZ_Timeout) printf ("Timeout...\n"); else printf("No data...\n");
      else
	printf("Reading at %.2f MB/s (Trg Rate: %.2f Hz)\n", (float)fNb/((float)ElapsedTime*1048.576f)
	       , (float)fNe*1000.0f/(float)ElapsedTime);

      nCycles= 0;
      fNb = 0;
      fNe = 0;
      PrevRateTime = CurrentTime;
    }

    gBytesRead = BufferSize;
    
    return ret;
}

unsigned int Digitizer::GetBytesRead(){

    return gBytesRead;
}

int Digitizer::GetReadOutData(char* destBuf, uint32_t byteSize){

    if(byteSize > 0){
      std::memcpy(destBuf, buffer, byteSize);
    }else{
      return -1;
    }

    return 0;
}

int Digitizer::AnalyzeEvent(){

    int ret;
    /* Analyze data */
    for(int i = 0; i < (int)NumEvents; i++) {

      /* Get one event from the readout buffer */
      ret = CAEN_DGTZ_GetEventInfo(handle, buffer, BufferSize, i, &EventInfo, &EventPtr);
      if (ret) {
	ErrCode = ERR_EVENT_BUILD;
	if (ErrCode) {
	  printf("\a%s\n", ErrMsg[ErrCode]);	      
	}
	return ret;
      }

      /* decode the event */
      if (WDcfg.Nbit == 8) {
	ret = CAEN_DGTZ_DecodeEvent(handle, EventPtr, (void**)&Event8);
      } else {
	
	if (BoardInfo.FamilyCode != CAEN_DGTZ_XX742_FAMILY_CODE) {
	  ret = CAEN_DGTZ_DecodeEvent(handle, EventPtr, (void**)&Event16);

	} else {

	  ret = CAEN_DGTZ_DecodeEvent(handle, EventPtr, (void**)&Event742);
	}
      }
      
      if (ret) {
	  ErrCode = ERR_EVENT_BUILD;
	  if (ErrCode) {
	    printf("\a%s\n", ErrMsg[ErrCode]);	      
	  }
	  return ret;
      }

      /* Write Event data to file */
      if (WDrun.ContinuousWrite || WDrun.SingleWrite) {
	  // Note: use a thread here to allow parallel readout and file writing
	  if (BoardInfo.FamilyCode == CAEN_DGTZ_XX742_FAMILY_CODE) {	
	      ret = digitFunc.WriteOutputFilesx742(&WDcfg, &WDrun, &EventInfo, Event742); 
	  
	  }else if (WDcfg.Nbit == 8) {
	      ret = digitFunc.WriteOutputFiles(&WDcfg, &WDrun, &EventInfo, Event8);

	  }else {
	      ret = digitFunc.WriteOutputFiles(&WDcfg, &WDrun, &EventInfo, Event16);
	  }
	  
	  if (ret) {
	      ErrCode = ERR_OUTFILE_WRITE;
	      if (ErrCode) {
		printf("\a%s\n", ErrMsg[ErrCode]);	      
	      }
	      return ret;
	  }

	  if (WDrun.SingleWrite) {
	    printf("Single Event saved to output files\n");
	    WDrun.SingleWrite = 0;
	  }
      }
    }

    return ret;    
}


