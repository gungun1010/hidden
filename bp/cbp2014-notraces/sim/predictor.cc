#include "predictor.h"


#define PHT_CTR_MAX  3
#define PHT_CTR_INIT 0

#define PC_RESERVE_BITS   17
#define CORRELATION_BITS  1

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
  currComponent    = 0xdeadbeef;//default to a dead value for current tage component
  currTageEntry    = 0xdeadbeef;//default to a dead value for current tage entry, meaning default to no found
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
  tageEntryBits = new UINT32 [TAGE_COMPONENTS];
  tag = new UINT32* [TAGE_COMPONENTS];
  counter = new char* [TAGE_COMPONENTS];
  useful = new char* [TAGE_COMPONENTS];
   
  for(UINT32 jj=0; jj< TAGE_COMPONENTS; jj++){
      //initlaize the size of tage components in geometric series fashion
      tageEntryBits[jj] =(UINT32) pow(GEO_SERIES_RATIO,jj) * GEO_SERIES_BEGIN;

      //initialize the tags to all 0 for each component
      tag[jj] = new UINT32 [1<<tageEntryBits[jj]];
      memset(tag[jj],0,1<<tageEntryBits[jj]);

      counter[jj] = new char [1<<tageEntryBits[jj]];
      memset(counter[jj],PHT_CTR_INIT,1<<tageEntryBits[jj]);
      
      useful[jj] = new char [1<<tageEntryBits[jj]];
      memset(useful[jj],0,1<<tageEntryBits[jj]);
  }
}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

bool   PREDICTOR::GetPrediction(UINT32 PC){
    
    bool ret;
    UINT32 found;

    ret = basePred(PC);
    altPred = ret;

    for(UINT32 indx=0; indx<TAGE_COMPONENTS; indx++){
        //try to get a tage prediction from shortest tage component to longest tage component
        found = tagePred(PC, indx);
        
        //if we have a tage match, the prediction is the longest hitted tage prediction
        if(found != 0xdeadbeef ) {
            ret = found;
            realPred = found;
            currComponent = indx;
        }
    }
    return ret; 
}


/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

void  PREDICTOR::UpdatePredictor(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget){
    updateBase(PC, resolveDir);

    updateTage(PC, resolveDir, predDir);
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

UINT32 PREDICTOR::tagePred(UINT32 PC, UINT32 indx){
    UINT32 myTag;
    UINT32 numTags;
    UINT32 ret = 0xdeadbeef;//prediction default to not found 
    
    //hash the PC with global branch history with the defined tag length 
    myTag = (PC^gbh) % TAG_LENGTH;

    //total number of tags in the tage component is 2^tageEntryBits
    numTags = 1<<tageEntryBits[indx];

    //finding matching tag
    for(UINT32 i=0; i<numTags; i++){
        if(myTag == tag[indx][i]){//found matching tag
            //provide prediction
            //saturation counter in action
            //found tag, save index of found tag
            currTageEntry = i;
            if(counter[indx][i] > PHT_CTR_MAX/2){
                ret = TAKEN;
            }else{
                ret = NOT_TAKEN;
            }
            break;
        }
    }

    return ret;
    //create a tag
//    tagLength = tageEntryBits[indx]<<1;
//    tag = (PC^gbh) % tageEntryBits[indx];
}

void PREDICTOR::updateBase(UINT32 PC, bool resolveDir){
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
    
    //update correlation bit
    if(corBits > 1){
        for(UINT32 i=0; i<corBits-1; i++){
            tableSelSR[i+1] = tableSelSR[i];
        }
    }

    tableSelSR[0] = resolveDir;
}

void PREDICTOR::updateTage(UINT32 PC, bool resolveDir, bool predDir){
   
   UINT32 tagePhtCounter;
   UINT32 newEntry;
   UINT32 numTags;
   
   //FIXME WHEN TO RESET USEFUL BIT`    
   //update useful bit 
   if(resolveDir == predDir && resolveDir != altPred){
       //set useful bit 
       useful[currComponent][currTageEntry] = 1;
   }

   //update tag
   numTags = 1 << tageEntryBits[currComponent];
   if(resolveDir != predDir){
        for(UINT32 i=0; i< numTags; i++){
            if(!(useful[currComponent][i])){
                tag[currComponent][i] = (PC^gbh) % TAG_LENGTH;
                newEntry = i;
            }
            break;
        }
   }
    
    //update counter
    if(currTageEntry != 0xdeadbeef){//if we find matching tag
        tagePhtCounter = counter[currComponent][currTageEntry];
        if(resolveDir == TAKEN){
            counter[currComponent][currTageEntry] = SatIncrement(tagePhtCounter, PHT_CTR_MAX);
        }else{
            counter[currComponent][currTageEntry] = SatDecrement(tagePhtCounter);
        }
    }else{
        if(resolveDir == TAKEN){
            counter[currComponent][newEntry] = SatIncrement(PHT_CTR_INIT, PHT_CTR_MAX);
        }else{
            counter[currComponent][newEntry] = SatDecrement(PHT_CTR_INIT);
        }
    }

}
