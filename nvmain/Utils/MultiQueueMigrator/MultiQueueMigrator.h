/*********************************************
 
Modified by dliu, based on CoinMigrator.h.

*********************************************/


#ifndef __NVMAIN_UTILS_MULTIQUEUEMIGRATOR_H__
#define __NVMAIN_UTILS_MULTIQUEUEMIGRATOR_H__

#include "src/NVMObject.h"
#include "src/Params.h"
#include "include/NVMainRequest.h"
namespace NVM {

#define MIG_READ_TAG GetTagGenerator( )->CreateTag("MIGREAD")
#define MIG_WRITE_TAG GetTagGenerator( )->CreateTag("MIGWRITE")

class MQMigrator;

class MultiQueueMigrator : public NVMObject
{
  public:
    MultiQueueMigrator( );
    ~MultiQueueMigrator( );

    void Init( Config *config );

    bool IssueAtomic( NVMainRequest *request );
    bool IssueCommand( NVMainRequest *request );
    bool RequestComplete( NVMainRequest *request );

    void Cycle( ncycle_t steps );

  private:
    bool promoBuffered, demoBuffered; 
    NVMAddress demotee, promotee; 
    NVMainRequest *promoRequest;
    NVMainRequest *demoRequest;

    ncounter_t numCols;
    bool queriedMemory;
    ncycle_t bufferReadLatency;
    Params *promotionChannelParams;
    ncounter_t totalPromotionPages;
    ncounter_t currentPromotionPage;
    ncounter_t promotionChannel;

    ncounter_t migrationCount;
    ncounter_t queueWaits;
    ncounter_t bufferedReads;
	ncounter_t channel_num;
	uint64_t trace_interval;
    bool CheckIssuable( NVMAddress address, OpType type );
    bool TryMigration( NVMainRequest *request, bool atomic );
    void ChooseVictim( MQMigrator *at, NVMAddress& promo, NVMAddress& victim );
	
	typedef struct
	{
		uint64_t pageNumber;	
		uint64_t referenceCounter;
		uint64_t expirationTime;
		uint64_t channelNumber;
	}PageType;
	
	uint64_t currentTime;

	std::list<PageType> *rankingQueues;
	std::list<uint64_t> DRAMPageList, demotedDRAMList;	
	std::map<uint64_t , uint64_t> page_access_times;

	void JoinQueue(PageType page);
	void Upgrade(PageType page);
	void Downgrade(PageType page);
	int LocateQueue(uint64_t pageNo);
	void AccessPage(int pos, uint64_t pageNo);
	void Display();
	void CheckQueue();
	bool IsInList(std::list<uint64_t> list, uint64_t pageNo);
	void RemoveFromList(std::list<uint64_t> list, uint64_t pageNo);
	uint64_t GetPageNumber(MQMigrator *at, NVMAddress address);
	uint64_t GetRealChannel(MQMigrator *at, NVMAddress address);
	std::ofstream file;
	std::ofstream origin_file;
};

};

#endif

