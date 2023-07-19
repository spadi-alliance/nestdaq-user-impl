#ifndef ScalerData_h
#define ScalerData_h

#include <cstdint>
#include <string>

namespace Scaler {

  constexpr uint64_t Magic {0x614472656c616353};    

  struct Bits{
    union {
      uint8_t d[8];

      struct {
	uint64_t word : 64;
      };
      struct {
	uint64_t ch : 32;
	uint64_t id : 32;
      };
    };
  };
    
  struct Header{
    uint64_t magic        {Magic};
    uint64_t length       {0}; //byte
  };  
}

#endif
