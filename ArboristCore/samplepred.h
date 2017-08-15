// This file is part of ArboristCore.

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
   @file samplepred.h

   @brief Class definitions supporting maintenance of per-predictor sample orderings.

   @author Mark Seligman

 */


#ifndef ARBORIST_SAMPLEPRED_H
#define ARBORIST_SAMPLEPRED_H

#include "param.h"

#include <vector>

/**
   @brief Container for staging initialization, viz. minimizing communication
   from host or head node.
*/
class StagePack {
  unsigned int rank;
  unsigned int sCount;
  unsigned int ctg;
  FltVal ySum;
 public:


  inline void Ref(unsigned int &_rank, unsigned int &_sCount, unsigned int &_ctg, FltVal &_ySum) const {
    _rank = rank;
    _sCount = sCount;
    _ctg = ctg;
    _ySum = ySum;
  }


  inline void Init(unsigned int _rank, unsigned int _sCount, unsigned int _ctg, FltVal _ySum) {
    rank = _rank;
    sCount = _sCount;
    ctg = _ctg;
    ySum = _ySum;
  }
};


/**
 */
class SPNode {
  static unsigned int ctgShift; // Pack:  nonzero iff categorical response.


 protected:
  FltVal ySum; // sum of response values associated with sample.
  unsigned int rank; // Rank, up to tie, or factor group.
  unsigned int sCount; // # occurrences of row sampled:  << # rows.


 public:
  static void Immutables(unsigned int ctgWidth);
  static void DeImmutables();


  /**
  
     @brief Initializes immutable field values with category packing.

     @param stagePack holds packed staging values.

     @return void.
  */
  inline void Init(unsigned int _rank, unsigned int _ctg, FltVal _ySum, unsigned int _sCount) {
    rank = _rank;
    ySum = _ySum;
    sCount = (_sCount << ctgShift) | _ctg; // Packed representation.
  }




  // These methods should only be called when the response is known
  // to be regression, as it relies on a packed representation specific
  // to that case.
  //

  /**
     @brief Reports SamplePred contents for regression response.  Cannot
     be used with categorical response, as 'sCount' value reported here
     is unshifted.

     @param _ySum outputs the response value.

     @param _rank outputs the predictor rank.

     @param _sCount outputs the multiplicity of the row in this sample.

     @return void.
   */
  inline void RegFields(FltVal &_ySum, unsigned int &_rank, unsigned int &_sCount) const {
    _ySum = ySum;
    _rank = rank;
    _sCount = sCount;
  }

  // These methods should only be called when the response is known
  // to be categorical, as it relies on a packed representation specific
  // to that case.
  //
  
  /**
     @brief Reports SamplePred contents for categorical response.  Can
     be called with regression response if '_yCtg' value ignored.

     @param _ySum outputs the proxy response value.

     @param _rank outputs the predictor rank.

     @param _yCtg outputs the response value.

     @return sample count, with output reference parameters.
   */
  inline unsigned int CtgFields(FltVal &_ySum, unsigned int &_yCtg) const {
    _ySum = ySum;
    _yCtg = sCount & ((1 << ctgShift) - 1);

    return sCount >> ctgShift;
  }


  /**
     @brief Reports SamplePred contents for categorical response.  Can
     be called with regression response if '_yCtg' value ignored.

     @param _ySum outputs the proxy response value.

     @param _rank outputs the predictor rank.

     @param _yCtg outputs the response value.

     @return sample count, with output reference parameters.
   */
  inline unsigned int CtgFields(FltVal &_ySum, unsigned int &_rank, unsigned int &_yCtg) const {
    _rank = rank;
    return CtgFields(_ySum, _yCtg);
  }


  /**
     @brief Accessor for 'rank' field

     @return rank value.
   */
  inline unsigned int Rank() const {
    return rank;
  }


  /**
     @brief Accessor for 'ySum' field

     @return sum of y-values for sample.
   */
  inline FltVal YSum() {
    return ySum;
  }

};


/**
 @brief Contains the sample data used by predictor-specific sample-walking pass.
*/
class SamplePred {
  // SamplePred appear in predictor order, grouped by node.  They store the
  // y-value, run class and sample index for the predictor position to which they
  // correspond.

  const unsigned int bagCount;
  const unsigned int nPred;

  // Predictor-based sample orderings, double-buffered by level value.
  //
  const unsigned int bufferSize; // <= nRow * nPred.
  const unsigned int pitchSP; // Pitch of SPNode vector, in bytes.
  const unsigned int pitchSIdx; // Pitch of SIdx vector, in bytes.

  std::vector<PathT> pathIdx;
  std::vector<unsigned int> stageOffset;
  std::vector<unsigned int> stageExtent; // Client:  debugging only.
  SPNode* nodeVec;

  // 'indexBase' could be boxed with SPNode.  While it is used in both
  // replaying and restaging, though, it plays no role in splitting.  Maintaining
  // a separate vector permits a 16-byte stride to be used for splitting.  More
  // significantly, it reduces memory traffic incurred by transposition on the
  // coprocessor.
  //
  unsigned int *indexBase; // RV index for this row.  Used by CTG as well as on replay.

  unsigned int *destRestage; // To coprocessor subclass;
  unsigned int *destSplit; // To coprocessor subclass;
  PathT *pathTest; // Exit.

 public:
  SamplePred(unsigned int _nPred, unsigned int _bagCount, unsigned int _bufferSize);
  ~SamplePred();


  SPNode *StageBounds(unsigned int predIdx, unsigned int safeOffset, unsigned int extent, unsigned int *&smpIdx);

  unsigned int Stage(const std::vector<class SampleNode> &sampleNode, const class RRNode *rrPred, const std::vector<unsigned int> &row2Sample, unsigned int explMax, unsigned int predIdx, unsigned int safeOffset, unsigned int extent, bool &singleton);
  void Stage(const std::vector<class SampleNode> &sampleNode, const class RRNode &rrNode, const std::vector<unsigned int> &row2Sample, SPNode *spn, unsigned int *smpIdx, unsigned int &expl);

  double BlockReplay(unsigned int predIdx, unsigned int sourceBit, unsigned int start, unsigned int extent, class BV *replayExpl, std::vector<class SumCount> &ctgExpl);

  
  void Prepath(const class IdxPath *idxPath, const unsigned int reachBase[], unsigned int predIdx, unsigned int bufIdx, unsigned int startIdx, unsigned int extent, unsigned int pathMask, bool idxUpdate, unsigned int pathCount[]);
  void Prepath(const class IdxPath *idxPath, const unsigned int reachBase[], bool idxUpdate, unsigned int startIdx, unsigned int extent, unsigned int pathMask, unsigned int idxVec[], PathT prepath[], unsigned int pathCount[]) const;
  void RestageRank(unsigned int predIdx, unsigned int bufIdx, unsigned int start, unsigned int extent, unsigned int reachOffset[], unsigned int rankPrev[], unsigned int rankCount[]);

  
  inline unsigned int BagCount() const {
    return bagCount;
  }

  
  inline unsigned int PitchSP() {
    return pitchSP;
  }

  inline unsigned int PitchSIdx() {
    return pitchSIdx;
  }


  /**
     @brief Returns the staging position for a dense predictor.
   */
  inline unsigned int StageOffset(unsigned int predIdx) {
    return stageOffset[predIdx];
  }


  // The category could, alternatively, be recorded in an object subclassed
  // under class SamplePred.  This would require that the value be restaged,
  // which happens for all predictors at all splits.  It would also require
  // that distinct SamplePred classes be maintained for SampleReg and
  // SampleCtg.  Recomputing the category value on demand, then, seems an
  // easier way to go.
  //

  /**
     @brief Toggles between positions in workspace double buffer, by level.

     @return workspace starting position for this level.
   */
  inline unsigned int BuffOffset(unsigned int bufferBit) const {
    return (bufferBit & 1) == 0 ? 0 : bufferSize;
  }

  /**

     @param predIdx is the predictor coordinate.

     @param level is the current level.

     @return starting position within workspace.
   */
  inline unsigned int BufferOff(unsigned int predIdx, unsigned int bufBit) const {
    return stageOffset[predIdx] + BuffOffset(bufBit);
  }


  /**
     @return base of the index buffer.
   */
  inline unsigned int *BufferIndex(unsigned int predIdx, unsigned int bufBit) {
    return indexBase + BufferOff(predIdx, bufBit);
  }


  /**
     @return base of node buffer.
   */
  inline SPNode *BufferNode(unsigned int predIdx, unsigned int bufBit) {
    return nodeVec + BufferOff(predIdx, bufBit);
  }
  
  
  /**
   */
  inline SPNode* Buffers(unsigned int predIdx, unsigned int bufBit, unsigned int*& sIdx) {
    unsigned int offset = BufferOff(predIdx, bufBit);
    sIdx = indexBase + offset;
    return nodeVec + offset;
  }


  /**
     @brief Allows lightweight lookup of predictor's SPNode vector.

     @param bufBit is the containing buffer, currently 0/1.
 
     @param predIdx is the predictor index.

     @return node vector section for this predictor.
   */
  SPNode* PredBase(unsigned int predIdx, unsigned int bufBit) const {
    return nodeVec + BufferOff(predIdx, bufBit);
  }
  

  /**
     @brief Returns buffer containing splitting information.
   */
  inline SPNode* SplitBuffer(unsigned int predIdx, unsigned int bufBit) {
    return nodeVec + BufferOff(predIdx, bufBit);
  }


  /**
   @brief Looks up source and target vectors.

   @param predIdx is the predictor column.

   @param level is the upcoming level.

   @return void, with output parameter vectors.
 */
  inline void Buffers(int predIdx, unsigned int bufBit, SPNode *&source, unsigned int *&sIdxSource, SPNode *&targ, unsigned int *&sIdxTarg) {
    source = Buffers(predIdx, bufBit, sIdxSource);
    targ = Buffers(predIdx, 1 - bufBit, sIdxTarg);
  }

  // To coprocessor subclass:
  inline void IndexBuffers(unsigned int predIdx, unsigned int bufBit, unsigned int *&sIdxSource, unsigned int *&sIdxTarg) {
    sIdxSource = indexBase + BufferOff(predIdx, bufBit);
    sIdxTarg = indexBase + BufferOff(predIdx, 1 - bufBit);
  }
  

  // TODO:  Move somewhere appropriate.
  /**
     @brief Finds the smallest power-of-two multiple >= 'count'.

     @param count is the size to align.

     @return alignment size.
   */
  static inline unsigned int AlignPow(unsigned int count, unsigned int pow) {
    return ((count + (1 << pow) - 1) >> pow) << pow;
  }


  /**
     @brief Accessor for staging extent field.
   */
  inline unsigned int StageExtent(unsigned int predIdx) {
    return stageExtent[predIdx];
  }

  
  /**
     @param Determines whether the predictors within a nonempty cell
     all have the same rank.

     @param extent is the number of indices subsumed by the cell.

     @return true iff cell consists of a single rank.
   */
  inline bool SingleRank(unsigned int predIdx, unsigned int bufIdx, unsigned int idxStart, unsigned int extent) {
    SPNode *spNode = BufferNode(predIdx, bufIdx);
    return extent > 0 ? (spNode[idxStart].Rank() == spNode[extent - 1].Rank()) : false;
  }


  /**
     @brief Singleton iff either:
     i) Dense and all indices implicit or ii) Not dense and all ranks equal.

     @param stageCount is the number of staged indices.

     @param predIdx is the predictor index at which to initialize.

     @return true iff entire staged set has single rank.  This might be
     a property of the training data or may arise from bagging. 
  */
  bool Singleton(unsigned int stageCount, unsigned int predIdx) {
    return bagCount == stageCount ? SingleRank(predIdx, 0, 0, bagCount) : (stageCount == 0 ? true : false);
  }
};

#endif
