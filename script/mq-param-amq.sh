#!/bin/bash

server=redis://127.0.0.1:6379/2

function param () {
  # "instance":"field" "value"
  #echo redis-cli -u $server set parameters:$1:$2 ${@:3}
  #redis-cli -u $server set parameters:$1:$2 ${@:3}
  echo redis-cli -u $server hset parameters:$1 ${@:2}
  redis-cli -u $server hset parameters:$1 ${@:2}
}

#===============================================================================================
# RLTDC 161~167, 170  HRTDC 168, 169
#===============================================================================================
#      isntance-id                field       value             field       value  
param  AmQStrTdcSampler-0         msiTcpIp   192.168.2.168   TdcType     2
param  AmQStrTdcSampler-1         msiTcpIp   192.168.2.169   TdcType     2
param  AmQStrTdcSampler-2         msiTcpIp   192.168.2.161   TdcType     1
param  AmQStrTdcSampler-3         msiTcpIp   192.168.2.162   TdcType     1
param  AmQStrTdcSampler-4         msiTcpIp   192.168.2.163   TdcType     1
param  AmQStrTdcSampler-5         msiTcpIp   192.168.2.164   TdcType     1
param  AmQStrTdcSampler-6         msiTcpIp   192.168.2.165   TdcType     1
param  AmQStrTdcSampler-7         msiTcpIp   192.168.2.166   TdcType     1
param  AmQStrTdcSampler-8         msiTcpIp   192.168.2.167   TdcType     1
param  AmQStrTdcSampler-9         msiTcpIp   192.168.2.170   TdcType     1
#param Sink-0 multipart false
#param Sink-1 multipart false
#param Sink-2 multipart false
#param Sink-3 multipart false

#
#
param FileSink-0 multipart true prefix 00
param FileSink-1 multipart true prefix 01
param FileSink-2 multipart true prefix 02
param FileSink-3 multipart true prefix 03
param FileSink-4 multipart true prefix 04
param FileSink-5 multipart true prefix 05
param FileSink-6 multipart true prefix 06
param FileSink-7 multipart true prefix 07
param FileSink-8 multipart true prefix 08
param FileSink-9 multipart true prefix 09
#param FileSink-1 multipart true 
#param Sink-2 multipart true 
#param Sink-3 multipart true 

