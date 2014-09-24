#ifndef REPL_STATE_H
#define REPL_STATE_H

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This file is distributed as part of the Cache Replacement Championship     //
// workshop held in conjunction with ISCA'2010.                               //
//                                                                            //
//                                                                            //
// Everyone is granted permission to copy, modify, and/or re-distribute       //
// this software.                                                             //
//                                                                            //
// Please contact Aamer Jaleel <ajaleel@gmail.com> should you have any        //
// questions                                                                  //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include <cstdlib>
#include <cassert>
#include "utils.h"
#include "crc_cache_defs.h"

#define SWITCH_THRES 3
#define SWITCH_MARGIN 100
#define SCORE_CHECK_ROUND 5
// Replacement Policies Supported
typedef enum 
{
    CRC_REPL_LRU        = 0,
    CRC_REPL_RANDOM     = 1,
    CRC_REPL_CONTESTANT = 2
} ReplacemntPolicy;

//switchable policy supported
typedef enum
{
    LRU = 0,
    CLOCK = 1
}SWITCHABLE_POLICY;

// Replacement State Per Cache Line
typedef struct
{
    UINT32  LRUage;

    // CONTESTANTS: Add extra state per cache line here
    UINT32  cacheLineAge;
    bool    used;
} LINE_REPLACEMENT_STATE;

//set miss threshold
typedef struct
{
    UINT32 access;
    UINT32 miss;
} MISS_PROPOTION;

//scoreboard for each policy
typedef struct
{
    COUNTER lru;
    COUNTER clock;
}SCORE_BOARD;
// The implementation for the cache replacement policy
class CACHE_REPLACEMENT_STATE
{

  private:
    UINT32 numsets;
    UINT32 assoc;
    UINT32 replPolicy;
    UINT32 segBoundary; //boundary for half of the cache in SLRU
     
    LINE_REPLACEMENT_STATE   **repl;

    COUNTER mytimer;  // tracks # of references to the cache

    // CONTESTANTS:  Add extra state for cache here
    LINE_REPLACEMENT_STATE  **myRepl;
    MISS_PROPOTION  prob;
    SCORE_BOARD     score;
    SWITCHABLE_POLICY    currPolicy;
    UINT8   *hand;
  public:

    // The constructor CAN NOT be changed
    CACHE_REPLACEMENT_STATE( UINT32 _sets, UINT32 _assoc, UINT32 _pol );

    INT32  GetVictimInSet( UINT32 tid, UINT32 setIndex, const LINE_STATE *vicSet, UINT32 assoc, Addr_t PC, Addr_t paddr, UINT32 accessType );
    void   UpdateReplacementState( UINT32 setIndex, INT32 updateWayID );

    void   SetReplacementPolicy( UINT32 _pol ) { replPolicy = _pol; } 
    
    void   IncrementTimer() { mytimer++; } 

    void   UpdateReplacementState( UINT32 setIndex, INT32 updateWayID, const LINE_STATE *currLine, 
                                   UINT32 tid, Addr_t PC, UINT32 accessType, bool cacheHit );

    ostream&   PrintStats( ostream &out);

  private:
    
    void   InitReplacementState();
    INT32  Get_Random_Victim( UINT32 setIndex );

    INT32  Get_LRU_Victim( UINT32 setIndex );
    INT32   Get_SWITCH_Victim( UINT32 setIndex );
    void    Get_MyLRU_Victim(UINT32 setIndex, INT32 &line);
    void    Get_MyCLOCK_Victim(UINT32 setIndex, INT32 &line);
    void   UpdateLRU( UINT32 setIndex, INT32 updateWayID );
    void    UpdateSWITCH( UINT32 setIndex, INT32 updateWayID, bool cacheHit);
    void    probMissRate(bool cacheHit);
};


#endif
