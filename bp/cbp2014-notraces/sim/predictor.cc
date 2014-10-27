#include "predictor.h"


#define PHT_CTR_MAX  0x11
#define PHT_CTR_INIT 0x10

#define PC_RESERVE_BITS   15
#define CORRELATION_BITS  2
#define TAGE_COMPONENTS   4
#define GEO_SERIES_BEGIN  1
#define GEO_SERIES_RATIO  2
#define TAG_LENGTH  5
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


  ////////////////////////////////////////////////////////////////
  //                Initialize base pht 
  ////////////////////////////////////////////////////////////////
  
  
  //allocate total of 2^17 UINT32 of memory, takes 17 bits from PC
  basePht = new UINT32* [numCor];
  
  //allocate memory for talbe selector shift register
  tableSelSR = new bool[corBits];
  
  for(UINT32 i=0; i<corBits; i++){
      tableSelSR[i] = TAKEN;
  }
  
  for(UINT32 ii=0; ii< numCor; ii++){
      basePht[ii] = new UINT32 [numPhtEntries];
  }

  for(UINT32 j=0; j< numCor; j++){
      for(UINT32 k=0; k< numPhtEntries; k++){
        //init to be 0x10, TAKEN
        basePht[j][k]=PHT_CTR_INIT; 
      }
  }

  ////////////////////////////////////////////////////////////////
  //                Initialize tage 
  ////////////////////////////////////////////////////////////////
  tageComponents = new UINT32 [TAGE_COMPONENTS];
  tag = new UINT32* [TAGE_COMPONENTS];
  counter = new char* [TAGE_COMPONENTS];
  useful = new bool* [TAGE_COMPONENTS];
   
  for(UINT32 jj=0; jj< TAGE_COMPONENTS; jj++){
      //initlaize the size of tage components in geometric series fashion
      tageComponents[jj] =(UINT32) pow(GEO_SERIES_RATIO,jj) * GEO_SERIES_BEGIN;

      //initialize the tags to all 0 for each component
      tag[jj] = new UINT32 [tageComponents[jj]];
      memset(tag[jj],0,tageComponents[jj]);

      counter[jj] = new UINT32 [tageComponents[jj]];
      memset(counter[jj],0,tageComponents[jj]);
      
      useful[jj] = new UINT32 [tageComponents[jj]];
      memset(useful[jj],0,tageComponents[jj]);
  }
}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

bool   PREDICTOR::GetPrediction(UINT32 PC){
    
    bool ret;

    ret = basePred(PC);
    for(UINT32 indx=0; indx<TAGE_COMPONENTS; indx++){
        tagePred(PC, indx);
    }
    return ret; 
}


/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

void  PREDICTOR::UpdatePredictor(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget){

  UINT32 basePhtIndex   = (PC^gbh) % (numPhtEntries);
  UINT32 tableNum   = correlation();
  UINT32 basePhtCounter = basePht[tableNum][basePhtIndex];
  // update the PHT, based on actual outcome, resolveDir
  // using saturation counter
  if(resolveDir == TAKEN){
    basePht[tableNum][basePhtIndex] = SatIncrement(basePhtCounter, PHT_CTR_MAX);
  }else{
    basePht[tableNum][basePhtIndex] = SatDecrement(basePhtCounter);
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
UINT32 PREDICTOR::correlation(){
    UINT32 ret=0;
    
    for(UINT32 i=0; i<corBits; i++){
        ret = ret<<1;
        ret = ret | tableSelSR[i];
    }
    return ret;
}

bool PREDICTOR::basePred(UINT32 PC){
    //PC^gbh PC xor global branch history, is because of the GShare semantic
    //% numPhtEntries (2^17), we are taking 17 bits of the PC as our entry
    //we are taking lowest 17 bits of the PC to construct our table entry
    //UINT32 basePhtIndex   = (PC^gbh) % (numPhtEntries);
    UINT32 basePhtIndex   ;
    basePhtIndex = (PC^gbh) % (numPhtEntries);
    UINT32 tableNum = correlation();
    UINT32 basePhtCounter = basePht[tableNum][basePhtIndex];
    bool ret; 

    //saturation counter in action
    if(basePhtCounter > PHT_CTR_MAX/2){
        ret = TAKEN; 
    }else{
        ret = NOT_TAKEN; 
    }

    return ret;
}

void PREDICTOR::tagePred(UINT32 PC, UINT32 indx){
    UINT32 tageEntry;
    UINT32 tag;
    //hash the PC with global branch history with the tage component length
    tageEntry = (PC^gbh) % tageComponents[indx];    
    tag = (PC^gbh) % TAG_LENGTH 
    
    //finding matching tag
    for(UINT32 i=0;i<TAG_LENGTH;i++){
        if(tag == tag[indx][i]){
            //provide prediction
        }
    }
    //create a tag
//    tagLength = tageComponents[indx]<<1;
//    tag = (PC^gbh) % tageComponents[indx];
}
