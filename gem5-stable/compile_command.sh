#########################################################################
# File Name: compile_command.sh
# Author: YuJie Chen
# mail: cyjseagull@163.com
# Created Time: Fri 30 Jan 2015 08:53:57 AM CST
# Function: 
# Para:
#########################################################################
#!/bin/bash
scons EXTRAS=../nvmain build/X86/gem5.opt -j10
