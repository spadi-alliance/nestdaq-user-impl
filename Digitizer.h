#ifndef _Digitizer_H_
#define _Digitizer_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <CAENDigitizer.h>
#include <CAENDigitizerType.h>

#include <unistd.h>
#include <stdint.h>   /* C99 compliant compilers: uint64_t */
#include <ctype.h>    /* toupper() */
#include <sys/time.h>

#include "DigitizerFunc.h"
#include "DigitizerConfig.h"
#include "DigitizerStruct.h"

class Digitizer {

 public:
    Digitizer() {};
    ~Digitizer() {};

    void RunInit();
    int  RunStart();
    int  RunAcqusition();
    int  RunStop();
    int  RunEnd();

    int ReadParameters(char *cfgFileName);
    int OpenDigitizer();
    int SetupParameters();
    int AllocateMemory();
    static long GetTime();
    int InterruptTimeout();
  
    int RunAcquisition();
    unsigned int GetBytesRead();
    int GetReadOutData(char* destBuf, uint32_t byteSize);

    WaveDumpConfig_t GetParameters(){ return WDcfg; }
    int AnalyzeEvent();
    
 private:
    WaveDumpConfig_t    WDcfg;
    WaveDumpRun_t       WDrun;
  //    CAEN_DGTZ_ErrorCode ret;
    int  handle;
    ERROR_CODES ErrCode;
    uint32_t AllocatedSize;
    uint32_t BufferSize;
    uint32_t NumEvents;
    char *buffer;
    char *EventPtr;

    std::string ConfigFileName;
    int isVMEDevice;
    int MajorNumber;

    int nCycles;
    uint64_t CurrentTime, PrevRateTime, ElapsedTime;

    CAEN_DGTZ_BoardInfo_t       BoardInfo;
    CAEN_DGTZ_EventInfo_t       EventInfo;

    CAEN_DGTZ_UINT16_EVENT_t    *Event16; /* generic event struct with 16 bit data (10, 12, 14 and 16 bit digitizers */

    CAEN_DGTZ_UINT8_EVENT_t     *Event8; /* generic event struct with 8 bit data (only for 8 bit digitizers) */ 
    CAEN_DGTZ_X742_EVENT_t      *Event742;  /* custom event struct with 8 bit data (only for 8 bit digitizers) */

    int ReloadCfgStatus; // Init to the bigger positive number

    int fNb, fNe;

    unsigned int    gBytesRead;
    DigitizerFunc   digitFunc;
    DigitizerConfig digitCfg;
};

#endif
