#ifndef _DigitizerFunc_H_
#define _DigitizerFunc_H_

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

#include "digitizer/fft.h"
#include "digitizer/X742CorrectionRoutines.h"

#include "DigitizerStruct.h"

class DigitizerFunc {

 public:
  DigitizerFunc() {};
  ~DigitizerFunc() {};

  static double linear_interp(double x0, double y0, double x1, double y1, double x);
    
  int WriteOutputFiles(WaveDumpConfig_t *WDcfg, WaveDumpRun_t *WDrun,
		       CAEN_DGTZ_EventInfo_t *EventInfo, void *Event);
  int WriteOutputFilesx742(WaveDumpConfig_t *WDcfg, WaveDumpRun_t *WDrun,
			   CAEN_DGTZ_EventInfo_t *EventInfo, CAEN_DGTZ_X742_EVENT_t *Event);

  int GetMoreBoardInfo(int handle, CAEN_DGTZ_BoardInfo_t BoardInfo, WaveDumpConfig_t *WDcfg);
  int WriteRegisterBitmask(int32_t handle, uint32_t address, uint32_t data, uint32_t mask);
  int CheckBoardFailureStatus(int handle);

  int DoProgramDigitizer(int handle, WaveDumpConfig_t* WDcfg, CAEN_DGTZ_BoardInfo_t BoardInfo);
  int ProgramDigitizerWithRelativeThreshold(int handle, WaveDumpConfig_t* WDcfg, CAEN_DGTZ_BoardInfo_t BoardInfo);
  int ProgramDigitizer(int handle, WaveDumpConfig_t WDcfg, CAEN_DGTZ_BoardInfo_t BoardInfo);

  int32_t BoardSupportsCalibration(CAEN_DGTZ_BoardInfo_t BoardInfo);
  int32_t BoardSupportsTemperatureRead(CAEN_DGTZ_BoardInfo_t BoardInfo);

  void Calibrate(int handle, WaveDumpRun_t *WDrun, CAEN_DGTZ_BoardInfo_t BoardInfo);

  int CalibrateXX740DCOffset(int handle, WaveDumpConfig_t *WDcfg, CAEN_DGTZ_BoardInfo_t BoardInfo);
  int32_t GetCurrentBaseline(int handle, WaveDumpConfig_t* WDcfg, char* buffer, char* EventPtr,
			     CAEN_DGTZ_BoardInfo_t BoardInfo, double *baselines); 
  int32_t SetRelativeThreshold(int handle, WaveDumpConfig_t* WDcfg, CAEN_DGTZ_BoardInfo_t BoardInfo);
  int CalibrateDCOffset(int handle, WaveDumpConfig_t *WDcfg, CAEN_DGTZ_BoardInfo_t BoardInfo);
  int SetCalibratedDCO(int handle, int ch, WaveDumpConfig_t *WDcfg, CAEN_DGTZ_BoardInfo_t BoardInfo);

private:

  ERROR_CODES ErrCode;
};

#endif
