/*******************************************

Modified by dliu, based on Migrator.cpp

*******************************************/

#include "Decoders/MQMigrator/MQMigrator.h"

#include <iostream>
#include <sstream>
#include <cassert>
//#include "src/NVMObject.h"
//#include "src/EventQueue.h"
using namespace NVM;

MQMigrator::MQMigrator( )
{
    migrating = false;
    inputPage = 0;
    outputPage = 0;
    
    migratedAccesses = 0;

    migrationMap.clear( );
    migrationState.clear( );
	//file.open("MQ-migration-traces.txt");
}


MQMigrator::~MQMigrator( )
{
	//file.close();
}


void MQMigrator::SetConfig( Config *config, bool /*createChildren*/ )
{
    /* 
     *  Each memory page will be given a one-dimensional key, so we need the
     *  size of the other dimensions to calculate this. Using GetValue is 
     *  slow during simulation, so we cache them to private members here.
     */
    numChannels = config->GetValue( "CHANNELS" );
    numBanks = config->GetValue( "BANKS" );
    numRanks = config->GetValue( "RANKS" );
    numSubarrays = config->GetValue( "ROWS" ) / config->GetValue( "MATHeight" );
}


void MQMigrator::RegisterStats( )
{
    AddStat(migratedAccesses);
}


/*
 *  Calculates a unique key for each possible unit of memory that can be
 *  migrated. In this case, we are migrating single rows of a bank.
 */
uint64_t MQMigrator::GetAddressKey( NVMAddress& address )
{
    uint64_t row, bank, rank, subarray, channel;
    address.GetTranslatedAddress( &row, NULL, &bank, &rank, &channel, &subarray );

    /* 
     * We will migrate entire memory pages, therefore only the column is
     * irrelevant.
     */
    return (row * numBanks * numRanks * numSubarrays * numChannels 
            + bank * numRanks * numSubarrays * numChannels
            + rank * numSubarrays * numChannels
            + subarray * numChannels
            + channel);
}


void MQMigrator::StartMigration( NVMAddress& promotee, NVMAddress& demotee )
{
    /* 
     *  The address being demoted is assumed to be in the "fast" memory and
     *  the address being promoted in the slow memory, therefore we define
     *  the promotion channel as the demotion address' value and similarly
     *  for demotion channel.
     */
    uint64_t demoChannel, promoChannel;
    demoChannel = promotee.GetChannel( );
    promoChannel = demotee.GetChannel( );

    /* Get unique keys for each page to migrate. */
    uint64_t promokey = GetAddressKey( promotee );
    uint64_t demokey = GetAddressKey( demotee );

    /* Ensure we are not already migrating a page. */
    assert( migrating == false );

    /*
     *  Set the new channel decodings immediately, but mark the migration
     *  as being in progress.
     */
    migrationMap[promokey] = promoChannel;
    migrationMap[demokey] = demoChannel;
    migrationState[promokey] = MQ_MIGRATION_READING;
    migrationState[demokey] = MQ_MIGRATION_READING;

    /*
     *  Only one migration is allowed at a time; These values hold the
     *  key values for each migration page and marks a migration in
     *  progress.
     */
    migrating = true;
    inputPage = promokey;
    outputPage = demokey;
}

void MQMigrator::SetMigrationState( NVMAddress& address, MQMigratorState newState )
{
	
    /* Get the key and set the new state; Ensure the state is really new. */
    uint64_t key = GetAddressKey( address );

    assert( migrationState.count( key ) != 0 );
    assert( migrationState[key] != newState );

    migrationState[key] = newState;

    /* If migration is done we can handle another migration */
    if( migrationState[inputPage] == MQ_MIGRATION_DONE &&
        migrationState[outputPage] == MQ_MIGRATION_DONE )
    {
        migrating = false;
    }
}


bool MQMigrator::Migrating( )
{
    return migrating;
}


/*
 *  If the page was migrated, we should read from the new channel it which
 *  it was placed, since the buffer may no longer be valid.
 */
bool MQMigrator::IsMigrated( NVMAddress& address )
{
    uint64_t key = GetAddressKey( address );
    bool rv = false;

    if( migrationMap.count( key ) != 0 && migrationState.count( key ) != 0
        && migrationState[key] == MQ_MIGRATION_DONE )
    {
        rv = true;
    }

    return rv;
}


/*
 *  If a request has been read from one channel and placed in our temporary
 *  swap buffer, the data in the bank may no longer be valid. Therefore, we
 *  need to read the data from the temporary swap buffer instead.
 */
bool MQMigrator::IsBuffered( NVMAddress& address )
{
    uint64_t key = GetAddressKey( address );
    bool rv = false;

    if( migrationMap.count( key ) != 0 && migrationState.count( key ) != 0
        && (migrationState[key] == MQ_MIGRATION_BUFFERED || migrationState[key] == MQ_MIGRATION_WRITING ) )
    {
        rv = true;
    }

    return rv;
}


void MQMigrator::Translate( uint64_t address, uint64_t *row, uint64_t *col, uint64_t *bank,
                          uint64_t *rank, uint64_t *channel, uint64_t *subarray )
{
    /* Use the default -- We will only change the channel if needed. */
    AddressTranslator::Translate( address, row, col, bank, rank, channel, subarray );
	/**
    std::cout << "\nMQMigrator::Translate address " << std::hex << address << " To:\n" \
                                       "old channel " << *channel << "\n" \
                                       "rank " << *channel << "\n" \
                                       "bank " << *channel << "\n" \
                                       "row " << *channel << "\n" \
                                       "col " << *channel << "\n" \
                                       "subarray " << *channel << "\n" \
                                                   << std::endl; 
   	*/ 

	/* This should be a unique key for this address. */
    NVMAddress keyAddress;
    keyAddress.SetTranslatedAddress( *row, *col, *bank, *rank, *channel, *subarray );
    keyAddress.SetPhysicalAddress( address );
    uint64_t key = GetAddressKey( keyAddress );
	ncounter_t r_channel = *channel;
	if( access_times.count( r_channel)==0)
	{
		access_times[ r_channel]= 1;
	}   
	else
		access_times[r_channel]++;

    /* Check if the page was migrated and migration is complete. */
    if( migrationMap.count( key ) != 0 )
    {
        if( migrationState[key] == MQ_MIGRATION_DONE )
        {
            *channel = migrationMap[key];
  			
//			std::cout << "\n------new channel " << std::hex << *channel <<std::endl;

            migratedAccesses++;
        }
    }
	ncounter_t m_channel = *channel;
	if( migrate_access_times.count( m_channel)==0)
	{
		migrate_access_times[ m_channel]= 1;
	}   
	else
		migrate_access_times[m_channel]++;
}


void MQMigrator::CreateCheckpoint( std::string dir )
{
    std::stringstream cpt_file;
    cpt_file.str("");
    cpt_file << dir << "/" << StatName( );

    std::ofstream cpt_handle;

    cpt_handle.open( cpt_file.str().c_str(), std::ofstream::out | std::ofstream::trunc | std::ofstream::binary );

    if( !cpt_handle.is_open() )
    {
        std::cout << StatName( ) << ": Warning: Could not open checkpoint file: "
                  << cpt_file.str() << std::endl;
    }
    else
    {
        /* 
         *  In-flight requests are not checkpointed (i.e., migrations). 
         *  Therefore, we assume requests have completed (i.e., there is some 
         *  draining process) and only checkpoint addresses and not state.
         */
        std::map<uint64_t, uint64_t>::iterator it;
        for( it = migrationMap.begin(); it != migrationMap.end(); it++ )
        {
            cpt_handle.write( (const char*)&(it->first), sizeof(uint64_t) );
            cpt_handle.write( (const char*)&(it->second), sizeof(uint64_t) );
        }

        cpt_handle.close( );

        /* Write checkpoint information. */
        /* Note: For future compatability only at the memory. This is not read during restoration. */
        std::string cpt_info = cpt_file.str() + ".json";

        cpt_handle.open( cpt_info.c_str(), std::ofstream::out | std::ofstream::trunc | std::ofstream::binary );

        if( !cpt_handle.is_open() )
        {
            std::cout << StatName( ) << ": Warning: Could not open checkpoint " 
                      << "info file: " << cpt_info << std::endl;
        }
        else
        {
            std::string cpt_info_str = "{\n\t\"Version\": 1\n}";
            cpt_handle.write( cpt_info_str.c_str(), cpt_info_str.length() ); 

            cpt_handle.close();
        }
    }
}


void MQMigrator::RestoreCheckpoint( std::string dir )
{
    std::stringstream cpt_file;
    cpt_file.str("");
    cpt_file << dir << "/" << StatName( );

    std::ifstream cpt_handle;

    cpt_handle.open( cpt_file.str().c_str(), std::ifstream::ate | std::ofstream::binary );

    if( !cpt_handle.is_open() )
    {
        std::cout << StatName( ) << ": Warning: Could not open checkpoint file: "
                  << cpt_file.str() << std::endl;
    }
    else
    {
        std::streampos cpt_size = cpt_handle.tellg( );

        if( cpt_size % (sizeof(uint64_t)*2) != 0 )
        {
            std::cout << StatName( ) << ": Warning: Excepted checkpoint size to be "
                      << "a multiple of " << (sizeof(uint64_t)*2) << std::endl;
        }

        cpt_handle.close( );

        cpt_handle.open( cpt_file.str().c_str(), std::ifstream::in | std::ifstream::binary );

        /*
         *  Checkpoint is assumed to only have addresses and channel mappings.
         */
        uint64_t addressMappings = static_cast<uint64_t>(cpt_size / (sizeof(uint64_t)*2));
        for( uint64_t mapping = 0; mapping < addressMappings; mapping++ )
        {
            uint64_t address, channel;

            cpt_handle.read( (char*)(&address), sizeof(uint64_t) );
            cpt_handle.read( (char*)(&channel), sizeof(uint64_t) );

            std::pair<uint64_t, uint64_t> migration( address, channel );
            migrationMap.insert( migration );
        }

        cpt_handle.close( );
    }
}

uint64_t MQMigrator::GetNewChannel( NVMAddress& address )
{
	uint64_t key = GetAddressKey(address);
	return migrationMap[key];
}

