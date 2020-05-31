// This file is part of ArboristCore.

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
   @file runaccum.h

   @brief Definitions for the Run classes, which maintain predictor
   runs, especially factor-valued predictors.

   @author Mark Seligman
 */

#ifndef SPLIT_RUNACCUM_H
#define SPLIT_RUNACCUM_H

#include <vector>

#include "splitcoord.h"
#include "sumcount.h"
#include "accum.h"

/**
   @brief Accumulates statistics for runs of factors having the same internal code.

   Allocated in bulk by Fortran-style workspace, the RunSet.
 */
struct FRNode {
  static IndexT noStart; // Inattainable starting index.
  PredictorT code; // Same 0-based value as internal code.
  IndexT sCount; // Sample count of factor run:  need not equal length.
  double sum; // Sum of responses associated with run.
  IndexRange range;

  FRNode() : sCount(0), sum(0.0), range(IndexRange()) {
  }


  /**
     @brief Initializer.
   */
  inline void set(PredictorT code,
                   IndexT sCount,
                   double sum,
                   IndexT start,
                   IndexT extent) {
    this->code = code;
    this->sCount = sCount;
    this->sum = sum;
    this->range = IndexRange(start, extent);
  }

  
  /**
     @brief Range accessor.  N.B.:  Should not be invoked on dense
     run, as 'start' will hold a reserved value.

     @return range of indices subsumed by run.
   */
  inline IndexRange getRange() const {
    return range;
  }


  /**
     @brief Accumulates run contents into caller.
   */
  inline void accum(IndexT& sCount,
                    double& sum) const {
    sCount += this->sCount;
    sum += this->sum;
  }


  /**
     @brief Implicit runs are characterized by a start value of 'noStart'.

     @return Whether this run is dense.
  */
  bool isImplicit() const {
    return range.getStart() == noStart;
  }
};


/**
   @brief Ad hoc container for simple priority queue.
 */
struct BHPair {
  double key;  // Comparitor value.
  PredictorT slot; // Slot index.
};


enum class SplitStyle;


/**
   RunAccums live only during a single level, from argmax pass one (splitting)
   through argmax pass two.  They accumulate summary information for split/
   predictor pairs anticipated to have two or more distinct runs.  RunAccums
   are not yet built for numerical predictors, which have so far been
   generally assumed to have dispersive values.

   Run lengths for a given predictor decrease, although not necessarily
   monotonically, with splitting.  Hence once a pair becomes a singleton, the
   fact is worth preserving for the duration of training.  Numerical predictors
   are assigned a nonsensical run length of zero, which is changed to a sticky
   value of unity, should a singleton be identified.  Run lengths are
   transmitted between levels duing restaging, which is the only phase to
   maintain a map between split nodes and their descendants.  Similarly, new
   singletons are very easy to identify when updating the partition.

   Other than the "bottom" value of unity, run lengths can generally only be
   known precisely by first walking the predictor codes.  Hence a conservative
   value is used for storage allocation.
*/
class RunAccum : public Accum {
protected:
  const PredictorT rcSafe; // Conservative run count.
  vector<FRNode> runZero; // SR block, partitioned by code.
  vector<BHPair> heapZero; // Sorting workspace.
  vector<PredictorT> idxRank; // Slot rank, according to ad-hoc ordering.
  vector<double> cellSum; // Categorical:  run x ctg checkerboard.
  double* rvZero; // Non-binary wide runs:  random variates for sampling.

  PredictorT implicitSlot; // Which run, if any has no explicit SR range.
  PredictorT runCount;  // Current high watermark.
  PredictorT runsLH; // Count of LH runs.
  PredictorT splitToken; // Cut or bits.
  IndexT implicitTrue; // # implicit true-sense indices:  post-encoding.

  // Temporal values read from SampleRank:
  IndexT codeSR;
  IndexT sCountSR;
  FltVal ySumSR;
  PredictorT ctgSR;
  
  /**
     @brief Sets run parameters and increments run count.
   */
  inline void append(PredictorT code,
		     IndexT sCount,
		     double sum,
		     IndexT start,
		     IndexT extent) {
    runZero[runCount++].set(code, sCount, sum, start, extent);
  }


  /**
     @brief As above, with implicit rank and extent suppled by the nux.
   */
  void append(const class SplitNux* cand,
	      IndexT sCount,
	      double sum);

  /**
     @brief Appends a run for the dense rank using residual values.

     @param cand is the splitting candidate with potential implicit state.

     @param sp is the frontier splitting environment.

     @param ctgSum is the per-category response over the frontier node.
  */
  void appendImplicit(const class SplitNux* cand,
		      const vector<double>& ctgSum = vector<double>(0));


  /**
     @brief Looks up run parameters by indirection through output vector.
     
     N.B.:  should not be called with a dense run.

     @return index range associated with run.
  */
  IndexRange getBounds(PredictorT slot) const {
    return runZero[slot].getRange();
  }


  /**
     @brief Sorts by random variate ut effect sampling w/o replacement.
   */
  void heapRandom();

  
  /**
     @brief Sorts by probability, binary response.
   */
  void heapBinary();

  
  /**
     @brief Sorts by mean response.
   */
  void heapMean();

public:
  static constexpr unsigned int maxWidth = 10; // Algorithmic threshold.

  
  RunAccum(const class SplitFrontier* splitFrontier,
	   const class SplitNux* cand,
	   PredictorT nCtg,
	   SplitStyle style,
	   PredictorT rcSafe_);

  
  /**
     @brief Counts the number of wide level extents.

     @return level extent iff beyond the threshold else zero.
   */
  IndexT countWide() const {
    return rcSafe > maxWidth ? rcSafe : 0;
  }


  /**
     @brief Determines whether it is necessary to expose the right-hand runs.

     Suitable for multi-criterion splits, for which there may be more than
     one implicit slot.
  */
  void implicitLeft();


  /**
     @brief Computes extent of left-implicit runs.
     
     @param lhBits is a bit representation of the LH slots.

     @return extent of implicit slot iff encoded in the LH else zero.
   */
  IndexT getImplicitLeftBits(PredictorT lhBits) {
    return implicitSlot < rcSafe && (lhBits & (1ul << implicitSlot)) ? getExtent(implicitSlot) : 0;
  }

  
  /**
     @return extent of implicit slot iff it lies left else zero.

     @param cut is the greatest LH slot index.
   */
  IndexT getImplicitLeftSlots(PredictorT cut) const {
    return implicitSlot <= cut ? getExtent(implicitSlot) : 0;
  }
  

  /**
     @brief Overwrites leading slots with sampled subset of runs.
  */
  void deWide(PredictorT nCtg);


  /**
     @brief Reorders the per-category response decomposition to compensate for run reordering.

     @param leadCount is the number of leading runs reordered.
   */
  void ctgReorder(PredictorT leadCount,
		  PredictorT nCtg);


  /**
     @brief Determines split having highest weighted variance.

     Runs initially sorted by mean response.
   */
  void maxVar();
  
  
  /**
     @brief Gini-based splitting for categorical response and predictor.

     Nodes are now represented compactly as a collection of runs.
     For each node, subsets of these collections are examined, looking for the
     Gini argmax beginning from the pre-bias.

     Iterates over nontrivial subsets, coded by unsigneds as bit patterns.  By
     convention, the final run is incorporated into RHS of the split, if any.
     Excluding the final run, then, the number of candidate LHS subsets is
     '2^(runCount-1) - 1'.

     @param ctgSum is the per-category sum of responses.
  */
  void ctgGini(const vector<double>& ctgSum);

  
  /**
     @brief As above, but specialized for binary response.
   */
  void binaryGini(const vector<double>& ctgSum);

  
  /**
     @brief Depopulates the heap associated with a pair and places sorted ranks into rank vector.

     @param pop is the number of elements to pop from the heap.
  */
  void slotReorder(PredictorT pop = 0);


  /**
     @brief Revises slot or bit contents for argmax accumulator.
   */
  void update(SplitStyle style);

  
  /**
     @brief Updates local vector bases with their respective offsets,
     addresses, now known.

     @param[in, out] rvOff accumulates wide level counts.
  */
  void reWide(vector<double>& rvWide,
	      IndexT& rvOff);

  
  /**
     @brief Accumulates runs for regression.
   */
  void regRuns(const class SplitNux* cand);

  
  /**
     @brief As above, but skips masked SR indices.
   */  
  void regRunsMasked(const class SplitNux* cand,
		     const class BranchSense* branchSense,
		     IndexT edgeRight,
		     IndexT edgeLeft);

  
  /**
     @brief Accumulates runs for classification.

     @param sumSlice is the per-category response decomposition.
  */
  void ctgRuns(const class SplitNux* cand,
	       const vector<double>& sumSlice);

  
  /**
     @brief Writes to heap arbitrarily:  sampling w/o replacement.

     @param leadCount is the number of leading elments to reorder.
  */
  void orderRandom(PredictorT leadCount);


  /**
     @brief Writes to heap, weighting by slot mean response.
  */
  void orderMean();


  /**
     @brief Writes to heap, weighting by category-1 probability.
  */
  void orderBinary();


  /**
     @brief Sets splitting token for possible elaboration.
   */
  inline void setToken(PredictorT token) {
    splitToken = token;
  }
  

  /**
     @brief Getter for runCount.

     @return run count.
   */
  inline auto getRunCount() const {
    return runCount;
  }


  inline auto getImplicitTrue() const {
    return implicitTrue;
  }
  

  /**
     @return sCount value of input slot.
   */
  inline auto getInputSCount(PredictorT slot) const {
    return runZero[slot].sCount;
  }


  /**
     @return sum value of input slot.
   */
  inline auto getInputSum(PredictorT slot) const {
    return runZero[slot].sum;
  }
  

  inline void resetRunCount(PredictorT runCount) {
    this->runCount = runCount;
  }
  

  /**
     @brief Safe-count getter.
   */
  inline auto getSafeCount() const {
    return rcSafe;
  }
  
  
  /**
     @brief Computes "effective" run count, for sample-based splitting.

     @return lesser of 'runCount' and 'maxWidth'.
   */
  inline PredictorT effCount() const {
    return runCount > maxWidth ? maxWidth : runCount;
  }


  /**
     @brief Accumulates contents at position referenced by a given index.

     @param slot is a run index.

     @param sCount[in, out] accumulates sample count using the output position.

     @param sum[in, out] accumulates the sum using the output position.
   */
  inline void sumAccum(PredictorT slot, IndexT& sCount, double& sum) const {
    runZero[slot].accum(sCount, sum);
  }


  /**
     @brief Resets top index and contents, if applicable.
     
     @param runStart is the previous top position.

     @param runIdx is the index from which to copy the top position.
   */
  inline void reset(PredictorT runStart, PredictorT runIdx) {
    if (runIdx != runCount) {
      runZero[runStart] = runZero[runIdx]; // New top value.
      runCount = runStart + 1;
    }
    else { // No new top, run-count restored.
      runCount = runStart;
    }
  }


  /**
     @brief Accumulates checkerboard values prior to writing next run.
   */
  inline void ctgAccum(PredictorT nCtg,
                       double ySum,
                       PredictorT yCtg) {
    cellSum[runCount * nCtg + yCtg] += ySum;
  }


  /**
     @return checkerboard value at slot for category.
   */
  inline double getCellSum(PredictorT runIdx,
			  PredictorT nCtg,
			  PredictorT yCtg) const {
    return cellSum[runIdx * nCtg + yCtg];
  }


  /**
     @brief Initializes category sums of the next run.

     @param nodeSum is the per-category response over the node (IndexSet).
   */
  inline void initCtg(const vector<double>& sumSlice) {
    double* ctgBase = &cellSum[runCount * sumSlice.size()];
    for (auto val : sumSlice) {
      *ctgBase++ = val;
    }
  }


  /**
     @brief Subtracts a run's per-category responses from the current run.

     @param nCtg is the response cardinality.

     @param runIdx is the run index.
   */
  inline void residCtg(PredictorT nCtg,
		       PredictorT runIdx) {
    double* ctgBase = &cellSum[runCount * nCtg];
    for (PredictorT ctg = 0; ctg < nCtg; ctg++) {
      ctgBase[ctg] -= cellSum[runIdx * nCtg + ctg];
    }
  }
  

  /**
     @brief Accumulates the two binary response sums associated with a slot.

     @param slot is a run index.

     @param[in, out] sum0 accumulates the response at rank 0.

     @param[in, out] sum1 accumulates the response at rank 1.

     @return true iff slot counts differ by at least unity.
   */
  inline bool accumBinary(PredictorT slot,
			  double& sum0,
			  double& sum1) {
    double cell0 = getCellSum(slot, 2, 0);
    sum0 += cell0;
    double cell1 = getCellSum(slot, 2, 1);
    sum1 += cell1;

    IndexT sCount = runZero[slot].sCount;
    PredictorT slotNext = slot+1;
    // Cannot test for floating point equality.  If sCount values are unequal,
    // then assumes the two slots are significantly different.  If identical,
    // then checks whether the response values are likely different, given
    // some jittering.
    // TODO:  replace constant with value obtained from class weighting.
    return sCount != runZero[slotNext].sCount ? true : getCellSum(slotNext, 2, 1) - cell1 > 0.9;
  }


  /**
    @brief Outputs sample and index counts at a given slot.

    @param slot is the run slot in question.

    @return total SR index count subsumed.
  */
  inline IndexT getExtent(PredictorT slot) const {
    return runZero[slot].range.getExtent();
  }


  /**
     @return level code at specified slot.
   */
  auto getCode(PredictorT slot) const {
    return runZero[slot].code;
  }

  
  auto getSum(PredictorT slot) const {
    return runZero[slot].sum;
  }

  
  auto getSCount(PredictorT slot) const {
    return runZero[slot].sCount;
  }

  
  /**
     @brief Decodes bit vector of argmax factor.

     @param lhBits encodes sampled LH/RH slot indices as on/off bits, respectively.
  */
  void leadBits(PredictorT lhBits);


  double subsetGini(const vector<double>& sumSlice,
		    unsigned int subset) const;


  
  /**
     @brief Emits the left-most codes as true-branch bit positions.

     True codes are enumerated from the left, regardless whether implicits present.
     Implicit codes affect sample-index (SR) update, but not final branch encoding.
     
     @return vector of indices corresponding to true-branch bits.
   */
  vector<PredictorT> getTrueBits() const;

  
  /**
     @brief Establishes cut position of argmax factor.

     @param cut is the final out slot of the LHS.
  */
  void leadSlots(PredictorT cut);


  /**
     @brief Appends a single slot to the lh set.
   */
  void topSlot();


  /**
     @return implicit count associated with a slot.
   */
  IndexT getImplicitExtent(PredictorT slot) const {
    return runZero[slot].isImplicit() ? getExtent(slot) : 0;
  }

  
  /**
     @return vector of block ranges associated with encoding.
   */
  vector<IndexRange> getRange(const class CritEncoding& enc) const;

  
  /**
     @return top-most block range associated with encoding.
   */
  IndexRange getTopRange(const class CritEncoding& enc) const;


  struct RunDump dump() const;
};


/**
   @brief Accumulates diagnostic statistics over the run vector.
 */
struct RunDump {
  IndexT sCount;
  double sum;
  vector<PredictorT> code;

  /**
     @brief Populates dump over specified vector range.
   */
  RunDump(const RunAccum* runAccum,
	  PredictorT runStart,
	  PredictorT runCount) : sCount(0), sum(0.0), code(vector<PredictorT>(runCount)) {
    for (PredictorT rc = runStart; rc < runStart + runCount; rc++) {
      code[rc] = runAccum->getCode(rc);
      sCount += runAccum->getSCount(rc);
      sum += runAccum->getSum(rc);
    }
  }
};

/**
   @brief Implementation of binary heap tailored to RunAccums.

   Not so much a class as a collection of static methods.
   TODO:  Templatize and move elsewhere.
*/
struct BHeap {
  /**
     @brief Determines index of parent.
   */
  static inline int parent(int idx) { 
    return (idx-1) >> 1;
  };


  /**
     @brief Empties the queue.

     @param pairVec are the queue records.

     @param[out] lhOut outputs the popped slots, in increasing order.

     @param pop is the number of elements to pop.  Caller enforces value > 0.
  */
  static void depopulate(BHPair pairVec[],
                         unsigned int lhOut[],
                         unsigned int pop);

  /**
     @brief Inserts a key, value pair into the heap at next vacant slot.

     Heap updates to move element with maximal key to the top.

     @param pairVec are the queue records.

     @param slot_ is the slot position.

     @param key_ is the associated key.
  */
  static void insert(BHPair pairVec[],
                     unsigned int slot_,
                     double key_);

  /**
     @brief Pops value at bottom of heap.

     @param pairVec are the queue records.

     @param bot indexes the current bottom.

     @return popped value.
  */
  static unsigned int slotPop(BHPair pairVec[],
                              int bot);
};


#endif
