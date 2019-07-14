// This file is part of framemap.

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
   @file summaryframe.h

   @brief Data frame representations for preformatting and training.

   @author Mark Seligman
 */

#ifndef FRAMEMAP_SUMMARYFRAME_H
#define FRAMEMAP_SUMMARYFRAME_H

#include "block.h"
#include "rankedframe.h"
#include "rleframe.h"

#include <vector>

using namespace std;

/**
   @brief Frame represented as row/rank summaries, with numeric block.
 */
class SummaryFrame {
  const unsigned int nRow;
  const unsigned int nPredNum;
  const vector<unsigned int> cardinality; // Factor predictor cardinalities.
  const unsigned int nPredFac;
  const unsigned int cardExtent; // Greatest factor footprint.
  const unsigned int nPred;
  const unique_ptr<RankedFrame> rankedFrame;
  const unique_ptr<BlockJagged<double> > numRanked;

public:

  SummaryFrame(const class RLEFrame* rleFrame,
               double autoCompress,
               const class Coproc* coproc);

  /**
     @brief Getter for rankedFrame.
   */
  inline const RankedFrame* getRankedFrame() const {
    return rankedFrame.get();
  }

  /**
     @brief Getter for numRanked.
   */
  inline const BlockJagged<double> *getNumRanked() const {
    return numRanked.get();
  }


  /**
     @brief Assumes numerical predictors packed in front of factor-valued.

     @return Position of fist factor-valued predictor.
  */
  inline unsigned int getFacFirst() const {
    return nPredNum;
  }

  
  /**
     @brief Determines whether predictor is numeric or factor.

     @param predIdx is internal predictor index.

     @return true iff index references a factor.
   */
  inline bool isFactor(unsigned int predIdx)  const {
    return predIdx >= getFacFirst();
  }

  /**
     @brief Looks up cardinality of a predictor.

     @param predIdx is the absolute predictor index.

     @return cardinality iff factor else 0.
   */
  inline unsigned int getCardinality(unsigned int predIdx) const {
    return predIdx < getFacFirst() ? 0 : cardinality[predIdx - getFacFirst()];
  }


  /**
     @brief Accessor for cardinality footprint.
   */
  inline auto getCardExtent() const {
    return cardExtent;
  }
  

  /**
     @brief Computes block-relative position for a predictor.

     @param[out] thisIsFactor outputs true iff predictor is factor-valued.

     @return block-relative index.
  */
  inline unsigned int getIdx(unsigned int predIdx, bool &thisIsFactor) const{
    thisIsFactor = isFactor(predIdx);
    return thisIsFactor ? predIdx - getFacFirst() : predIdx;
  }


  /**
     @brief Determines a dense position for factor-valued predictors.

     @param predIdx is a predictor index.

     @param nStride is a stride value.

     @param[out] thisIsFactor is true iff predictor is factor-valued.

     @return strided factor offset, if factor, else predictor index.
   */
  inline unsigned int getFacStride(unsigned int predIdx,
				unsigned int nStride,
				bool &thisIsFactor) const {
    unsigned int facIdx = getIdx(predIdx, thisIsFactor);
    return thisIsFactor ? nStride * getNPredFac() + facIdx : predIdx;
  }


  /**
     @return number or observation rows.
   */
  inline unsigned int getNRow() const {
    return nRow;
  }

  /**
     @return number of observation predictors.
  */
  inline unsigned int getNPred() const {
    return nPred;
  }

  /**
     @return number of factor predictors.
   */
  inline unsigned int getNPredFac() const {
    return nPredFac;
  }

  /**
     @return number of numerical predictors.
   */
  inline unsigned int getNPredNum() const {
    return nPredNum;
  }


  /**
     @brief Fixes contiguous factor ordering as numerical preceding factor.

     @return Position of first numerical predictor.
  */
  static constexpr unsigned int getNumFirst() {
    return 0ul;
  }


  /**
     @brief Positions predictor within numerical block.

     @param predIdx is the core-ordered index of a predictor assumed to be numeric.

     @return Position of predictor within numerical block.
  */
  inline unsigned int getNumIdx(unsigned int predIdx) const {
    return predIdx - getNumFirst();
  }

  /**
     @brief Derives split values for a numerical predictor by synthesizing
     a fractional intermediate rank and interpolating.

     @param predIdx is the predictor index.

     @param rankRange is the range of ranks.

     @return interpolated predictor value at synthesized rank.
  */
  inline double getQuantRank(unsigned int predIdx,
                             IndexRange rankRange,
                             const vector<double> &splitQuant) const {
    double rankNum = rankRange.idxLow + splitQuant[predIdx] * rankRange.idxExtent;
    unsigned int rankFloor = floor(rankNum);
    unsigned int rankCeil = ceil(rankNum);
    double valFloor = numRanked->getVal(predIdx, rankFloor);
    double valCeil = numRanked->getVal(predIdx, rankCeil);
    return valFloor + (rankNum - rankFloor) * (valCeil - valFloor);
  }


  virtual unique_ptr<class SamplePred> SamplePredFactory(unsigned int bagCount) const;

  virtual unique_ptr<class SPReg> SPRegFactory(unsigned int bagCount) const;


  virtual unique_ptr<class SPCtg> SPCtgFactory(unsigned int bagCount,
                                               unsigned int _nCtg) const; 
};

#endif
