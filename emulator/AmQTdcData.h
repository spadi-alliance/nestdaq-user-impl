#ifndef AmQTdcData_H
#define AmQTdcData_H

#include <iostream>
#include <cstdint>
#include <string>


struct AmQDataWord {
    uint8_t data[8];
};  //64 bit

namespace LRTdc {

struct AmQHeader {
    uint8_t tdc       {0x0b};
    uint8_t heartbeat {0x1c};
    uint8_t spillon   {0x18};
    uint8_t spilloff  {0x14};
};

struct AmQTdcWord {
    // tdc data starting from LSB
    uint32_t resv  {0}; // 24;
    uint32_t tdc   {0}; // 19;
    uint32_t tot   {0}; // 8;
    uint32_t ch    {0}; // 7;
    uint32_t dtype {0}; // 6;
};

/* | dtype(6) | ch(7) | tot(8) | tdc(19) | resv(24) |  */

struct HeartBeatWord {
    //  heartbeat word
    uint32_t resv     {0};
    uint32_t hb_frame {0};
    uint32_t nspill   {0};
    uint32_t flag     {0};
    uint32_t dtype    {0};
};

/* | dtype(6) | falg(10) | spill#(8) | hb_frame(16) | resv(24) |  */

struct SpillWord {
    // spill word
    uint32_t resv      {0};
    uint32_t hb_count  {0};
    uint32_t nspill    {0};
    uint32_t flag      {0};
    uint32_t dtype     {0};
};

/* | dtype(6) | falg(10) | spill#(8) | hb_count(16) | resv(24) |  */

}


namespace HRTdc {

struct AmQHeader {
    uint8_t tdc       {0x0b};
    uint8_t heartbeat {0x1c};
    uint8_t spillon   {0x18};
    uint8_t spilloff  {0x14};
};

struct AmQTdcWord {
    // tdc data starting from LSB
    uint32_t estimator  {0}; // 11 bit estimator;
    uint32_t csemifine  {0}; // 2 bit semi-fine count;
    uint32_t tdc        {0}; // 16 bit heartbeat frame;
    uint32_t tot        {0}; // 22;
    uint32_t ch         {0}; // 7;
    uint32_t dtype      {0}; // 6;
};

/* | dtype(6) | ch(7) | tot(22) | tdc(16) | csemi(2) | esti(11) |  */

struct HeartBeatWord {
    //  heartbeat word
    uint32_t resv       {0};
    uint32_t hb_frame   {0};
    uint32_t nspill     {0};
    uint32_t flag       {0};
    uint32_t dtype      {0};
};

/* | dtype(6) | falg(10) | spill#(8) | hb_frame(16) | resv(24) |  */

struct SpillWord {
    // spill word
    uint32_t resv       {0};
    uint32_t hb_frame   {0}; // in 1st word
    uint32_t hb_count   {0}; // in 2nd word
    uint32_t nspill     {0};
    uint32_t flag       {0};
    uint32_t dtype      {0};
};

/* | dtype(6) | falg(10) | spill#(8) | hb_frame(16) | resv(24) |  */
/* | dtype(6) | falg(10) | spill#(8) | hb_count(16) | resv(24) |  */

}

#endif
