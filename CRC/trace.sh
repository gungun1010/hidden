#!/bin/bash
pinkit/pin-2.7-31933-gcc.3.4.6-ia32_intel64-linux/pin -t ./bin/CMPsim.gentrace.64 -threads 1 -o traces/ls.out -- /bin/ls
cd runs/
../bin/CMPsim.usetrace.64 -threads 1 -t ../traces/ls.out.trace.gz -o ls.stats -cache UL3:1024:64:16 -LLCrepl 1

