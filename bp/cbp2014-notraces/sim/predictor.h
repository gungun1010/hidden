#ifndef _PREDICTOR_H_
#define _PREDICTOR_H_

#include "utils.h"
#include "tracer.h"
#include <math.h>
#include <string.h>
/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

class PREDICTOR{

  // The state is defined for Gshare, change for your design

 private:
  UINT32  gbh;           // global history register, global bracnch history
  UINT32  **basePht;          // pattern history table, BP table entry
  UINT32  pcReserveBits; // history length
  UINT32  corBits;       // correlation bits
  UINT32  numCor;           // number of correlated tables
  UINT32  numPhtEntries; // entries in basePht 
  bool    *tableSelSR;      //table selector shift register
  UINT32     *tageEntryBits;  //the length of he array is the number of componnents, value of the element is the number of bits of  each table entry
  UINT32     **tag;
  char        **counter;    //counter for tage component entry, 3 bits used
  char        **useful;     //useful bit for the entry of each tage component, 1 bit used
  UINT32      currComponent;
  UINT32      currTageEntry;
  bool        altPred;
  bool        realPred;
 public:

  // The interface to the four functions below CAN NOT be changed

  PREDICTOR(void);
  bool    GetPrediction(UINT32 PC);  
  void    UpdatePredictor(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget);
  void    TrackOtherInst(UINT32 PC, OpType opType, UINT32 branchTarget);
  UINT32    correlation();
  bool      basePred(UINT32 PC);
  UINT32    tagePred(UINT32 PC, UINT32 indx);
  void      updateBase(UINT32 PC, bool resolveDir);
  void      updateTage(UINT32 PC, bool resolveDir, bool predDir);
  // Contestants can define their own functions below

};



/***********************************************************/
#endif

