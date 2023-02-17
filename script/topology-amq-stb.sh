#!/bin/bash

#host=127.0.0.1
#port=6379
#db=0
server=redis://127.0.0.1:6379/0

#function config_endpoint () {
function endpoint () {
  # Usage: 
  #   config_endpoint "service" "channel" "parameters"
  
  echo redis-cli -u $server hset daq_service:topology:endpoint:$1:$2 ${@:3}  
  redis-cli -u $server hset daq_service:topology:endpoint:$1:$2 ${@:3}  
}

#function config_link () {
function link () {
  # config_link "service1" "channel" "service2" "channel" "parameters"
  
  echo redis-cli -u $server set daq_service:topology:link:$1:$2,$3:$4 non
  redis-cli -u $server set daq_service:topology:link:$1:$2,$3:$4 none
}


echo "---------------------------------------------------------------------"
echo " config endpoint (socket)"
echo "---------------------------------------------------------------------"
#------------------------------------------------------------------------------------
#            service                channel         options
#------------------------------------------------------------------------------------

# Sampler 
endpoint     AmQStrTdcSampler        data           type push  method bind 

# subtime frame builder
endpoint     STFBuilder              data-in        type pull method connect 
endpoint     STFBuilder              data-out       type push method connect    

# Sink
endpoint     FileSink                in             type pull  method bind


echo "---------------------------------------------------------------------"
echo " config link"
echo "---------------------------------------------------------------------"
#------------------------------------------------------------------------------------
#       service1                channel1              service2        channel2      
#------------------------------------------------------------------------------------

link    AmQStrTdcSampler        data                 STFBuilder       data-in
link    STFBuilder              data-out             FileSink         in
