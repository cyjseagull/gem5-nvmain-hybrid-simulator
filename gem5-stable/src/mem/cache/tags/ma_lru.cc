/*
 * Copyright (c) 2012-2013 ARM Limited
 * All rights reserved.
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2003-2005 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Erik Hallnor
 */

/**
 * @file
 * Definitions of LRU tag store.
 */

#include <string>

#include "base/intmath.hh"
#include "debug/Cache.hh"
#include "debug/CacheRepl.hh"
#include "mem/cache/tags/ma_lru.hh"
#include "mem/cache/base.hh"
#include "sim/core.hh"
#define SAMPLING_INTERVAL 1000
#define HIT_TIME 20
#define SAMPLING_SET_DIVISION 32

using namespace std;

MALRU::MALRU(const Params *p)
    :BaseTags(p), assoc(p->assoc),
     numSets(p->size / (p->block_size * p->assoc)),
     sequentialAccess(p->sequential_access), DRAMPenalty(300), PCMPenalty(900), 
	 border(p->assoc / 2 - 1), sampleTimes(0), keyDelay(332), Pdram(0.f), Ppcm(0.f)
{
    // Check parameters
    if (blkSize < 4 || !isPowerOf2(blkSize)) {
        fatal("Block size must be at least 4 and a power of 2");
    }
    if (numSets <= 0 || !isPowerOf2(numSets)) {
        fatal("# of sets must be non-zero and a power of 2");
    }
    if (assoc <= 0) {
        fatal("associativity must be greater than zero");
    }
    if (hitLatency <= 0) {
        fatal("access latency must be greater than zero");
    }

    blkMask = blkSize - 1;
    setShift = floorLog2(blkSize);
    setMask = numSets - 1;
    tagShift = setShift + floorLog2(numSets);
    warmedUp = false;
    /** @todo Make warmup percentage a parameter. */
    warmupBound = numSets * assoc; 
    
	sets = new SetType[numSets];
    blks = new BlkType[numSets * assoc];
    // allocate data storage in one big chunk
    numBlocks = numSets * assoc;
    dataBlks = new uint8_t[numBlocks * blkSize];

    unsigned blkIndex = 0;       // index into blks array
    for (unsigned i = 0; i < numSets; ++i) {
        sets[i].assoc = assoc;

        sets[i].blks = new BlkType*[assoc];

        // link in the data blocks
        for (unsigned j = 0; j < assoc; ++j) {
            // locate next cache block
            BlkType *blk = &blks[blkIndex];
            blk->data = &dataBlks[blkSize*blkIndex];
            ++blkIndex;

            // invalidate new cache block
            blk->invalidate();

            //EGH Fix Me : do we need to initialize blk?

            // Setting the tag to j is just to prevent long chains in the hash
            // table; won't matter because the block is invalid
            blk->tag = j;
            blk->whenReady = 0;
            blk->isTouched = false;
            blk->size = blkSize;
            sets[i].blks[j]=blk;
            blk->set = i;
        }
    }

	DRAMStack = new Addr*[numSets / SAMPLING_SET_DIVISION];
	PCMStack = new Addr*[numSets / SAMPLING_SET_DIVISION];

	for(int i = 0; i < numSets / SAMPLING_SET_DIVISION; i++)
	{
		DRAMStack[i] = new Addr[assoc];
		PCMStack[i] = new Addr[assoc];
	}
	
	ZDRAM = new float*[assoc+2];
	ZPCM = new float*[assoc+2];
	for(int i = 0; i < assoc + 2; i++)
	{
		ZDRAM[i] = new float[assoc+1];
		ZPCM[i] = new float[assoc+1];
	}	

	DReuseDistance = new uint64_t[assoc+1];	
	PReuseDistance = new uint64_t[assoc+1];	
	hybridDistance = new uint64_t[assoc+1];

	DRAMDistribution = new float[assoc+1];
	PCMDistribution = new float[assoc+1];

	addrList.clear();
	//grtprintQueue.clear();
	sampleQueue.clear();
	otsuList.clear();
}

MALRU::~MALRU()
{
    delete [] dataBlks;
    delete [] blks;
    delete [] sets;
}

void
MALRU::insertSendTime(PacketPtr pkt, uint64_t time)
{
	Element temp;
	temp.pc = pkt->req->getPC();
	temp.address = pkt->getAddr();
	temp.sendTime = time;
	temp.recvTime = 0;
	temp.state = 0;
	temp.opType = pkt->cmdString();
	addrList.push_back(temp);

	//grtprintQueue.push_back(temp);

	if(extractSet(pkt->getAddr()) % SAMPLING_SET_DIVISION == 0)
	{
		Data tmp;
		tmp.addr = pkt->getAddr();
		tmp.type = -1;
		sampleQueue.push_back(tmp);
	}
}

void
MALRU::insertRecvTime(PacketPtr pkt, uint64_t time)
{	
	Addr tempAddr = pkt->getAddr();
	std::vector<Element>::iterator iter;
	
	bool flag = false;
	iter = addrList.end();
	do
	{
		iter--;

		if((*iter).address == tempAddr)
		{
			if((*iter).recvTime != 0)
			{
				continue;
			}
		
			(*iter).recvTime = time;
			flag = true;
			
		//	printInfo((*iter).pc, (*iter).opType, (*iter).address, (*iter).state, (*iter).sendTime, (*iter).recvTime);
			checkOTSU(((*iter).recvTime - (*iter).sendTime)/500);
			
			break;
		}

	}while(iter != addrList.begin());
/**
	for(iter = addrList.begin(); iter != addrList.end(); iter++)
	{
		if((*iter).address == tempAddr)
		{
			if((*iter).recvTime != 0)
			{
				continue;
			}
		
			(*iter).recvTime = time;
			flag = true;
			
			printInfo((*iter).pc, (*iter).opType, (*iter).address, (*iter).state, (*iter).sendTime, (*iter).recvTime);
			
			break;
		}
	}	
*/
	assert(flag);

	std::list<Element>::iterator qiter;
	//grt
/*	flag = false;
	for(qiter = printQueue.begin(); qiter != printQueue.end(); qiter++)
	{
		if((*qiter).address == tempAddr)
		{
			if((*qiter).recvTime != 0)
			{
				continue;
			}
			
			(*qiter).recvTime = time;
			flag = true;
			break;
		}
	}
	assert(flag);
	
	qiter = printQueue.begin();
	while(qiter != printQueue.end() && (*qiter).recvTime != 0)
	{
		printInfo((*qiter).pc, (*qiter).opType, (*qiter).address, (*qiter).state, (*qiter).sendTime, (*qiter).recvTime);
				
		printQueue.pop_front();
		qiter = printQueue.begin();
	}
*/
	//sampling.
	if(extractSet(tempAddr) % SAMPLING_SET_DIVISION == 0)
	{
		std::list<Data>::iterator sampleiter;
		
		flag = false;
		for(sampleiter = sampleQueue.begin(); sampleiter != sampleQueue.end(); sampleiter++)
		{
			if((*sampleiter).addr == tempAddr)
			{
				if((*sampleiter).type != -1)
				{
					continue;
				}	
				(*sampleiter).type = isShortLatency(tempAddr) ? 0 : 1; 
				flag = true;
				break;
			}
		}
		assert(flag);

		sampleiter = sampleQueue.begin();
		while(sampleiter != sampleQueue.end() && (*sampleiter).type != -1)
		{
			referenceStack((*sampleiter).addr, (*sampleiter).type);		
		
			sampleQueue.pop_front();
			sampleiter = sampleQueue.begin();
		}	
	}
}
/*
void
MALRU::handleHit(PacketPtr pkt, uint64_t time)
{
	if(printQueue.size()==0)
	{
		printInfo(pkt->req->getPC(), pkt->cmdString(), pkt->getAddr(), 1, time, time);
	}
	else
	{
		Element temp;
		temp.pc = pkt->req->getPC();
		temp.address = pkt->getAddr();
		temp.sendTime = time;
		temp.recvTime = time;
		temp.state = 1;
		temp.opType = pkt->cmdString();
	
		printQueue.push_back(temp);
	}
}
*/

void
MALRU::checkOTSU(uint64_t delay)
{
	if(otsuList.size() == 0)
	{
		otsuList.push_back({delay, 1});
	}	
	else
	{
		bool flag = false;
		std::list<OTSU>::iterator iter;
		for(iter = otsuList.begin(); iter != otsuList.end(); iter++)
		{
			if(delay == (*iter).delay)
			{
				(*iter).count++;
				flag = true;
			}
		}
		
		if(!flag)
			otsuList.push_back({delay, 1});
	}

}

void
MALRU::calKeyDelay()
{
	float w0, w1, u0, u1, deltaTemp, deltaMax;
	uint64_t delayTemp;
	uint64_t total = 0;
	
	std::list<OTSU>::iterator iter, inneriter;
	
	for(iter = otsuList.begin(); iter != otsuList.end(); iter++)
	{
		total += (*iter).count;	
	}
	
	for(iter = otsuList.begin(); iter != otsuList.end(); iter++)
	{
		delayTemp = (*iter).delay;
		w0 = w1 = u0 = u1 = deltaTemp = deltaMax = 0.f;
		
		for(inneriter = otsuList.begin(); inneriter != otsuList.end(); inneriter++)
		{
			float temp = (*inneriter).count * 1.0f/ total; 
			
			if((*inneriter).delay <= delayTemp)
			{
				w0 += temp;
				u0 += (*inneriter).delay * temp; 
			}	
			else
			{
				w1 += temp;
				u1 += (*inneriter).delay * temp; 
			}
		}
		
		u0 /= w0;
		u1 /= w1;
		deltaTemp = w0 * w1 * (u0 - u1) * (u0 - u1);
		if(deltaTemp > deltaMax)
		{
			deltaMax = deltaTemp;
			keyDelay = delayTemp;
			DRAMPenalty = u0;
			PCMPenalty = u1;
		}
		
	}

	std::cout<<"\nkeyDelay = "<<keyDelay<<std::endl;
	std::cout<<"DRAM Penalty = "<<DRAMPenalty<<std::endl;
	std::cout<<"PCM Penalty = "<<PCMPenalty<<std::endl;
}

void
MALRU::printInfo(uint64_t pc, std::string op, uint64_t addr, unsigned state, uint64_t st, uint64_t rt)
{
	std::cout<<"0x"<<std::hex<<pc
				<<" "<<op
				<<" 0x"<<std::hex<<addr
				<<" "<<state
	//			<<" "<<std::dec<<st
	//			<<" "<<std::dec<<rt
				<<" "<<std::dec<<rt-st<<std::endl;
}


void
MALRU::samplingReference(Addr addr, Addr* sampleStack, uint64_t* distance)
{
	for(int i = 0; i < assoc; i++)
	{
		if(extractTag(sampleStack[i]) == extractTag(addr)) //match the tag of the address.
		{
			moveToMRU(addr, sampleStack);
		//	if(warmedUp)
			distance[i]++;
			
			return;			
		}	
	}

	//miss, distance = M+1;
	for(int i = assoc - 1; i > 0; i-- )
	{
		sampleStack[i] = sampleStack[i-1];
	}
	
	//addr into MRU
	sampleStack[0] = addr;
//	if(warmedUp)
	distance[assoc]++;
}

void
MALRU::moveToMRU(Addr addr, Addr* sampleStack)
{
	if(extractTag(sampleStack[0]) == extractTag(addr))
	{
		return;
	}

	int i = 0;
	Addr next = addr;
	
	do
	{
		assert(i < assoc);
		
		Addr temp = sampleStack[i];
		sampleStack[i] = next;
		next = temp;
		++i;
	}while(extractTag(next) != extractTag(addr));
}

bool
MALRU::isShortLatency(Addr addr)
{
	assert(addrList.size() != 0);
    std::vector<Element>::iterator iter;
	iter = addrList.end();
	iter--;
	for( ; ; iter--)
	{
		if((*iter).address == addr)
		{
			return (*iter).recvTime - (*iter).sendTime > 200000 ? false : true;		
		}
	
		if(iter == addrList.begin())
		{
			break;
		}
	}
	assert(false);
}

void
MALRU::referenceStack(Addr addr, int type)
{
    unsigned set = extractSet(addr);
	if(type == 0)
	{
		samplingReference(addr, DRAMStack[set/SAMPLING_SET_DIVISION], DReuseDistance);
	}
	else
	{
		samplingReference(addr, PCMStack[set/SAMPLING_SET_DIVISION], PReuseDistance);
	}
	
	sampleTimes++;
	
	if(sampleTimes > SAMPLING_INTERVAL)
	{	
		calKeyDelay();
		otsuList.clear();
		calDistribution();
		int tmpBorder = calBorder();
		std::cout<<"Border: "<<tmpBorder + 1<<std::endl;
		sampleTimes = 0;	

		for(int i = 0; i < assoc + 1; i++)
		{
			DReuseDistance[i] = 0;
			PReuseDistance[i] = 0;
		}

	/**	for(int i = 0; i < numSets / SAMPLING_SET_DIVISION; i++)
		{
			for(int j = 0; j < assoc; j++)
			{
				DRAMStack[i][j] = 0;
				PCMStack[i][j] = 0;
			}
		}
	*/	
	}
}

int
MALRU::calBorder()
{
	int min = assoc - 1;
	float minCost = calCost(min);
	std::cout<<assoc-1<<": "<<minCost<<std::endl;
	for(int i = assoc - 2; i > 0; i--)
	{
		float temp = calCost(i);
		std::cout<<i<<": "<<temp<<std::endl;
		if(temp < minCost)
		{
			minCost = temp;
			min = i;
		}
	}	
	
	return min - 1;
}

float
MALRU::calHybridRate(bool isDRAM)
{
	float sum = 0.f;
	float s;
	
	if(isDRAM)
	{
		for(int i = 1; i < assoc + 1; i++)
		{
			s = 0.f;
			for(int j = i; j < assoc + 1; j++)
			{	
				float t = ZDRAM[j][i];
				s += t; 
			}

			s *= DRAMDistribution[i-1];
			sum += s;
		}
	}
	else
	{
		for(int i = 1; i < assoc + 1; i++)
		{
			s = 0.f;
			for(int j = i; j < assoc + 1; j++)
			{	
				float t = ZPCM[j][i];
				s += t; 
			}

			s *= PCMDistribution[i-1];
			sum += s;
		}
	}

	return sum;
}

float
MALRU::calCost(int location)
{
	for(int j = 0; j < assoc + 2; j++)
	{
		for(int i = 0; i < assoc + 1; i++)
		{
			ZDRAM[j][i] = 0.f;
			ZPCM[j][i] = 0.f;
		}
	}
	ZDRAM[1][1] = Pdram;
	ZPCM[1][1] = Ppcm;
		
	float *tempSum = new float[assoc + 1];
	float *tempSum2 = new float[assoc + 1];
	for(int i = 0; i < assoc + 1; i++)
	{
		tempSum[i] = 0.f;
		tempSum2[i] = 0.f;
	}
	tempSum[1] += ZDRAM[1][1];
	tempSum2[1] += ZPCM[1][1];

	for(int j = 2; j < assoc + 1; j++)
	{
		for(int i = 1; i < assoc + 1 && i <= j; i++)
		{
			if(j <= location + 1)
			{
				float tmpD = 1 - Pdram * calSumDis(DRAMDistribution, 1, i-1) - Ppcm * calSumDis(PCMDistribution, 1, j-i);
                float tmpP = 1 - Ppcm * calSumDis(PCMDistribution, 1, i-1) - Pdram * calSumDis(DRAMDistribution, 1, j-i);	
				
				ZDRAM[j][i] = tmpD < 0.0000000001 ? 0 : ( ZDRAM[j-1][i] * Ppcm * calSumDis(PCMDistribution, j-i, assoc+1) 
                            + ZDRAM[j-1][i-1] * Pdram * calSumDis(DRAMDistribution, i, assoc+1) ) / tmpD ;	
					
				ZPCM[j][i] = tmpP < 0.0000000001 ? 0 : ( ZPCM[j-1][i] * Pdram * calSumDis(DRAMDistribution, j-i, assoc+1) 
                            + ZPCM[j-1][i-1] * Ppcm * calSumDis(PCMDistribution, i, assoc+1) ) / tmpP;	
			}
			else
			{
				ZDRAM[j][i] = 0;

				if(i - j + location + 1 > 0)
					ZPCM[j][i] = ZPCM[location+1][i-j+location+1];
				else
					ZPCM[j][i] = 0.f;
			}

			tempSum[i] += ZDRAM[j][i];
			tempSum2[i] += ZPCM[j][i];
		}
	}
	
	for(int i = 1; i < assoc + 1; i++)
	{
		float td = 1 - tempSum[i];
		float tp = 1 - tempSum2[i];

		ZDRAM[assoc+1][i] = td < 0.0001 ? 0 : td;
		ZPCM[assoc+1][i] = tp < 0.0001 ? 0 : tp;
	}
	/**	
	std::cout<<"\nCheck DRAM'Z:\n##################"<<std::endl;
	for(int j = 1; j < assoc + 2; j++)
	{
		for(int i = 1; i < assoc + 1; i++)
		{
			std::cout<<ZDRAM[j][i]<<"  ";
		}
		std::cout<<"\n";
	}	
	std::cout<<"##################"<<std::endl;
		
	std::cout<<"\nCheck PCM'Z:\n##################"<<std::endl;
	for(int j = 1; j < assoc + 2; j++)
	{
		for(int i = 1; i < assoc + 1; i++)
		{
			std::cout<<ZPCM[j][i]<<"  ";
		}
		std::cout<<"\n";
	}	
	std::cout<<"##################"<<std::endl;
	*/
	float DRAMRate = calHybridRate(true);
	float PCMRate = calHybridRate(false);
	//std::cout<<"DRAM Hybrid Hit Rate: "<<DRAMRate<<"\nPCM Hybrid Hit Rate: "<<PCMRate<<std::endl;
	return (HIT_TIME + DRAMPenalty * (1-DRAMRate))*Pdram
				+ (HIT_TIME + PCMPenalty * (1-PCMRate))*Ppcm;	
}

/**
float
MALRU::calZ(float p_a, float p_b, float *dis_a, float *dis_b, int j, int i)
{
	if(i == 1 && j == 1)
	{
		return p_a;
	}

	if(j < i || i < 1)
	{
		return 0;
	}

	float temp = 1 - p_a * calSumDis(dis_a, 1, i-1) - p_b * calSumDis(dis_b, 1, j-i);	

	return temp == 0 ? 0 : ( calZ(p_a, p_b, dis_a, dis_b, j-1, i) * p_b * calSumDis(dis_b, j-i, assoc+1) 
				+ calZ(p_a, p_b, dis_a, dis_b, j-1, i-1) * p_a * calSumDis(dis_a, i, assoc+1) ) / temp;	
}

float
MALRU::calZA(int j, int i, int location)
{
	if(j > location + 1)
	{
		return 0;
	}

	return calZ(Pdram, Ppcm, DRAMDistribution, PCMDistribution, j, i);
}

float
MALRU::calZB(int j, int i, int location)
{
	if(j <= location + 1)
	{
		return calZ(Ppcm, Pdram, PCMDistribution, DRAMDistribution, j, i);
	}

	return calZB(location+1, i-j+location+1, location);
}
*/

float
MALRU::calSumDis(float *distribution, int begin, int end)
{//cal the sum of some dis.
	float sum = 0.f;
	for(int i = begin; i <= end; i++)
	{
		sum += distribution[i-1];// -1 important
	}

	return sum;
}

void
MALRU::calDistribution()
{//cal the reuse distance distribution of pcm and dram, Pdram, and Ppcm.
	int64_t totalDRAM, totalPCM;
	totalDRAM = 0;
	totalPCM = 0;
	
	for(int i = 0; i < assoc + 1; i++)
	{
		totalDRAM += DReuseDistance[i]; 
		totalPCM += PReuseDistance[i]; 
	}
	
	for(int i = 0; i < assoc + 1; i++)
	{
		DRAMDistribution[i] = (totalDRAM == 0) ? 0 : DReuseDistance[i] * 1.0 / totalDRAM;
		PCMDistribution[i] = (totalPCM == 0) ? 0 : PReuseDistance[i] * 1.0 / totalPCM;
	}

	Pdram = totalDRAM * 1.0 / (totalDRAM + totalPCM);
	Ppcm = totalPCM * 1.0 / (totalDRAM + totalPCM);

	std::cout<<"\nDRAM: "<<totalDRAM<<"\t"<<Pdram<<std::endl;
	std::cout<<"======================="<<std::endl;
	for(int i = 0; i < assoc + 1; i++)
	{
		std::cout<<i<<"\t"<<DReuseDistance[i]<<"\t"<<DRAMDistribution[i]<<std::endl; 
	}
	std::cout<<"======================="<<std::endl;

	std::cout<<"\nPCM: "<<totalPCM<<"\t"<<Ppcm<<std::endl;
	std::cout<<"======================="<<std::endl;
	for(int i = 0; i < assoc + 1; i++)
	{
		std::cout<<i<<"\t"<<PReuseDistance[i]<<"\t"<<PCMDistribution[i]<<std::endl; 
	}
	std::cout<<"======================="<<std::endl;
}

void		
MALRU::handleSample(Addr addr, int type)
{
	assert(type != -1);
	
	if(sampleQueue.size() == 0)
	{
		referenceStack(addr, type);
   	}
	else
	{
		sampleQueue.push_back({addr, type});
	}
}

MALRU::BlkType*
MALRU::accessBlock(PacketPtr pkt, bool is_secure, Cycles &lat, int master_id)
{
	Addr addr = pkt->getAddr();
    Addr tag = extractTag(addr);
    unsigned set = extractSet(addr);
    BlkType *blk = sets[set].findBlk(tag, is_secure);
    lat = hitLatency;

    // Access all tags in parallel, hence one in each way.  The data side
    // either accesses all blocks in parallel, or one block sequentially on
    // a hit.  Sequential access with a miss doesn't access data.
    tagAccesses += assoc;
    if (sequentialAccess) {
        if (blk != NULL) {
            dataAccesses += 1;
        }
    } else {
        dataAccesses += assoc;
    }

    if (blk != NULL) {
        // move this block to head of the MRU list
        sets[set].moveToHead(blk);
        DPRINTF(CacheRepl, "set %x: moving blk %x (%s) to MRU\n",
                set, regenerateBlkAddr(tag, set), is_secure ? "s" : "ns");
        if (blk->whenReady > curTick()
            && cache->ticksToCycles(blk->whenReady - curTick()) > hitLatency) {
            lat = cache->ticksToCycles(blk->whenReady - curTick());
        }
        blk->refCount += 1;

		//Added for new policy.
		if(set % SAMPLING_SET_DIVISION == 0)
		{
			handleSample(addr, blk->type);
		}
	}	

    return blk;
}


MALRU::BlkType*
MALRU::findBlock(Addr addr, bool is_secure) const
{
    Addr tag = extractTag(addr);
    unsigned set = extractSet(addr);
    BlkType *blk = sets[set].findBlk(tag, is_secure);
    return blk;
}

MALRU::BlkType*
MALRU::findVictim(Addr addr)
{
    unsigned set = extractSet(addr);
    // grab a replacement candidate
	BlkType *blk = NULL;
	
/**	std::cout<<"\nfindVictim from set "<<set<<std::endl;
	std::cout<<"=================="<<std::endl;

	for(int i = 0; i < assoc; i++)
	{
		std::cout<<"type: "<<sets[set].blks[i]->type<<std::endl;
	}	
	
	std::cout<<"==================\n"<<std::endl;

*/
	if(-1 == border)
	{
      	blk = sets[set].blks[assoc-1];
	}
	else
	{
		blk = sets[set].blks[assoc-1];

		if(1 == blk->type)
		{
			std::cout<<"PCM in LRU"<<std::endl;
			int way = assoc-2;
			while(way > border)
			{
				blk = sets[set].blks[way];
				std::cout<<"way: "<<way<<"\ttype: "<<blk->type<<std::endl;
				if(blk->type == 0)
				{
					std::cout<<"Victim: DRAM in pos "<<way<<std::endl;
					break;
				}
				way--;
			}

			if(way == border)
			{
				blk = sets[set].blks[assoc-1];
			}
		}
	}

    if (blk->isValid()) {
        DPRINTF(CacheRepl, "set %x: selecting blk %x for replacement\n",
                set, regenerateBlkAddr(blk->tag, set));
    }
    return blk;
}

void
MALRU::insertBlock(PacketPtr pkt, BlkType *blk)
{
    Addr addr = pkt->getAddr();
    MasterID master_id = pkt->req->masterId();
    uint32_t task_id = pkt->req->taskId();
    bool is_secure = pkt->isSecure();
    if (!blk->isTouched) {
        tagsInUse++;
        blk->isTouched = true;
        if (!warmedUp && tagsInUse.value() >= warmupBound) {
            warmedUp = true;
            warmupCycle = curTick();
        }
    }

    // If we're replacing a block that was previously valid update
    // stats for it. This can't be done in findBlock() because a
    // found block might not actually be replaced there if the
    // coherence protocol says it can't be.
    if (blk->isValid()) {
        replacements[0]++;
        totalRefs += blk->refCount;
        ++sampledRefs;
        blk->refCount = 0;

        // deal with evicted block
        assert(blk->srcMasterId < cache->system->maxMasters());
        occupancies[blk->srcMasterId]--;

        blk->invalidate();
    }

    blk->isTouched = true;
    // Set tag for new block.  Caller is responsible for setting status.
    blk->tag = extractTag(addr);
    if (is_secure)
        blk->status |= BlkSecure;

    // deal with what we are bringing in
    assert(master_id < cache->system->maxMasters());
    occupancies[master_id]++;
    blk->srcMasterId = master_id;
    blk->task_id = task_id;
    blk->tickInserted = curTick();
	/** Added for new policy. */
	blk->type = isShortLatency(pkt->getAddr()) ? 0 : 1; 
    std::vector<Element>::iterator iter;
	//std::cout<<"insertBlock addr: "<<std::hex<<pkt->getAddr()<<std::endl;
/**	std::cout<<"-----------------------"<<std::endl;
	for(iter = addrList.begin(); iter != addrList.end(); iter++)
	{
		std::cout<<std::hex<<(*iter).address<<"\t"<<std::dec<<(*iter).recvTime-(*iter).sendTime<<std::endl;
	}
	std::cout<<"-----------------------\n"<<std::endl;
   */ 
	unsigned set = extractSet(addr);
    sets[set].moveToHead(blk);
/*
	std::cout<<"set: "<<std::dec<<set<<std::endl;
	std::cout<<"*********************"<<std::endl;

	for(int i = 0; i < assoc; i++)
	{
		std::cout<<i<<"\ttype: "<<std::dec<<sets[set].blks[i]->type<<std::endl;
	}	
	
	std::cout<<"*********************\n"<<std::endl;
  */
	 // We only need to write into one tag and one data block.
    tagAccesses += 1;
    dataAccesses += 1;
}

void
MALRU::invalidate(BlkType *blk)
{
    assert(blk);
    assert(blk->isValid());
    tagsInUse--;
    assert(blk->srcMasterId < cache->system->maxMasters());
    occupancies[blk->srcMasterId]--;
    blk->srcMasterId = Request::invldMasterId;
    blk->task_id = ContextSwitchTaskId::Unknown;
    blk->tickInserted = curTick();

    // should be evicted before valid blocks
    unsigned set = blk->set;
	//Added for new policy.
	blk->type = -1;
    sets[set].moveToTail(blk);
}

void
MALRU::clearLocks()
{
    for (int i = 0; i < numBlocks; i++){
        blks[i].clearLoadLocks();
    }
}

MALRU *
MALRUParams::create()
{
    return new MALRU(this);
}
std::string
MALRU::print() const {
    std::string cache_state;
    for (unsigned i = 0; i < numSets; ++i) {
        // link in the data blocks
        for (unsigned j = 0; j < assoc; ++j) {
            BlkType *blk = sets[i].blks[j];
            if (blk->isValid())
                cache_state += csprintf("\tset: %d block: %d %s\n", i, j,
                        blk->print());
        }
    }
    if (cache_state.empty())
        cache_state = "no valid tags\n";
    return cache_state;
}

void
MALRU::cleanupRefs()
{
    for (unsigned i = 0; i < numSets*assoc; ++i) {
        if (blks[i].isValid()) {
            totalRefs += blks[i].refCount;
            ++sampledRefs;
        }
    }
}

void
MALRU::computeStats()
{
    for (unsigned i = 0; i < ContextSwitchTaskId::NumTaskId; ++i) {
        occupanciesTaskId[i] = 0;
        for (unsigned j = 0; j < 5; ++j) {
            ageTaskId[i][j] = 0;
        }
    }

    for (unsigned i = 0; i < numSets * assoc; ++i) {
        if (blks[i].isValid()) {
            assert(blks[i].task_id < ContextSwitchTaskId::NumTaskId);
            occupanciesTaskId[blks[i].task_id]++;
            Tick age = curTick() - blks[i].tickInserted;
            assert(age >= 0);

            int age_index;
            if (age / SimClock::Int::us < 10) { // <10us
                age_index = 0;
            } else if (age / SimClock::Int::us < 100) { // <100us
                age_index = 1;
            } else if (age / SimClock::Int::ms < 1) { // <1ms
                age_index = 2;
            } else if (age / SimClock::Int::ms < 10) { // <10ms
                age_index = 3;
            } else
                age_index = 4; // >10ms

            ageTaskId[blks[i].task_id][age_index]++;
        }
    }
}

