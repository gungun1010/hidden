#include "predictor.h"


#define PHT_CTR_MAX  3
#define PHT_CTR_INIT 0

#define PC_RESERVE_BITS   15
#define CORRELATION_BITS  1

#define TAGE_COMPONENTS   3
#define FIRST_TAGE_LENGTH 14
#define TAG_LENGTH  13
#define USEFUL_RESET    256000
#define UPPER 0
#define LOWER 1
/////////////// STORAGE BUDGET JUSTIFICATION ////////////////
// Total storage budget: 32KB + 17 bits
// Total PHT (pattern history table) entries: 2^17 
// Total PHT size = 2^17 * 2 bits per saturationPrediction = 2^18 bits = 32KB
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
  provider    = 0xdeadbeef;//default to a dead value for current tage component
  altPred          = NOT_TAKEN; //base prediction default to not taken
  usefulRst        = 0; //useful reset starts at 0
  usefulAlt        = UPPER;
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
  componentLen = new UINT32 [TAGE_COMPONENTS];
  tag = new UINT32* [TAGE_COMPONENTS];
  counter = new char* [TAGE_COMPONENTS];
  useful = new char* [TAGE_COMPONENTS];
  
  for(UINT32 jj=0; jj< TAGE_COMPONENTS; jj++){
      //initlaize the size of tage components in geometric series fashion
      //componentLen[jj] =(UINT32) pow(GEO_SERIES_RATIO,jj) * GEO_SERIES_BEGIN;
      componentLen[jj] = FIRST_TAGE_LENGTH + jj;

      //initialize the tags to all 0 for each component
      tag[jj] = new UINT32 [1<<componentLen[jj]];
      memset(tag[jj],0,1<<componentLen[jj]);

      counter[jj] = new char [1<<componentLen[jj]];
      memset(counter[jj],PHT_CTR_INIT,1<<componentLen[jj]);
      
      useful[jj] = new char [1<<componentLen[jj]];
      memset(useful[jj],0,1<<componentLen[jj]);
  }
}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

bool   PREDICTOR::GetPrediction(UINT32 PC){
    
    bool ret;
    UINT32 found;
    
    //get base prediction from a 2-bit saturation predictor with coorelation
    ret = basePred(PC);
 
    //record current base prediction to alternative prediction
    altPred = ret;
    
    //default to a dead value for current tage component
    provider = 0xdeadbeef;
    
    //loop thru tage components
    for(UINT32 indx=0; indx<TAGE_COMPONENTS; indx++){
        //try to get a tage prediction from shortest tage component to longest tage component
        found = tagePred(PC, indx);
        
        //if we have a tage match, the prediction is the longest hitted tage prediction
        if(found != 0xdeadbeef ) {
            ret = found;
            provider = indx;
        }
    }
    return ret; 
}


/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

void  PREDICTOR::UpdatePredictor(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget){
    if(provider == 0xdeadbeef){
        updateBase(PC, resolveDir);
    }else{
        updateTage(PC, resolveDir, predDir);
    }
   
   //update global history
    gbh = (gbh << 1);

    if(resolveDir == TAKEN){
        gbh++; 
    }

    usefulRst++;
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
    UINT32 basePhtIndex;
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
    //total number of tags in the tage component is 2^componentLen
    UINT32 tageIndx= (PC^gbh) % componentLen[indx];
    UINT32 myTag = (PC ^ (gbh << 2)) % TAG_LENGTH;  
    //prediction default to not found 
    UINT32 ret = 0xdeadbeef;
    //hash the PC with global branch history with the defined tag length 
        
    //found matching tag
    //break away from the loop
    if(myTag == tag[indx][tageIndx]){
        //provide prediction
        //found tag, save index of found tag

        //saturation counter in action
        if(counter[indx][tageIndx] == PHT_CTR_MAX){
            ret = TAKEN;
        }else if(counter[indx][tageIndx] == PHT_CTR_INIT){
            ret = NOT_TAKEN;
        }
    }

    return ret;
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
   UINT32 tageIndx= (PC^gbh) % componentLen[provider];
   UINT32 myTag = (PC ^ (gbh << 2)) % TAG_LENGTH;  
   UINT32 numEntries;
   
   //update useful bit 
   //set useful bit when prediction is correct and alternative prediction is incorrect
   //we have to find a matching in order for this if() to be true
   if(predDir != altPred){

       if(predDir == resolveDir){
           //set useful bit
           if(useful[provider][tageIndx] != PHT_CTR_MAX) 
                useful[provider][tageIndx]++;
       }else{
           if(useful[provider][tageIndx] != PHT_CTR_INIT)
               useful[provider][tageIndx]--;
       }
       

       //reset all useful bits when it saturates
       if(usefulRst >= USEFUL_RESET){
           //reset useful bits
           for(UINT32 i=0; i<TAGE_COMPONENTS; i++){
               numEntries = 1<<componentLen[i];
               for(UINT32 j=0; j < numEntries; j++){
                   if(usefulAlt == UPPER){
                        useful[i][j]= useful[i][j] & 1; //reset upper useful bit
                        usefulAlt = LOWER;
                   }else{
                       useful[i][j]= useful[i][j] & 2; //reset lower useful bit
                       usefulAlt = UPPER;
                   }
               }
           }

           //reset branch counter
           usefulRst = 0;
       }
   }
   
    //update counter when we can find matching tag (provider)
    //if we dont have a matching tag, we dont do anything here
    tagePhtCounter = counter[provider][tageIndx];

    //update in a saturation counter fashion
    if(resolveDir == TAKEN){
        counter[provider][tageIndx] = SatIncrement(tagePhtCounter, PHT_CTR_MAX);
    }else{
        counter[provider][tageIndx] = SatDecrement(tagePhtCounter);
    }


   //FIXME: how ?
   //add new entry
   //get number of tags in this tage components
   //new entry is allocated only on misprediction
   if(resolveDir != predDir){//misprediction
       if(provider < TAGE_COMPONENTS-1){
           numEntries = 1<<componentLen[provider+1];
           //allocate on provider+1 compoent
           for(UINT32 i=0; i< numEntries; i++){
                if(useful[provider+1][i] == 0){
                    counter[provider+1][i]=1;
                    tag[provider+1][i] = myTag;
                    break;
                }
           }
       }else{
           numEntries = 1<<componentLen[provider];
           //allocate on provider compoent
           for(UINT32 i=0; i< numEntries; i++){
                if(useful[provider][i] == 0){
                    counter[provider][i]=1;
                    tag[provider][i] = myTag;
                    break;
                }
           }
       }
   }
}
