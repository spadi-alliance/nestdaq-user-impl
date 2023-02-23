#ifndef AmQTdc_H
#define AmQTdc_H

#include <iostream>
#include <cstdint>
#include <string>

class AmQTdc {
 public :
  AmQTdc();
  ~AmQTdc(){ delete[] StackBuf; delete[] databuf; }

  int LRgenerator(unsigned int iseq, unsigned char* buf);
  int HRgenerator(unsigned int iseq, unsigned char* buf);

  int packet_generator(int type, unsigned char* sbuf);

  void set_HBrate(unsigned int rate){HBrate = rate;}
  unsigned int get_HBrate(){return HBrate;}

  void set_WordCount(int fnWordCount){ nWordCount = fnWordCount; }
  unsigned int get_WCount(){return nWordCount;}

  void Init();
  void Delete();

  unsigned int get_nseq(){return nseq;}
  void initHBF(){ 
    nseq = 0;
    sflag = 0;
  }

  static bool LRcompare(const LRTdc::AmQTdcWord& a1, const LRTdc::AmQTdcWord& a2);
  static bool HRcompare(const HRTdc::AmQTdcWord& a1, const HRTdc::AmQTdcWord& a2);

 private :

  const int fnByte      {8};
  const int maxTdcHits  {128*4};
  const int maxNdelimByte   {32};

  unsigned char* databuf;
  
  bool mdebug;
  unsigned int HBrate;
  int TotalSize;
  unsigned char* StackBuf;
  int StackSize;
  unsigned int nWordCount;
  unsigned int nseq;
  
  unsigned int hbframe_count;
  unsigned int spill_count;

  int sflag {0};
};

#endif


