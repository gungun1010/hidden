#include "predictor.h"


#define PHT_CTR_MAX  0x11
#define PHT_CTR_INIT 0x10

#define PC_RESERVE_BITS   17

/////////////// STORAGE BUDGET JUSTIFICATION ////////////////
// Total storage budget: 32KB + 17 bits
// Total PHT (pattern history table) entries: 2^17 
// Total PHT size = 2^17 * 2 bits/counter = 2^18 bits = 32KB
// GHR size: 17 bits
// Total Size = PHT size + GHR size
/////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

PREDICTOR::PREDICTOR(void){

  pcReserveBits    = PC_RESERVE_BITS;//how long do we keep the table
  gbh              = 0;//global history register
  numPhtEntries    = (1<< pcReserveBits);//left shift 1 for 17 bits, this is equal to 2^17

  //allocate total of 2^17 UINT32 of memory, takes 17 bits from PC
  pht = new UINT32[numPhtEntries];
  for(UINT32 ii=0; ii< numPhtEntries; ii++){
    //init to be 0x10, TAKEN
    pht[ii]=PHT_CTR_INIT; 
  }
  
}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

bool   PREDICTOR::GetPrediction(UINT32 PC){

  //PC^gbh PC xor global branch history, is because of the GShare semantic
  //% numPhtEntries (2^17), we are taking 17 bits of the PC as our entry
  UINT32 phtIndex   = (PC^gbh) % (numPhtEntries);
  UINT32 phtCounter = pht[phtIndex];
  
  if(phtCounter > PHT_CTR_MAX/2){
    return TAKEN; 
  }else{
    return NOT_TAKEN; 
  }
  
}


/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

void  PREDICTOR::UpdatePredictor(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget){

  UINT32 phtIndex   = (PC^gbh) % (numPhtEntries);
  UINT32 phtCounter = pht[phtIndex];

  // update the PHT, based on actual outcome, resolveDir
  // using saturation counter
  if(resolveDir == TAKEN){
    pht[phtIndex] = SatIncrement(phtCounter, PHT_CTR_MAX);
  }else{
    pht[phtIndex] = SatDecrement(phtCounter);
  }

  // update the gbh
  // gbh stores last 32 BP result (resolveDir), though we only use 17 bits of it
  gbh = (gbh << 1);

  if(resolveDir == TAKEN){
    gbh++; 
  }

}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

void    PREDICTOR::TrackOtherInst(UINT32 PC, OpType opType, UINT32 branchTarget){

  // This function is called for instructions which are not
  // conditional branches, just in case someone decides to design
  // a predictor that uses information from such instructions.
  // We expect most contestants to leave this function untouched.

  return;
}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////
