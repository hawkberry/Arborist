// This file is part of ArboristCore.

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
   @file runset.h

   @brief Definitions for the Run classes, which maintain predictor
   runs, especially factor-valued predictors.

   @author Mark Seligman

 */

// FRNodes hold field values accumulated from runs of factors having the
// same value.  That is, they group factor-valued predictors into block
// representations. These values live for a single level, so must be consumed
// before a new level is started.
//
#ifndef ARBORIST_RUNSET_H
#define ARBORIST_RUNSET_H

#include <vector>

#include "typeparam.h"

/**
 */
class FRNode {
 public:
  unsigned int rank;
  unsigned int start; // Buffer position of start of factor run.
  unsigned int extent; // Total indices subsumed.
  unsigned int sCount; // Sample count of factor run:  not always same as length.
  double sum; // Sum of responses associated with run.

  FRNode() : start(0), extent(0), sCount(0), sum(0.0) {}

  bool isImplicit();

  
  inline void init(unsigned int _rank,
                   unsigned int _sCount,
                   double _sum,
                   unsigned int _start,
                   unsigned int _extent) {
    rank = _rank;
    sCount = _sCount;
    sum = _sum;
    start = _start;
    extent = _extent;
  }

  
  /**
     @brief Replay accessor.  N.B.:  Should not be invoked on dense
     run, as 'start' will hold a reserved value.

     @param _extent outputs the count of indices subsumed.

     @return void.
   */
  inline void replayRef(unsigned int &_start, unsigned int &_extent) {
    _start = start;
    _extent = extent;
  }


  /**
     @brief Rank accessor.

     @return rank.
   */
  inline unsigned int getRank() const {
    return rank;
  }
};


class BHPair {
 public:
  double key; unsigned int slot;
};

/**
  @brief  Runs only:  caches pre-computed workspace starting indices to
  economize on address recomputation during splitting.
*/
class RunSet {
  bool hasImplicit; // Whether dense run present.
  int runOff; // Temporary offset storage.
  int heapOff; //
  int outOff; //
  FRNode *runZero; // Base for this run set.
  BHPair *heapZero; // Heap workspace.
  unsigned int *outZero; // Final LH and/or output for heap-ordered slots.
  double *ctgZero; // Categorical:  run x ctg checkerboard.
  double *rvZero; // Non-binary wide runs:  random variates for sampling.
  unsigned int runCount;  // Current high watermark:  not subject to shrinking.
  unsigned int runsLH; // Count of LH runs.
 public:
  const static unsigned int maxWidth = 10;
  static unsigned int ctgWidth;
  static unsigned int noStart;
  unsigned int safeRunCount;

  RunSet() : hasImplicit(false), runOff(0), heapOff(0), outOff(0), runZero(0), heapZero(0), outZero(0), ctgZero(0), rvZero(0), runCount(0), runsLH(0), safeRunCount(0) {}

  bool implicitLeft() const;
  void writeImplicit(unsigned int denseRank, unsigned int sCountTot, double sumTot, unsigned int denseCount, const double nodeSum[] = 0);
  unsigned int deWide();
  void dePop(unsigned int pop = 0);

  /**
     @brief Updates local vector bases with their respective offsets,
     addresses, now known.
  */
  void reBase(vector<FRNode> &facRun,
              vector<BHPair> &bHeap,
              vector<unsigned int> &lhOut,
              vector<double> &ctgSum,
              vector<double> &rvWide);

  void offsetCache(unsigned int _runOff, unsigned int _heapOff, unsigned int _outOff);
  void heapRandom();
  void heapMean();
  void heapBinary();


  /**
     @brief Accessor for runCount field.

     @return reference to run count.
   */
  inline unsigned int getRunCount() const {
    return runCount;
  }


  inline void setRunCount(unsigned int runCount) {
    this->runCount = runCount;
  }
  
  
  inline unsigned int getSafeCount() const {
    return safeRunCount;
  }
  
  
  /**
     @brief "Effective" run count, for the sake of splitting, is the lesser
     of the true run count and 'maxWidth'.

     @return effective run count
   */
  inline unsigned int effCount() const {
    return runCount > maxWidth ? maxWidth : runCount;
  }


  /**
     @brief Looks up sum and sample count associated with a given output slot.

     @param outPos is a position in the output vector.

     @param sCount outputs the sample count at the dereferenced output slot.

     @return sum at dereferenced position, with output reference parameter.
   */
  inline double sumHeap(unsigned int outPos, unsigned int &sCount) {
    unsigned int slot = outZero[outPos];
    sCount = runZero[slot].sCount;
    
    return runZero[slot].sum;
  }

  /**
     @brief Sets run parameters and increments run count.

     @return void.
   */
  inline void write(unsigned int rank, unsigned int sCount, double sum, unsigned int extent, unsigned int start = noStart) {
    runZero[runCount++].init(rank, sCount, sum, start, extent);
    hasImplicit = (start == noStart ? true : false);
  }


  /**
     @return checkerboard value at slot for category.
   */
  inline double getSumCtg(unsigned int slot, unsigned int yCtg) const {
    return ctgZero[slot * ctgWidth + yCtg];
  }


  /**
     @brief Accumulates checkerboard values prior to writing topmost
     run.

     @return void.
   */
  inline void accumCtg(unsigned int yCtg, double ySum) {
    ctgZero[runCount * ctgWidth + yCtg] += ySum;
  }


  inline void setSumCtg(unsigned int yCtg, double ySum) {
    ctgZero[runCount * ctgWidth + yCtg] = ySum;
  }


  /**
     @brief Looks up the two binary response sums associated with a given
     output slot.

     @param outPos is a position in the output vector.

     @param[in, out] sum0 accumulates the response at rank 0.

     @param[in, out] sum1 accumulates the response at rank 1.

     @return true iff slot counts differ by at least unity.
   */
  inline bool accumBinary(unsigned int outPos, double &sum0, double &sum1) {
    unsigned int slot = outZero[outPos];
    double cell0 = getSumCtg(slot, 0);
    sum0 += cell0;
    double cell1 = getSumCtg(slot, 1);
    sum1 += cell1;

    unsigned int sCount = runZero[slot].sCount;
    unsigned int slotNext = outZero[outPos+1];
    // Cannot test for floating point equality.  If sCount values are unequal,
    // then assumes the two slots are significantly different.  If identical,
    // then checks whether the response values are likely different, given
    // some jittering.
    // TODO:  replace constant with value obtained from class weighting.
    return sCount != runZero[slotNext].sCount ? true : getSumCtg(slotNext, 1) - cell1 > 0.9;
  }


  /**
    @brief Outputs sample and index counts at a given slot.

    @param liveIdx is a cached offset for the pair.

    @param pos is the position to dereference in the rank vector.

    @param count outputs the sample count.

    @return total index count subsumed, with reference accumulator.
  */
  inline unsigned int lHCounts(unsigned int slot, unsigned int &sCount) const {
    FRNode *fRun = &runZero[slot];
    sCount = fRun->sCount;
    return  fRun->extent;
  }


  inline unsigned int getRunsLH() const {
    return runsLH;
  }

  unsigned int getRank(unsigned int outSlot) const;
  unsigned int lHBits(unsigned int lhBits, unsigned int &lhSampCt);
  unsigned int lHSlots(unsigned int outPos, unsigned int &lhSampCt);
  void bounds(unsigned int outSlot, unsigned int &start, unsigned int &extent) const;
};


class Run {
  const unsigned int noRun;  // Inattainable run index for tree.
  unsigned int setCount;
  vector<RunSet> runSet;
  vector<FRNode> facRun; // Workspace for FRNodes used along level.
  vector<BHPair> bHeap;
  vector<unsigned int> lhOut; // Vector of lh-bound slot indices.
  vector<double> ctgSum;
  vector<double> rvWide;

  void reBase();
  void runSets(const vector<unsigned int>& safeCount);


  inline unsigned int getRunCount(unsigned int rsIdx) const {
    return runSet[rsIdx].getRunCount();
  }

  inline unsigned int getRank(unsigned int idx, unsigned int outSlot) const {
    return runSet[idx].getRank(outSlot);
  }

  inline void runBounds(unsigned int idx, unsigned int outSlot, unsigned int &start, unsigned int &extent) const {
    runSet[idx].bounds(outSlot, start, extent);
  }

  inline unsigned int getRunsLH(unsigned int rsIdx) const {
    return runSet[rsIdx].getRunsLH();
  }


 public:
  const unsigned int ctgWidth;
  Run(unsigned int _ctgWidth, unsigned int nRow, unsigned int noCand);
  void levelClear();
  void offsetsReg(const vector<unsigned int> &safeCount);
  void offsetsCtg(const vector<unsigned int> &safeCount);
  bool isRun(const class SplitCand& cand) const;

  bool replay(const class SplitCand& cand,
              class IndexSet* iSet,
              class PreTree* preTree,
              const class IndexLevel* index) const;
  
  inline bool isRun(unsigned int setIdx) const {
    return setIdx != noRun;
  }


  inline unsigned int getNoRun() const {
    return noRun;
  }


  inline RunSet *rSet(unsigned int rsIdx) {
    return &runSet[rsIdx];
  }

  
  /**
     @brief Presets runCount field to a conservative value for
     the purpose of allocating storage.
   */
  unsigned int getSafeCount(unsigned int idx) const {
    return runSet[idx].safeRunCount;
  }

  
  void countSafe(unsigned int idx, unsigned int count) {
    runSet[idx].safeRunCount = count;
  }


};


/**
   @brief Implementation of binary heap tailored to RunSets.  Not
   so much a class as a collection of static methods.

   TODO:  Templatize and move elsewhere.
*/
struct BHeap {
 public:
  static inline int parent(int idx) { 
    return (idx-1) >> 1;
  };


  static void depopulate(BHPair pairVec[],
                         unsigned int lhOut[],
                         unsigned int pop);

  static void insert(BHPair pairVec[],
                     unsigned int slot_,
                     double key_);

  static unsigned int slotPop(BHPair pairVec[],
                              int bot);
};

#endif
