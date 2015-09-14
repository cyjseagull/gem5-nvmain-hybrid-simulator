# cpu2006_se.py
# Simple configuration script

import os, optparse, sys  
      
import m5    
from m5.objects import *  
from m5.util import addToPath  
      
addToPath('../common')  

import Options  
import Simulation  
from Caches import *  
import CacheConfig  
import MemConfig
import cpu2006  

# Get paths we might need.  It's expected this file is in m5/configs/example.
config_path = os.path.dirname(os.path.abspath(__file__))
print config_path
config_root = os.path.dirname(config_path)
print config_root
m5_root = os.path.dirname(config_root)
print m5_root

parser = optparse.OptionParser()
Options.addCommonOptions(parser)
Options.addSEOptions(parser)

# Benchmark options

parser.add_option("-b", "--benchmark", default="",
                 help="The benchmark to be loaded.")

parser.add_option("--chkpt", default="",
                 help="The checkpoint to load.")

execfile(os.path.join(config_root, "common", "Options.py"))

(options, args) = parser.parse_args()

if args:
    print "Error: script doesn't take any positional arguments"
    sys.exit(1)

process = []

if options.benchmark == 'perlbench':
   process.append(cpu2006.perlbench)
elif options.benchmark == 'bzip2':
   process.append(cpu2006.bzip2)
elif options.benchmark == 'gcc':
   process.append(cpu2006.gcc)
elif options.benchmark == 'bwaves':
   process.append(cpu2006.bwaves)
elif options.benchmark == 'gamess':
   process.append(cpu2006.gamess)
elif options.benchmark == 'mcf':
   process.append(cpu2006.mcf)
elif options.benchmark == 'milc':
   process.append(cpu2006.milc)
elif options.benchmark == 'zeusmp':
   process.append(cpu2006.zeusmp)
elif options.benchmark == 'gromacs':
   process.append(cpu2006.gromacs)
elif options.benchmark == 'cactusADM':
   process.append(cpu2006.cactusADM)
elif options.benchmark == 'leslie3d':
   process.append(cpu2006.leslie3d)
elif options.benchmark == 'namd':
   process.append(cpu2006.namd)
elif options.benchmark == 'gobmk':
   process.append(cpu2006.gobmk)
elif options.benchmark == 'dealII':
   process.append(cpu2006.dealII)
elif options.benchmark == 'soplex':
   process.append(cpu2006.soplex)
elif options.benchmark == 'povray':
   process.append(cpu2006.povray)
elif options.benchmark == 'calculix':
   process.append(cpu2006.calculix)
elif options.benchmark == 'hmmer':
   process.append(cpu2006.hmmer)
elif options.benchmark == 'sjeng':
   process.append(cpu2006.sjeng)
elif options.benchmark == 'GemsFDTD':
   process.append(cpu2006.GemsFDTD)
elif options.benchmark == 'libquantum':
   process.append(cpu2006.libquantum)
elif options.benchmark == 'h264ref':
   process.append(cpu2006.h264ref)
elif options.benchmark == 'tonto':
   process.append(cpu2006.tonto)
elif options.benchmark == 'lbm':
   process.append(cpu2006.lbm)
elif options.benchmark == 'omnetpp':
   process.append(cpu2006.omnetpp)
elif options.benchmark == 'astar':
   process.append(cpu2006.astar)
elif options.benchmark == 'wrf':
   process.append(cpu2006.wrf)
elif options.benchmark == 'sphinx3':
   process.append(cpu2006.sphinx3)
elif options.benchmark == 'xalancbmk':
   process.append(cpu2006.xalancbmk)
elif options.benchmark == 'specrand_i':
   process.append(cpu2006.specrand_i)
elif options.benchmark == 'specrand_f':
   process.append(cpu2006.specrand_f)

if options.chkpt != "":
   process[0].chkpt = options.chkpt

(CPUClass, test_mem_mode, FutureClass) = Simulation.setCPUClass(options)

#np = options.num_cpus 
np = 1

MemClass = Simulation.setMemClass(options)

system = System(cpu = [CPUClass(cpu_id=i) for i in xrange(np)],
                mem_mode = test_mem_mode,
                mem_ranges = [AddrRange(options.mem_size)],
                cache_line_size = options.cacheline_size)

# Create a top-level voltage domain
system.voltage_domain = VoltageDomain(voltage = options.sys_voltage)

# Create a source clock for the system and set the clock period
system.clk_domain = SrcClockDomain(clock =  options.sys_clock,
                                   voltage_domain = system.voltage_domain)

# Create a CPU voltage domain
system.cpu_voltage_domain = VoltageDomain()

# Create a separate clock domain for the CPUs
system.cpu_clk_domain = SrcClockDomain(clock = options.cpu_clock,
                                       voltage_domain =
                                       system.cpu_voltage_domain)

# All cpus belong to a common cpu_clk_domain, therefore running at a common
# frequency.
for cpu in system.cpu:
    cpu.clk_domain = system.cpu_clk_domain

for i in xrange(np):   
    system.cpu[i].workload = process[i]

system.membus = CoherentBus()

system.system_port = system.membus.slave
CacheConfig.config_cache(options, system)
MemConfig.config_mem(options, system)

root = Root(full_system = False, system = system)
Simulation.run(options, root, system, FutureClass)
