// This file is part of ArboristCore.

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
   @file splitpred.cc

   @brief Methods to implement splitting of index-tree levels.

   @author Mark Seligman
 */


#include "index.h"
#include "splitpred.h"
#include "splitsig.h"
#include "level.h"
#include "runset.h"
#include "samplenux.h"
#include "samplepred.h"
#include "callback.h"
#include "framemap.h"
#include "rowrank.h"

vector<double> SPReg::mono;
unsigned int SPReg::predMono = 0;

/**
  @brief Constructor.  Initializes 'runFlags' to zero for the single-split root.
 */
SplitPred::SplitPred(const FrameTrain *_frameTrain,
		     const RowRank *_rowRank,
		     unsigned int _bagCount) :
  rowRank(_rowRank),
  frameTrain(_frameTrain),
  bagCount(_bagCount),
  noSet(bagCount * frameTrain->NPredFac()),
  splitSig(new SplitSig(frameTrain->NPred())) {
}


/**
   @brief Destructor.  Deletes dangling 'runFlags' vector, which should be
   nonempty.
 */
SplitPred::~SplitPred() {
  delete run;
  delete splitSig;
}


/**
   @brief Caches a local copy of the mono[] vector.
 */
void SPReg::Immutables(const vector<double> &feMono) {
  predMono = 0;
  for (auto monoProb : feMono) {
    mono.push_back(monoProb);
    predMono += monoProb != 0.0;
  }
}


void SPReg::DeImmutables() {
  mono.clear();
  predMono = 0;
}


/**
   @brief Constructor.
 */
SPReg::SPReg(const FrameTrain *_frameTrain,
	     const RowRank *_rowRank,
	     unsigned int _bagCount) :
  SplitPred(_frameTrain, _rowRank, _bagCount),
  ruMono(vector<double>(0)) {
  run = new Run(0, frameTrain->NRow(), noSet);
}


/**
   @brief Constructor.

   @param sampleCtg is the sample vector for the tree, included for category lookup.
 */
SPCtg::SPCtg(const FrameTrain *_frameTrain,
	     const RowRank *_rowRank,
	     unsigned int _bagCount,
	     unsigned int _nCtg):
  SplitPred(_frameTrain, _rowRank, _bagCount),
  nCtg(_nCtg) {
  run = new Run(nCtg, frameTrain->NRow(), noSet);
}


RunSet *SplitPred::RSet(unsigned int setIdx) const {
  return run->RSet(setIdx);
}


unsigned int SplitPred::DenseRank(unsigned int predIdx) const {
  return rowRank->DenseRank(predIdx);
}


/**
   @brief Sets quick lookup offets for Run object.

   @return void.
 */
void SPReg::RunOffsets(const vector<unsigned int> &runCount) {
  run->RunSets(runCount);
  run->OffsetsReg();
}


/**
   @brief Sets quick lookup offsets for Run object.
 */
void SPCtg::RunOffsets(const vector<unsigned int> &runCount) {
  run->RunSets(runCount);
  run->OffsetsCtg();
}


/**
   @brief Initializes only those field values known before restaging or
   adjusted following.

   @return void.
 */
void SplitCoord::InitEarly(unsigned int splitIdx,
			   unsigned int predIdx,
			   unsigned int bufIdx) {
  this->splitIdx = splitIdx;
  this->predIdx = predIdx;
  this->bufIdx = bufIdx;
}


/**
   @brief Initializes field values known only following restaging.  Entry
   singletons should not reach here.

   @return void
 */
void SplitCoord::InitLate(const Level *levelFront,
			  const IndexLevel *index,
			  unsigned int vecPos,
			  unsigned int setIdx) {
  this->vecPos = vecPos;
  this->setIdx = setIdx;
  unsigned int extent;
  preBias = index->SplitFields(splitIdx, idxStart, extent, sCount, sum);
  implicit = levelFront->AdjustDense(splitIdx, predIdx, idxStart, extent);
  idxEnd = idxStart + extent - 1; // May overflow if singleton:  invalid.
}


/**
   @brief
   // TODO:  Virtualize:  unnecessary for coprocessor, which employs autonomous splitting.
 */
void SplitPred::Preschedule(unsigned int splitIdx,
			    unsigned int predIdx,
			    unsigned int bufIdx) {
  SplitCoord sg;
  sg.InitEarly(splitIdx, predIdx, bufIdx);
  splitCoord.push_back(sg);
}


/**
   @brief Walks the list of split candidates and invalidates those which
   restaging has marked unsplitable as well as singletons persisting since
   initialization or as a result of bagging.  Fills in run counts, which
   values restaging has established precisely.
*/
void SplitPred::ScheduleSplits(const IndexLevel *index,
			       const Level *levelFront) {
  vector<unsigned int> runCount;
  vector<SplitCoord> sc2;
  for (auto & sg : splitCoord) {
    sg.Schedule(levelFront, index, noSet, runCount, sc2);
  }
  splitCoord = move(sc2);

  RunOffsets(runCount);
}


/**
   @brief Retains split coordinate iff target is not a singleton.  Pushes
   back run counts, if applicable.

   @param sg holds partially-initialized split coordinates.

   @param runCount accumulates nontrivial run counts.

   @param sc2 accumulates "actual" splitting coordinates.

   @return void, with output reference vectors.
 */
void SplitCoord::Schedule(const Level *levelFront,
			  const IndexLevel *index,
			  unsigned int noSet,
			  vector<unsigned int> &runCount,
			  vector<SplitCoord> &sc2) {
  unsigned int rCount;
  if (levelFront->ScheduleSplit(splitIdx, predIdx, rCount)) {
    InitLate(levelFront, index, sc2.size(), rCount > 1 ? runCount.size() : noSet);
    if (rCount > 1) {
      runCount.push_back(rCount);
    }
    sc2.push_back(*this);
  }
}


/**
   @brief Invoked from splitting methods to precipitate creation of signature
   for candidate split.

   @return void.
*/
void SplitPred::SSWrite(unsigned int splitIdx,
			unsigned int predIdx,
			unsigned int setPos,
			unsigned int bufIdx,
			const NuxLH &nux) const {
  splitSig->Write(splitIdx, predIdx, setPos, bufIdx, nux);
}


/**
   @brief Initializes level about to be split
 */
void SplitPred::LevelInit(IndexLevel *index) {
  splitCount = index->NSplit();
  LevelPreset(index); // virtual
  splitSig->LevelInit(splitCount);
}


/**
   @brief Base method.  Deletes per-level run and split-flags vectors.

   @return void.
 */
void SplitPred::LevelClear() {
  run->LevelClear();
  splitSig->LevelClear();
}


/**
   @brief Determines whether indexed predictor is a factor.

   @param predIdx is the predictor index.

   @return true iff predictor is a factor.
 */
bool SplitPred::IsFactor(unsigned int predIdx) const {
  return frameTrain->IsFactor(predIdx);
}


/**
   @brief Determines whether indexed predictor is numerica.

   @param predIdx is the predictor index.

   @return true iff predictor is numeric.
 */
unsigned int SPCtg::NumIdx(unsigned int predIdx) const {
  return frameTrain->NumIdx(predIdx);
}


/**
   @brief Run objects should not be deleted until after splits have been consumed.
 */
void SPReg::LevelClear() {
  SplitPred::LevelClear();
}


SPReg::~SPReg() {
}


SPCtg::~SPCtg() {
}


void SPCtg::LevelClear() {
  SplitPred::LevelClear();
}


/**
   @brief Sets level-specific values for the subclass.

   @param index contains the current level's index sets and state.

   @return void.
*/
void SPReg::LevelPreset(IndexLevel *index) {
  if (predMono > 0) {
    unsigned int monoCount = splitCount * frameTrain->NPred(); // Clearly too big.
    ruMono = move(CallBack::rUnif(monoCount));
  }

  index->SetPrebias();
}


/**
   @brief As above, but categorical response.  Initializes per-level sum and
   FacRun vectors.

   @return void.
*/
void SPCtg::LevelPreset(IndexLevel *index) {
  LevelInitSumR(frameTrain->NPredNum());
  sumSquares = move(vector<double>(splitCount));
  ctgSum = move(vector<double>(splitCount * nCtg));
  fill(sumSquares.begin(), sumSquares.end(), 0.0);
  fill(ctgSum.begin(), ctgSum.end(), 0.0);
  index->SumsAndSquares(nCtg, sumSquares, ctgSum);
  index->SetPrebias();
}


/**
   @brief Initializes the accumulated-sum checkerboard used by
   numerical predictors.

   @param nPredNum is the number of numerical predictors.

   @return void.
 */
void SPCtg::LevelInitSumR(unsigned int nPredNum) {
  if (nPredNum > 0) {
    ctgSumAccum = move(vector<double>(nPredNum * nCtg * splitCount));
    fill(ctgSumAccum.begin(), ctgSumAccum.end(), 0.0);
  }
}


/**
   @brief Determines whether a regression pair undergoes constrained splitting.

   @return The sign of the constraint, if within the splitting probability, else zero.
*/
int SPReg::MonoMode(unsigned int splitIdx,
		    unsigned int predIdx) const {
  if (predMono == 0)
    return 0;

  double monoProb = mono[predIdx];
  int sign = monoProb > 0.0 ? 1 : (monoProb < 0.0 ? -1 : 0);
  return sign * ruMono[splitIdx] < monoProb ? sign : 0;
}


void SplitPred::Split(const SamplePred *samplePred,
		      vector<SSNode> &argMax) {
  Split(samplePred);
  ArgMax(argMax);

  splitCoord.clear();
}


void SPCtg::Split(const SamplePred *samplePred) {
  // Guards cast to int for OpenMP 2.0 back-compatibility.
  int splitPos;
#pragma omp parallel default(shared) private(splitPos)
  {
#pragma omp for schedule(dynamic, 1)
    for (splitPos = 0; splitPos < int(splitCoord.size()); splitPos++) {
      splitCoord[splitPos].Split(this, samplePred);
    }
  }
}


void SPReg::Split(const SamplePred *samplePred) {
  // Guards cast to int for OpenMP 2.0 back-compatibility.
  int splitPos;
#pragma omp parallel default(shared) private(splitPos)
  {
#pragma omp for schedule(dynamic, 1)
    for (splitPos = 0; splitPos < int(splitCoord.size()); splitPos++) {
      splitCoord[splitPos].Split(this, samplePred);
    }
  }
}


void SplitPred::ArgMax(vector<SSNode> &argMax) {
  int splitIdx;
#pragma omp parallel default(shared) private(splitIdx)
  {
#pragma omp for schedule(dynamic, 1)
    for (splitIdx = 0; splitIdx < int(splitCount); splitIdx++) {
      argMax[splitIdx].ArgMax(splitSig, splitIdx);
    }
  }
}


/**
   @brief  Regression splitting based on type:  numeric or factor.
 */
void SplitCoord::Split(const SPReg *spReg, const SamplePred *samplePred) {
  if (spReg->IsFactor(predIdx)) {
    SplitFac(spReg, samplePred->PredBase(predIdx, bufIdx));
  }
  else {
    SplitNum(spReg, samplePred->PredBase(predIdx, bufIdx));
  }
}


/**
   @brief Categorical splitting based on type:  numeric or factor.
 */
void SplitCoord::Split(SPCtg *spCtg,
		       const SamplePred *samplePred) {
  if (spCtg->IsFactor(predIdx)) {
    SplitFac(spCtg, samplePred->PredBase(predIdx, bufIdx));
  }
  else {
    SplitNum(spCtg, samplePred->PredBase(predIdx, bufIdx));
  }
}


void SplitCoord::SplitNum(const SPReg *spReg,
			  const SampleRank spn[]) {
  NuxLH nux;
  if (SplitNum(spReg, spn, nux)) {
    spReg->SSWrite(splitIdx, predIdx, setIdx, bufIdx, nux);
  }
}


/**
   @brief Gini-based splitting method.

   @return void.
*/
void SplitCoord::SplitNum(SPCtg *spCtg,
			  const SampleRank spn[]) {
  NuxLH nux;
  if (SplitNum(spCtg, spn, nux)) {
    spCtg->SSWrite(splitIdx, predIdx, setIdx, bufIdx, nux);
  }
}


void SplitCoord::SplitFac(const SPReg *spReg,
			  const SampleRank spn[]) {
  NuxLH nux;
  if (SplitFac(spReg, spn, nux)) {
    spReg->SSWrite(splitIdx, predIdx, setIdx, bufIdx, nux);
  }
}


void SplitCoord::SplitFac(const SPCtg *spCtg,
			  const SampleRank spn[]) {
  NuxLH nux;
  if (SplitFac(spCtg, spn, nux)) {
    spCtg->SSWrite(splitIdx, predIdx, setIdx, bufIdx, nux);
  }
}


bool SplitCoord::SplitFac(const SPCtg *spCtg,
			  const SampleRank spn[],
			  NuxLH &nux) {
  RunSet *runSet = spCtg->RSet(setIdx);
  RunsCtg(spCtg, runSet, spn);

  if (spCtg->CtgWidth() == 2) {
    return SplitBinary(spCtg, runSet, nux);
  }
  else {
    return SplitRuns(spCtg, runSet, nux);
  }
}


// The four major classes of splitting supported here are based on either
// Gini impurity or weighted variance.  New variants may be supplied in
// future.


/**
   @brief Weighted-variance splitting method.

   @param nux outputs split nucleus.

   @return true iff pair splits.
 */
bool SplitCoord::SplitFac(const SPReg *spReg,
			  const SampleRank spn[],
			  NuxLH &nux) {
  RunSet *runSet = spReg->RSet(setIdx);
  RunsReg(runSet, spn, spReg->DenseRank(predIdx));
  runSet->HeapMean();
  runSet->DePop();

  return HeapSplit(runSet, nux);
}


/**
   @brief Invokes regression/numeric splitting method, currently only Gini available.

   @param indexSet[] is the vector of index nodes.

   @param nodeBase is the vector of SamplePred nodes for this level.

   @return void.
*/
bool SplitCoord::SplitNum(const SPReg *spReg,
			  const SampleRank spn[],
			  NuxLH &nux) {
  int monoMode = spReg->MonoMode(vecPos, predIdx);
  if (monoMode != 0) {
    return implicit > 0 ? SplitNumDenseMono(monoMode > 0, spn, spReg, nux) : SplitNumMono(monoMode > 0, spn, nux);
  }
  else {
    return implicit > 0 ? SplitNumDense(spn, spReg, nux) : SplitNum(spn, nux);
  }

}


/**
   @brief Weighted-variance splitting method.

   @return void.
*/
bool SplitCoord::SplitNum(const SampleRank spn[],
			  NuxLH &nux) {
  unsigned int rkRight, sampleCount;
  FltVal ySum;
  spn[idxEnd].RegFields(ySum, rkRight, sampleCount);
  double sumR = ySum;
  unsigned int sCountL = sCount - sampleCount; // >= 1: counts up to, including, this index. 
  unsigned int lhSampCt = 0;
  unsigned int lhSup = idxEnd;
  double maxInfo = preBias;

  // Walks samples backward from the end of nodes so that ties are not split.
  // Signing values avoids decrementing below zero.
  for (int i = int(idxEnd) - 1; i >= int(idxStart); i--) {
    unsigned int sCountR = sCount - sCountL;
    double sumL = sum - sumR;
    double idxGini = (sumL * sumL) / sCountL + (sumR * sumR) / sCountR;
    unsigned int rkThis;
    spn[i].RegFields(ySum, rkThis, sampleCount);
    if (idxGini > maxInfo && rkThis != rkRight) {
      lhSampCt = sCountL;
      lhSup = i;
      maxInfo = idxGini;
    }
    sCountL -= sampleCount;
    sumR += ySum;
    rkRight = rkThis;
  }

  if (maxInfo > preBias) {
    nux.InitNum(idxStart, lhSup + 1 - idxStart, lhSampCt, maxInfo - preBias, spn[lhSup].Rank(), spn[lhSup+1].Rank());
    return true;
  }
  else {
    return false;
  }
}


/**
   @brief Experimental.  Needs refactoring.

   @return void.
*/
bool SplitCoord::SplitNumDense(const SampleRank spn[],
			       const SPReg *spReg,
			       NuxLH &nux) {
  unsigned int rankDense = spReg->DenseRank(predIdx);
  double sumDense = sum;
  unsigned int sCountDense = sCount;
  unsigned int denseLeft, denseRight;
  unsigned int denseCut = spReg->Residuals(spn, idxStart, idxEnd, rankDense, denseLeft, denseRight, sumDense, sCountDense);

  unsigned int idxNext, idxFinal;
  unsigned int rkRight, sampleCount;
  FltVal ySum;
  if (denseRight) {
    ySum = sumDense;
    rkRight = rankDense;
    sampleCount = sCountDense;
    idxNext = idxEnd;
    idxFinal = idxStart;
  }
  else {
    spn[idxEnd].RegFields(ySum, rkRight, sampleCount);
    idxNext = idxEnd - 1;
    idxFinal = denseLeft ? idxStart : denseCut;
  }
  double sumR = ySum;
  unsigned int sCountL = sCount - sampleCount;
  unsigned int lhSampCt = 0;
  double maxInfo = preBias;

  unsigned int rankLH = 0;
  unsigned int rankRH = 0; // Splitting rank bounds.
  unsigned int rhInf = idxEnd + 1;  // Always non-negative.
  for (int i = int(idxNext); i >= int(idxFinal); i--) {
    unsigned int sCountR = sCount - sCountL;
    double sumL = sum - sumR;
    double idxGini = (sumL * sumL) / sCountL + (sumR * sumR) / sCountR;
    unsigned int rkThis;
    spn[i].RegFields(ySum, rkThis, sampleCount);
    if (idxGini > maxInfo && rkThis != rkRight) {
      lhSampCt = sCountL;
      rankLH = rkThis;
      rankRH = rkRight;
      rhInf = i + 1;
      maxInfo = idxGini;
    }
    sCountL -= sampleCount;
    sumR += ySum;
    rkRight = rkThis;
  }

  // Evaluates the dense component, if not of highest rank.
  if (denseCut != idxEnd) {
    unsigned int sCountR = sCount - sCountL;
    double sumL = sum - sumR;
    double idxGini = (sumL * sumL) / sCountL + (sumR * sumR) / sCountR;
    if (idxGini > maxInfo) {
      lhSampCt = sCountL;
      rhInf = idxFinal;
      rankLH = rankDense;
      rankRH = rkRight;
      maxInfo = idxGini;
    }
  
    if (!denseLeft) { // Walks remaining indices, if any, with rank below dense.
      sCountL -= sCountDense;
      sumR += sumDense;
      rkRight = rankDense;
      for (int i = idxFinal - 1; i >= int(idxStart); i--) {
	unsigned int sCountR = sCount - sCountL;
	double sumL = sum - sumR;
	double idxGini = (sumL * sumL) / sCountL + (sumR * sumR) / sCountR;
	unsigned int rkThis;
	spn[i].RegFields(ySum, rkThis, sampleCount);
	if (idxGini > maxInfo && rkThis != rkRight) {
	  lhSampCt = sCountL;
	  rhInf = i + 1;
	  rankLH = rkThis;
	  rankRH = rkRight;
	  maxInfo = idxGini;
	}
	sCountL -= sampleCount;
	sumR += ySum;
	rkRight = rkThis;
      }
    }
  }

  if (maxInfo > preBias) {
    unsigned int lhDense = rankLH >= rankDense ? implicit : 0;
    unsigned int lhIdxTot = rhInf - idxStart + lhDense;
    nux.InitNum(idxStart, lhIdxTot, lhSampCt, maxInfo - preBias, rankLH, rankRH, lhDense);
    return true;
  }
  else {
    return false;
  }
}


/**
   @brief TODO:  Merge with counterparts.

   @return void.
*/
bool SplitCoord::SplitNumDenseMono(bool increasing,
				   const SampleRank spn[],
				   const SPReg *spReg,
				   NuxLH &nux) {
  unsigned int rankDense = spReg->DenseRank(predIdx);
  double sumDense = sum;
  unsigned int sCountDense = sCount;
  unsigned int denseLeft, denseRight;
  unsigned int denseCut = spReg->Residuals(spn, idxStart, idxEnd, rankDense, denseLeft, denseRight, sumDense, sCountDense);

  unsigned int idxNext, idxFinal;
  unsigned int rkRight, sampleCount;
  FltVal ySum;
  if (denseRight) {
    ySum = sumDense;
    rkRight = rankDense;
    sampleCount = sCountDense;
    idxNext = idxEnd;
    idxFinal = idxStart;
  }
  else {
    spn[idxEnd].RegFields(ySum, rkRight, sampleCount);
    idxNext = idxEnd - 1;
    idxFinal = denseLeft ? idxStart : denseCut;
  }
  double sumR = ySum;
  unsigned int sCountL = sCount - sampleCount;
  unsigned int lhSampCt = 0;
  double maxInfo = preBias;

  unsigned int rankLH = 0;
  unsigned int rankRH = 0; // Splitting rank bounds.
  unsigned int rhInf = idxEnd + 1;  // Always non-negative.
  for (int i = int(idxNext); i >= int(idxFinal); i--) {
    unsigned int sCountR = sCount - sCountL;
    double sumL = sum - sumR;
    double idxGini = (sumL * sumL) / sCountL + (sumR * sumR) / sCountR;
    unsigned int rkThis;
    spn[i].RegFields(ySum, rkThis, sampleCount);
    if (idxGini > maxInfo && rkThis != rkRight) {
      bool up = (sumL * sCountR <= sumR * sCountL);
      if (increasing ? up : !up) {
        lhSampCt = sCountL;
        rankLH = rkThis;
        rankRH = rkRight;
        rhInf = i + 1;
        maxInfo = idxGini;
      }
    }
    sCountL -= sampleCount;
    sumR += ySum;
    rkRight = rkThis;
  }

  // Evaluates the dense component, if not of highest rank.
  if (denseCut != idxEnd) {
    unsigned int sCountR = sCount - sCountL;
    double sumL = sum - sumR;
    double idxGini = (sumL * sumL) / sCountL + (sumR * sumR) / sCountR;
    if (idxGini > maxInfo) {
      lhSampCt = sCountL;
      rhInf = idxFinal;
      rankLH = rankDense;
      rankRH = rkRight;
      maxInfo = idxGini;
    }
  
    if (!denseLeft) {  // Walks remaining indices, if any, with rank below dense.
      sCountL -= sCountDense;
      sumR += sumDense;
      rkRight = rankDense;
      for (int i = idxFinal - 1; i >= int(idxStart); i--) {
	unsigned int sCountR = sCount - sCountL;
	double sumL = sum - sumR;
	double idxGini = (sumL * sumL) / sCountL + (sumR * sumR) / sCountR;
	unsigned int rkThis;
	spn[i].RegFields(ySum, rkThis, sampleCount);
	if (idxGini > maxInfo && rkThis != rkRight) {
	  bool up = (sumL * sCountR <= sumR * sCountL);
	  if (increasing ? up : !up) {
	    lhSampCt = sCountL;
	    rhInf = i + 1;
	    rankLH = rkThis;
	    rankRH = rkRight;
	    maxInfo = idxGini;
	  }
	}
	sCountL -= sampleCount;
	sumR += ySum;
	rkRight = rkThis;
      }
    }
  }

  if (maxInfo > preBias) {
    unsigned int lhDense = rankLH >= rankDense ? implicit : 0;
    unsigned int lhIdxTot = rhInf - idxStart + lhDense;
    nux.InitNum(idxStart, lhIdxTot, lhSampCt, maxInfo - preBias, rankLH, rankRH, lhDense);
    return true;
  }
  else {
    return false;
  }
}


/**
   @brief Weighted-variance splitting method.

   @return void.
*/
bool SplitCoord::SplitNumMono(bool increasing,
			      const SampleRank spn[],
			      NuxLH &nux) {
  unsigned int rkRight, sampleCount;
  FltVal ySum;
  spn[idxEnd].RegFields(ySum, rkRight, sampleCount);
  double sumR = ySum;
  unsigned int sCountL = sCount - sampleCount; // >= 1: counts up to, including, this index. 
  unsigned int lhSampCt = 0;
  unsigned int lhSup = idxEnd;
  double maxInfo = preBias;

  // Walks samples backward from the end of nodes so that ties are not split.
  // Signing values avoids decrementing below zero.
  for (int i = int(idxEnd) - 1; i >= int(idxStart); i--) {
    int sCountR = sCount - sCountL;
    double sumL = sum - sumR;
    double idxGini = (sumL * sumL) / sCountL + (sumR * sumR) / sCountR;
    unsigned int rkThis;
    spn[i].RegFields(ySum, rkThis, sampleCount);
    if (idxGini > maxInfo && rkThis != rkRight) {
      bool up = (sumL * sCountR <= sumR * sCountL);
      if (increasing ? up : !up) {
        lhSampCt = sCountL;
        lhSup = i;
        maxInfo = idxGini;
      }
    }
    sCountL -= sampleCount;
    sumR += ySum;
    rkRight = rkThis;
  }
  if (maxInfo > preBias) {
    nux.InitNum(idxStart, lhSup + 1 - idxStart, lhSampCt, maxInfo - preBias, spn[lhSup].Rank(), spn[lhSup + 1].Rank());
    return true;
  }
  else {
    return false;
  }
}


/**
   @brief Imputes dense rank values as residuals.

   @param idxSup outputs the sup of index values having ranks below the
   dense rank.

   @param sumDense inputs the reponse sum over the node and outputs the
   residual sum.

   @param sCount dense input the response sample count over the node and
   outputs the residual count.

   @return supremum of indices to the left ot the dense rank.
*/
unsigned int SPReg::Residuals(const SampleRank spn[],
			      unsigned int idxStart,
			      unsigned int idxEnd,
			      unsigned int rankDense,
			      unsigned int &denseLeft,
			      unsigned int &denseRight,
			      double &sumDense,
			      unsigned int &sCountDense) const {
  unsigned int denseCut = idxEnd; // Defaults to highest index.
  double sumTot = 0.0;
  unsigned int sCountTot = 0;
  for (int idx = int(idxEnd); idx >= int(idxStart); idx--) {
    unsigned int sampleCount, rkThis;
    FltVal ySum;
    spn[idx].RegFields(ySum, rkThis, sampleCount);
    denseCut = rkThis > rankDense ? idx : denseCut;
    sCountTot += sampleCount;
    sumTot += ySum;
  }
  sumDense -= sumTot;
  sCountDense -= sCountTot;

  // Dense blob is either left, right or neither.
  denseRight = (denseCut == idxEnd && spn[denseCut].Rank() < rankDense);  
  denseLeft = (denseCut == idxStart && spn[denseCut].Rank() > rankDense);
  
  return denseCut;
}


/**
   @brief Imputes dense rank values as residuals.

   @param idxSup outputs the sup of index values having ranks below the
   dense rank.

   @return true iff left bound has rank less than dense rank.
*/
unsigned int SPCtg::Residuals(const SampleRank spn[],
			      unsigned int splitIdx,
			      unsigned int idxStart,
			      unsigned int idxEnd,
			      unsigned int rankDense,
			      bool &denseLeft,
			      bool &denseRight,
			      double &sumDense,
			      unsigned int &sCountDense,
			      vector<double> &ctgSumDense) const {
  vector<double> ctgAccum;
  ctgSumDense.reserve(nCtg);
  ctgAccum.reserve(nCtg);
  for (unsigned int ctg = 0; ctg < nCtg; ctg++) {
    ctgSumDense.push_back(CtgSum(splitIdx, ctg));
    ctgAccum.push_back(0.0);
  }
  unsigned int denseCut = idxEnd; // Defaults to highest index.
  double sumTot = 0.0;
  unsigned int sCountTot = 0;
  for (int idx = int(idxEnd); idx >= int(idxStart); idx--) {
    // Accumulates statistics over explicit range.
    unsigned int yCtg, rkThis;
    FltVal ySum;
    unsigned int sampleCount = spn[idx].CtgFields(ySum, rkThis, yCtg);
    ctgAccum[yCtg] += ySum;
    denseCut = rkThis >= rankDense ? idx : denseCut;
    sCountTot += sampleCount;
    sumTot += ySum;
  }
  sumDense -= sumTot;
  sCountDense -= sCountTot;
  for (unsigned int ctg = 0; ctg < nCtg; ctg++) {
    ctgSumDense[ctg] -= ctgAccum[ctg];
  }

  // Dense blob is either left, right or neither.
  denseRight = (denseCut == idxEnd && spn[denseCut].Rank() < rankDense);  
  denseLeft = (denseCut == idxStart && spn[denseCut].Rank() > rankDense);

  return denseCut;
}


void SPCtg::ApplyResiduals(unsigned int splitIdx,
			   unsigned int predIdx,
			   double &ssL,
			   double &ssR,
			   vector<double> &sumDenseCtg) {
  unsigned int numIdx = NumIdx(predIdx);
  for (unsigned int ctg = 0; ctg < nCtg; ctg++) {
    double ySum = sumDenseCtg[ctg];
    double sumRCtg = CtgSumAccum(splitIdx, numIdx, ctg, ySum);
    ssR += ySum * (ySum + 2.0 * sumRCtg);
    double sumLCtg = CtgSum(splitIdx, ctg) - sumRCtg;
    ssL += ySum * (ySum - 2.0 * sumLCtg);
  }
}


bool SplitCoord::SplitNum(SPCtg *spCtg,
			  const SampleRank spn[],
			  NuxLH &nux) {
  if (implicit > 0) {
    return NumCtgDense(spCtg, spn, nux);
  }
  else {
    return NumCtg(spCtg, spn, nux);
  }
}


bool SplitCoord::NumCtg(SPCtg *spCtg,
			const SampleRank spn[],
			NuxLH &nux) {
  unsigned int sCountL = sCount;
  unsigned int rkRight = spn[idxEnd].Rank();
  double sumL = sum;
  double ssL = spCtg->SumSquares(splitIdx);
  double ssR = 0.0;
  double maxInfo = preBias;
  unsigned int rankRH = 0;
  unsigned int rankLH = 0;
  unsigned int rhInf = idxEnd;
  unsigned int lhSampCt = 0;
  lhSampCt = NumCtgGini(spCtg, spn, idxEnd, idxStart, sCountL, rkRight, sumL, ssL, ssR, maxInfo, rankLH, rankRH, rhInf);

  if (maxInfo > preBias) {
    unsigned int lhIdxTot = rhInf - idxStart;
    nux.InitNum(idxStart, lhIdxTot, lhSampCt, maxInfo - preBias, rankLH, rankRH, 0);
    return true;
  }
  else {
    return false;
  }
}


unsigned int SplitCoord::NumCtgGini(SPCtg *spCtg,
				    const SampleRank spn[],
				    unsigned int idxNext,
				    unsigned int idxFinal,
				    unsigned int &sCountL,
				    unsigned int &rkRight,
				    double &sumL,
				    double &ssL,
				    double &ssR,
				    double &maxGini,
				    unsigned int &rankLH,
				    unsigned int &rankRH,
				    unsigned int &rhInf) {
  unsigned int lhSampCt = 0;
  unsigned int numIdx = spCtg->NumIdx(predIdx);
  // Signing values avoids decrementing below zero.
  for (int idx = int(idxNext); idx >= int(idxFinal); idx--) {
    FltVal ySum;    
    unsigned int yCtg, rkThis;
    unsigned int sampleCount = spn[idx].CtgFields(ySum, rkThis, yCtg);
    FltVal sumR = sum - sumL;
    if (rkThis != rkRight && spCtg->StableDenoms(sumL, sumR)) {
      FltVal cutGini = ssL / sumL + ssR / sumR;
      if (cutGini > maxGini) {
        lhSampCt = sCountL;
	rankLH = rkThis;
	rankRH = rkRight;
	rhInf = idx + 1;
        maxGini = cutGini;
      }
    }
    rkRight = rkThis;

    sCountL -= sampleCount;
    sumL -= ySum;

    double sumRCtg = spCtg->CtgSumAccum(splitIdx, numIdx, yCtg, ySum);
    ssR += ySum * (ySum + 2.0 * sumRCtg);
    double sumLCtg = spCtg->CtgSum(splitIdx, yCtg) - sumRCtg;
    ssL += ySum * (ySum - 2.0 * sumLCtg);
  }

  return lhSampCt;
}


bool SplitCoord::NumCtgDense(SPCtg *spCtg,
			     const SampleRank spn[],
			     NuxLH &nux) {
  unsigned int rankDense = spCtg->DenseRank(predIdx);
  double sumDense = sum;
  unsigned int sCountDense = sCount;
  bool denseLeft, denseRight;
  vector<double> sumDenseCtg;
  unsigned int denseCut = spCtg->Residuals(spn, splitIdx, idxStart, idxEnd, rankDense, denseLeft, denseRight, sumDense, sCountDense, sumDenseCtg);

  unsigned int idxFinal;
  unsigned int sCountL = sCount;
  unsigned int rkRight;
  double sumL = sum;
  double ssL = spCtg->SumSquares(splitIdx);
  double ssR = 0.0;
  if (denseRight) { // Implicit values to the far right.
    idxFinal = idxStart;
    rkRight = rankDense;
    spCtg->ApplyResiduals(splitIdx, predIdx, ssL, ssR, sumDenseCtg);
    sCountL -= sCountDense;
    sumL -= sumDense;
  }
  else {
    idxFinal = denseLeft ? idxStart : denseCut + 1;
    rkRight = spn[idxEnd].Rank();
  }

  double maxInfo = preBias;
  unsigned int rankRH = 0;
  unsigned int rankLH = 0;
  unsigned int rhInf = idxEnd;
  unsigned int lhSampCt = NumCtgGini(spCtg, spn, idxEnd, idxFinal, sCountL, rkRight, sumL, ssL, ssR, maxInfo, rankLH, rankRH, rhInf);

  // Evaluates the dense component, if not of highest rank.
  if (denseCut != idxEnd) {
    FltVal sumR = sum - sumL;
    if (spCtg->StableDenoms(sumL, sumR)) {
      FltVal cutGini = ssL / sumL + ssR / sumR;
      if (cutGini >  maxInfo) {
	lhSampCt = sCountL;
	rhInf = idxFinal;
	rankLH = rankDense;
	rankRH = rkRight;
	maxInfo = cutGini;
      }
    }

    // Walks remaining indices, if any with ranks below dense.
    if (!denseLeft) {
      spCtg->ApplyResiduals(splitIdx, predIdx, ssR, ssL, sumDenseCtg);
      sCountL -= sCountDense;
      sumL -= sumDense;
      lhSampCt = NumCtgGini(spCtg, spn, denseCut, idxStart, sCountL, rkRight, sumL, ssL, ssR, maxInfo, rankLH, rankRH, rhInf);
    }
  }

  if (maxInfo > preBias) {
    unsigned int lhDense = rankLH >= rankDense ? implicit : 0;
    unsigned int lhIdxTot = rhInf - idxStart + lhDense;
    nux.InitNum(idxStart, lhIdxTot, lhSampCt, maxInfo - preBias, rankLH, rankRH, lhDense);
    return true;
  }
  else {
    return false;
  }
}


/**
   Regression runs always maintained by heap.
*/
void SplitCoord::RunsReg(RunSet *runSet,
			 const SampleRank spn[],
			 unsigned int rankDense) const {
  double sumHeap = 0.0;
  unsigned int sCountHeap = 0;
  unsigned int rkThis = spn[idxEnd].Rank();
  unsigned int frEnd = idxEnd;

  // Signing values avoids decrementing below zero.
  //
  for (int i = int(idxEnd); i >= int(idxStart); i--) {
    unsigned int rkRight = rkThis;
    unsigned int sampleCount;
    FltVal ySum;
    spn[i].RegFields(ySum, rkThis, sampleCount);

    if (rkThis == rkRight) { // Same run:  counters accumulate.
      sumHeap += ySum;
      sCountHeap += sampleCount;
    }
    else { // New run:  flush accumulated counters and reset.
      runSet->Write(rkRight, sCountHeap, sumHeap, frEnd - i, i+1);

      sumHeap = ySum;
      sCountHeap = sampleCount;
      frEnd = i;
    }
  }
  
  // Flushes the remaining run.  Also flushes the implicit run, if dense.
  //
  runSet->Write(rkThis, sCountHeap, sumHeap, frEnd - idxStart + 1, idxStart);
  if (implicit > 0) {
    runSet->WriteImplicit(rankDense, sCount, sum, implicit);
  }
}


/**
   @brief Splits runs sorted by binary heap.

   @param runSet contains all run parameters.

   @param outputs computed split parameters.

   @return true iff node splits.
*/
bool SplitCoord::HeapSplit(RunSet *runSet,
			   NuxLH &nux) const {
  unsigned int lhSCount = 0;
  double sumL = 0.0;
  int cut = -1; // Top index of lh ords in 'facOrd' (q.v.).
  double maxGini = preBias;
  for (unsigned int outSlot = 0; outSlot < runSet->RunCount() - 1; outSlot++) {
    unsigned int sCountRun;
    sumL += runSet->SumHeap(outSlot, sCountRun);
    lhSCount += sCountRun;
    unsigned int sCountR = sCount - lhSCount;
    double sumR = sum - sumL;
    double cutGini = (sumL * sumL) / lhSCount + (sumR * sumR) / sCountR;
    if (cutGini > maxGini) {
      maxGini = cutGini;
      cut = outSlot;
    }
  }

  if (cut >= 0) {
    unsigned int lhIdxCount = runSet->LHSlots(cut, lhSCount);
    nux.Init(idxStart, lhIdxCount, lhSCount, maxGini - preBias);
    return true;
  }
  else {
    return false;
  }
}


/**
   @brief Builds categorical runs.  Very similar to regression case, but the runs
   also resolve response sum by category.  Further, heap is optional, passed only
   when run count has been estimated to be wide:

*/
void SplitCoord::RunsCtg(const SPCtg *spCtg,
			 RunSet *runSet,
			 const SampleRank spn[]) const {
  double sumLoc = 0.0;
  unsigned int sCountLoc = 0;
  unsigned int rkThis = spn[idxEnd].Rank();

  // Signing values avoids decrementing below zero.
  unsigned int frEnd = idxEnd;
  for (int i = int(idxEnd); i >= int(idxStart); i--) {
    unsigned int rkRight = rkThis;
    unsigned int yCtg;
    FltVal ySum;
    unsigned int sampleCount = spn[i].CtgFields(ySum, rkThis, yCtg);

    if (rkThis == rkRight) { // Current run's counters accumulate.
      sumLoc += ySum;
      sCountLoc += sampleCount;
    }
    else { // Flushes current run and resets counters for next run.
      runSet->Write(rkRight, sCountLoc, sumLoc, frEnd - i, i + 1);

      sumLoc = ySum;
      sCountLoc = sampleCount;
      frEnd = i;
    }
    runSet->AccumCtg(yCtg, ySum);
  }

  
  // Flushes remaining run.
  runSet->Write(rkThis, sCountLoc, sumLoc, frEnd - idxStart + 1, idxStart);
  if (implicit > 0) {
    runSet->WriteImplicit(spCtg->DenseRank(predIdx), sCount, sum, implicit, spCtg->ColumnSums(splitIdx));
  }
}


/**
   @brief Splits blocks of categorical runs.

   @param sum is the sum of response values for this index node.

   @param maxGini outputs the highest observed Gini value.
   
   @param lhSampCt outputs LHS sample count.

   @return index count of LHS, with output reference parameters.

   Nodes are now represented compactly as a collection of runs.
   For each node, subsets of these collections are examined, looking for the
   Gini argmax beginning from the pre-bias.

   Iterates over nontrivial subsets, coded by integers as bit patterns.  By
   convention, the final run is incorporated into the RHS of the split, if any.
   Excluding the final run, then, the number of candidate LHS subsets is
   '2^(runCount-1) - 1'.
*/
bool SplitCoord::SplitRuns(const SPCtg *spCtg,
			   RunSet *runSet,
			   NuxLH &nux) {
  unsigned int countEff = runSet->DeWide();

  unsigned int slotSup = countEff - 1; // Uses post-shrink value.
  unsigned int lhBits = 0;
  unsigned int leftFull = (1 << slotSup) - 1;
  double maxGini = preBias;
  // Nonempty subsets as binary-encoded integers:
  for (unsigned int subset = 1; subset <= leftFull; subset++) {
    double sumL = 0.0;
    double ssL = 0.0;
    double ssR = 0.0;
    for (unsigned int yCtg = 0; yCtg < spCtg->CtgWidth(); yCtg++) {
      double sumCtg = 0.0; // Sum at this category over subset slots.
      for (unsigned int slot = 0; slot < slotSup; slot++) {
	if ((subset & (1 << slot)) != 0) {
	  sumCtg += runSet->SumCtg(slot, yCtg);
	}
      }
      double totSum = spCtg->CtgSum(splitIdx, yCtg); // Sum at this category over node.
      sumL += sumCtg;
      ssL += sumCtg * sumCtg;
      ssR += (totSum - sumCtg) * (totSum - sumCtg);
    }
    double sumR = sum - sumL;
    // Only relevant for case weighting:  otherwise sums are >= 1.
    if (spCtg->StableSums(sumL, sumR)) {
      double subsetGini = ssR / sumR + ssL / sumL;
      if (subsetGini > maxGini) {
        maxGini = subsetGini;
        lhBits = subset;
      }
    }
  }

  if (lhBits > 0) {
    unsigned int lhSampCt;
    unsigned int lhIdxCount = runSet->LHBits(lhBits, lhSampCt);
    nux.Init(idxStart, lhIdxCount, lhSampCt, maxGini - preBias);
    return true;
  }
  else {
    return false;
  }
}


/**
   @brief Adapated from SplitRuns().  Specialized for two-category case in
   which LH subsets accumulate.  This permits running LH 0/1 sums to be
   maintained, as opposed to recomputed, as the LH set grows.

   @return true iff the node splits.
 */
bool SplitCoord::SplitBinary(const SPCtg *spCtg,
			     RunSet *runSet,
			     NuxLH &nux) {
  runSet->HeapBinary();
  runSet->DePop();

  double maxGini = preBias;
  double totR0 = spCtg->CtgSum(splitIdx, 0); // Sum at this category over node.
  double totR1 = spCtg->CtgSum(splitIdx, 1);
  double sumL0 = 0.0; // Running sum at category 0 over subset slots.
  double sumL1 = 0.0; // "" 1 " 
  int cut = -1;
  for (unsigned int outSlot = 0; outSlot < runSet->RunCount() - 1; outSlot++) {
    double cell0, cell1;
    bool splitable = runSet->SumBinary(outSlot, cell0, cell1);

    sumL0 += cell0;
    sumL1 += cell1;

    FltVal sumL = sumL0 + sumL1;
    FltVal sumR = sum - sumL;
    // sumR, sumL magnitudes can be ignored if no large case/class weightings.
    if (splitable && spCtg->StableDenoms(sumL, sumR)) {
      FltVal ssL = sumL0 * sumL0 + sumL1 * sumL1;
      FltVal ssR = (totR0 - sumL0) * (totR0 - sumL0) + (totR1 - sumL1) * (totR1 - sumL1);
      FltVal cutGini = ssR / sumR + ssL / sumL;
       if (cutGini > maxGini) {
        maxGini = cutGini;
        cut = outSlot;
      }
    } 
  }

  if (cut >= 0) {
    unsigned int sCountL;
    unsigned int lhIdxCount = runSet->LHSlots(cut, sCountL);
    nux.Init(idxStart, lhIdxCount, sCountL, maxGini - preBias);
    return true;
  }
  else {
    return false;
  }
}
