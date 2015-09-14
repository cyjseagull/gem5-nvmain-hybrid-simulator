/***********************************************

Modified by dliu, based on CoinMigrator.cpp.

***********************************************/ 


#include "Utils/MultiQueueMigrator/MultiQueueMigrator.h"
#include "Decoders/MQMigrator/MQMigrator.h"
#include "NVM/nvmain.h"
#include "src/SubArray.h"
#include "src/EventQueue.h"
#include "include/NVMHelpers.h"

#define QUEUE_NUM 8
//#define LIFE_TIME 5
#define LIFE_TIME 4
//#define THRESHOLD_QUEUE 3
#define THRESHOLD_QUEUE 4


using namespace NVM;

MultiQueueMigrator::MultiQueueMigrator( )
{
    /*
     *  We will eventually be injecting requests to perform migration, so we
     *  would like IssueCommand to be called on the original request first so
     *  that we do not unintentially fill up the transaction queue causing 
     *  the original request triggering migration to fail.
     */
    SetHookType( NVMHOOK_BOTHISSUE );

    promoRequest = NULL;
    demoRequest = NULL;
    promoBuffered = false;
    demoBuffered = false;

    migrationCount = 0;
    queueWaits = 0;
    bufferedReads = 0;

    queriedMemory = false;
    promotionChannelParams = NULL;
    currentPromotionPage = 0;

	currentTime = 0;
	rankingQueues = new std::list<PageType>[QUEUE_NUM];
	file.open("rank-based-page-migration-traces.txt");
	origin_file.open("no-migration-page-traces.txt");
	trace_interval = 1000;	//default trace interval
}


MultiQueueMigrator::~MultiQueueMigrator( )
{
	if( file)
	{
		file.close();
	}
	if(origin_file)
	{
		origin_file.close();
	}
}


void MultiQueueMigrator::Init( Config *config )
{

    /* Specifies with channel is the "fast" memory. */
    promotionChannel = 0;
    config->GetValueUL( "PromotionChannel", promotionChannel );
	std::cout<<"promotion channel is"<< promotionChannel<<std::endl;
    /* If we want to simulate additional latency serving buffered requests. */
    bufferReadLatency = 4;
    config->GetValueUL( "MigrationBufferReadLatency", bufferReadLatency );
	config->GetValueUL("CHANNELS" ,channel_num ); 
	if( config->KeyExists("TraceInterval"))
	{
		trace_interval = config->GetValue("TraceInterval");
	}
	/* 
     *  We migrate entire rows between banks, so the column count needs to
     *  match across all channels for valid results.
     */
    numCols = config->GetValue( "COLS" );
	file<< "access time statistics of every channel every "<<trace_interval<<" cycles"<<std::endl;
	origin_file<< "access time statistics of every channel every "<<trace_interval<<" cycles"<<std::endl;
	for( int i=0 ; i<channel_num ;i++)
	{
		file<<"channel_"<<i<<"  ";
		origin_file<<"channel_"<<i<<"  ";
	}
	file<<std::endl;
	origin_file<<std::endl;

    AddStat(migrationCount);
    AddStat(queueWaits);
    AddStat(bufferedReads);
}


bool MultiQueueMigrator::IssueAtomic( NVMainRequest *request )
{
    /* For atomic mode, we just swap the pages instantly. */
    return TryMigration( request, true );
}


bool MultiQueueMigrator::IssueCommand( NVMainRequest *request )
{
    /* 
     *  In cycle-accurate mode, we must read each page, buffer it, enqueue a
     *  write request, and wait for write completion.
     */
    return TryMigration( request, false );
}


bool MultiQueueMigrator::RequestComplete( NVMainRequest *request )
{
//	std::cout<<"\nMultiQueueMigrator   -----  RequestComplete: "<<std::hex
  //                     <<request->address.GetPhysicalAddress()<<"  FFFFFFFF\n"<<std::endl;
    if( NVMTypeMatches(NVMain) && GetCurrentHookType( ) == NVMHOOK_PREISSUE )
    {
	//	std::cout<<"\nRequestComplete: If?  ---  Yes! FFFFFFFF \n "<<std::endl;
        /* Ensure the Migrator translator is used. */
        MQMigrator *migratorTranslator = dynamic_cast<MQMigrator *>(parent->GetTrampoline( )->GetDecoder( ));
        assert( migratorTranslator != NULL );

        if( request->owner == parent->GetTrampoline( ) && request->tag == MIG_READ_TAG )
        {
            /* A migration read completed, update state. */
            migratorTranslator->SetMigrationState( request->address, MQ_MIGRATION_BUFFERED ); 

            /* If both requests are buffered, we can attempt to write. */
            bool bufferComplete = false;

            if( (request == promoRequest 
                 && migratorTranslator->IsBuffered( demotee ))
                || (request == demoRequest
                 && migratorTranslator->IsBuffered( promotee )) )
            {
                bufferComplete = true;
            }

            /* Make a new request to issue for write. Parent will delete current pointer. */
            if( request == promoRequest )
            {
                promoRequest = new NVMainRequest( );
                *promoRequest = *request;
            }
            else if( request == demoRequest )
            {
                demoRequest = new NVMainRequest( );
                *demoRequest = *request;
            }
            else
            {
                assert( false );
            }

            /* Swap the address and set type to write. */
            if( bufferComplete )
            {
                /* 
                 *  Note: once IssueCommand is called, this hook may receive
                 *  a different parent, but fail the NVMTypeMatch check. As a
                 *  result we need to save a pointer to the NVMain class we
                 *  are issuing requests to.
                 */
			//	std::cout<<"\nBoth_Buffer_Complete == true. FFFFFFFF\n"<<std::endl;
                NVMObject *savedParent = parent->GetTrampoline( );

                NVMAddress tempAddress = promoRequest->address;
                promoRequest->address = demoRequest->address;
                demoRequest->address = tempAddress;

                demoRequest->type = WRITE;
                promoRequest->type = WRITE;

                demoRequest->tag = MIG_WRITE_TAG;
                promoRequest->tag = MIG_WRITE_TAG;

                /* Try to issue these now, otherwise we can try later. */
                bool demoIssued, promoIssued;

                demoIssued = savedParent->GetChild( demoRequest )->IssueCommand( demoRequest );
                promoIssued = savedParent->GetChild( promoRequest )->IssueCommand( promoRequest );

                if( demoIssued )
                {
                    migratorTranslator->SetMigrationState( demoRequest->address, MQ_MIGRATION_WRITING );
                }
                if( promoIssued )
                {
				//	std::cout<<"\npromo_Mig_Write Issued: "<<std::hex
                //                   <<promoRequest->address.GetPhysicalAddress()<<"\n"<<std::endl;
                    migratorTranslator->SetMigrationState( promoRequest->address, MQ_MIGRATION_WRITING );
                }

                promoBuffered = !promoIssued;
                demoBuffered = !demoIssued;
            }
        }
        /* A write completed. */
        else if( request->owner == parent->GetTrampoline( ) && request->tag == MIG_WRITE_TAG )
        {
            // Note: request should be deleted by parent
            migratorTranslator->SetMigrationState( request->address, MQ_MIGRATION_DONE );

            if( request == promoRequest )
            {
			//	std::cout<<"\n######  promo_Mig_Done. FFFFFFFF\n"<<std::endl;
            }
            else if( request == demoRequest )
            {
			//	std::cout<<"\n######  demo_Mig_Done. FFFFFFFF\n"<<std::endl;
            }
            migrationCount++;
            
			if( (request == promoRequest 
                 && migratorTranslator->IsMigrated( demotee ))
                || (request == demoRequest
                 && migratorTranslator->IsMigrated( promotee )) )
            {
               // std::cout<<"\nBoth migrations are done.\n"<<std::endl;
				
            }
        }
        /* Some other request completed, see if we can ninja issue some migration writes that did not queue. */
        else if( promoBuffered || demoBuffered )
        {
            bool demoIssued, promoIssued;

            if( promoBuffered )
            {
                promoIssued = parent->GetTrampoline( )->GetChild( promoRequest )->IssueCommand( promoRequest );
                promoBuffered = !promoIssued;
            }

            if( demoBuffered )
            {
                demoIssued = parent->GetTrampoline( )->GetChild( demoRequest )->IssueCommand( demoRequest );
                demoBuffered = !demoIssued;
            }
        }
    }

    return true;
}


bool MultiQueueMigrator::CheckIssuable( NVMAddress address, OpType type )
{
    NVMainRequest request;

    request.address = address;
    request.type = type;

    return parent->GetTrampoline( )->GetChild( &request )->IsIssuable( &request );
}


bool MultiQueueMigrator::TryMigration( NVMainRequest *request, bool atomic )
{
//	std::cout<<"\nMultiQueueMigrator::TryMigration ---- addr: "<<std::hex<<request->address.GetPhysicalAddress()<<"\n"<<std::endl;
    bool rv = true;

    if( NVMTypeMatches(NVMain) )
    {
        /* Ensure the Migrator translator is used. */
        MQMigrator *migratorTranslator = dynamic_cast<MQMigrator *>(parent->GetTrampoline( )->GetDecoder( ));
        assert( migratorTranslator != NULL );

        /* Migrations in progress must be served from the buffers during migration. */
        if( GetCurrentHookType( ) == NVMHOOK_PREISSUE && migratorTranslator->IsBuffered( request->address ) )
        {
            /* Short circuit this request so it is not queued. */
            rv = false;

            /* Complete the request, adding some buffer read latency. */
            GetEventQueue( )->InsertEvent( EventResponse, parent->GetTrampoline( ), request,
                              GetEventQueue()->GetCurrentCycle()+bufferReadLatency );

            bufferedReads++;

            return rv;
        }

        /* Don't inject results before the original is issued to prevent deadlock */
        if( GetCurrentHookType( ) != NVMHOOK_POSTISSUE )
        {
            return rv;
        }
		
		currentTime++; 
		
		//***********************traces start
		uint64_t pageNo = migratorTranslator->GetAddressKey(request->address);
 		uint64_t channelNo = request->address.GetChannel();
		if (GetEventQueue()->GetCurrentCycle()%trace_interval==0 )
		{
			for( int i=0 ; i<channel_num ; i++)
			{
				file<<migratorTranslator->migrate_access_times[i]<<"   ";
				origin_file<< migratorTranslator->access_times[i]<<"   ";
			}
			file<<std::endl;
			origin_file<<std::endl;
		}
		//***********************traces end
	
		//this request is issued to access dram memory
		if( channelNo == promotionChannel && !IsInList(DRAMPageList, pageNo) )
		{
			DRAMPageList.push_back(pageNo);
		}       
		//get ranking queue num for the request
		int location = LocateQueue(pageNo);
	
		if( location == -1 )
		{
			PageType page = { pageNo, 1, currentTime + LIFE_TIME, channelNo };
			//insert the request to queue[0] (LRU)
 			JoinQueue(page);	
		}	
		else
		{
			AccessPage(location, pageNo);	
		}	

		Display();

        /* See if any migration is possible (i.e., no migration is in progress) */
        bool migrationPossible = false;

        if( !migratorTranslator->Migrating( ) 
            && !migratorTranslator->IsMigrated( request->address )
			&& channelNo != promotionChannel )
        {
                migrationPossible = true;
        }
		else if(migratorTranslator->Migrating( ))
		{	
		//	std::cout<<"\nPossible?  ------ No ------ 1\n"<<std::endl;
		}
		else if(migratorTranslator->IsMigrated( request->address ))
		{	
	//		std::cout<<"\nPossible?  ------ No ------ 2\n"<<std::endl;
		}
		else if(channelNo == promotionChannel)
		{	
	//		std::cout<<"\nPossible?  ------ No ------ 3\n"<<std::endl;
		}

        if( migrationPossible )
        {
	//		std::cout<<"\n Possible?   ----    Yes!\n"<<std::endl;
            assert( !demoBuffered && !promoBuffered );

            if( location == THRESHOLD_QUEUE && !migratorTranslator->IsMigrated( request->address ))
            {
	//			std::cout<<"\n Migrate?   ----    Yes!\n"<<std::endl;
                /* 
                 *  Note: once IssueCommand is called, this hook may receive
                 *  a different parent, but fail the NVMTypeMatch check. As a
                 *  result we need to save a pointer to the NVMain class we
                 *  are issuing requests to.
                 */
                NVMObject *savedParent = parent->GetTrampoline( );

                /* Discard the unused column address. */
                uint64_t row, bank, rank, channel, subarray;
                request->address.GetTranslatedAddress( &row, NULL, &bank, &rank, &channel, &subarray );
                uint64_t promoteeAddress = migratorTranslator->ReverseTranslate( row, 0, bank, rank, channel, subarray ); 

                promotee.SetPhysicalAddress( promoteeAddress );
                promotee.SetTranslatedAddress( row, 0, bank, rank, channel, subarray );

                /* Pick a victim to replace. */
                ChooseVictim( migratorTranslator, promotee, demotee );

                assert( migratorTranslator->IsMigrated( demotee ) == false );
                assert( migratorTranslator->IsMigrated( promotee ) == false );

                if( atomic )
                {
                    migratorTranslator->StartMigration( request->address, demotee );
                    migratorTranslator->SetMigrationState( promotee, MQ_MIGRATION_DONE );
                    migratorTranslator->SetMigrationState( demotee, MQ_MIGRATION_DONE );
                }
                /* Lastly, make sure we can queue the migration requests. */
                else if( CheckIssuable( promotee, READ ) &&
                         CheckIssuable( demotee, READ ) )
                {
                    migratorTranslator->StartMigration( request->address, demotee );

                    promoRequest = new NVMainRequest( ); 
                    demoRequest = new NVMainRequest( );

                    promoRequest->address = promotee;
                    promoRequest->type = READ;
                    promoRequest->tag = MIG_READ_TAG;
                    promoRequest->burstCount = numCols;

                    demoRequest->address = demotee;
                    demoRequest->type = READ;
                    demoRequest->tag = MIG_READ_TAG;
                    demoRequest->burstCount = numCols;

                    promoRequest->owner = savedParent;
                    demoRequest->owner = savedParent;
                    savedParent->IssueCommand( promoRequest );
                    savedParent->IssueCommand( demoRequest );
                }
                else
                {
                    queueWaits++;
                }
            }
        }

		CheckQueue();
    }

    return rv;
}


void MultiQueueMigrator::ChooseVictim( MQMigrator *at, NVMAddress& /*promotee*/, NVMAddress& victim )
{
    /*
     *  Since this is no method called after every module in the system is 
     *  initialized, we check here to see if we have queried the memory system
     *  about the information we need.
     */
    if( !queriedMemory )
    {
        /*
         *  Our naive replacement policy will simply circle through all the pages
         *  in the fast memory. In order to count the pages we need to count the
         *  number of rows in the fast memory channel. We do this by creating a
         *  dummy request which would route to the fast memory channel. From this
         *  we can grab it's config pointer and calculate the page count.
         */
        NVMainRequest queryRequest;

        queryRequest.address.SetTranslatedAddress( 0, 0, 0, 0, promotionChannel, 0 );
        queryRequest.address.SetPhysicalAddress( 0 );
        queryRequest.type = READ;
        queryRequest.owner = this;

        NVMObject *curObject = NULL;
        FindModuleChildType( &queryRequest, SubArray, curObject, parent->GetTrampoline( ) );

        SubArray *promotionChannelSubarray = NULL;
        promotionChannelSubarray = dynamic_cast<SubArray *>( curObject );

        assert( promotionChannelSubarray != NULL );
        Params *p = promotionChannelSubarray->GetParams( );
        promotionChannelParams = p;

        totalPromotionPages = p->RANKS * p->BANKS * p->ROWS;
        currentPromotionPage = 0;

        if( p->COLS != numCols )
        {
            std::cout << "Warning: Page size of fast and slow memory differs." << std::endl;
        }

        queriedMemory = true;
    }

    /*
     *  From the current promotion page, simply craft some translated address together
     *  as the victim address.
     */
    uint64_t victimRank, victimBank, victimRow, victimSubarray, subarrayCount;
    ncounter_t promoPage; 
	
	uint64_t victimAddress, pageNo;
	do
	{
		promoPage = currentPromotionPage;
	
    	victimRank = promoPage % promotionChannelParams->RANKS;
    	promoPage >>= NVM::mlog2( promotionChannelParams->RANKS );

    	victimBank = promoPage % promotionChannelParams->BANKS;
    	promoPage >>= NVM::mlog2( promotionChannelParams->BANKS );

    	subarrayCount = promotionChannelParams->ROWS / promotionChannelParams->MATHeight;
    	victimSubarray = promoPage % subarrayCount;
    	promoPage >>= NVM::mlog2( subarrayCount );

    	victimRow = promoPage;

    	victim.SetTranslatedAddress( victimRow, 0, victimBank, victimRank, promotionChannel, victimSubarray );
    	victimAddress = at->ReverseTranslate( victimRow, 0, victimBank, victimRank, promotionChannel, victimSubarray );
    	victim.SetPhysicalAddress( victimAddress );
		pageNo = at->GetAddressKey(victim);
	
		currentPromotionPage = (currentPromotionPage + 1) % totalPromotionPages;
	}while(IsInList(DRAMPageList, pageNo));
    
	//	std::cout<<"\nChooseVictim: "<<std::hex<<victimAddress<<"\n"<<std::endl;
}


void MultiQueueMigrator::Cycle( ncycle_t /*steps*/ )
{

}

void MultiQueueMigrator::JoinQueue(PageType page)
{
	rankingQueues[0].push_back(page); 
}

void MultiQueueMigrator::Upgrade(PageType page)
{
	
}

void MultiQueueMigrator::Downgrade(PageType page)
{
	
}

int MultiQueueMigrator::LocateQueue(uint64_t pageNo)
{
	std::list<PageType>::iterator iter;
	for(int pos = 0; pos < QUEUE_NUM; pos++)
	{
		for(iter = rankingQueues[pos].begin(); iter !=rankingQueues[pos].end(); iter++)
		{
			if((*iter).pageNumber == pageNo)
			{
				return pos;
			}		
		}
	}
	return -1;
}

void MultiQueueMigrator::AccessPage(int pos, uint64_t pageNo)
{
	std::list<PageType>::iterator iter;
	for(iter = rankingQueues[pos].begin(); iter != rankingQueues[pos].end(); iter++)
	{
		if((*iter).pageNumber == pageNo)
		{
			(*iter).referenceCounter += 1;
			(*iter).expirationTime = currentTime + LIFE_TIME;
		
			if((pos < QUEUE_NUM - 1) && (*iter).referenceCounter >= (2 << pos))
			{
				rankingQueues[pos+1].push_back(*iter);
			}
			else
			{
				rankingQueues[pos].push_back(*iter);
			}

			rankingQueues[pos].erase(iter);
			
			return;		
		}
	}

}

void MultiQueueMigrator::Display()
{
//	std::cout<<"\nDisplay:\nDRAMPageList:\n"<<std::endl;
	
	std::list<uint64_t>::iterator it;
//	for(it = DRAMPageList.begin(); it != DRAMPageList.end(); it++)
//	{
//		std::cout<<std::hex<<*it<<"\t";
//	}
	
	std::list<PageType>::iterator iter;
	for(int pos = 0; pos < QUEUE_NUM; pos++)
	{
//		std::cout<<"\n************  "<<std::dec<<pos<<"  ***********\n"<<std::endl;
		for(iter = rankingQueues[pos].begin(); iter != rankingQueues[pos].end(); iter++)
		{
//			std::cout<<std::hex<<(*iter).pageNumber<<"\t"<<std::dec<<(*iter).referenceCounter<<"\t"<<
//						std::dec<<(*iter).expirationTime<<std::endl;
		}
	}
}

void MultiQueueMigrator::CheckQueue()
{
	std::list<PageType>::iterator iter;
	for(int pos = 0; pos < QUEUE_NUM; pos++)
	{
		iter = rankingQueues[pos].begin();
		if(iter != rankingQueues[pos].end() && currentTime > (*iter).expirationTime)
		{
			//Exist and expirate.
			if((*iter).channelNumber == promotionChannel)
			{
				//DRAM page.
				bool in = IsInList(demotedDRAMList, (*iter).pageNumber);
				if(in)
				{
					rankingQueues[pos].pop_front();
					RemoveFromList(demotedDRAMList, (*iter).pageNumber);
				}
				else if(pos > 0)
				{
					(*iter).expirationTime = currentTime + LIFE_TIME;
					rankingQueues[pos - 1].push_back(*iter);
					demotedDRAMList.push_back((*iter).pageNumber);
					rankingQueues[pos].pop_front();
				}
				else
				{
					rankingQueues[pos].pop_front();
				}
			}
			else
			{
				//PCM page.
				if(pos > 0)
				{
					(*iter).expirationTime = currentTime + LIFE_TIME;
					rankingQueues[pos - 1].push_back(*iter);
					rankingQueues[pos].pop_front();
				}	
				else
				{
					rankingQueues[pos].pop_front();
				}
			}
		}	
	}
}

bool MultiQueueMigrator::IsInList(std::list<uint64_t> list, uint64_t pageNo)
{
	std::list<uint64_t>::iterator iter;
	for(iter = list.begin(); iter != list.end(); iter++)
	{
		if(pageNo == *iter)
		{
			return true;
		}
	}
	
	return false;
}

void MultiQueueMigrator::RemoveFromList(std::list<uint64_t> list, uint64_t pageNo)
{
	std::list<uint64_t>::iterator iter;
	for(iter = list.begin(); iter != list.end(); iter++)
	{
		if(pageNo == *iter)
		{
			list.erase(iter);
			return;
		}
	}	
}

uint64_t MultiQueueMigrator::GetPageNumber( MQMigrator *at, NVMAddress address )
{
	uint64_t pageNum = at->GetAddressKey(address);

	if(at->IsMigrated(address))
	{
		uint64_t channel;
		address.GetTranslatedAddress(NULL, NULL, NULL, NULL, &channel, NULL);
		
		uint64_t newChannel = at->GetNewChannel(address);
		pageNum = pageNum - channel + newChannel;
	}
	
	return pageNum;
}

uint64_t MultiQueueMigrator::GetRealChannel( MQMigrator *at, NVMAddress address )
{
	if(at->IsMigrated(address))
	{
		return at->GetNewChannel(address);		
	}
	
	return address.GetChannel();
}


