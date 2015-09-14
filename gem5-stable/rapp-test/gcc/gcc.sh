#!/bin/sh
BENCH_HOME=/home/liao/chenyujie/benchs/cpu_spec2006/benchspec/CPU2006/403.gcc
REPATH=../..
NVMAINREPATH=../../..
$REPATH/build/X86/gem5.opt \
--stats-file=gem5-DRAMCache-stats.log		\
$REPATH/configs/example/se.py \
--cpu-type=detailed   --num-cpus=4   \
--cpu-clock=2GHz		\
--mem-type=nvmain --nvmain-config=$NVMAINREPATH/nvmain/Config/Hybrid_example.config     \
--caches   \
--maxinsts=$1   \
--l1i_size 32kB --l1d_size 32kB		\
--l2cache --l2_size 256kB    \
--l3cache --l3_size 1MB	 \
--cmd="$BENCH_HOME/exe/gcc_base.amd64-m64-gcc48-nn" --options="$BENCH_HOME/data/ref/input/s04.i" -n 1   
