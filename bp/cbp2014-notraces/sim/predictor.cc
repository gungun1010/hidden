#include "predictor.h"


#define PHT_CTR_MAX  3
#define PHT_CTR_INIT 0

#define PC_RESERVE_BITS   16
#define CORRELATION_BITS  1

#define BTB_SIZE       64
#define MIS_PRED_THRES 3
#define BLACKLIST_SIZE 512
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
        pht[j][k]=PHT_CTR_INIT; 
      }
  }
  
  //init BTB
  btbEntry = new UINT32[BTB_SIZE];
  btbVal = new bool [BTB_SIZE];
  btbAge = new UINT32 [BTB_SIZE];
  btbMisPred = new UINT32 [BTB_SIZE];
  matching = false;
  currIndx = 0;
  loc =0;

  for(UINT32 indx=0; indx<BTB_SIZE; indx++){
    btbEntry[indx] = 0;
    btbVal[indx] = NOT_TAKEN;
    btbAge[indx] = 0; 
    btbMisPred[indx] =0;
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
  
  matching = false; 
  //find PC in the btb
  //cout<<endl;
  for(UINT32 indx=0; indx<BTB_SIZE; indx++){
      if(PC == btbEntry[indx]){
          //cout<<"found matching"<<endl;
          matching = true;
          currIndx = indx;
          return btbVal[indx];
      }
  }
  
  //cout<<"no matching in btb"<<endl;
  //stick with correlated-GShare if PC is not in btb 
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
  UINT32 btbIndx;

  //update BTB
  if(!matching){
       //try find an empty slot in BTB
      for(btbIndx=0; btbIndx<BTB_SIZE; btbIndx++){
          if(btbEntry[btbIndx] == 0){//found empty slot
                //if the current PC is in the blacklist, we don't add it to the btb
                if(std::find(blackList.begin(), blackList.end(), PC)!=blackList.end()){
                    break;
                }else{
                    //insert BTB entry, prediction, and age
                    btbEntry[btbIndx]=PC;
                    btbVal[btbIndx]=resolveDir;
                    btbAge[btbIndx]=0;
                    break;
                }
          }
      }

      //if empty slot not found
      //find oldest slot
      if(btbIndx >= BTB_SIZE){
          for(UINT32 i=0; i<BTB_SIZE; i++){
              if(btbAge[i] >= BTB_SIZE-1){
                //if the current PC is in the blacklist, we don't add it to the btb
                if(std::find(blackList.begin(), blackList.end(), PC)!=blackList.end()){
                    break;
                }else{
                    //insert BTB entry, prediction, and age
                    btbEntry[i]=PC;
                    btbVal[i]=resolveDir;
                    btbAge[i]=0;
                    break;
                }
              }
          }
      }

  }else{//if there is a matching

      if(resolveDir != predDir){
           //cout<<"mis predict on matching"<<endl;
           btbMisPred[currIndx]++;
           btbVal[currIndx]=resolveDir;

           //flush the entry if the outcome is too volatile
           if(btbMisPred[currIndx]>=MIS_PRED_THRES){
                //add to black list
                //keep in mind that the blacklist has limited size
                if(blackList.size()<BLACKLIST_SIZE){
                    blackList.push_back(btbEntry[currIndx]);
                }else{
                    //we rewind blacklist index when it is full
                    if(loc>=BLACKLIST_SIZE){
                        loc =0;
                    }
                    
                    //loc is 0 to begin with after push_back completes
                    blackList[loc]=btbEntry[currIndx];
                    loc++;
                }

                //flush
                btbEntry[currIndx]=0;
                btbVal[currIndx]=NOT_TAKEN;
                btbAge[currIndx]=0;
                btbMisPred[currIndx]=0;
           }
      }else{
          //reset age on matching btb entry
          btbAge[currIndx]=0;
      }
  }

  //update btb entry's age
  for(btbIndx=0; btbIndx<BTB_SIZE; btbIndx++){
    //  cout<<btbIndx<<" "<<btbAge[btbIndx]<<endl;
      if(btbAge[btbIndx]<BTB_SIZE-1){
          btbAge[btbIndx]++;
      }
  }

  //we only update pht when the correlated GShare is in effect
  if(!matching){
      //update saturation counter
      if(resolveDir == TAKEN){
        pht[tableNum][phtIndex] = SatIncrement(phtCounter, PHT_CTR_MAX);
      }else{
        pht[tableNum][phtIndex] = SatDecrement(phtCounter);
      }
  }
  
  // update the gbh, always update global branch history
  // gbh stores last 32 BP result (resolveDir), though we only use 17 bits of it
  gbh = (gbh << 1);

  if(resolveDir == TAKEN){
    gbh++; 
  }

  if(!matching){
      //update correlation bit
      if(corBits > 1){
          for(UINT32 i=0; i<corBits-1; i++){
              tableSelSR[i+1] = tableSelSR[i];
          }
      }
      
      tableSelSR[0] = resolveDir;
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

//this function is unused
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

