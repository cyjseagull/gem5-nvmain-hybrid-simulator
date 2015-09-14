HOME=/home/liao/chenyujie/benchs/cpu_spec2006/benchspec/CPU2006/401.bzip2
REPATH=../..
RENVMAINPATH=../../..
$REPATH/build/X86/gem5.opt \
--stats-file=gem5-DRAMCache-stats.log  \
$REPATH/configs/example/se.py \
--cpu-type=detailed   \
--cpu-clock=2GHz        \
--mem-type=nvmain --nvmain-config=$RENVMAINPATH/nvmain/Config/Hybrid_example.config         \
--maxinsts=$1       \
--caches    \
--l1i_size 32kB --l1d_size 32kB     \
--l2cache --l2_size 256kB    \
--l3cache --l3_size 1MB \
--cmd="$HOME/exe/bzip2_base.amd64-m64-gcc48-nn" --options="$HOME/data/ref/input/control" -n 1        \
