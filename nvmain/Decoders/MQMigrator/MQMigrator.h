/****************************************************

Modified by dliu, based on Migrator.h

****************************************************/

#ifndef __MULTIQUEUE_MIGRATOR_H__
#define __MULTIQUEUE_MIGRATOR_H__

#include "src/AddressTranslator.h"
#include "src/Config.h"
#include "include/NVMAddress.h"
#include "src/NVMObject.h"
#include "src/EventQueue.h"
namespace NVM
{


enum MQMigratorState
{      
    MQ_MIGRATION_UNKNOWN = 0, // Error state
    MQ_MIGRATION_READING,     // Read in progress for this page
    MQ_MIGRATION_BUFFERED,    // Read is done, waiting for writes to be queued
    MQ_MIGRATION_WRITING,     // Writes queued, waiting for request complete
    MQ_MIGRATION_DONE         // Migration successfully completed
};


class MQMigrator : public AddressTranslator
{
  public:
    MQMigrator();
    ~MQMigrator();

    void SetConfig( Config *config, bool createChildren = true );

    virtual void Translate( uint64_t address, uint64_t *row, uint64_t *col, uint64_t *bank, 
                            uint64_t *rank, uint64_t *channel, uint64_t *subarray );
    using AddressTranslator::Translate;

    void StartMigration( NVMAddress& promotee, NVMAddress& demotee );
    void SetMigrationState( NVMAddress& address, MQMigratorState newState );
    bool Migrating( );
    bool IsBuffered( NVMAddress& address );
    bool IsMigrated( NVMAddress& address );

    void RegisterStats( );

    void CreateCheckpoint( std::string dir );
    void RestoreCheckpoint( std::string dir );

    uint64_t GetAddressKey( NVMAddress& address );
	uint64_t GetNewChannel( NVMAddress& address );
  
	std::map<uint64_t , uint64_t> access_times;
	std::map<uint64_t , uint64_t> migrate_access_times;
  private:
    std::map<uint64_t, uint64_t> migrationMap;
    std::map<uint64_t, MQMigratorState> migrationState;

    uint64_t numChannels, numBanks, numRanks, numSubarrays;

    /* Pages being swapped in and swapped out. */
    bool migrating;
    uint64_t inputPage, outputPage;

    ncounter_t migratedAccesses;
	//std::ofstream file;

};


};


#endif

