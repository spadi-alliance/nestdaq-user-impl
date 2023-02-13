#ifndef RBCPDEC_
#define RBCPDEC_

/*************************************************
*                                                *
* SiTCP Remote Bus Control Protocol              *
* Header file                                    *
*                                                *
* 2010/05/31 Tomohisa Uchida                     *
*                                                *
*************************************************/
struct rbcp_header{
  unsigned char type;
  unsigned char command;
  unsigned char id;
  unsigned char length;
  unsigned int address;
};

#endif
