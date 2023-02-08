#include <cstdio>
#include <iostream>
#include "AmQLRTdcData.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

int main(){
  
  std::cout << " Start generator " << std::endl;
  
  AmQLRTdc amq;
  unsigned int iseq = 0;
  unsigned int HBrate = amq.get_HBrate();
  unsigned int itval = 0;

  unsigned char* data_buf;
  unsigned int buf_size = 128*4*HBrate + 32; //byte
  data_buf = new unsigned char[buf_size];

  std::cout <<"++HBrate: " << HBrate << std::endl;
  
  std::ofstream fout("./emul.dat", std::ios::binary);
  if(fout.fail()){
    std::cerr << "File open error!" << std::endl;
    return -1;
  }

  while(true){
    //    memset(data_buf, 0, buf_size);

    int dsize = amq.packet_generator(iseq, data_buf);
    iseq++;

    if(iseq == HBrate + 20){
      iseq = 0;
      itval++;
    }
    
    if(itval==2) break;

    if(dsize == -1){
      std::cout << "still..."<< std::endl;
      continue;
    }else if(dsize == -4){
      std::cout << "spill out"<< std::endl;
      continue;
    }

    //    int BufSize = 512*8;
    int BufSize = 256*8;

    if(dsize == BufSize){

      std::cout << "out 64 bit word count: "<< dsize << std::endl;
      for(int i=0; i< dsize; i++){
	printf("%02x ", data_buf[i]);
	if( ((i+1)%8) == 0 ){
	  printf("\n");
	}
      }
      
      fout.write(reinterpret_cast<const char*>(&data_buf[0]), dsize);

    }else{
      std::cout << "something wrong...humm"<< std::endl;
    }
    

  }

  fout.close();

  delete []data_buf;

  return 0;
}
