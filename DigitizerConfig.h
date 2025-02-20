#ifndef _DigitizerConfig_H_
#define _DigitizerConfig_H_

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

#include "digitizer/spi.h"
#include "digitizer/fft.h"
#include "digitizer/flash.h"
#include "digitizer/X742CorrectionRoutines.h"

#include "DigitizerStruct.h"

class DigitizerConfig {

 public:
  
  DigitizerConfig() {}
  ~DigitizerConfig() {}
  
  static void SetDefaultConfiguration(WaveDumpConfig_t *WDcfg);
  int ParseConfigFile(FILE *f_ini, WaveDumpConfig_t *WDcfg);
  int Load_DAC_Calibration_From_Flash(int handle, WaveDumpConfig_t *WDcfg,
				       CAEN_DGTZ_BoardInfo_t BoardInfo);
  void Save_DAC_Calibration_To_Flash(int handle, WaveDumpConfig_t WDcfg,
				CAEN_DGTZ_BoardInfo_t BoardInfo);
 private:
  int dc_file[MAX_CH];
  float dc_8file[8];
  int thr_file[MAX_CH];
};

#endif
