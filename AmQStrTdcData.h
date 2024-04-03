#ifndef AmQStrTdcData_h
#define AmQStrTdcData_h

#include <cstdint>

namespace AmQStrTdc::Data {

  struct Word {
    uint8_t d[8];
  };

  inline namespace v0 {

  struct Bits {
    union {
      uint8_t d[8];

      struct {
	uint64_t raw : 64;
      };

      // common
      struct {
	uint64_t com_rsv : 58; // [57:0]
	uint64_t head    :  6; // [63:58]
      };
   
      // lrtdc
      struct {
	uint64_t zero_t1    : 16; // [15:0]	  	  
	uint64_t tdc        : 19; // [34:16]
	uint64_t tot        : 16; // [50:35]
	uint64_t ch         :  7; // [57:51]
	uint64_t dtype      :  6; // [63:58]
      };

      // hrtdc
      struct {
	uint64_t hrtdc      : 29; // [28:0]
	uint64_t hrtot      : 22; // [50:29]
	uint64_t hrch       :  7; // [57:51]
	uint64_t hrdtype    :  6; // [63:58]
      };

      // heartbeat
      struct {
	uint64_t zero_h     : 24;
	uint64_t hbframe    : 16; //[39,24]
	uint64_t hbspilln   :  8; //[47,40]
	uint64_t hbflag     : 10; //[57,48]
	uint64_t htype      : 6; //[63,58]
      };

      // spill
      struct {
	uint64_t zero_s    : 24;	  
	uint64_t hbfrco    : 16; //[39,24]
	uint64_t spilln    :  8; //[47,40]
	uint64_t spflag    : 10; //[57,48]
	uint64_t stype     :  6; //[63,58]
      };

    };
  };

  enum HeadTypes {
    Data          = 0x0B,
    Heartbeat     = 0x1C,
    Trailer       = 0x0D,
    //      ErrorRecovery = 0xE,
    SpillOn       = 0x18,
    SpillEnd      = 0x14
  };

  } // namespace v0


  namespace v1 {

  struct Bits {
    union {
      uint8_t d[8];

      struct {
	uint64_t raw : 64;
      };

      // common
      struct {
	uint64_t com_rsv : 58; // [57:0]
	uint64_t head    :  6; // [63:58]
      };
   
      // lrtdc
      struct {
	uint64_t zero_t1    : 16; // [15:0]	  	  
	uint64_t tdc        : 19; // [34:16]
	uint64_t tot        : 16; // [50:35]
	uint64_t ch         :  7; // [57:51]
	uint64_t dtype      :  6; // [63:58]
      };

      // hrtdc
      struct {
	uint64_t hrtdc      : 29; // [28:0]
	uint64_t hrtot      : 22; // [50:29]
	uint64_t hrch       :  7; // [57:51]
	uint64_t hrdtype    :  6; // [63:58]
      };

      // 1st heartbeat
      struct {
	uint64_t zero_h     : 24;
	uint64_t toffset    : 16; // [39,24]
	uint64_t hbflag     : 16; // [55,40]
	uint64_t reserve1   : 2;  // [57,56]
	uint64_t hbtype1    : 6;  // [63,58]
      };

      // 2nd heartbeat
      struct {
	uint64_t transSize : 20;	  
	uint64_t geneSize  : 20; // [39,20]
	uint64_t userReg   : 16; // [55,40]
	uint64_t reserve2  :  2; // [57,56]
	uint64_t hbtype2   :  6; // [63,58]
      };

    };
  };

  enum HeadTypes {
    Data              = 0x0B,
    Heartbeat1st      = 0x1C,
    Heartbeat2nd      = 0x1E,    
    Trailer           = 0x0D,
    ThrottlingT1Start = 0x19,
    ThrottlingT1End   = 0x11,
    ThrottlingT2Start = 0x1A,
    ThrottlingT2End   = 0x12,        
  };

  } // namespace v1
  
  
} // namespace AmQStrTdc::Data



#endif
