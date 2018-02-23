// This file is part of ArboristCore.

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
   @file frameblock.h

   @brief Class definitions for maintenance of type-based data blocks.

   @author Mark Seligman
 */

#ifndef ARBORIST_FRAMEBLOCK_H
#define ARBORIST_FRAMEBLOCK_H

#include <vector>

#include "typeparam.h"


/**
   @brief Abstract class for blocks of predictor values.
 */
class BlockNum {
 protected:
  double *blockNumT; // Iterator state
  const unsigned int nPredNum;
 public:

 BlockNum(unsigned int _nPredNum) : nPredNum(_nPredNum) {}
  virtual ~BlockNum() {}

  static BlockNum *Factory(const vector<double> &_valNum, const vector<unsigned int> &_rowStart, const vector<unsigned int> &_runLength, const vector<unsigned int> &_predStart, double *_feNumT, unsigned int _nPredNum);

  virtual void Transpose(unsigned int rowStart, unsigned int rowEnd) = 0;


  inline const double *Row(unsigned int rowOff) {
    return blockNumT + nPredNum * rowOff;
  }
};


class BlockSparse : public BlockNum {
  const vector<double> &valNum;
  const vector<unsigned int> &rowStart;
  const vector<unsigned int> &runLength;
  const vector<unsigned int> &predStart;
  double *val;
  unsigned int *rowNext;
  unsigned int *idxNext;

 public:

  /**
     @brief Sparse constructor.
   */
  BlockSparse(const vector<double> &_valNum,
	      const vector<unsigned int> &_rowStart,
	      const vector<unsigned int> &_runLength,
	      const vector<unsigned int> &_predStart);
  ~BlockSparse();
  void Transpose(unsigned int rowStart, unsigned int rowEnd);
};


class BlockNumDense : public BlockNum {
  double *feNumT;
 public:


 BlockNumDense(double *_feNumT,
	       unsigned int _nPredNum) :
  BlockNum(_nPredNum), feNumT(_feNumT) {
    blockNumT = _feNumT;
  }


  ~BlockNumDense() {
  }

  
  /**
     @brief Resets starting position to block within region previously
     transposed.

     @param rowStart is the first row of the block.

     @param rowEnd is the sup row.  Unused here.

     @return void.
   */
  inline void Transpose(unsigned int rowStart, unsigned int rowEnd) {
    blockNumT = feNumT + nPredNum * rowStart;
  }
};


class BlockFac {
  const unsigned int nPredFac;
  unsigned int *feFac; // Factors, may or may not already be transposed.
  unsigned int *blockFacT; // Iterator state.

 public:

  /**
     @brief Dense constructor:  currently pre-transposed.
   */
 BlockFac(unsigned int *_feFacT, unsigned int _nPredFac) : nPredFac(_nPredFac), feFac(_feFacT) {
  }
  static BlockFac *Factory(unsigned int *_feFacT, unsigned int _nPredFac);
  
  /**
     @brief Resets starting position to block within region previously
     transposed.

     @param rowStart is the first row of the block.

     @param rowEnd is the sup row.  Unused here.

     @return void.
   */
  inline void Transpose(unsigned int rowStart, unsigned int rowEnd) {
    blockFacT = feFac + nPredFac * rowStart;
  }


  /**
     @brief Computes the starting position of a row of transposed
     predictor values.

     @param rowOff is the buffer offset for the row.

     @return pointer to beginning of transposed row.
   */
  inline const unsigned int *Row(unsigned int rowOff) const {
    return blockFacT + rowOff * nPredFac;
  }
};



/**
   @brief Singleton subclass instances:  training or prediction.
 */
class FrameMap {
 protected:
  unsigned int nRow;
  unsigned int nPredNum;
  unsigned int nPredFac;
 public:

 FrameMap(unsigned int _nRow, unsigned int _nPredNum, unsigned int _nPredFac) : nRow(_nRow), nPredNum(_nPredNum), nPredFac(_nPredFac) {
  }
  
  /**
     @brief Assumes numerical predictors packed in front of factor-valued.

     @return Position of fist factor-valued predictor.
  */
  inline unsigned int FacFirst() const {
    return nPredNum;
  }

  
  /**
     @brief Determines whether predictor is numeric or factor.

     @param predIdx is internal predictor index.

     @return true iff index references a factor.
   */
  inline bool IsFactor(unsigned int predIdx)  const {
    return predIdx >= FacFirst();
  }


  /**
     @brief Computes block-relative position for a predictor.
   */
  inline unsigned int FacIdx(int predIdx, bool &isFactor) const{
    isFactor = IsFactor(predIdx);
    return isFactor ? predIdx - FacFirst() : predIdx;
  }


  inline unsigned int FacStride(unsigned int predIdx,
				unsigned int nStride,
				bool &isFactor) const {
    unsigned int facIdx = FacIdx(predIdx, isFactor);
    return isFactor ? nStride * nPredFac + facIdx : predIdx;
  }


  /**
     @return number or observation rows.
   */
  inline unsigned int NRow() const {
    return nRow;
  }

  /**
     @return number of observation predictors.
  */
  inline unsigned int NPred() const {
    return nPredFac + nPredNum;
  }

  /**
     @return number of factor predictors.
   */
  inline unsigned int NPredFac() const {
    return nPredFac;
  }

  /**
     @return number of numerical predictors.
   */
  inline unsigned int NPredNum() const {
    return nPredNum;
  }


  /**
     @brief Fixes contiguous factor ordering as numerical preceding factor.

     @return Position of first numerical predictor.
  */
  inline unsigned int NumFirst() const {
    return 0;
  }


  /**
     @brief Positions predictor within numerical block.

     @param predIdx is the core-ordered index of a predictor assumed to be numeric.

     @return Position of predictor within numerical block.
   */
  inline unsigned int NumIdx(int predIdx) const {
    return predIdx - NumFirst();
  }


  /**
     @brief Assumes numerical predictors packed ahead of factor-valued.

     @return Position of last numerical predictor.
  */
  inline unsigned int NumSup() const {
    return nPredNum;
  }

  
  /**
     @brief Same assumptions about predictor ordering.

     @return Position of last factor-valued predictor.
  */
  inline unsigned int FacSup() const {
    return nPredNum + nPredFac;
  }
};


/**
   @brief Training caches numerical predictors for evaluating splits.
 */
class FrameTrain : public FrameMap {
  const vector<unsigned int> &feCard; // Factor predictor cardinalities.
  const unsigned int cardMax;  // High watermark of factor cardinalities.

 public:
  FrameTrain(const vector<unsigned int> &_feCard,
	  unsigned int _nPred,
	  unsigned int _nRow);


  /**
   @brief Computes cardinality of factor-valued predictor, or zero if not a
   factor.

   @param predIdx is the internal predictor index.

   @return factor cardinality or zero.
  */
  inline int FacCard(int predIdx) const {
    return IsFactor(predIdx) ? feCard[predIdx - FacFirst()] : 0;
  }

  
  /**
     @brief Maximal predictor cardinality.  Useful for packing.

     @return highest cardinality, if any, among factor predictors.
   */
  inline unsigned int CardMax() const {
    return cardMax;
  }
};


class FramePredict : public FrameMap {
  BlockNum *blockNum;
  BlockFac *blockFac;

 public:
  static const unsigned int rowBlock = 0x2000;

  FramePredict(BlockNum *_blockNum,
	       BlockFac *_blockFac,
	    unsigned int _nPredNum,
	    unsigned int _nPredFac,
	    unsigned int _nRow);
  ~FramePredict();


  inline void BlockTranspose(unsigned int rowStart,
			     unsigned int rowEnd) const {
    blockNum->Transpose(rowStart, rowEnd);
    blockFac->Transpose(rowStart, rowEnd);
  }


  /**
     @return base address for (transposed) numeric values at row.
   */
  inline const double *RowNum(unsigned int rowOff) const {
    return blockNum->Row(rowOff);
  }


  /**
     @return base address for (transposed) factor values at row.
   */
  inline const unsigned int *RowFac(unsigned int rowOff) const {
    return blockFac->Row(rowOff);
  }
};

#ifdef REPLACE
class FrameBlock {
 protected:
  unsigned int idxStart; // Starting offset within external container.
  unsigned int extent;  // Extent within external container.
};

class FBNum : public FrameBlock {
  double *num;
};


class FBFac : public FrameBlock {
  unsigned int *fac;
  unsigned int cardMax;
};


#endif // REPLACE
#endif
