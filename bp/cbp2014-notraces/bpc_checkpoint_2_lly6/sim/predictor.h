#ifndef _PREDICTOR_H_
#define _PREDICTOR_H_

#include "utils.h"
#include "tracer.h"


/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

class PREDICTOR{

  // The state is defined for Gshare, change for your design

 private:
  UINT32  gbh;           // global history register, global bracnch history
  UINT32  **pht;          // pattern history table, BP table entry
  UINT32  pcReserveBits; // history length
  UINT32  corBits;       // correlation bits
  UINT32  numCor;           // number of correlated tables
  UINT32  numPhtEntries; // entries in pht 
  bool    *tableSelSR;      //table selector shift register

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

