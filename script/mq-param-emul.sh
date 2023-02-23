#!/bin/bash

server=redis://127.0.0.1:6379/2

function param () {
  # "instance":"field" "value"
  #echo redis-cli -u $server set parameters:$1:$2 ${@:3}
  #redis-cli -u $server set parameters:$1:$2 ${@:3}
  echo redis-cli -u $server hset parameters:$1 ${@:2}
  redis-cli -u $server hset parameters:$1 ${@:2}
}

#======================================================================================================================================
#      isntance-id    field  value    field     value            field   value     field         value  field   value  field   value
param  Sampler-0      text   Hello    msiTcpIp  192.168.2.171    TdcType   1       max-iterations 0     MaxHBF  256    sflag   
param  Sampler-1      text   world    msiTcpIp  192.168.2.169    TdcType   1       max-iterations 0     MaxHBF  256    sflag   1      
param  Sampler-2      text   hoge     msiTcpIp  192.168.2.161    TdcType   1       max-iterations 0     MaxHBF  256    sflag   1     
param  Sampler-3      text   piyo     msiTcpIp  192.168.2.162    TdcType   1       max-iterations 0     MaxHBF  256    sflag   1    
param  Sampler-4      text   piyo     msiTcpIp  192.168.2.163    TdcType   1       max-iterations 0     MaxHBF  256    sflag   0     
param  Sampler-5      text   piyo     msiTcpIp  192.168.2.164    TdcType   1       max-iterations 0     MaxHBF  256    sflag   0     
param  Sampler-6      text   piyo     msiTcpIp  192.168.2.165    TdcType   1       max-iterations 0     MaxHBF  256    sflag   0     
param  Sampler-7      text   piyo     msiTcpIp  192.168.2.166    TdcType   1       max-iterations 0     MaxHBF  256    sflag   0
param  Sampler-8      text   piyo     msiTcpIp  192.168.2.167    TdcType   1       max-iterations 0     MaxHBF  256    sflag   0           
param  Sampler-9      text   piyo     msiTcpIp  192.168.2.170    TdcType   1       max-iterations 0     MaxHBF  256    sflag   0            

#param Sink-0 multipart false
#param Sink-1 multipart false
#param Sink-2 multipart false
#param Sink-3 multipart false

param FileSink-0  multipart true  prefix emdata ext .dat 
param FileSink-1  multipart true  prefix emdata ext .dat 
param FileSink-2  multipart true  prefix emdata ext .dat 
param FileSink-3  multipart true  prefix emdata ext .dat 
param FileSink-4  multipart true  prefix emdata ext .dat 
param FileSink-5  multipart true  prefix emdata ext .dat 
param FileSink-7  multipart true  prefix emdata ext .dat 
param FileSink-8  multipart true  prefix emdata ext .dat 
param FileSink-9  multipart true  prefix emdata ext .dat 

