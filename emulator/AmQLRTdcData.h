#ifndef AmQLRTdcData_H
#define AmQLRTdcData_H

#include <iostream>
#include <cstdint>
#include <string>
/*
struct AmQHeader {
  uint8_t tdc       {0x00};
  uint8_t heartbeat {0x38};
  uint8_t spillon   {0x30};
  uint8_t spilloff  {0x28};
};
*/

struct AmQHeader {
  uint8_t tdc       {0x0b};
  uint8_t heartbeat {0x1c};
  uint8_t spillon   {0x18};
  uint8_t spilloff  {0x14};
};

struct AmQTdcWord {
  // tdc data starting from LSB 
  uint32_t resv; // 24;
  uint32_t tdc; // 19;
  uint32_t tot; // 8;
  uint32_t ch; // 7;
  uint32_t dtype; // 6;
};

/* | dtype(6) | ch(7) | tot(8) | tdc(19) | resv(24) |  */

struct HeartBeatWord {
  //  heartbeat word 
  uint32_t resv;
  uint32_t hb_frame;
  uint32_t nspill;
  uint32_t flag;
  uint32_t dtype;
};

/* | dtype(6) | falg(10) | spill#(8) | hb_frame(16) | resv(24) |  */
  
struct SpillWord {
  // spill word
  uint32_t resv;
  uint32_t hb_count;
  uint32_t nspill;
  uint32_t flag;
  uint32_t dtype;
};

/* | dtype(6) | falg(10) | spill#(8) | hb_count(16) | resv(24) |  */
  
struct AmQDataWord {
  uint8_t data[8];
};  //64 bit  

class AmQLRTdc {
 public :
  AmQLRTdc();
  ~AmQLRTdc(){ delete[] StackBuf; delete[] databuf; }

  int generator(unsigned int iseq, unsigned char* buf);
  int packet_generator(unsigned int iseq, unsigned char* sbuf);
  unsigned int get_HBrate(){return HBrate;}
  unsigned int get_WCount(){return nWordCount;}
  void set_WordCount(int fnWordCount){ nWordCount = fnWordCount; }
  static bool compare(const AmQTdcWord& a1, const AmQTdcWord& a2);
 private :

  const int fnByte      {8};
  const int maxTdcHits  {128*4};
  const int maxNdelimByte   {32};

  HeartBeatWord hb_delmt_;
  SpillWord     spill_delmt_;
  AmQTdcWord    tdc_data_[128*4];
  AmQDataWord   data_word_;
  AmQHeader     header_;

  unsigned char* databuf;
  
  bool mdebug;
  unsigned int HBrate;
  int TotalSize;
  unsigned char* StackBuf;
  int StackSize;
  int nWordCount;
};

#endif


