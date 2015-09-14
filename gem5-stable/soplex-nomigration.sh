./build/X86/gem5.opt \
--stats-file=gem5-DRAMCache-stats.log  \
configs/example/se.py \
--cpu-type=detailed   \
--cpu-clock=2GHz        \
--mem-type=nvmain --nvmain-config=../nvmain/Config/Hybrid_NoMigration_example.config         \
--maxinsts=$1       \
--caches    \
--l1i_size 32kB --l1d_size 32kB     \
--l2cache --l2_size 256kB    \
--l3cache --l3_size 1MB \
--cmd="/home/liao/chenyujie/benchs/cpu_spec2006/benchspec/CPU2006/450.soplex/exe/soplex_base.amd64-m64-gcc48-nn" --options="-m10000 /home/liao/chenyujie/benchs/cpu_spec2006/benchspec/CPU2006/450.soplex/exe/ref.mps" --output="soplex.ref.out" -n 1
