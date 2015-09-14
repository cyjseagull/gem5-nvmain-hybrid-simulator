# gem5-nvmain-hybrid-simulator
1 DRAM-NVM Hybrid-Simulator
-----------------------------
### 1.1 Full System Simultor : gem5
（1）Home Page of Simulator：http://www.gem5.org/Main_Page

### 1.2 Memory System Simulator: NVMain
（1）Home Page of Simulator: http://wiki.nvmain.org/    (unfortunately , home page of nvmain has already crashed)

### 1.3 how to attach nvmain with gem5
* environment requirement
        * python version < 3.0; 
        * scons >= 0.98.1
        * gcc >= 4.6.0
* compile gem5 with nvmain
        * patch gem5 with nvmain patch file: nvmain-classic-gem5-9850
        patch -p1 < final_patch/nvmain-classic-gem5-9850
        * compile with scons
        scons EXTRAS=nvmain-path -jn (n represents compiling threads num)
* run system with nvmain memory
        you can refer to example in directory rapp-test/gcc:--mem-type=nvmain --nvmain-config=nvmain-config-file-path

2 Cache Optimization Module in gem5-nvmain-hybrid-simulator
--------------------------------
    （1）Architecture and Principle of Cache Optimization in DRAM-PCM hybrid memory 
![principle of cache optimization](https://raw.github.com/cyjseagull/gem5-nvmain-hybrid-simulator/master/images/cache-optimization.png)
* adjust cache replacement policy according to cache miss penalty in DRAM-PCM hybrid memory system , for example: evict data block in LLC cache which fetched from PCM is expensive than data block fetched from DRAM . When evicting cache block from LLC cache , evict data block from DRAM first;
* recalculate cache replacement priority according to penalty of cache miss in memory system and reuse probability of cache block;

        （2）related source code
* gem5-stable/src/mem/cache/tags/
  
3. Rank-Based-Page-Placement Module in hybrid simulator implemented by multi-level-queue
--------------------------------
        （1）Principle of Rank-Based-Page-Placement Strategy
![principle of Rank-Based-Page-Placement](https://raw.github.com/cyjseagull/gem5-nvmain-hybrid-simulator/master/images/MultiQue.PNG)
* classfy memory pages into hot-pages and cold-pages according to access time;
* page access information is managed by multi-level-queue;


        （2）related source code
* nvmain/Utils/MultiQueMigrator/
* Decoders/Migrator/
* configuration file example: Config/Hybrid_example.config
* simple example scripts running CPU SPEC2006 benchmarks are placed in directory: gem5-stable/rapp-test


4. Programmer-Specific-Page-Migration in Hybrid memory system
---------------------------------------




