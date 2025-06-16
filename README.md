[![Build Status](https://dev.azure.com/eclipse-cyclonedds/cyclonedds/_apis/build/status/Pull%20requests?branchName=master)](https://dev.azure.com/eclipse-cyclonedds/cyclonedds/_build/latest?definitionId=4&branchName=master)
[![Coverity Status](https://scan.coverity.com/projects/19078/badge.svg)](https://scan.coverity.com/projects/eclipse-cyclonedds-cyclonedds)
[![Coverage](https://img.shields.io/azure-devops/coverage/eclipse-cyclonedds/cyclonedds/4/master)](https://dev.azure.com/eclipse-cyclonedds/cyclonedds/_build/latest?definitionId=4&branchName=master)
[![License](https://img.shields.io/badge/License-EPL%202.0-blue)](https://choosealicense.com/licenses/epl-2.0/)
[![License](https://img.shields.io/badge/License-EDL%201.0-blue)](https://choosealicense.com/licenses/edl-1.0/)

# This branch supports CycloneDDS over FreeRTOS-Plus-TCP stack on FreeRTOS.  
CycloneDDS looks have support on FreeRTOS+LWIP Stack,  
however lacking FreeRTOS-Plus-TCP Stack support  
This change managed to add +TCP support  

* base CycloneDDS version0.9.1  
* ddsrt socket / thread ...  

[Changes]  
  +TCP socket  
  ddsrt header and source  
  sockaddr ifaddr  
  .init_array section  
  log sink  
  multicast/unicast peer define in xml  
  TLS-Thread_Local_Storage from FreeRTOS SW impl instread of toolchain  
  sock_waitset  
  sock fdset  
  sendmsg/recvmsg for UDP  
  dds_align from 0.10.2  
  FragmentSize for jumbo and normal  

# DDS Readme refer to master readme
Link:
https://github.com/polejoe/cycloneDDS_RTOS/blob/master/README.md


