#include "replacement_state.h"

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

/*
** This file implements the cache replacement state. Users can enhance the code
** below to develop their cache replacement ideas.
**
*/


////////////////////////////////////////////////////////////////////////////////
// The replacement state constructor:                                         //
// Inputs: number of sets, associativity, and replacement policy to use       //
// Outputs: None                                                              //
//                                                                            //
// DO NOT CHANGE THE CONSTRUCTOR PROTOTYPE                                    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
CACHE_REPLACEMENT_STATE::CACHE_REPLACEMENT_STATE( UINT32 _sets, UINT32 _assoc, UINT32 _pol )
{

    numsets    = _sets;
    assoc      = _assoc;
    replPolicy = _pol;

    mytimer    = 0;
    
    prob.access = 0;
    prob.miss = 0;
    currPolicy = CLOCK;
    score.lru = 0;
    score.clock = 1;

    InitReplacementState();
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function initializes the replacement policy hardware by creating      //
// storage for the replacement state on a per-line/per-cache basis.           //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
void CACHE_REPLACEMENT_STATE::InitReplacementState()
{
    // Create the state for sets, then create the state for the ways
    repl  = new LINE_REPLACEMENT_STATE* [ numsets ];

    // ensure that we were able to create replacement state
    assert(repl);

    // Create the state for the sets
    for(UINT32 setIndex=0; setIndex<numsets; setIndex++) 
    {
        repl[ setIndex ]  = new LINE_REPLACEMENT_STATE[ assoc ];

        for(UINT32 line=0; line<assoc; line++) 
        {
            // initialize stack position (for true LRU)
            repl[ setIndex ][ line ].LRUage = line;
        }
    }

    //MRU+LRU
    //when we have more than say 30% miss rate, we switch policy 
    // Contestants:  ADD INITIALIZATION FOR YOUR HARDWARE HERE
    myRepl = new LINE_REPLACEMENT_STATE* [ numsets ];
    hand = new UINT8 [numsets];
    // ensure that we were able to create replacement state
    assert(myRepl);
    
    // Create the state for the sets
    for(UINT32 setIndex=0; setIndex<numsets; setIndex++) 
    {
        myRepl[ setIndex ]  = new LINE_REPLACEMENT_STATE[ assoc ];
        hand[ setIndex ] = 0; //initially, hand is pointing at first line in each set
        
        for(UINT32 lineIndx=0; lineIndx<assoc; lineIndx++) 
        {
            // initialize stack position (for MRU)
            // 0 being MRU, ASSOC-1 being LRU
            myRepl[ setIndex ][ lineIndx ].cacheLineAge = lineIndx;
            myRepl[ setIndex ][ lineIndx ].used = false;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function is called by the cache on every cache miss. The input        //
// arguments are the thread id, set index, pointers to ways in current set    //
// and the associativity.  We are also providing the PC, physical address,    //
// and accesstype should you wish to use them at victim selection time.       //
// The return value is the physical way index for the line being replaced.    //
// Return -1 if you wish to bypass LLC.                                       //
//                                                                            //
// vicSet is the current set. You can access the contents of the set by       //
// indexing using the wayID which ranges from 0 to assoc-1 e.g. vicSet[0]     //
// is the first way and vicSet[4] is the 4th physical way of the cache.       //
// Elements of LINE_STATE are defined in crc_cache_defs.h                     //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
INT32 CACHE_REPLACEMENT_STATE::GetVictimInSet( UINT32 tid, UINT32 setIndex, const LINE_STATE *vicSet, UINT32 assoc,
                                               Addr_t PC, Addr_t paddr, UINT32 accessType )
{
    // If no invalid lines, then replace based on replacement policy
    if( replPolicy == CRC_REPL_LRU ) 
    {
        return Get_LRU_Victim( setIndex );
    }
    else if( replPolicy == CRC_REPL_RANDOM )
    {
        return Get_Random_Victim( setIndex );
    }
    else if( replPolicy == CRC_REPL_CONTESTANT )
    {
        // Contestants:  ADD YOUR VICTIM SELECTION FUNCTION HERE
        return Get_SWITCH_Victim( setIndex );
    }

    // We should never get here
    assert(0);

    return -1; // Returning -1 bypasses the LLC
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function is called by the cache after every cache hit/miss            //
// The arguments are: the set index, the physical way of the cache,           //
// the pointer to the physical line (should contestants need access           //
// to information of the line filled or hit upon), the thread id              //
// of the request, the PC of the request, the accesstype, and finall          //
// whether the line was a cachehit or not (cacheHit=true implies hit)         //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
void CACHE_REPLACEMENT_STATE::UpdateReplacementState( 
    UINT32 setIndex, INT32 updateWayID, const LINE_STATE *currLine, 
    UINT32 tid, Addr_t PC, UINT32 accessType, bool cacheHit )
{
    // What replacement policy?
    if( replPolicy == CRC_REPL_LRU ) 
    {
        UpdateLRU( setIndex, updateWayID );
    }
    else if( replPolicy == CRC_REPL_RANDOM )
    {
        // Random replacement requires no replacement state update
    }
    else if( replPolicy == CRC_REPL_CONTESTANT )
    {
        // Contestants:  ADD YOUR UPDATE REPLACEMENT STATE FUNCTION HERE
        // Feel free to use any of the input parameters to make
        // updates to your replacement policy
        UpdateSWITCH( setIndex, updateWayID, cacheHit );
    }
    
    
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//////// HELPER FUNCTIONS FOR REPLACEMENT UPDATE AND VICTIM SELECTION //////////
//                                                                            //
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function finds the LRU victim in the cache set by returning the       //
// cache block at the bottom of the LRU stack. Top of LRU stack is '0'        //
// while bottom of LRU stack is 'assoc-1'                                     //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
INT32 CACHE_REPLACEMENT_STATE::Get_LRU_Victim( UINT32 setIndex )
{
    // Get pointer to replacement state of current set
    LINE_REPLACEMENT_STATE *replSet = repl[ setIndex ];

    INT32   lruWay   = 0;

    // Search for victim whose stack position is assoc-1
    for(UINT32 way=0; way<assoc; way++) 
    {
        if( replSet[way].LRUage == (assoc-1) ) 
        {
            lruWay = way;
            break;
        }
    }

    // return lru way
    return lruWay;
}

void CACHE_REPLACEMENT_STATE::Get_MyLRU_Victim(UINT32 setIndex,INT32 &line)
{
    // Get pointer to replacement state of current set
    LINE_REPLACEMENT_STATE *replSet = myRepl[ setIndex ];

    for(UINT32 lineIndx=0; lineIndx<assoc; lineIndx++) 
    {
        if( replSet[lineIndx].cacheLineAge == (assoc-1) ) 
        {
            line = lineIndx;
            break;
        }
    }
}


void CACHE_REPLACEMENT_STATE::Get_MyCLOCK_Victim(UINT32 setIndex,INT32 &line)
{
    
    LINE_REPLACEMENT_STATE *replSet = myRepl[ setIndex ];
    
    for(;;){
       //if we find an unused cache line, this is the victim
       if(!(replSet[ hand[ setIndex ] ].used) ){
           line = hand[ setIndex ];
           break;
       }else{
           //if the current cache line the hand is pointing to is used, we reset
           //then the hand points to next cache line 
           replSet[ hand[ setIndex ] ].used = false;
  
           hand[ setIndex ]++;//hand pointing to next cacheline unused

           //We have fixed 16-way assoc
           //so the hand circles back at the last one
           if(hand[ setIndex ] > 0xf){
               hand[ setIndex ] = 0;
           }
       }
    }
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function finds a random victim in the cache set                       //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
INT32 CACHE_REPLACEMENT_STATE::Get_Random_Victim( UINT32 setIndex )
{
    INT32 way = (rand() % assoc);
    
    return way;
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function finds the SWITCH victim in the cache set by returning the       //
// cache block at the top  of the LRU stack. Top of LRU stack is '0'          //
// while bottom of LRU stack is 'assoc-1'                                     //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
INT32 CACHE_REPLACEMENT_STATE::Get_SWITCH_Victim( UINT32 setIndex )
{
    INT32   line=0;

    //default policy is MRU, and default to not switch to CLOCK
    if(currPolicy == LRU){
        if(score.lru >= score.clock){
            Get_MyLRU_Victim(setIndex,line);
        }else{
            Get_MyCLOCK_Victim(setIndex,line);
        }        
    }else if(currPolicy == CLOCK){// Famous CLOCK policy
        if(score.clock >= score.lru){
            Get_MyCLOCK_Victim(setIndex,line);
        }else{
            Get_MyLRU_Victim(setIndex,line);
        }
    }

    // return mru line
    return line;
}
////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function implements the LRU update routine for the traditional        //
// LRU replacement policy. The arguments to the function are the physical     //
// way and set index.                                                         //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
void CACHE_REPLACEMENT_STATE::UpdateLRU( UINT32 setIndex, INT32 updateWayID )
{
    // Determine current LRU stack position
    UINT32 currLRUage = repl[ setIndex ][ updateWayID ].LRUage;

    // Update the stack position of all lines before the current line
    // Update implies incremeting their stack positions by one
    for(UINT32 way=0; way<assoc; way++) 
    {
        if( repl[setIndex][way].LRUage < currLRUage ) 
        {
            repl[setIndex][way].LRUage++;
        }
    }

    // Set the LRU stack position of new line to be zero
    repl[ setIndex ][ updateWayID ].LRUage = 0;
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function implements the mru update routine for the traditional        //
// mru replacement policy. The arguments to the function are the physical     //
// way and set index.                                                         //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
void CACHE_REPLACEMENT_STATE::UpdateSWITCH( UINT32 setIndex, INT32 updateWayID, bool cacheHit )
{
    // Determine current MRU stack position
    UINT32 currcacheLineAge = myRepl[ setIndex ][ updateWayID ].cacheLineAge;
    
    probMissRate(cacheHit);
    
    if(currPolicy == LRU){
        // Update the stack position of all lines before the current line
        // Update implies incremeting their stack positions by one
        for(UINT32 lineIndx=0; lineIndx<assoc; lineIndx++) 
        {
            if( myRepl[setIndex][lineIndx].cacheLineAge < currcacheLineAge ) 
            {
                myRepl[setIndex][lineIndx].cacheLineAge++;
            }
        }

        // Set the MRU stack position of new line to be zero
        myRepl[ setIndex ][ updateWayID ].cacheLineAge = 0;
    }else{
        if(cacheHit){ 
            myRepl[ setIndex ][ updateWayID ].used = true;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This functon switchs the policy when miss rate >30%                        //
// return true to switch policy, falsue to not switch                         //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
void CACHE_REPLACEMENT_STATE::probMissRate(bool cacheHit){
    
    //count the total access 
    prob.access++;

    //count the miss
    if(!cacheHit){
       prob.miss++;
    }
  
    //check if we want to switch on every SWITCH_MARGIN access 
    //we switch if the miss rate is more than SWITCH_MARGIN*10 %
    
    if(prob.access >= SWITCH_MARGIN){ 
        if((prob.miss * SWITCH_THRES) > prob.access){
            if(currPolicy == LRU){
                currPolicy = CLOCK;
            }else if(currPolicy == CLOCK){
                currPolicy = LRU;
            }
        }else{
            if(currPolicy == LRU){
                score.lru++;
            }else if(currPolicy == CLOCK){
                score.clock++;
            }
        }
        
        //reset prob for next cycle (next SWITCH_MARGIN access)
        prob.access = 0;
        prob.miss = 0;

        if((score.lru+score.clock)>SCORE_CHECK_ROUND){
            score.lru = 0;
            score.clock = 1;
        }
    }
}
////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// The function prints the statistics for the cache                           //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
ostream & CACHE_REPLACEMENT_STATE::PrintStats(ostream &out)
{

    out<<"=========================================================="<<endl;
    out<<"=========== Replacement Policy Statistics ================"<<endl;
    out<<"=========================================================="<<endl;

    // CONTESTANTS:  Insert your statistics printing here
    if(replPolicy == CRC_REPL_LRU){
        out<<"LRU"<<endl;
    }else if(replPolicy == CRC_REPL_RANDOM){
        out<<"RANDOM"<<endl;
    }else{
        out<<"leon"<<endl;
    }
    return out;
    
}

