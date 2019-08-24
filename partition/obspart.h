// This file is part of ArboristCore.

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
   @file obspart.h

   @brief Observation frame, partitioned by tree node.

   @author Mark Seligman
 */

#ifndef PARTITION_OBSPART_H
#define PARTITION_OBSPART_H


#include "splitcoord.h"
#include "typeparam.h"

#include <vector>

#include "samplenux.h" // Temporary


/**
   @brief Summarizes staging operation.
 */
class StageCount {
 public:
  unsigned int expl;
  bool singleton;
};


/**
 @brief Contains the sample data used by predictor-specific sample-walking pass.
*/
class ObsPart {
  const unsigned int nPred;
  // ObsPart appear in predictor order, grouped by node.  They store the
  // y-value, run class and sample index for the predictor position to which they
  // correspond.


  // Predictor-based sample orderings, double-buffered by level value.
  //
  const IndexT bagCount;
  const IndexT bufferSize; // <= nRow * nPred.

  vector<PathT> pathIdx;
  vector<unsigned int> stageOffset;
  vector<unsigned int> stageExtent; // Client:  debugging only.
  SampleRank* nodeVec;

  // 'indexBase' could be boxed with SampleRank.  While it is used in both
  // replaying and restaging, though, it plays no role in splitting.  Maintaining
  // a separate vector permits a 16-byte stride to be used for splitting.  More
  // significantly, it reduces memory traffic incurred by transposition on the
  // coprocessor.
  //
  IndexT* indexBase; // RV index for this row.  Used by CTG as well as on replay.

 protected:
  unsigned int *destRestage; // Coprocessor restaging.
  unsigned int *destSplit; // Coprocessor restaging.

  
 public:
  ObsPart(const class SummaryFrame* frame, IndexT bagCount_);
  virtual ~ObsPart();


  /**
     @brief Sets staging boundaries for a given predictor.
  */
  void setStageBounds(const class RankedFrame* rankedFrame,
                      unsigned int predIdx);


  /**
     @brief Loops through the predictors to stage.
  */
  vector<StageCount> stage(const class RankedFrame* rankedFrame,
                           const vector<SampleNux> &sampleNode,
                           const class Sample* sample);

  /**
     @brief Stages ObsPart objects in non-decreasing predictor order.

     @param predIdx is the predictor index.
  */
  void stage(const class RankedFrame* rankedFrame,
             const vector<SampleNux> &sampleNode,
             const class Sample* sample,
             unsigned int predIdx,
             StageCount& stageCount);

  void stage(const class RankedFrame* rankedFrame,
             unsigned int rrTot,
             const vector<class SampleNux> &sampleNode,
             const class Sample* sample,
             vector<class StageCount> &stageCount);

  /**
     @brief Fills in sampled response summary and rank information associated
     with an RowRank reference.

     @param rowRank summarizes an element of the compressed design matrix.

     @param spn is the cell to initialize.

     @param smpIdx is the associated sample index.

     @param expl accumulates the current explicitly staged offset.
 */
  void stage(const vector<class SampleNux> &sampleNode,
             const class RowRank &rowRank,
             const class Sample *sample,
             unsigned int &expl,
             SampleRank spn[],
             unsigned int smpIdx[]) const;


  /**
   @brief Looks up SampleRank block and dispatches appropriate replay method.

   @param blockStart is the starting SampleRank index for the split.

   @param blockExtent is the number of explicit such indices subsumed.

   @param replayExpl sets bits corresponding to explicit indices defined
   by the split.  Indices are either node- or subtree-relative, depending
   on Bottom's current indexing mode.

   @param ctgExpl summarizes explicit sum and sample count by category.

   @return sum of explicit responses within the block.
  */
  double blockReplay(const class SplitFrontier* splitFrontier,
                     class IndexSet* iSet,
                     const IndexRange& range,
                     bool leftExpl,
                     class BV *replayExpl,
                     class BV* replayLeft,
                     vector<SumCount>& ctgCrit);


  
  /**
     @brief Drives restaging from an ancestor node and level to current level.

     @param levelBack is the ancestor's level.

     @param levelFront is the current level.

     @param mrra is the ancestor.

     @param bufIdx is the buffer indes of the ancestor.
   */
  void restage(class Level *levelBack,
               class Level *levelFront,
               const SplitCoord &mrra,
               unsigned int bufIdx);
  
  /**
     @brief Localizes copies of the paths to each index position.

     Also localizes index positions themselves, if in a node-relative regime.

     @param reachBase is non-null iff index offsets enter as node relative.

     @param idxUpdate is true iff the index is to be updated.

     @param startIdx is the beginning index of the cell.

     @param extent is the count of indices in the cell.

     @param pathMask mask the relevant bits of the path value.

     @param idxVec inputs the index offsets, relative either to the
     current subtree or the containing node and may output an updated
     value.

     @param[out] prePath outputs the (masked) path reaching the current index.

     @param pathCount enumerates the number of times a path is hit.  Only
     client is currently dense packing.
  */
  void prepath(const class IdxPath *idxPath,
               const unsigned int reachBase[],
               bool idxUpdate,
               const IndexRange& idxRange,
               unsigned int pathMask,
               unsigned int idxVec[],
               PathT prepath[],
               unsigned int pathCount[]) const;

  /**
     @brief Pass-through to Path method.

     Looks up reaching cell in appropriate buffer.
     Parameters as above.
  */
  void prepath(const class IdxPath *idxPath,
               const unsigned int reachBase[],
               unsigned int predIdx,
               unsigned int bufIdx,
               const IndexRange& idxRange,
               unsigned int pathMask,
               bool idxUpdate,
               unsigned int pathCount[]);

  /**
     @brief Restages and tabulates rank counts.
  */
  void rankRestage(unsigned int predIdx,
                   unsigned int bufIdx,
                   const IndexRange& idxRange,
                   unsigned int reachOffset[],
                   unsigned int rankPrev[],
                   unsigned int rankCount[]);

  void indexRestage(const class IdxPath *idxPath,
                    const unsigned int reachBase[],
                    const SplitCoord& mrra,
                    unsigned int bufIdx,
                    const IndexRange& idxRange,
                    unsigned int pathMask,
                    bool idxUpdate,
                    unsigned int reachOffset[],
                    unsigned int splitOffset[]);


  inline IndexT getBagCount() const {
    return bagCount;
  }


  /**
     @brief Returns the staging position for a dense predictor.
   */
  inline unsigned int getStageOffset(unsigned int predIdx) const {
    return stageOffset[predIdx];
  }


  // The category could, alternatively, be recorded in an object subclassed
  // under class ObsPart.  This would require that the value be restaged,
  // which happens for all predictors at all splits.  It would also require
  // that distinct ObsPart classes be maintained for SampleReg and
  // SampleCtg.  Recomputing the category value on demand, then, seems an
  // easier way to go.
  //

  /**
     @brief Toggles between positions in workspace double buffer, by level.

     @return workspace starting position for this level.
   */
  inline IndexT buffOffset(unsigned int bufferBit) const {
    return (bufferBit & 1) == 0 ? 0 : bufferSize;
  }

  /**

     @param predIdx is the predictor coordinate.

     @param level is the current level.

     @return starting position within workspace.
   */
  inline IndexT bufferOff(unsigned int predIdx, unsigned int bufBit) const {
    return stageOffset[predIdx] + buffOffset(bufBit);
  }


  /**
     @return base of the index buffer.
   */
  inline IndexT *bufferIndex(unsigned int predIdx, unsigned int bufBit) const {
    return indexBase + bufferOff(predIdx, bufBit);
  }


  /**
     @return base of node buffer.
   */
  inline SampleRank *bufferNode(unsigned int predIdx, unsigned int bufBit) const {
    return nodeVec + bufferOff(predIdx, bufBit);
  }
  
  
  /**
   */
  inline SampleRank* buffers(unsigned int predIdx, unsigned int bufBit, IndexT*& sIdx) const {
    IndexT offset = bufferOff(predIdx, bufBit);
    sIdx = indexBase + offset;
    return nodeVec + offset;
  }


  

  inline IndexT* indexBuffer(unsigned int predIdx, unsigned int bufBit) const {
    IndexT offset = bufferOff(predIdx, bufBit);
    return indexBase + offset;
  }


  /**
     @brief Passes through to above after looking up splitting parameters.
   */
  SampleRank* buffers(const class SplitFrontier* splitFrontier,
                      const class IndexSet* iSet,
                      IndexT*& sIdx);


  /**
     @brief As above, but outputs only the index base.

     @return index base associated with the tree node.
   */
  IndexT* indexBuffer(const class SplitFrontier* splitFrontier,
                         const class IndexSet* iSet);

  
  /**
     @brief Allows lightweight lookup of predictor's SampleRank vector.

     @param bufBit is the containing buffer, currently 0/1.
 
     @param predIdx is the predictor index.

     @return node vector section for this predictor.
   */
  SampleRank* PredBase(const SplitCoord& splitCoord, unsigned int bufBit) const {
    return nodeVec + bufferOff(splitCoord.predIdx, bufBit);
  }
  

  /**
     @brief Returns buffer containing splitting information.
   */
  inline SampleRank* Splitbuffer(unsigned int predIdx, unsigned int bufBit) {
    return nodeVec + bufferOff(predIdx, bufBit);
  }


  /**
   @brief Looks up source and target vectors.

   @param predIdx is the predictor column.

   @param level is the upcoming level.

   @return void, with output parameter vectors.
 */
  inline void buffers(int predIdx,
                      unsigned int bufBit,
                      SampleRank*& source,
                      IndexT*& sIdxSource,
                      SampleRank*& targ,
                      IndexT*& sIdxTarg) {
    source = buffers(predIdx, bufBit, sIdxSource);
    targ = buffers(predIdx, 1 - bufBit, sIdxTarg);
  }

  // To coprocessor subclass:
  inline void indexBuffers(unsigned int predIdx,
                           unsigned int bufBit,
                           IndexT*& sIdxSource,
                           IndexT*& sIdxTarg) {
    sIdxSource = indexBase + bufferOff(predIdx, bufBit);
    sIdxTarg = indexBase + bufferOff(predIdx, 1 - bufBit);
  }
  

  // TODO:  Move somewhere appropriate.
  /**
     @brief Finds the smallest power-of-two multiple >= 'count'.

     @param count is the size to align.

     @return alignment size.
   */
  static constexpr unsigned int alignPow(unsigned int count, unsigned int pow) {
    return ((count + (1 << pow) - 1) >> pow) << pow;
  }


  /**
     @param Determines whether the predictors within a nonempty cell
     all have the same rank.

     @param extent is the number of indices subsumed by the cell.

     @return true iff cell consists of a single rank.
   */
  inline bool singleRank(unsigned int predIdx,
                         unsigned int bufIdx,
                         unsigned int idxStart,
                         unsigned int extent) const {
    SampleRank *spNode = bufferNode(predIdx, bufIdx);
    return extent > 0 ? (spNode[idxStart].getRank() == spNode[extent - 1].getRank()) : false;
  }


  /**
     @brief Singleton iff either:
     i) Dense and all indices implicit or ii) Not dense and all ranks equal.

     @param stageCount is the number of staged indices.

     @param predIdx is the predictor index at which to initialize.

     @return true iff entire staged set has single rank.  This might be
     a property of the training data or may arise from bagging. 
  */
  bool singleton(IndexT stageCount,
                 PredictorT predIdx) const {
    return bagCount == stageCount ? singleRank(predIdx, 0, 0, bagCount) : (stageCount == 0 ? true : false);
  }
};

#endif
