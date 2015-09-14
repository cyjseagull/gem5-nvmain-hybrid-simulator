#!/bin/sh 
../../build/X86/gem5.opt \
--stats-file=gem5-DRAMCache-stats.log		\
../../configs/example/se.py \
--cpu-type=detailed   --num-cpus=4   \
--cpu-clock=2GHz		\
--mem-type=nvmain --nvmain-config=../../../nvmain/Config/Hybrid_NoMigration_example.config     \
--caches   \
--maxinsts=$1   \
--l1i_size 32kB --l1d_size 32kB		\
--l2cache --l2_size 256kB    \
--l3cache --l3_size 1MB	 \
--cmd="/home/liao/chenyujie/benchs/cpu_spec2006/benchspec/CPU2006/403.gcc/exe/gcc_base.amd64-m64-gcc48-nn" --options="/home/liao/chenyujie/benchs/cpu_spec2006/benchspec/CPU2006/403.gcc/data/ref/input/s04.i" -n 1   
