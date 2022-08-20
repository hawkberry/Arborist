/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
   @file accum.cc

   @brief Methods implementing a generic split accumulator.

   @author Mark Seligman

 */

#include "accum.h"
#include "splitfrontier.h"
#include "splitnux.h"
#include "branchsense.h"


Accum::Accum(const SplitFrontier* splitFrontier,
	     const SplitNux& cand) :
  obsCell(splitFrontier->getPredBase(cand)),
  sampleIndex(splitFrontier->getIdxBuffer(cand)),
  obsStart(cand.getObsStart()),
  obsEnd(cand.getObsEnd() - cand.getNMissing()),
  sumCount(filterMissing(cand)),
  cutResidual(obsStart + cand.getPreresidual()),
  implicitCand(cand.getImplicitCount()),
  sum(sumCount.sum),
  sCount(sumCount.sCount) {
}


SumCount Accum::filterMissing(const SplitNux& cand) const {
  double sumCand = cand.getSum();
  IndexT sCountCand = cand.getSCount();
  for (IndexT obsIdx = obsEnd; obsIdx != obsEnd + cand.getNMissing(); obsIdx++) {
    Obs obs = obsCell[obsIdx];
    sumCand -= obs.getYSum();
    sCountCand -= obs.getSCount();
  }

  // Regression:  info = (sumCand * sumCand) / sCountCand; 
  // Ctg: info = sumSquaresCand / sumCand  
  return SumCount(sumCand, sCountCand);
}


void Accum::filterMissingCtg(const SplitNux& cand,
			     double& ssL,
			     vector<double>& ctgSum) const {
  for (IndexT obsIdx = obsEnd; obsIdx != obsEnd + cand.getNMissing(); obsIdx++) {
    const Obs& obs = obsCell[obsIdx];
    PredictorT ctg = obs.getCtg();
    double ySum = obs.getYSum();
    ssL -= ySum * ySum;
    ctgSum[ctg] -= ySum;
  }
}


bool Accum::findEdge(const BranchSense* branchSense,
		     bool leftward,
		     IndexT idxTerm,
		     bool sense,
		     IndexT& edge) const {
  // Breaks out and returns true iff matching-sense sample found.
  if (leftward) { // Decrement to start.
    for (edge = idxTerm; edge > obsStart; edge--) {
      if (branchSense->isExplicit(sampleIndex[edge]) == sense) {
	return true;
      }
    }
    if (branchSense->isExplicit(sampleIndex[edge]) == sense) {
      return true;
    }
  }
  else { // Increment to end.
    for (edge = idxTerm; edge != obsEnd; edge++) {
      if (branchSense->isExplicit(sampleIndex[edge]) == sense) {
	return true;
      }
    }
  }

  return false; // No match.
}


