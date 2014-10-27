#include "predictor.h"


#define PHT_CTR_MAX  3
#define PHT_CTR_INIT 0

#define PC_RESERVE_BITS   17
#define CORRELATION_BITS  1
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
  corBits          = CORRELATION_BITS;
  gbh              = 0;//global branch history
  numPhtEntries    = (1<< pcReserveBits);//left shift 1 for 17 bits, this is equal to 2^17
  numCor           = (1<< corBits);

  //allocate total of 2^17 UINT32 of memory, takes 17 bits from PC
  pht = new UINT32* [numCor];
  
  //allocate memory for talbe selector shift register
  tableSelSR = new bool[corBits];
  
  for(UINT32 i=0; i<corBits; i++){
      tableSelSR[i] = TAKEN;
  }
  
  for(UINT32 ii=0; ii< numCor; ii++){
      pht[ii] = new UINT32 [numPhtEntries];
  }

  for(UINT32 j=0; j< numCor; j++){
      for(UINT32 k=0; k< numPhtEntries; k++){
        //init to be 0x10, TAKEN
        pht[j][k]=PHT_CTR_INIT; 
      }
  }
  
}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

bool   PREDICTOR::GetPrediction(UINT32 PC){

  //PC^gbh PC xor global branch history, is because of the GShare semantic
  //% numPhtEntries (2^17), we are taking 17 bits of the PC as our entry
  //we are taking lowest 17 bits of the PC to construct our table entry
  //UINT32 phtIndex   = (PC^gbh) % (numPhtEntries);
  UINT32 phtIndex   ;
  phtIndex   = (PC^gbh) % (numPhtEntries);
  UINT32 tableNum = correlation();
  UINT32 phtCounter = pht[tableNum][phtIndex];
  
  
  //saturation counter in action
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
  UINT32 tableNum   = correlation();
  UINT32 phtCounter = pht[tableNum][phtIndex];
  // update the PHT, based on actual outcome, resolveDir
  // using saturation counter
  if(resolveDir == TAKEN){
    pht[tableNum][phtIndex] = SatIncrement(phtCounter, PHT_CTR_MAX);
  }else{
    pht[tableNum][phtIndex] = SatDecrement(phtCounter);
  }

  // update the gbh
  // gbh stores last 32 BP result (resolveDir), though we only use 17 bits of it
  gbh = (gbh << 1);

  if(resolveDir == TAKEN){
    gbh++; 
  }

  if(corBits > 1){
      for(UINT32 i=0; i<corBits-1; i++){
          tableSelSR[i+1] = tableSelSR[i];
      }
  }
  
  tableSelSR[0] = resolveDir;
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
UINT32 PREDICTOR::concatenate(UINT32 a, UINT32 b){
    UINT32  result;
    UINT8   seg1, seg2, seg3, seg4;
    
    seg4 = b & 0xff;
    seg3 = a & 0xff;
    seg2 = (b>>8) & 0xff;
    seg1 = (a>>8) & 0xff;

    result = seg1<<24 | seg2<<16 | seg3<<8 | seg4;
    //result = seg1<<24 | seg3<<16 | seg2<<8 | seg4;
    return result;
}

UINT32 PREDICTOR::correlation(){
    UINT32 ret=0;
    
    for(UINT32 i=0; i<corBits; i++){
        ret = ret<<1;
        ret = ret | tableSelSR[i];
    }
    return ret;
}

