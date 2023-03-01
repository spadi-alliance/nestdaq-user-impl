#ifndef AmQStrTdcData_h
#define AmQStrTdcData_h

#include <cstdint>

namespace AmQStrTdc::Data {

//  namespace HrTdc{

struct Word {
    uint8_t d[8];
};

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

        // tdc
        struct {
            uint64_t tdc     : 29; // [28:0]
            uint64_t tot     : 22; // [50:29]
            uint64_t ch      :  7; // [57:51]
            uint64_t dtype   :  2; // [63:58]
        };

        // heartbeat
        struct {
            uint64_t hb_rsv   : 24; //[23:0]
            uint64_t hbframe  : 16; //[39:24]
            uint64_t hbspilln :  8; //[47:40]
            uint64_t hbflag   : 10; //[57:48]
            uint64_t htype    :  6; //[63:58]
        };

        // spill
        struct {
            uint64_t sp_rsv    : 24; //[23:0]
            uint64_t hbfrco    : 16; //[39:24]
            uint64_t spilln    :  8; //[47:40]
            uint64_t spflag    : 10; //[57:48]
            uint64_t stype     :  6; //[63:58]
        };

    };
};

enum HeadTypes {
    Data          = 0x0B,
    Heartbeat     = 0x1C,
    //      ErrorRecovery = 0xE,
    SpillOn       = 0x18,
    SpillEnd      = 0x14
};

//    enum DataTypes {
//      LeadingEdge  = 0b00,
//      BusyStart    = 0b01,
//      BusyEnd      = 0b10
//    };

//  } //namespce HrTdc

} // nemacspace AmQStrTdc::Data



#endif
