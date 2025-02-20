#ifndef V1740D_H_
#define V1740D_H_

#include <sys/time.h> /* struct timeval, select() */
#include <termios.h>  /* tcgetattr(), tcsetattr() */
#include <stdlib.h>   /* atexit(), exit() */
#include <unistd.h>   /* read() */
#include <sys/types.h>
#include <sys/stat.h>

/* System library includes */
#include <stdio.h>
#include <time.h>
#include <sys/timeb.h>

#include <errno.h>
#include <limits.h>

#include <sys/time.h> /* struct timeval, select() */
#include <termios.h> /* tcgetattr(), tcsetattr() */
#include <stdlib.h> /* atexit(), exit() */
#include <unistd.h> /* read() */
#include <stdio.h> /* printf() */
#include <string.h> /* memcpy() */
#include <stdint.h> /* memcpy() */

#include <CAENDigitizer.h>

#define MAX_CHANNELS 64
#define MAX_GROUPS 8

#define CAEN_USE_DIGITIZERS
#define IGNORE_DPP_DEPRECATED

#define CONNECTION_TYPE_USB        0
#define CONNECTION_TYPE_OPT        1
#define CONNECTION_TYPE_USB_A4818  5
#define CONNECTION_TYPE_AUTO       255

#define MAX_AGGR_NUM_PER_BLOCK_TRANSFER   1023 /* MAX 1023 */

#define ENABLE_TEST_PULSE 1

#define MAX_PATH          260
#define REFRESH_RATE 1000 /* ms */

typedef struct
{
    uint32_t ConnectionType;
    uint32_t ConnectionLinkNum;
    uint32_t ConnectionConetNode;
    uint32_t ConnectionVMEBaseAddress;

    uint32_t RecordLength;
    uint32_t PreTrigger;
    uint32_t ActiveChannel;
    uint32_t PreGate;
    uint32_t ChargeSensitivity;
    uint32_t FixedBaseline;
    uint32_t BaselineMode;
    uint32_t TrgMode;
    uint32_t TrgSmoothing;
    uint32_t TrgHoldOff;
    uint32_t TriggerThreshold[64];

    uint32_t DCoffset[8];
    uint32_t NevAggr;
    uint32_t PulsePol;
    uint32_t EnChargePed;
    uint32_t DisTrigHist;
    uint32_t DisSelfTrigger;
    uint32_t EnTestPulses;
    uint32_t TestPulsesRate;
    uint32_t DefaultTriggerThr;
    uint64_t ChannelTriggerMask;
    CAEN_DGTZ_DPP_AcqMode_t AcqMode;
    CAEN_DGTZ_DPP_QDC_Params_t DPPParams;
  
} BoardParameters;


class V1740D {

 public:

    V1740D() { mDecode = false; };
    ~V1740D() {};

    bool mDecode;
  
    int  SetupAcquisition(const BoardParameters& params);
    int  RunAcquisition();
    int  CleanUp();
    int  RunStart();
    int  RunStop();

    /* Board parameters utility functions */
    void SetDefaultParameters(BoardParameters *params);
    int  LoadConfigurationFile(char * fname, BoardParameters *params);
    int  SetupParameters(BoardParameters *params, char *fname);
    int  ConfigureDigitizer(int handle, int gEquippedGroups, BoardParameters *params);

    int GetBytesRead();
    int GetReadOutData(char* destBuf, uint32_t byteSize);
    void SetFlag();
    
 private:

    int  gHandle;                                         /* CAEN library handle */
    BoardParameters   gParams;                            /* Board parameters structure       */
  //    CAEN_DGTZ_DPP_QDC_Params_t *DPPParams;                /* Board DP-QDC parameters          */
    
    /* Variable declarations */
    unsigned int gActiveChannel;                            /* Active channel for data analysis */
    unsigned int gEquippedChannels;                         /* Number of equipped channels      */
    unsigned int gEquippedGroups;                           /* Number of equipped groups        */
  
    uint64_t     gExtendedTimeTag[MAX_CHANNELS];            /* Extended Time Tag                */
    uint64_t     gETT[MAX_CHANNELS];                        /* Extended Time Tag                */
    uint64_t     gCurrTimeTag[MAX_CHANNELS];                /* Current Time Tag                */
    uint64_t     gPrevTimeTag[MAX_CHANNELS];                /* Previous Time Tag                */
  
    CAEN_DGTZ_BoardInfo_t          gBoardInfo;              /* Board Informations               */
    CAEN_DGTZ_DPP_QDC_Event_t*     gEvent[MAX_CHANNELS];    /* Events                           */
  
    CAEN_DGTZ_DPP_QDC_Waveforms_t* gWaveforms;              /* Waveforms                        */
  
    /* Variable definitions */
    int          gSWTrigger;                                /* Signal for software trigger          */
    int          running;                                   /* Readout is running                   */
    int          gAcqrun;                                   /* Acquisition is on                           */
    int          grp4stats;                                 /* Selected group for statistics        */
    int          gLoops;                                    /* Number of acquisition loops          */
    int          gToggleTrace;                              /* Signal for trace toggle from user    */
    int          gAnalogTrace;                              /* Signal for analog trace toggle       */
    int          gRestart    ;                              /* Signal acquisition restart from user */
    int          gStopped;                                  /* Signal acquisition stopped from user */
    char *       gAcqBuffer;                                /* Acquisition buffer                   */
    char *       gEventPtr;                                 /* Events buffer pointer                */
    int          gAcquisitionBufferAllocated;               /* Buffers are allocated                */

  
    /* Runtime statistics */
    unsigned int gBytesRead;                                /* Number of bytes read                */
    uint64_t     gTotEvCnt;                                 /* Total number of events              */
    uint64_t     gPrevTotEvCnt;                             /* Previous Total number of events     */
    uint64_t     gChEvCnt[MAX_CHANNELS];                    /* Channel event counter               */
    uint64_t     gPrevChEvCnt[MAX_CHANNELS];                /* Previous Channel event counter      */
  
    long         gCurrTime;                                 /* Current time                     */
    long         gPrevTime;                                 /* Previous saved time              */
    long         gRunStartTime;                             /* Time of run start                */
    long         gRunElapsedTime;                           /* Elapsed time since start         */
    long         gUpdateElapsedTime;                        /* Elapsed time since last update   */
  
};

#endif
