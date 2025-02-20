#ifndef _DigitizerStruct_H_
#define _DigitizerStruct_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <CAENDigitizer.h>
#include <CAENDigitizerType.h>

#define Sleep(t) usleep((t)*1000);

#define MAX_CH  64          /* max. number of channels */
#define MAX_SET 16          /* max. number of independent settings */
#define MAX_GROUPS  8       /* max. number of groups */
#define MAX_GW  1000        /* max. number of generic write commads */

#define VME_INTERRUPT_LEVEL      1
#define VME_INTERRUPT_STATUS_ID  0xAAAA
#define INTERRUPT_TIMEOUT        200  // ms

#define CFGRELOAD_CORRTABLES_BIT (0)
#define CFGRELOAD_DESMODE_BIT (1)

#define NPOINTS 2
#define NACQS   50

typedef enum {
	OFF_BINARY= 0x00000001,	 // Bit 0: 1 = BINARY, 0 =ASCII
	OFF_HEADER= 0x00000002,	 // Bit 1: 1 = include header, 0 = just samples data
} OUTFILE_FLAGS;

typedef struct{
	float cal[MAX_SET];
	float offset[MAX_SET];
}DAC_Calibration_data;



/* Error messages */
typedef enum {
    ERR_NONE= 0,
    ERR_CONF_FILE_NOT_FOUND,
    ERR_DGZ_OPEN,
    ERR_BOARD_INFO_READ,
    ERR_INVALID_BOARD_TYPE,
    ERR_DGZ_PROGRAM,
    ERR_MALLOC,
    ERR_RESTART,
    ERR_INTERRUPT,
    ERR_READOUT,
    ERR_EVENT_BUILD,
    ERR_HISTO_MALLOC,
    ERR_UNHANDLED_BOARD,
    ERR_OUTFILE_WRITE,
    ERR_OVERTEMP,
    ERR_BOARD_FAILURE,

    ERR_DUMMY_LAST,
} ERROR_CODES;

extern char ErrMsg[ERR_DUMMY_LAST][100];
extern CAEN_DGTZ_IRQMode_t INTERRUPT_MODE;
extern CAEN_DGTZ_DRS4Correction_t X742Tables[MAX_X742_GROUP_SIZE];

//extern CAEN_DGTZ_IRQMode_t INTERRUPT_MODE = CAEN_DGTZ_IRQ_MODE_ROAK;
//extern CAEN_DGTZ_DRS4Correction_t X742Tables[MAX_X742_GROUP_SIZE];

typedef struct {
    int LinkType;
    int LinkNum;
    int ConetNode;
    uint32_t BaseAddress;
    int Nch;
    int Nbit;
    float Ts;
    int NumEvents;
    uint32_t RecordLength;
    int PostTrigger;
    int InterruptNumEvents;
    int TestPattern;
    CAEN_DGTZ_EnaDis_t DesMode;
    //int TriggerEdge;
    CAEN_DGTZ_IOLevel_t FPIOtype;
    CAEN_DGTZ_TriggerMode_t ExtTriggerMode;
    uint16_t EnableMask;

    CAEN_DGTZ_TriggerMode_t ChannelTriggerMode[MAX_SET];
    CAEN_DGTZ_PulsePolarity_t PulsePolarity[MAX_SET];
    uint32_t DCoffset[MAX_SET];
    int32_t  DCoffsetGrpCh[MAX_SET][MAX_SET];
    uint32_t Threshold[MAX_SET];
    int Version_used[MAX_SET];
    uint8_t GroupTrgEnableMask[MAX_SET];
    uint32_t MaxGroupNumber;
	
    uint32_t FTDCoffset[MAX_SET];
    uint32_t FTThreshold[MAX_SET];
    CAEN_DGTZ_TriggerMode_t	FastTriggerMode;
    uint32_t	 FastTriggerEnabled;
    int GWn;
    uint32_t GWaddr[MAX_GW];
    uint32_t GWdata[MAX_GW];
    uint32_t GWmask[MAX_GW];
  //    OUTFILE_FLAGS OutFileFlags;
    int OutFileFlags;
    uint16_t DecimationFactor;
    int useCorrections;
    int UseManualTables;
    char TablesFilenames[MAX_X742_GROUP_SIZE][1000];
    CAEN_DGTZ_DRS4Frequency_t DRS4Frequency;
    int StartupCalibration;
    DAC_Calibration_data DAC_Calib;
    char ipAddress[25];

    int dc_file[MAX_CH];
    float dc_8file[8];
    int thr_file[MAX_CH];

} WaveDumpConfig_t;


typedef struct WaveDumpRun_t {
    int Quit;
    int AcqRun;
    int ContinuousTrigger; //software trigger
    int ContinuousWrite;
    int SingleWrite;
    int Restart;
    FILE *fout[MAX_CH];
} WaveDumpRun_t;

#endif
