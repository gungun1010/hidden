#ifndef _PREDICTOR_H_
#define _PREDICTOR_H_

#include "utils.h"
#include "tracer.h"
#include <vector>
#include <algorithm>
using namespace std;
/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

class PREDICTOR{

  // The state is defined for Gshare, change for your design

 private:
  UINT32  gbh;              // global history register, global bracnch history
  UINT32  **pht;            // pattern history table, BP table entry
  UINT32  pcReserveBits;    // history length

  //correlation variables
  UINT32  corBits;          // correlation bits
  UINT32  numCor;           // number of correlated tables
  UINT32  numPhtEntries;    // entries in pht 
  bool    *tableSelSR;      //table selector shift register

  //btb variables
  UINT32  *btbEntry;        //branch target buffer entries, 9 bits per entry
  bool    *btbVal;          //btb's target prediction, 1 bit per entry
  UINT32    *btbAge;        //btb's entry age, 9 bits per entry
  bool    matching;         //global matching flag for BTB lookup, 1 bit
  UINT32  currIndx;         //matching index of the entry, 9 bits
  UINT32  *btbMisPred;      //miss prediction when matching, 2 bits per entry
  vector<UINT32> blackList; //black list to hold highly volatile branch, 32 bit
  UINT32  loc;              //blackList index for times when blackList is full

 public:

  // The interface to the four functions below CAN NOT be changed

  PREDICTOR(void);
  bool    GetPrediction(UINT32 PC);  
  void    UpdatePredictor(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget);
  void    TrackOtherInst(UINT32 PC, OpType opType, UINT32 branchTarget);
  UINT32    concatenate(UINT32 pc, UINT32 gbh);
  UINT32    correlation();
  // Contestants can define their own functions below

};



/***********************************************************/
#endif

