#include <iostream>
#include <cstring>
#include <inttypes.h>

#include "Digitizer.h"
#include "DigitizerFunc.h"
#include "DigitizerConfig.h"


double DigitizerFunc::linear_interp(double x0, double y0, double x1, double y1, double x) {
  //    if (x1 - x0 == 0) {
    const double EPSILON = 1e-9;
    if (std::fabs(x1 - x0) < EPSILON) {
        fprintf(stderr, "Cannot interpolate values with same x.\n");
        return HUGE_VAL;
    }
    else {
        const double m = (y1 - y0) / (x1 - x0);
        const double q = y1 - m * x1;
        return m * x + q;
    }
}

int DigitizerFunc::WriteOutputFiles(WaveDumpConfig_t *WDcfg, WaveDumpRun_t *WDrun,
				    CAEN_DGTZ_EventInfo_t *EventInfo, void *Event){

    int ch, j, ns;
    CAEN_DGTZ_UINT16_EVENT_t  *Event16 = NULL;
    CAEN_DGTZ_UINT8_EVENT_t   *Event8 = NULL;

    if (WDcfg->Nbit == 8)
        Event8 = (CAEN_DGTZ_UINT8_EVENT_t *)Event;
    else
        Event16 = (CAEN_DGTZ_UINT16_EVENT_t *)Event;

    for (ch = 0; ch < WDcfg->Nch; ch++) {
        int Size = (WDcfg->Nbit == 8) ? Event8->ChSize[ch] : Event16->ChSize[ch];
        if (Size <= 0) {
            continue;
        }

        // Check the file format type
        if( WDcfg->OutFileFlags & OFF_BINARY) {
            // Binary file format
            uint32_t BinHeader[6];
            BinHeader[0] = (WDcfg->Nbit == 8) ? Size + 6*sizeof(*BinHeader) : Size*2 + 6*sizeof(*BinHeader);
            BinHeader[1] = EventInfo->BoardId;
            BinHeader[2] = EventInfo->Pattern;
            BinHeader[3] = ch;
            BinHeader[4] = EventInfo->EventCounter;
            BinHeader[5] = EventInfo->TriggerTimeTag;
            if (!WDrun->fout[ch]) {
                char fname[100];
                sprintf(fname, "swave%d.dat",ch);
                if ((WDrun->fout[ch] = fopen(fname, "wb")) == NULL)
                    return -1;
            }
            if( WDcfg->OutFileFlags & OFF_HEADER) {
                // Write the Channel Header
                if(fwrite(BinHeader, sizeof(*BinHeader), 6, WDrun->fout[ch]) != 6) {
                    // error writing to file
                    fclose(WDrun->fout[ch]);
                    WDrun->fout[ch]= NULL;
                    return -1;
                }
            }
            if (WDcfg->Nbit == 8)
                ns = (int)fwrite(Event8->DataChannel[ch], 1, Size, WDrun->fout[ch]);
            else
                ns = (int)fwrite(Event16->DataChannel[ch] , 1 , Size*2, WDrun->fout[ch]) / 2;

            if (ns != Size) {
                // error writing to file
                fclose(WDrun->fout[ch]);
                WDrun->fout[ch]= NULL;
                return -1;
            }
        } else {
            // Ascii file format
            if (!WDrun->fout[ch]) {
                char fname[100];
                sprintf(fname, "swave%d.txt", ch);
                if ((WDrun->fout[ch] = fopen(fname, "w")) == NULL)
                    return -1;
            }
            if( WDcfg->OutFileFlags & OFF_HEADER) {
                // Write the Channel Header
                fprintf(WDrun->fout[ch], "Record Length: %d\n", Size);
                fprintf(WDrun->fout[ch], "BoardID: %2d\n", EventInfo->BoardId);
                fprintf(WDrun->fout[ch], "Channel: %d\n", ch);
                fprintf(WDrun->fout[ch], "Event Number: %d\n", EventInfo->EventCounter);
                fprintf(WDrun->fout[ch], "Pattern: 0x%04X\n", EventInfo->Pattern & 0xFFFF);
                fprintf(WDrun->fout[ch], "Trigger Time Stamp: %u\n", EventInfo->TriggerTimeTag);
                fprintf(WDrun->fout[ch], "DC offset (DAC): 0x%04X\n", WDcfg->DCoffset[ch] & 0xFFFF);
            }
            for(j=0; j<Size; j++) {
                if (WDcfg->Nbit == 8)
                    fprintf(WDrun->fout[ch], "%d\n", Event8->DataChannel[ch][j]);
                else
                    fprintf(WDrun->fout[ch], "%d\n", Event16->DataChannel[ch][j]);
            }
        }
        if (WDrun->SingleWrite) {
            fclose(WDrun->fout[ch]);
            WDrun->fout[ch]= NULL;
        }
    }
    return 0;

}


int DigitizerFunc::WriteOutputFilesx742(WaveDumpConfig_t *WDcfg, WaveDumpRun_t *WDrun,
				    CAEN_DGTZ_EventInfo_t *EventInfo, CAEN_DGTZ_X742_EVENT_t *Event)
{
    int gr,ch, j, ns;
    char trname[10], flag = 0; 

    for (gr=0;gr<(WDcfg->Nch/8);gr++) {
        if (Event->GrPresent[gr]) {
            for(ch=0; ch<9; ch++) {
                int Size = Event->DataGroup[gr].ChSize[ch];
                if (Size <= 0) {
                    continue;
                }

                // Check the file format type
                if( WDcfg->OutFileFlags & OFF_BINARY) {
                    // Binary file format
                    uint32_t BinHeader[8];
                    BinHeader[0] = (WDcfg->Nbit == 8) ? Size + 8*sizeof(*BinHeader) : Size*4 + 8*sizeof(*BinHeader);
                    BinHeader[1] = EventInfo->BoardId;
                    BinHeader[2] = EventInfo->Pattern;
                    BinHeader[3] = (gr * 8) + ch;
                    BinHeader[4] = EventInfo->EventCounter;
                    BinHeader[5] = Event->DataGroup[gr].TriggerTimeTag;
                    if ((gr * 9 + ch) == 8 || (gr * 9 + ch) == 17)
                        BinHeader[6] = WDcfg->FTDCoffset[0] & 0xFFFF;
                    else if ((gr * 9 + ch) == 26 || (gr * 9 + ch) == 35)
                        BinHeader[6] = WDcfg->FTDCoffset[2] & 0xFFFF;
                    else
                        BinHeader[6] = WDcfg->DCoffset[gr] & 0xFFFF;
                    
                    BinHeader[7] = (uint32_t)Event->DataGroup[gr].StartIndexCell;
                    if (!WDrun->fout[(gr*9+ch)]) {
                        char fname[100];
                        if ((gr*9+ch) == 8) {
                            sprintf(fname, "sTR_0_%d.dat",gr);
                            sprintf(trname,"TR_0_0");
                            flag = 1;
                        }
                        else if ((gr*9+ch) == 17) {
                            sprintf(fname, "sTR_0_%d.dat",gr);
                            sprintf(trname,"TR_0_1");
                            flag = 1;
                        }
                        else if ((gr*9+ch) == 26) {
                            sprintf(fname, "sTR_1_%d.dat",gr);
                            sprintf(trname,"TR_1_0");
                            flag = 1;
                        }
                        else if ((gr*9+ch) == 35) {
                            sprintf(fname, "TR_1_%d.dat",gr);
                            sprintf(trname,"TR_1_1");
                            flag = 1;
                        }
                        else 	{
                            sprintf(fname, "swave_%d.dat",(gr*8)+ch);
                            flag = 0;
                        }
                        if ((WDrun->fout[(gr*9+ch)] = fopen(fname, "wb")) == NULL)
                            return -1;
                    }
                    if( WDcfg->OutFileFlags & OFF_HEADER) {
                        // Write the Channel Header
                        if(fwrite(BinHeader, sizeof(*BinHeader), 8, WDrun->fout[(gr*9+ch)]) != 8) {
                            // error writing to file
                            fclose(WDrun->fout[(gr*9+ch)]);
                            WDrun->fout[(gr*9+ch)]= NULL;
                            return -1;
                        }
                    }
                    ns = (int)fwrite( Event->DataGroup[gr].DataChannel[ch] , 1 , Size*4, WDrun->fout[(gr*9+ch)]) / 4;
                    if (ns != Size) {
                        // error writing to file
                        fclose(WDrun->fout[(gr*9+ch)]);
                        WDrun->fout[(gr*9+ch)]= NULL;
                        return -1;
                    }
                } else {
                    // Ascii file format
                    if (!WDrun->fout[(gr*9+ch)]) {
                        char fname[100];
                        if ((gr*9+ch) == 8) {
                            sprintf(fname, "sTR_0_%d.txt",gr);
                            sprintf(trname,"TR_0_0");
                            flag = 1;
                        }
                        else if ((gr*9+ch) == 17) {
                            sprintf(fname, "sTR_0_%d.txt",gr );
                            sprintf(trname,"TR_0_1");
                            flag = 1;
                        }
                        else if ((gr*9+ch) == 26) {
                            sprintf(fname, "sTR_1_%d.txt",gr);
                            sprintf(trname,"TR_1_0");
                            flag = 1;
                        }
                        else if ((gr*9+ch) == 35) {
                            sprintf(fname, "sTR_1_%d.txt",gr);
                            sprintf(trname,"TR_1_1");
                            flag = 1;
                        }
                        else 	{
                            sprintf(fname, "swave_%d.txt",(gr*8)+ch);
                            flag = 0;
                        }
                        if ((WDrun->fout[(gr*9+ch)] = fopen(fname, "w")) == NULL)
                            return -1;
                    }
                    if( WDcfg->OutFileFlags & OFF_HEADER) {
                        // Write the Channel Header
                        fprintf(WDrun->fout[(gr*9+ch)], "Record Length: %d\n", Size);
                        fprintf(WDrun->fout[(gr*9+ch)], "BoardID: %2d\n", EventInfo->BoardId);
                        if (flag)
                            fprintf(WDrun->fout[(gr*9+ch)], "Channel: %s\n",  trname);
                        else
                            fprintf(WDrun->fout[(gr*9+ch)], "Channel: %d\n",  (gr*8)+ ch);
                        fprintf(WDrun->fout[(gr*9+ch)], "Event Number: %d\n", EventInfo->EventCounter);
                        fprintf(WDrun->fout[(gr*9+ch)], "Pattern: 0x%04X\n", EventInfo->Pattern & 0xFFFF);
                        fprintf(WDrun->fout[(gr*9+ch)], "Trigger Time Stamp: %u\n", Event->DataGroup[gr].TriggerTimeTag);
                        if ((gr * 9 + ch) == 8 || (gr * 9 + ch) == 17)
                            fprintf(WDrun->fout[(gr*9+ch)], "DC offset (DAC): 0x%04X\n", WDcfg->FTDCoffset[0] & 0xFFFF);
                        else if ((gr * 9 + ch) == 26 || (gr * 9 + ch) == 35)
                            fprintf(WDrun->fout[(gr * 9 + ch)], "DC offset (DAC): 0x%04X\n", WDcfg->FTDCoffset[2] & 0xFFFF);
                        else
                            fprintf(WDrun->fout[(gr * 9 + ch)], "DC offset (DAC): 0x%04X\n", WDcfg->DCoffset[gr] & 0xFFFF);
                        fprintf(WDrun->fout[(gr*9+ch)], "Start Index Cell: %d\n", Event->DataGroup[gr].StartIndexCell);
                        flag = 0;
                    }
                    for(j=0; j<Size; j++) {
                        fprintf(WDrun->fout[(gr*9+ch)], "%f\n", Event->DataGroup[gr].DataChannel[ch][j]);
                    }
                }
                if (WDrun->SingleWrite) {
                    fclose(WDrun->fout[(gr*9+ch)]);
                    WDrun->fout[(gr*9+ch)]= NULL;
                }
            }
        }
    }

    return 0;
}

/*! \fn      int GetMoreBoardNumChannels(CAEN_DGTZ_BoardInfo_t BoardInfo,  WaveDumpConfig_t *WDcfg)
*   \brief   calculate num of channels, num of bit and sampl period according to the board type
*
*   \param   BoardInfo   Board Type
*   \param   WDcfg       pointer to the config. struct
*   \return  0 = Success; -1 = unknown board type
*/
int DigitizerFunc::GetMoreBoardInfo(int handle, CAEN_DGTZ_BoardInfo_t BoardInfo, WaveDumpConfig_t *WDcfg){
    int ret;
    switch(BoardInfo.FamilyCode) {
        CAEN_DGTZ_DRS4Frequency_t freq;

    case CAEN_DGTZ_XX724_FAMILY_CODE:
    case CAEN_DGTZ_XX781_FAMILY_CODE:
    case CAEN_DGTZ_XX782_FAMILY_CODE:
    case CAEN_DGTZ_XX780_FAMILY_CODE:
        WDcfg->Nbit = 14; WDcfg->Ts = 10.0; break;
    case CAEN_DGTZ_XX720_FAMILY_CODE: WDcfg->Nbit = 12; WDcfg->Ts = 4.0;  break;
    case CAEN_DGTZ_XX721_FAMILY_CODE: WDcfg->Nbit =  8; WDcfg->Ts = 2.0;  break;
    case CAEN_DGTZ_XX731_FAMILY_CODE: WDcfg->Nbit =  8; WDcfg->Ts = 2.0;  break;
    case CAEN_DGTZ_XX751_FAMILY_CODE: WDcfg->Nbit = 10; WDcfg->Ts = 1.0;  break;
    case CAEN_DGTZ_XX761_FAMILY_CODE: WDcfg->Nbit = 10; WDcfg->Ts = 0.25;  break;
    case CAEN_DGTZ_XX740_FAMILY_CODE: WDcfg->Nbit = 12; WDcfg->Ts = 16.0; break;
    case CAEN_DGTZ_XX725_FAMILY_CODE: WDcfg->Nbit = 14; WDcfg->Ts = 4.0; break;
    case CAEN_DGTZ_XX730_FAMILY_CODE: WDcfg->Nbit = 14; WDcfg->Ts = 2.0; break;
    case CAEN_DGTZ_XX742_FAMILY_CODE: 
        WDcfg->Nbit = 12; 
        if ((ret = CAEN_DGTZ_GetDRS4SamplingFrequency(handle, &freq)) != CAEN_DGTZ_Success) return CAEN_DGTZ_CommError;
        switch (freq) {
        case CAEN_DGTZ_DRS4_1GHz:
            WDcfg->Ts = 1.0;
            break;
        case CAEN_DGTZ_DRS4_2_5GHz:
            WDcfg->Ts = (float)0.4;
            break;
        case CAEN_DGTZ_DRS4_5GHz:
            WDcfg->Ts = (float)0.2;
            break;
		case CAEN_DGTZ_DRS4_750MHz:
            WDcfg->Ts = (float)(1.0 / 750.0 * 1000.0);
            break;
        }
        switch(BoardInfo.FormFactor) {
        case CAEN_DGTZ_VME64_FORM_FACTOR:
        case CAEN_DGTZ_VME64X_FORM_FACTOR:
            WDcfg->MaxGroupNumber = 4;
            break;
        case CAEN_DGTZ_DESKTOP_FORM_FACTOR:
        case CAEN_DGTZ_NIM_FORM_FACTOR:
        default:
            WDcfg->MaxGroupNumber = 2;
            break;
        }
        break;
    default: return -1;
    }
    if (((BoardInfo.FamilyCode == CAEN_DGTZ_XX751_FAMILY_CODE) ||
        (BoardInfo.FamilyCode == CAEN_DGTZ_XX731_FAMILY_CODE) ) && WDcfg->DesMode)
        WDcfg->Ts /= 2;

    switch(BoardInfo.FamilyCode) {
    case CAEN_DGTZ_XX724_FAMILY_CODE:
    case CAEN_DGTZ_XX781_FAMILY_CODE:
    case CAEN_DGTZ_XX782_FAMILY_CODE:
    case CAEN_DGTZ_XX780_FAMILY_CODE:
    case CAEN_DGTZ_XX720_FAMILY_CODE:
    case CAEN_DGTZ_XX721_FAMILY_CODE:
    case CAEN_DGTZ_XX751_FAMILY_CODE:
    case CAEN_DGTZ_XX761_FAMILY_CODE:
    case CAEN_DGTZ_XX731_FAMILY_CODE:
        switch(BoardInfo.FormFactor) {
        case CAEN_DGTZ_VME64_FORM_FACTOR:
        case CAEN_DGTZ_VME64X_FORM_FACTOR:
            WDcfg->Nch = 8;
            break;
        case CAEN_DGTZ_DESKTOP_FORM_FACTOR:
        case CAEN_DGTZ_NIM_FORM_FACTOR:
            WDcfg->Nch = 4;
            break;
        }
        break;
    case CAEN_DGTZ_XX725_FAMILY_CODE:
    case CAEN_DGTZ_XX730_FAMILY_CODE:
        switch(BoardInfo.FormFactor) {
        case CAEN_DGTZ_VME64_FORM_FACTOR:
        case CAEN_DGTZ_VME64X_FORM_FACTOR:
            WDcfg->Nch = 16;
            break;
        case CAEN_DGTZ_DESKTOP_FORM_FACTOR:
        case CAEN_DGTZ_NIM_FORM_FACTOR:
            WDcfg->Nch = 8;
            break;
        }
        break;
    case CAEN_DGTZ_XX740_FAMILY_CODE:
        switch( BoardInfo.FormFactor) {
        case CAEN_DGTZ_VME64_FORM_FACTOR:
        case CAEN_DGTZ_VME64X_FORM_FACTOR:
            WDcfg->Nch = 64;
            break;
        case CAEN_DGTZ_DESKTOP_FORM_FACTOR:
        case CAEN_DGTZ_NIM_FORM_FACTOR:
            WDcfg->Nch = 32;
            break;
        }
        break;
    case CAEN_DGTZ_XX742_FAMILY_CODE:
        switch( BoardInfo.FormFactor) {
        case CAEN_DGTZ_VME64_FORM_FACTOR:
        case CAEN_DGTZ_VME64X_FORM_FACTOR:
            WDcfg->Nch = 36;
            break;
        case CAEN_DGTZ_DESKTOP_FORM_FACTOR:
        case CAEN_DGTZ_NIM_FORM_FACTOR:
            WDcfg->Nch = 16;
            break;
        }
        break;
    default:
        return -1;
    }
    return 0;
}


/*! \fn      int WriteRegisterBitmask(int32_t handle, uint32_t address, uint32_t data, uint32_t mask)
*   \brief   writes 'data' on register at 'address' using 'mask' as bitmask
*
*   \param   handle :   Digitizer handle
*   \param   address:   Address of the Register to write
*   \param   data   :   Data to Write on the Register
*   \param   mask   :   Bitmask to use for data masking
*   \return  0 = Success; negative numbers are error codes
*/
int DigitizerFunc::WriteRegisterBitmask(int32_t handle, uint32_t address, uint32_t data, uint32_t mask) {
    int32_t ret = CAEN_DGTZ_Success;
    uint32_t d32 = 0xFFFFFFFF;

    ret = CAEN_DGTZ_ReadRegister(handle, address, &d32);
    if(ret != CAEN_DGTZ_Success)
        return ret;

    data &= mask;
    d32 &= ~mask;
    d32 |= data;
    ret = CAEN_DGTZ_WriteRegister(handle, address, d32);
    return ret;
}

int DigitizerFunc::CheckBoardFailureStatus(int handle) {

	int ret = 0;
	uint32_t status = 0;
	ret = CAEN_DGTZ_ReadRegister(handle, 0x8104, &status);
	if (ret != 0) {
		printf("Error: Unable to read board failure status.\n");
		return -1;
	}

	Sleep(200);

	//read twice (first read clears the previous status)
	ret = CAEN_DGTZ_ReadRegister(handle, 0x8104, &status);
	if (ret != 0) {
		printf("Error: Unable to read board failure status.\n");
		return -1;
	}

	if(!(status & (1 << 7))) {
		printf("Board error detected: PLL not locked.\n");
		return -1;
	}

	return 0;
}


/*! \fn      int DoProgramDigitizer(int handle, WaveDumpConfig_t WDcfg)
*   \brief   configure the digitizer according to the parameters read from
*            the cofiguration file and saved in the WDcfg data structure
*
*   \param   handle   Digitizer handle
*   \param   WDcfg:   WaveDumpConfig data structure
*   \return  0 = Success; negative numbers are error codes
*/
int DigitizerFunc::DoProgramDigitizer(int handle, WaveDumpConfig_t* WDcfg, CAEN_DGTZ_BoardInfo_t BoardInfo){
    int i, j, ret = 0;

    /* reset the digitizer */
    ret |= CAEN_DGTZ_Reset(handle);
    if (ret != 0) {
        printf("Error: Unable to reset digitizer.\nPlease reset digitizer manually then restart the program\n");
        return -1;
    }

    // Set the waveform test bit for debugging
    if (WDcfg->TestPattern)
        ret |= CAEN_DGTZ_WriteRegister(handle, CAEN_DGTZ_BROAD_CH_CONFIGBIT_SET_ADD, 1<<3);
    // custom setting for X742 boards
    if (BoardInfo.FamilyCode == CAEN_DGTZ_XX742_FAMILY_CODE) {
      ret |= CAEN_DGTZ_SetFastTriggerDigitizing(handle, (CAEN_DGTZ_EnaDis_t)WDcfg->FastTriggerEnabled);
        ret |= CAEN_DGTZ_SetFastTriggerMode(handle,WDcfg->FastTriggerMode);
    }
    if ((BoardInfo.FamilyCode == CAEN_DGTZ_XX751_FAMILY_CODE) || (BoardInfo.FamilyCode == CAEN_DGTZ_XX731_FAMILY_CODE)) {
        ret |= CAEN_DGTZ_SetDESMode(handle, WDcfg->DesMode);
    }
    ret |= CAEN_DGTZ_SetRecordLength(handle, WDcfg->RecordLength);
    ret |= CAEN_DGTZ_GetRecordLength(handle, &(WDcfg->RecordLength));

    if (BoardInfo.FamilyCode == CAEN_DGTZ_XX740_FAMILY_CODE || BoardInfo.FamilyCode == CAEN_DGTZ_XX724_FAMILY_CODE) {
        ret |= CAEN_DGTZ_SetDecimationFactor(handle, WDcfg->DecimationFactor);
    }

    ret |= CAEN_DGTZ_SetPostTriggerSize(handle, WDcfg->PostTrigger);
    if(BoardInfo.FamilyCode != CAEN_DGTZ_XX742_FAMILY_CODE) {
        uint32_t pt;
        ret |= CAEN_DGTZ_GetPostTriggerSize(handle, &pt);
        WDcfg->PostTrigger = pt;
    }
    ret |= CAEN_DGTZ_SetIOLevel(handle, WDcfg->FPIOtype);
    if( WDcfg->InterruptNumEvents > 0) {
        // Interrupt handling
        if( ret |= CAEN_DGTZ_SetInterruptConfig( handle, CAEN_DGTZ_ENABLE,
            VME_INTERRUPT_LEVEL, VME_INTERRUPT_STATUS_ID,
            (uint16_t)WDcfg->InterruptNumEvents, INTERRUPT_MODE)!= CAEN_DGTZ_Success) {
                printf( "\nError configuring interrupts. Interrupts disabled\n\n");
                WDcfg->InterruptNumEvents = 0;
        }
    }
	
    ret |= CAEN_DGTZ_SetMaxNumEventsBLT(handle, WDcfg->NumEvents);
    ret |= CAEN_DGTZ_SetAcquisitionMode(handle, CAEN_DGTZ_SW_CONTROLLED);
    ret |= CAEN_DGTZ_SetExtTriggerInputMode(handle, WDcfg->ExtTriggerMode);

    if ((BoardInfo.FamilyCode == CAEN_DGTZ_XX740_FAMILY_CODE) || (BoardInfo.FamilyCode == CAEN_DGTZ_XX742_FAMILY_CODE)){
        ret |= CAEN_DGTZ_SetGroupEnableMask(handle, WDcfg->EnableMask);
        for(i=0; i<(WDcfg->Nch/8); i++) {
            if (WDcfg->EnableMask & (1<<i)) {
                if (BoardInfo.FamilyCode == CAEN_DGTZ_XX742_FAMILY_CODE) {
                    for(j=0; j<8; j++) {
                        if (WDcfg->DCoffsetGrpCh[i][j] != -1)
                            ret |= CAEN_DGTZ_SetChannelDCOffset(handle,(i*8)+j, WDcfg->DCoffsetGrpCh[i][j]);
			else
                            ret |= CAEN_DGTZ_SetChannelDCOffset(handle, (i * 8) + j, WDcfg->DCoffset[i]);

                    }
                }
                else {
                    if(WDcfg->Version_used[i] == 1)
		        ret |= SetCalibratedDCO(handle, i, WDcfg, BoardInfo);
		    else
		        ret |= CAEN_DGTZ_SetGroupDCOffset(handle, i, WDcfg->DCoffset[i]);
		    
                    ret |= CAEN_DGTZ_SetGroupSelfTrigger(handle, WDcfg->ChannelTriggerMode[i], (1<<i));
                    ret |= CAEN_DGTZ_SetGroupTriggerThreshold(handle, i, WDcfg->Threshold[i]);
                    ret |= CAEN_DGTZ_SetChannelGroupMask(handle, i, WDcfg->GroupTrgEnableMask[i]);
                } 
                ret |= CAEN_DGTZ_SetTriggerPolarity(handle, i, (CAEN_DGTZ_TriggerPolarity_t)WDcfg->PulsePolarity[i]); //.TriggerEdge

            }
        }
    } else {
        ret |= CAEN_DGTZ_SetChannelEnableMask(handle, WDcfg->EnableMask);
        for (i = 0; i < WDcfg->Nch; i++) {
            if (WDcfg->EnableMask & (1<<i)) {
				if (WDcfg->Version_used[i] == 1)
					ret |= SetCalibratedDCO(handle, i, WDcfg, BoardInfo);
				else
					ret |= CAEN_DGTZ_SetChannelDCOffset(handle, i, WDcfg->DCoffset[i]);
                if (BoardInfo.FamilyCode != CAEN_DGTZ_XX730_FAMILY_CODE &&
                    BoardInfo.FamilyCode != CAEN_DGTZ_XX725_FAMILY_CODE)
                    ret |= CAEN_DGTZ_SetChannelSelfTrigger(handle, WDcfg->ChannelTriggerMode[i], (1<<i));
                ret |= CAEN_DGTZ_SetChannelTriggerThreshold(handle, i, WDcfg->Threshold[i]);
                ret |= CAEN_DGTZ_SetTriggerPolarity(handle, i, (CAEN_DGTZ_TriggerPolarity_t)WDcfg->PulsePolarity[i]); //.TriggerEdge
            }
        }
        if (BoardInfo.FamilyCode == CAEN_DGTZ_XX730_FAMILY_CODE ||
            BoardInfo.FamilyCode == CAEN_DGTZ_XX725_FAMILY_CODE) {
            // channel pair settings for x730 boards
            for (i = 0; i < WDcfg->Nch; i += 2) {
                if (WDcfg->EnableMask & (0x3 << i)) {
                    CAEN_DGTZ_TriggerMode_t mode = WDcfg->ChannelTriggerMode[i];
                    uint32_t pair_chmask = 0;

                    // Build mode and relevant channelmask. The behaviour is that,
                    // if the triggermode of one channel of the pair is DISABLED,
                    // this channel doesn't take part to the trigger generation.
                    // Otherwise, if both are different from DISABLED, the one of
                    // the even channel is used.
                    if (WDcfg->ChannelTriggerMode[i] != CAEN_DGTZ_TRGMODE_DISABLED) {
                        if (WDcfg->ChannelTriggerMode[i + 1] == CAEN_DGTZ_TRGMODE_DISABLED)
                            pair_chmask = (0x1 << i);
                        else
                            pair_chmask = (0x3 << i);
                    }
                    else {
                        mode = WDcfg->ChannelTriggerMode[i + 1];
                        pair_chmask = (0x2 << i);
                    }

                    pair_chmask &= WDcfg->EnableMask;
                    ret |= CAEN_DGTZ_SetChannelSelfTrigger(handle, mode, pair_chmask);
                }
            }
        }
    }
    if (BoardInfo.FamilyCode == CAEN_DGTZ_XX742_FAMILY_CODE) {
        for(i=0; i<(WDcfg->Nch/8); i++) {
            ret |= CAEN_DGTZ_SetDRS4SamplingFrequency(handle, WDcfg->DRS4Frequency);
            ret |= CAEN_DGTZ_SetGroupFastTriggerDCOffset(handle,i,WDcfg->FTDCoffset[i]);
            ret |= CAEN_DGTZ_SetGroupFastTriggerThreshold(handle,i,WDcfg->FTThreshold[i]);
        }
    }

    /* execute generic write commands */
    for(i=0; i<WDcfg->GWn; i++)
        ret |= WriteRegisterBitmask(handle, WDcfg->GWaddr[i], WDcfg->GWdata[i], WDcfg->GWmask[i]);

    if (ret)
        printf("Warning: errors found during the programming of the digitizer.\nSome settings may not be executed\n");

    return ret;
}

/*! \fn      int ProgramDigitizerWithRelativeThreshold(int handle, WaveDumpConfig_t WDcfg)
*   \brief   configure the digitizer according to the parameters read from the cofiguration
*            file and saved in the WDcfg data structure, performing a calibration of the
*            DCOffset to set the required BASELINE_LEVEL.
*
*   \param   handle   Digitizer handle
*   \param   WDcfg:   WaveDumpConfig data structure
*   \return  0 = Success; negative numbers are error codes
*/
int DigitizerFunc::ProgramDigitizerWithRelativeThreshold(int handle, WaveDumpConfig_t* WDcfg, CAEN_DGTZ_BoardInfo_t BoardInfo) {
    int32_t ret;

    if ((ret = DoProgramDigitizer(handle, WDcfg, BoardInfo)) != 0)
        return ret;
    if (BoardInfo.FamilyCode != CAEN_DGTZ_XX742_FAMILY_CODE) { //XX742 not considered
        if ((ret = SetRelativeThreshold(handle, WDcfg, BoardInfo)) != CAEN_DGTZ_Success) {
            fprintf(stderr, "Error setting relative threshold. Fallback to normal ProgramDigitizer.");
            DoProgramDigitizer(handle, WDcfg, BoardInfo); // Rollback
            return ret;
        }
    }
    return CAEN_DGTZ_Success;
}

/*! \fn      int ProgramDigitizerWithRelativeThreshold(int handle, WaveDumpConfig_t WDcfg)
*   \brief   configure the digitizer according to the parameters read from the cofiguration
*            file and saved in the WDcfg data structure, performing a calibration of the
*            DCOffset to set the required BASELINE_LEVEL (if provided).
*
*   \param   handle   Digitizer handle
*   \param   WDcfg:   WaveDumpConfig data structure
*   \return  0 = Success; negative numbers are error codes
*/
int DigitizerFunc::ProgramDigitizer(int handle, WaveDumpConfig_t WDcfg, CAEN_DGTZ_BoardInfo_t BoardInfo) {
    
    int32_t ch;
    for (ch = 0; ch < (int32_t)BoardInfo.Channels; ch++) {
        if (WDcfg.EnableMask & (1 << ch) && WDcfg.Version_used[ch] == 1)
            return ProgramDigitizerWithRelativeThreshold(handle, &WDcfg, BoardInfo);
    }
    return DoProgramDigitizer(handle, &WDcfg, BoardInfo);
}

/*! \brief   return TRUE if board descriped by 'BoardInfo' supports
*            calibration or not.
*
*   \param   BoardInfo board descriptor
*/
int32_t DigitizerFunc::BoardSupportsCalibration(CAEN_DGTZ_BoardInfo_t BoardInfo) {
    return
        BoardInfo.FamilyCode == CAEN_DGTZ_XX761_FAMILY_CODE ||
        BoardInfo.FamilyCode == CAEN_DGTZ_XX751_FAMILY_CODE ||
        BoardInfo.FamilyCode == CAEN_DGTZ_XX730_FAMILY_CODE ||
        BoardInfo.FamilyCode == CAEN_DGTZ_XX725_FAMILY_CODE;
}

/*! \brief   return TRUE if board descriped by 'BoardInfo' supports
*            temperature read or not.
*
*   \param   BoardInfo board descriptor
*/
int32_t DigitizerFunc::BoardSupportsTemperatureRead(CAEN_DGTZ_BoardInfo_t BoardInfo) {
    return
        BoardInfo.FamilyCode == CAEN_DGTZ_XX751_FAMILY_CODE ||
        BoardInfo.FamilyCode == CAEN_DGTZ_XX730_FAMILY_CODE ||
        BoardInfo.FamilyCode == CAEN_DGTZ_XX725_FAMILY_CODE;
}

/*! \brief   Write the event data on x742 boards into the output files
*
*   \param   WDrun Pointer to the WaveDumpRun data structure
*   \param   WDcfg Pointer to the WaveDumpConfig data structure
*   \param   EventInfo Pointer to the EventInfo data structure
*   \param   Event Pointer to the Event to write
*/
void DigitizerFunc::Calibrate(int handle, WaveDumpRun_t *WDrun, CAEN_DGTZ_BoardInfo_t BoardInfo) {
    printf("\n");
    if (BoardSupportsCalibration(BoardInfo)) {
        if (WDrun->AcqRun == 0) {
            int32_t ret = CAEN_DGTZ_Calibrate(handle);
            if (ret == CAEN_DGTZ_Success) {
                printf("ADC Calibration check: the board is calibrated.\n");
            }
            else {
                printf("ADC Calibration failed. CAENDigitizer ERR %d\n", ret);
            }
            printf("\n");
        }
        else {
            printf("Can't run ADC calibration while acquisition is running.\n");
        }
    }
    else {
        printf("ADC Calibration not needed for this board family.\n");
    }
}


/*! \fn      void CalibrateXX740DCOffset(int handle, WaveDumpConfig_t WDcfg, CAEN_DGTZ_BoardInfo_t BoardInfo)
*   \brief   calibrates DAC of enabled channel groups (only if BASELINE_SHIFT is in use)
*
*   \param   handle   Digitizer handle
*   \param   WDcfg:   Pointer to the WaveDumpConfig_t data structure
*   \param   BoardInfo: structure with the board info
*/
int DigitizerFunc::CalibrateXX740DCOffset(int handle, WaveDumpConfig_t *WDcfg, CAEN_DGTZ_BoardInfo_t BoardInfo){
	float cal[MAX_CH];
	float offset[MAX_CH] = { 0 };
	int i = 0, acq = 0, k = 0, p=0, g = 0;
	for (i = 0; i < MAX_CH; i++)
		cal[i] = 1;
	//	CAEN_DGTZ_ErrorCode ret;
	int ret;
	CAEN_DGTZ_AcqMode_t mem_mode;
	uint32_t  AllocatedSize;

	//	ERROR_CODES ErrCode = ERR_NONE;
	ErrCode = ERR_NONE;
	
	uint32_t BufferSize;
	CAEN_DGTZ_EventInfo_t       EventInfo;
	char *buffer = NULL;
	char *EventPtr = NULL;
	CAEN_DGTZ_UINT16_EVENT_t    *Event16 = NULL;

	float avg_value[NPOINTS][MAX_CH] = { 0 };
	uint32_t dc[NPOINTS] = { 25,75 }; //test values (%)
	uint32_t groupmask = 0;

	ret = CAEN_DGTZ_GetAcquisitionMode(handle, &mem_mode);//chosen value stored
	if (ret)
	    printf("Error trying to read acq mode!!\n");
	ret = CAEN_DGTZ_SetAcquisitionMode(handle, CAEN_DGTZ_SW_CONTROLLED);
	if (ret)
	    printf("Error trying to set acq mode!!\n");
	ret = CAEN_DGTZ_SetExtTriggerInputMode(handle, CAEN_DGTZ_TRGMODE_DISABLED);
	if (ret)
	    printf("Error trying to set ext trigger!!\n");
	ret = CAEN_DGTZ_SetMaxNumEventsBLT(handle, 1);
	if (ret)
	    printf("Warning: error setting max BLT number\n");
	ret = CAEN_DGTZ_SetDecimationFactor(handle, 1);
	if (ret)
	    printf("Error trying to set decimation factor!!\n");
	for (g = 0; g< (int32_t)BoardInfo.Channels; g++) //BoardInfo.Channels is number of groups for x740 boards
	    groupmask |= (1 << g);
	ret = CAEN_DGTZ_SetGroupSelfTrigger(handle, CAEN_DGTZ_TRGMODE_DISABLED, groupmask);
	if (ret)
	    printf("Error disabling self trigger\n");
	ret = CAEN_DGTZ_SetGroupEnableMask(handle, groupmask);
	if (ret)
	    printf("Error enabling channel groups.\n");
	///malloc
	ret = CAEN_DGTZ_MallocReadoutBuffer(handle, &buffer, &AllocatedSize);
	if (ret) {
	    ErrCode = ERR_MALLOC;
	    if (ErrCode) {
	      printf("\a%s\n", ErrMsg[ErrCode]);
	    }
	    return ErrCode;
	}

	ret = CAEN_DGTZ_AllocateEvent(handle, (void**)&Event16);
	if (ret != CAEN_DGTZ_Success) {
	    ErrCode = ERR_MALLOC;
	    if (ErrCode) {
	      printf("\a%s\n", ErrMsg[ErrCode]);
	    }
	    return ErrCode;
	}

	printf("Starting DAC calibration...\n");

	for (p = 0; p < NPOINTS; p++) {

	    // BoardInfo.Channels is number of groups for x740 boards
	    for (i = 0; i < (int32_t)BoardInfo.Channels; i++) { 
	      ret = CAEN_DGTZ_SetGroupDCOffset(handle, (uint32_t)i, (uint32_t)((float)(std::abs((int)(dc[p] - 100)))*(655.35)));
		if (ret)
		    printf("Error setting group %d test offset\n", i);
	    }
        
	    Sleep(200);

	    CAEN_DGTZ_ClearData(handle);

	    ret = CAEN_DGTZ_SWStartAcquisition(handle);
	    if (ret) {
	        printf("Error starting X740 acquisition\n");
		if (ErrCode) {
                    printf("\a%s\n", ErrMsg[ErrCode]);
		}
		return ErrCode;	    
	    }

	    //baseline values of the NACQS	    
	    int value[NACQS][MAX_CH] = { 0 }; 
	    for (acq = 0; acq < NACQS; acq++) {
	        CAEN_DGTZ_SendSWtrigger(handle);

		ret = CAEN_DGTZ_ReadData(handle, CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT, buffer, &BufferSize);
		if (ret) {
		    ErrCode = ERR_READOUT;
		    if (ErrCode) {
		      printf("\a%s\n", ErrMsg[ErrCode]);
		    }
		    return ErrCode;	    
		}
		if (BufferSize == 0)
		    continue;

		ret = CAEN_DGTZ_GetEventInfo(handle, buffer, BufferSize, 0, &EventInfo, &EventPtr);
		if (ret) {
		    ErrCode = ERR_EVENT_BUILD;
		    if (ErrCode) {
		        printf("\a%s\n", ErrMsg[ErrCode]);
		    }
		    return ErrCode;	    
		}
		// decode the event //
		ret = CAEN_DGTZ_DecodeEvent(handle, EventPtr, (void**)&Event16);
		if (ret) {
		    ErrCode = ERR_EVENT_BUILD;
		    if (ErrCode) {
		        printf("\a%s\n", ErrMsg[ErrCode]);
		    }
		    return ErrCode;	    
		}
		for (g = 0; g < (int32_t)BoardInfo.Channels; g++) {
   		    for (k = 1; k < 21; k++){ //mean over 20 samples
		        value[acq][g] += (int)(Event16->DataChannel[g * 8][k]);
		    } 
		    value[acq][g] = (value[acq][g] / 20);
		}

	    }//for acq

	    ///check for clean baselines
	    for (g = 0; g < (int32_t)BoardInfo.Channels; g++) {
	        int max = 0;
		int mpp = 0;
		int size = (int)pow(2, (double)BoardInfo.ADC_NBits);
		int *freq = (int*)calloc(size, sizeof(int));
		//find the most probable value mpp
		for (k = 0; k < NACQS; k++) {
		    if (value[k][g] > 0 && value[k][g] < size) {
		        freq[value[k][g]]++;
			if (freq[value[k][g]] > max) {
			  max = freq[value[k][g]];
			  mpp = value[k][g];
			}
		    }
		}
		free(freq);
		//discard values too far from mpp
		int ok = 0;
		for (k = 0; k < NACQS; k++) {
		    if (value[k][g] >= (mpp - 5) && value[k][g] <= (mpp + 5)) {
		      avg_value[p][g] = avg_value[p][g] + (float)value[k][g];
		      ok++;
		    }
		}
		avg_value[p][g] = (avg_value[p][g] / (float)ok) * 100.f / (float)size;
	    }

	    CAEN_DGTZ_SWStopAcquisition(handle);
	}  //close for p

	for (g = 0; g < (int32_t)BoardInfo.Channels; g++) {
	    cal[g] = ((float)(avg_value[1][g] - avg_value[0][g]) / (float)(dc[1] - dc[0]));
	    offset[g] = (float)(dc[1] * avg_value[0][g] - dc[0] * avg_value[1][g]) / (float)(dc[1] - dc[0]);
	    printf("Group %d DAC calibration ready.\n",g);
	    printf("Cal %f   offset %f\n", cal[g], offset[g]);

	    WDcfg->DAC_Calib.cal[g] = cal[g];
	    WDcfg->DAC_Calib.offset[g] = offset[g];
	}

	CAEN_DGTZ_ClearData(handle);

	///free events e buffer
	CAEN_DGTZ_FreeReadoutBuffer(&buffer);

	CAEN_DGTZ_FreeEvent(handle, (void**)&Event16);

	ret |= CAEN_DGTZ_SetMaxNumEventsBLT(handle, WDcfg->NumEvents);
	ret |= CAEN_DGTZ_SetDecimationFactor(handle,WDcfg->DecimationFactor);
	ret |= CAEN_DGTZ_SetPostTriggerSize(handle, WDcfg->PostTrigger);
	ret |= CAEN_DGTZ_SetAcquisitionMode(handle, mem_mode);
	ret |= CAEN_DGTZ_SetExtTriggerInputMode(handle, WDcfg->ExtTriggerMode);
	ret |= CAEN_DGTZ_SetGroupEnableMask(handle, WDcfg->EnableMask);
	for (i = 0; i < (int)BoardInfo.Channels; i++) {
	    if (WDcfg->EnableMask & (1 << i))
	        ret |= CAEN_DGTZ_SetGroupSelfTrigger(handle, WDcfg->ChannelTriggerMode[i], (1 << i));
	}
	if (ret)
	    printf("Error setting recorded parameters\n");

	DigitizerConfig cfg;
	cfg.Save_DAC_Calibration_To_Flash(handle, *WDcfg, BoardInfo);


	return 0;
}

/*! \fn      int32_t GetCurrentBaseline(int handle, WaveDumpConfig_t* WDcfg, char* buffer, char* EventPtr, CAEN_DGTZ_BoardInfo_t BoardInfo, int32_t *baselines)
*   \brief   gets the current baseline for every enabled channel
*
*   \param   handle     Digitizer handle
*   \param   WDcfg:     Pointer to the WaveDumpConfig_t data structure
*   \param   buffer:    Pointer to readout buffer (must already be allocated)
*   \param   EventPtr:  Pointer to an already allocated event buffer
*   \param   BoardInfo: Structure with the board info
*   \param   baselines: Array of at least 'BoardInfo.Channels' elements which will be filled with the resulting baselines
*/
int32_t DigitizerFunc::GetCurrentBaseline(int handle, WaveDumpConfig_t* WDcfg, char* buffer, char* EventPtr,
					     CAEN_DGTZ_BoardInfo_t BoardInfo, double *baselines) {
    CAEN_DGTZ_ErrorCode ret;
    uint32_t BufferSize = 0;
    int32_t i = 0, ch;
    uint32_t max_sample = 0x1 << WDcfg->Nbit;

    CAEN_DGTZ_UINT16_EVENT_t* Event16 = (CAEN_DGTZ_UINT16_EVENT_t*)EventPtr;
    CAEN_DGTZ_UINT8_EVENT_t* Event8 = (CAEN_DGTZ_UINT8_EVENT_t*)EventPtr;

    int32_t* histo = (int32_t*)malloc(max_sample * sizeof(*histo));
    if (histo == NULL) {
        fprintf(stderr, "Can't allocate histogram.\n");
        return ERR_MALLOC;
    }

    if ((ret = CAEN_DGTZ_ClearData(handle)) != CAEN_DGTZ_Success) {
        fprintf(stderr, "Can't clear data.\n");
	return ret;
    }

    if ((ret = CAEN_DGTZ_SWStartAcquisition(handle)) != CAEN_DGTZ_Success) {
        fprintf(stderr, "Can't start acquisition.\n");
	return ret;
    }

    for (i = 0; i < 100 && BufferSize == 0; i++) {
        if ((ret = CAEN_DGTZ_SendSWtrigger(handle)) != CAEN_DGTZ_Success) {
            fprintf(stderr, "Can't send SW trigger.\n");
	    return ret;
        }
        if ((ret = CAEN_DGTZ_ReadData(handle, CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT, buffer, &BufferSize)) != CAEN_DGTZ_Success) {
            fprintf(stderr, "Can't read data.\n");
	    return ret;
        }
    }

    if ((ret = CAEN_DGTZ_SWStopAcquisition(handle)) != CAEN_DGTZ_Success) {
        fprintf(stderr, "Can't stop acquisition.\n");
	return ret;
    }

    if (BufferSize == 0) {
        fprintf(stderr, "Can't get SW trigger events.\n");
	return ret;
    }

    if ((ret = CAEN_DGTZ_DecodeEvent(handle, buffer, (void**)&EventPtr)) != CAEN_DGTZ_Success) {
        fprintf(stderr, "Can't decode events\n");
	return ret;
    }

    memset(baselines, 0, BoardInfo.Channels * sizeof(*baselines));
    for (ch = 0; ch < (int32_t)BoardInfo.Channels; ch++) {
        if (WDcfg->EnableMask & (1 << ch)) {

	    //for x740 boards shift to channel 0 of next group
            int32_t  event_ch = (BoardInfo.FamilyCode == CAEN_DGTZ_XX740_FAMILY_CODE) ? (ch * 8) : ch; 
            uint32_t size = (WDcfg->Nbit == 8) ? Event8->ChSize[event_ch] : Event16->ChSize[event_ch];
            uint32_t s;
            uint32_t maxs = 0;

            memset(histo, 0, max_sample * sizeof(*histo));
            for (s = 0; s < size; s++) {
                uint16_t value = (WDcfg->Nbit == 8) ? Event8->DataChannel[event_ch][s] : Event16->DataChannel[event_ch][i];
                if (value < max_sample) {
                    histo[value]++;
                    if (histo[value] > histo[maxs])
                        maxs = value;
                }
            }

            if (WDcfg->PulsePolarity[ch] == CAEN_DGTZ_PulsePolarityPositive)
                baselines[ch] = maxs * 100.0 / max_sample;
            else if (WDcfg->PulsePolarity[ch] == CAEN_DGTZ_PulsePolarityNegative)
                baselines[ch] = 100.0 * (1.0 - (double)maxs / max_sample);
        }
    }

    return ret;
}

/*! \fn      int32_t SetRelativeThreshold(int handle, WaveDumpConfig_t *WDcfg, CAEN_DGTZ_BoardInfo_t BoardInfo)
*   \brief   Finds the correct DCOffset for the BASELINE_LEVEL given in configuration file for each channel and
*            sets the threshold relative to it. To find the baseline, for each channel, two DCOffset values are
*            tried, and in the end the value given by the line passing from them is used. This function ignores
*            the DCOffset calibration previously loaded.
*
*   \param   handle   Digitizer handle
*   \param   WDcfg:   Pointer to the WaveDumpConfig_t data structure
*   \param   BoardInfo: structure with the board info
*/
int32_t DigitizerFunc::SetRelativeThreshold(int handle, WaveDumpConfig_t* WDcfg, CAEN_DGTZ_BoardInfo_t BoardInfo) {
    int32_t ch, ret;
    uint32_t  AllocatedSize;
    
    char* buffer = NULL;
    char* evtbuff = NULL;
    uint32_t exdco[MAX_SET];
    double baselines[MAX_SET];
    double dcocalib[MAX_SET][2];

    //preliminary check: if baseline shift is not enabled for any channel quit
    int should_start = 0;
    for (ch = 0; ch < (int32_t)BoardInfo.Channels; ch++) {
        if (WDcfg->EnableMask & (1 << ch) && WDcfg->Version_used[ch] == 1) {
            should_start = 1;
            break;
        }
    }
    if (!should_start)
        return CAEN_DGTZ_Success;

    // Memory allocation
    ret = CAEN_DGTZ_MallocReadoutBuffer(handle, &buffer, &AllocatedSize);
    if (ret) {
        ret = ERR_MALLOC;
	if (evtbuff != NULL)
	  CAEN_DGTZ_FreeEvent(handle, (void**)&evtbuff);
	if (buffer != NULL)
	    CAEN_DGTZ_FreeReadoutBuffer(&buffer);
    
	return ret;
    }
    ret |= CAEN_DGTZ_AllocateEvent(handle, (void**)&evtbuff);
    if (ret != CAEN_DGTZ_Success) {
        ret = ERR_MALLOC;
	if (evtbuff != NULL)
	  CAEN_DGTZ_FreeEvent(handle, (void**)&evtbuff);
	if (buffer != NULL)
	    CAEN_DGTZ_FreeReadoutBuffer(&buffer);
    
	return ret;
    }
    
    for (ch = 0; ch < (int32_t)BoardInfo.Channels; ch++) {
        if (WDcfg->EnableMask & (1 << ch) && WDcfg->Version_used[ch] == 1) {
            // Assume the uncalibrated DCO is not far from the correct one
            if (WDcfg->PulsePolarity[ch] == CAEN_DGTZ_PulsePolarityPositive)
                exdco[ch] = (uint32_t)((100.0 - WDcfg->dc_file[ch]) * 655.35);
            else if (WDcfg->PulsePolarity[ch] == CAEN_DGTZ_PulsePolarityNegative)
                exdco[ch] = (uint32_t)(WDcfg->dc_file[ch] * 655.35);

            if (BoardInfo.FamilyCode == CAEN_DGTZ_XX740_FAMILY_CODE)
                ret = CAEN_DGTZ_SetGroupDCOffset(handle, ch, exdco[ch]);
            else
                ret = CAEN_DGTZ_SetChannelDCOffset(handle, ch, exdco[ch]);

            if (ret != CAEN_DGTZ_Success) {
                fprintf(stderr, "Error setting DCOffset for channel %d\n", ch);
		if (evtbuff != NULL)
		  CAEN_DGTZ_FreeEvent(handle, (void**)&evtbuff);
		if (buffer != NULL)
		    CAEN_DGTZ_FreeReadoutBuffer(&buffer);
    
		return ret;
            }
        }
    }
    // Sleep some time to let the DAC move
    Sleep(200);
    if ((ret = GetCurrentBaseline(handle, WDcfg, buffer, evtbuff, BoardInfo, baselines)) != CAEN_DGTZ_Success){
        fprintf(stderr, "Error setting DCOffset for channel %d\n", ch);
	if (evtbuff != NULL)
	  CAEN_DGTZ_FreeEvent(handle, (void**)&evtbuff);
	if (buffer != NULL)
	    CAEN_DGTZ_FreeReadoutBuffer(&buffer);
    
	return ret;
    }
    
    for (ch = 0; ch < (int32_t)BoardInfo.Channels; ch++) {
        if (WDcfg->EnableMask & (1 << ch) && WDcfg->Version_used[ch] == 1) {
            double newdco = 0.0;
            // save results of this round
            dcocalib[ch][0] = baselines[ch];
            dcocalib[ch][1] = exdco[ch];
            // ... and perform a new round, using measured value and theoretical zero
            if (WDcfg->PulsePolarity[ch] == CAEN_DGTZ_PulsePolarityPositive)
                newdco = linear_interp(0, 65535, baselines[ch], exdco[ch], WDcfg->dc_file[ch]);
            else if (WDcfg->PulsePolarity[ch] == CAEN_DGTZ_PulsePolarityNegative)
                newdco = linear_interp(0, 0, baselines[ch], exdco[ch], WDcfg->dc_file[ch]);

	    if (newdco < 0)
                exdco[ch] = 0;
            else if (newdco > 65535)
                exdco[ch] = 65535;
            else
                exdco[ch] = (uint32_t)newdco;

	    if (BoardInfo.FamilyCode == CAEN_DGTZ_XX740_FAMILY_CODE)
                ret = CAEN_DGTZ_SetGroupDCOffset(handle, ch, exdco[ch]);
            else
                ret = CAEN_DGTZ_SetChannelDCOffset(handle, ch, exdco[ch]);

            if (ret != CAEN_DGTZ_Success) {
                fprintf(stderr, "Error setting DCOffset for channel %d\n", ch);
		if (evtbuff != NULL)
		  CAEN_DGTZ_FreeEvent(handle, (void**)&evtbuff);
		if (buffer != NULL)
		    CAEN_DGTZ_FreeReadoutBuffer(&buffer);
    
		return ret;
            }
        }
    }
    // Sleep some time to let the DAC move
    Sleep(200);
    
    if ((ret = GetCurrentBaseline(handle, WDcfg, buffer, evtbuff, BoardInfo, baselines)) != CAEN_DGTZ_Success){
        if (evtbuff != NULL)
	  CAEN_DGTZ_FreeEvent(handle, (void**)&evtbuff);
	if (buffer != NULL)
	    CAEN_DGTZ_FreeReadoutBuffer(&buffer);
    
	return ret;
    }
    for (ch = 0; ch < (int32_t)BoardInfo.Channels; ch++) {
        if (WDcfg->EnableMask & (1 << ch) && WDcfg->Version_used[ch] == 1) {
            // Now we have two real points to use for interpolation
            double newdco = linear_interp(dcocalib[ch][0], dcocalib[ch][1], baselines[ch], exdco[ch],
					  WDcfg->dc_file[ch]);
            if (newdco < 0)
                exdco[ch] = 0;
            else if (newdco > 65535)
                exdco[ch] = 65535;
            else
                exdco[ch] = (uint32_t)newdco;
            if (BoardInfo.FamilyCode == CAEN_DGTZ_XX740_FAMILY_CODE)
                ret = CAEN_DGTZ_SetGroupDCOffset(handle, ch, exdco[ch]);
            else
                ret = CAEN_DGTZ_SetChannelDCOffset(handle, ch, exdco[ch]);
            if (ret != CAEN_DGTZ_Success) {
                fprintf(stderr, "Error setting DCOffset for channel %d\n", ch);
		if (evtbuff != NULL)
		  CAEN_DGTZ_FreeEvent(handle, (void**)&evtbuff);
		if (buffer != NULL)
		    CAEN_DGTZ_FreeReadoutBuffer(&buffer);
    
		return ret;
            }
        }
    }
    Sleep(200);
    if ((ret = GetCurrentBaseline(handle, WDcfg, buffer, evtbuff, BoardInfo, baselines)) != CAEN_DGTZ_Success){
        if (evtbuff != NULL)
	  CAEN_DGTZ_FreeEvent(handle, (void**)&evtbuff);
	if (buffer != NULL)
 	    CAEN_DGTZ_FreeReadoutBuffer(&buffer);
     
	return ret;
    }
    for (ch = 0; ch < (int32_t)BoardInfo.Channels; ch++) {
        if (WDcfg->EnableMask & (1 << ch) && WDcfg->Version_used[ch] == 1) {
            if (fabs((baselines[ch] - WDcfg->dc_file[ch]) / WDcfg->dc_file[ch]) > 0.05)
                fprintf(stderr, "WARNING: set BASELINE_LEVEL for ch%d differs from settings for more than 5%c.\n", ch, '%');

	    uint32_t thr = 0;
            if (WDcfg->PulsePolarity[ch] == CAEN_DGTZ_PulsePolarityPositive)
                thr = (uint32_t)(baselines[ch] / 100.0 * (0x1 << WDcfg->Nbit)) + WDcfg->Threshold[ch];
            else
                thr = (uint32_t)((100 - baselines[ch]) / 100.0 * (0x1 << WDcfg->Nbit)) - WDcfg->Threshold[ch];

	    if (BoardInfo.FamilyCode == CAEN_DGTZ_XX740_FAMILY_CODE)
                ret = CAEN_DGTZ_SetGroupTriggerThreshold(handle, ch, thr);
            else
                ret = CAEN_DGTZ_SetChannelTriggerThreshold(handle, ch, thr);

	    if (ret != CAEN_DGTZ_Success) {
                fprintf(stderr, "Error setting DCOffset for channel %d\n", ch);
		if (evtbuff != NULL)
		  CAEN_DGTZ_FreeEvent(handle, (void**)&evtbuff);
		if (buffer != NULL)
		    CAEN_DGTZ_FreeReadoutBuffer(&buffer);
    
		return ret;
            }
        }
    }
    printf("Relative threshold correctly set.\n");
    
    if (evtbuff != NULL)
      CAEN_DGTZ_FreeEvent(handle, (void**)&evtbuff);
    if (buffer != NULL)
        CAEN_DGTZ_FreeReadoutBuffer(&buffer);
    
    return ret;
}

/*! \fn      void CalibrateDCOffset(int handle, WaveDumpConfig_t WDcfg, CAEN_DGTZ_BoardInfo_t BoardInfo)
*   \brief   calibrates DAC of enabled channels (only if BASELINE_SHIFT is in use)
*
*   \param   handle   Digitizer handle
*   \param   WDcfg:   Pointer to the WaveDumpConfig_t data structure
*   \param   BoardInfo: structure with the board info
*/
int DigitizerFunc::CalibrateDCOffset(int handle, WaveDumpConfig_t *WDcfg, CAEN_DGTZ_BoardInfo_t BoardInfo){
	float cal[MAX_CH];
	float offset[MAX_CH] = { 0 };
	int i = 0, k = 0, p = 0, acq = 0, ch = 0;
	for (i = 0; i < MAX_CH; i++)
		cal[i] = 1;
	//	CAEN_DGTZ_ErrorCode ret;
	int ret;
	CAEN_DGTZ_AcqMode_t mem_mode;
	uint32_t  AllocatedSize;
	uint32_t BufferSize;
	CAEN_DGTZ_EventInfo_t       EventInfo;
	char *buffer = NULL;
	char *EventPtr = NULL;
	CAEN_DGTZ_UINT16_EVENT_t    *Event16 = NULL;
	CAEN_DGTZ_UINT8_EVENT_t     *Event8 = NULL;

	float avg_value[NPOINTS][MAX_CH] = { 0 };
	uint32_t dc[NPOINTS] = { 25,75 }; //test values (%)
	uint32_t chmask = 0;

	ErrCode = ERR_NONE;	

	ret = CAEN_DGTZ_GetAcquisitionMode(handle, &mem_mode);//chosen value stored
	if (ret)
	    printf("Error trying to read acq mode!!\n");
	ret = CAEN_DGTZ_SetAcquisitionMode(handle, CAEN_DGTZ_SW_CONTROLLED);
	if (ret)
	    printf("Error trying to set acq mode!!\n");
	ret = CAEN_DGTZ_SetExtTriggerInputMode(handle, CAEN_DGTZ_TRGMODE_DISABLED);
	if (ret)
	    printf("Error trying to set ext trigger!!\n");
	for (ch = 0; ch < (int32_t)BoardInfo.Channels; ch++)
	    chmask |= (1 << ch);
	ret = CAEN_DGTZ_SetChannelSelfTrigger(handle, CAEN_DGTZ_TRGMODE_DISABLED, chmask);
	if (ret)
	    printf("Warning: error disabling channels self trigger\n");
	ret = CAEN_DGTZ_SetChannelEnableMask(handle, chmask);
	if (ret)
	    printf("Warning: error enabling channels.\n");
	ret = CAEN_DGTZ_SetMaxNumEventsBLT(handle, 1);
	if (ret)
	    printf("Warning: error setting max BLT number\n");
	if (BoardInfo.FamilyCode == CAEN_DGTZ_XX740_FAMILY_CODE || BoardInfo.FamilyCode == CAEN_DGTZ_XX724_FAMILY_CODE) {
	    ret = CAEN_DGTZ_SetDecimationFactor(handle, 1);
	    if (ret)
	      printf("Error trying to set decimation factor!!\n");
	}

	///malloc
	ret = CAEN_DGTZ_MallocReadoutBuffer(handle, &buffer, &AllocatedSize);
	if (ret) {
	    ErrCode = ERR_MALLOC;
	    if (ErrCode) {
	        printf("\a%s\n", ErrMsg[ErrCode]);
	    }
	    return ret;
	}
	if (WDcfg->Nbit == 8)
	    ret = CAEN_DGTZ_AllocateEvent(handle, (void**)&Event8);
	else {
	    ret = CAEN_DGTZ_AllocateEvent(handle, (void**)&Event16);
	}
	if (ret != CAEN_DGTZ_Success) {
	    ErrCode = ERR_MALLOC;
	    if (ErrCode) {
	        printf("\a%s\n", ErrMsg[ErrCode]);
	    }
	    return ret;
	}

	printf("Starting DAC calibration...\n");
	
	for (p = 0; p < NPOINTS; p++){
	    //set new dco  test value to all channels
	    for (ch = 0; ch < (int32_t)BoardInfo.Channels; ch++) {
                ret = CAEN_DGTZ_SetChannelDCOffset(handle, (uint32_t)ch, (uint32_t)((float)(std::abs((int)(dc[p] - 100)))*(655.35)));
		if (ret)
		  printf("Error setting ch %d test offset\n", ch);
	    }
        
	    Sleep(200);

	    CAEN_DGTZ_ClearData(handle);

	    ret = CAEN_DGTZ_SWStartAcquisition(handle);
	    if (ret){
  	        printf("Error starting acquisition\n");
		if (ErrCode) {
		  printf("\a%s\n", ErrMsg[ErrCode]);
		}
		return ret;
	    }
		
	    int value[NACQS][MAX_CH] = { 0 };//baseline values of the NACQS
	    for (acq = 0; acq < NACQS; acq++){
	        CAEN_DGTZ_SendSWtrigger(handle);

		ret = CAEN_DGTZ_ReadData(handle, CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT, buffer, &BufferSize);
		if (ret) {
		    ErrCode = ERR_READOUT;
		    if (ErrCode) {
		      printf("\a%s\n", ErrMsg[ErrCode]);
		    }
		    return ret;
		}
		if (BufferSize == 0)
		    continue;
		ret = CAEN_DGTZ_GetEventInfo(handle, buffer, BufferSize, 0, &EventInfo, &EventPtr);
		if (ret) {
		    ErrCode = ERR_EVENT_BUILD;
		    if (ErrCode) {
		      printf("\a%s\n", ErrMsg[ErrCode]);
		    }
		    return ret;
		}
		// decode the event //
		if (WDcfg->Nbit == 8)
		    ret = CAEN_DGTZ_DecodeEvent(handle, EventPtr, (void**)&Event8);
		else
		    ret = CAEN_DGTZ_DecodeEvent(handle, EventPtr, (void**)&Event16);

		if (ret) {
		    ErrCode = ERR_EVENT_BUILD;
		    if (ErrCode) {
		      printf("\a%s\n", ErrMsg[ErrCode]);
		    }
		    return ret;
		}

		for (ch = 0; ch < (int32_t)BoardInfo.Channels; ch++){
		    for (i = 1; i < 21; i++) {
		        if (WDcfg->Nbit == 8)
			    value[acq][ch] += (int)(Event8->DataChannel[ch][i]);
			else
			    value[acq][ch] += (int)(Event16->DataChannel[ch][i]);
		    }
		    value[acq][ch] = (value[acq][ch] / 20);
		}
		
	    }//for acq

	    ///check for clean baselines
	    for (ch = 0; ch < (int32_t)BoardInfo.Channels; ch++) {
	        int max = 0, ok = 0;
		int mpp = 0;
		int size = (int)pow(2, (double)BoardInfo.ADC_NBits);
		int *freq = (int*)calloc(size, sizeof(int));

		//find most probable value mpp
		for (k = 0; k < NACQS; k++) {
		    if (value[k][ch] > 0 && value[k][ch] < size) {
		        freq[value[k][ch]]++;
			if (freq[value[k][ch]] > max) {
			    max = freq[value[k][ch]];
			    mpp = value[k][ch];
			}
		    }
		}
		free(freq);
		//discard values too far from mpp
		for (k = 0; k < NACQS; k++) {
		    if (value[k][ch] >= (mpp - 5) && value[k][ch] <= (mpp + 5)) {
		      avg_value[p][ch] = avg_value[p][ch] + (float)value[k][ch];
		      ok++;
		    }
		}
		//calculate final best average value
		avg_value[p][ch] = (avg_value[p][ch] / (float)ok) * 100.f / (float)size;
	    }

	    CAEN_DGTZ_SWStopAcquisition(handle);
	}//close for p

	for (ch = 0; ch < (int32_t)BoardInfo.Channels; ch++) {
	    cal[ch] = ((float)(avg_value[1][ch] - avg_value[0][ch]) / (float)(dc[1] - dc[0]));
	    offset[ch] = (float)(dc[1] * avg_value[0][ch] - dc[0] * avg_value[1][ch]) / (float)(dc[1] - dc[0]);
	    printf("Channel %d DAC calibration ready.\n", ch);
	    //printf("Channel %d --> Cal %f   offset %f\n", ch, cal[ch], offset[ch]);

	    WDcfg->DAC_Calib.cal[ch] = cal[ch];
	    WDcfg->DAC_Calib.offset[ch] = offset[ch];
	}

	CAEN_DGTZ_ClearData(handle);

	///free events e buffer
	CAEN_DGTZ_FreeReadoutBuffer(&buffer);
	if (WDcfg->Nbit == 8)
	    CAEN_DGTZ_FreeEvent(handle, (void**)&Event8);
	else
	    CAEN_DGTZ_FreeEvent(handle, (void**)&Event16);

	//reset settings
	ret |= CAEN_DGTZ_SetMaxNumEventsBLT(handle, WDcfg->NumEvents);
	ret |= CAEN_DGTZ_SetPostTriggerSize(handle, WDcfg->PostTrigger);
	ret |= CAEN_DGTZ_SetAcquisitionMode(handle, mem_mode);
	ret |= CAEN_DGTZ_SetExtTriggerInputMode(handle, WDcfg->ExtTriggerMode);
	ret |= CAEN_DGTZ_SetChannelEnableMask(handle, WDcfg->EnableMask);

	if (BoardInfo.FamilyCode == CAEN_DGTZ_XX740_FAMILY_CODE || BoardInfo.FamilyCode == CAEN_DGTZ_XX724_FAMILY_CODE)
	    ret |= CAEN_DGTZ_SetDecimationFactor(handle, WDcfg->DecimationFactor);
	if (ret)
	    printf("Error resetting some parameters after DAC calibration\n");

	//reset self trigger mode settings
	if (BoardInfo.FamilyCode == CAEN_DGTZ_XX730_FAMILY_CODE || BoardInfo.FamilyCode == CAEN_DGTZ_XX725_FAMILY_CODE) {
	    // channel pair settings for x730 boards
	    for (i = 0; i < WDcfg->Nch; i += 2) {
	        if (WDcfg->EnableMask & (0x3 << i)) {
		  CAEN_DGTZ_TriggerMode_t mode = WDcfg->ChannelTriggerMode[i];
		  uint32_t pair_chmask = 0;
		  
		  if (WDcfg->ChannelTriggerMode[i] != CAEN_DGTZ_TRGMODE_DISABLED) {
		      if (WDcfg->ChannelTriggerMode[i + 1] == CAEN_DGTZ_TRGMODE_DISABLED)
			  pair_chmask = (0x1 << i);
		      else
			  pair_chmask = (0x3 << i);
				}
		  else {
		      mode = WDcfg->ChannelTriggerMode[i + 1];
		      pair_chmask = (0x2 << i);
		  }

		  pair_chmask &= WDcfg->EnableMask;
		  ret |= CAEN_DGTZ_SetChannelSelfTrigger(handle, mode, pair_chmask);
		}
	    }
	}
	else {
	    for (i = 0; i < WDcfg->Nch; i++) {
	      if (WDcfg->EnableMask & (1 << i))
		ret |= CAEN_DGTZ_SetChannelSelfTrigger(handle, WDcfg->ChannelTriggerMode[i], (1 << i));
	    }
	}
	if (ret)
	    printf("Error resetting self trigger mode after DAC calibration\n");

	DigitizerConfig cfg;
	cfg.Save_DAC_Calibration_To_Flash(handle, *WDcfg, BoardInfo);

	return ret;
}

/*! \fn      void SetCalibratedDCO(int handle, WaveDumpConfig_t *WDcfg, CAEN_DGTZ_BoardInfo_t BoardInfo)
*   \brief   sets the calibrated DAC value using calibration data (only if BASELINE_SHIFT is in use)
*
*   \param   handle   Digitizer handle
*   \param   WDcfg:   Pointer to the WaveDumpConfig_t data structure
*   \param   BoardInfo: structure with the board info
*/
int DigitizerFunc::SetCalibratedDCO(int handle, int ch, WaveDumpConfig_t *WDcfg, CAEN_DGTZ_BoardInfo_t BoardInfo) {
	int ret = CAEN_DGTZ_Success;
	if (WDcfg->Version_used[ch] == 0) //old DC_OFFSET config, skip calibration
		return ret;
	if (WDcfg->PulsePolarity[ch] == CAEN_DGTZ_PulsePolarityPositive) {
	        WDcfg->DCoffset[ch] = (uint32_t)((float)(fabs((((float)WDcfg->dc_file[ch] - WDcfg->DAC_Calib.offset[ch]) / WDcfg->DAC_Calib.cal[ch]) - 100.))*(655.35));
		if (WDcfg->DCoffset[ch] > 65535) WDcfg->DCoffset[ch] = 65535;
		//		if (WDcfg->DCoffset[ch] < 0) WDcfg->DCoffset[ch] = 0;
	}
	else if (WDcfg->PulsePolarity[ch] == CAEN_DGTZ_PulsePolarityNegative) {
		WDcfg->DCoffset[ch] = (uint32_t)((float)(fabs(((fabs(WDcfg->dc_file[ch] - 100.) - WDcfg->DAC_Calib.offset[ch]) / WDcfg->DAC_Calib.cal[ch]) - 100.))*(655.35));
		//		if (WDcfg->DCoffset[ch] < 0) WDcfg->DCoffset[ch] = 0;
		if (WDcfg->DCoffset[ch] > 65535) WDcfg->DCoffset[ch] = 65535;
	}

	if (BoardInfo.FamilyCode == CAEN_DGTZ_XX740_FAMILY_CODE) {
		ret = CAEN_DGTZ_SetGroupDCOffset(handle, (uint32_t)ch, WDcfg->DCoffset[ch]);
		if (ret)
			printf("Error setting group %d offset\n", ch);
	}
	else {
		ret = CAEN_DGTZ_SetChannelDCOffset(handle, (uint32_t)ch, WDcfg->DCoffset[ch]);
		if (ret)
			printf("Error setting channel %d offset\n", ch);
	}

	return ret;
}
