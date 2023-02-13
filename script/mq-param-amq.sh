#!/bin/bash

server=redis://127.0.0.1:6379/2

function param () {
  # "instance":"field" "value"
  #echo redis-cli -u $server set parameters:$1:$2 ${@:3}
  #redis-cli -u $server set parameters:$1:$2 ${@:3}
  echo redis-cli -u $server hset parameters:$1 ${@:2}
  redis-cli -u $server hset parameters:$1 ${@:2}
}

#=============================================================
# RLTDC 161~167, 170  HR 168, 169
#=============================================================
#      isntance-id                field       value  
param  AmQStrTdcSampler-0         ModuleTcpIp   192.168.2.168
param  AmQStrTdcSampler-1         ModuleTcpIp   192.168.2.169
param  AmQStrTdcSampler-2         ModuleTcpIp   192.168.2.161
param  AmQStrTdcSampler-3         ModuleTcpIp   192.168.2.162
#param Sink-0 multipart false
#param Sink-1 multipart false
#param Sink-2 multipart false
#param Sink-3 multipart false

param Sink-0 multipart true
param Sink-1 multipart true 
param Sink-2 multipart true 
param Sink-3 multipart true 

