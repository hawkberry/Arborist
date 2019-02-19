// This file is part of ArboristCore.

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
   @file predict.h

   @brief Data structures and methods for prediction.

   @author Mark Seligman
 */

#ifndef ARBORIST_PREDICT_H
#define ARBORIST_PREDICT_H

#include <vector>
#include <algorithm>

#include "typeparam.h"


/**
   @brief Consolidates common components required by all prediction entries.
 */
struct PredictBox {
  const class FramePredict* framePredict;
  const class Forest* forest;
  const class BitMatrix* bag;
  class LeafFrame* leafFrame; // Subclasses to regression or classification.
  const bool validate;

  PredictBox(const FramePredict* framePredict_,
             const Forest* forest_,
             const BitMatrix* bag_,
             LeafFrame* leaf_,
             bool validate_,
             unsigned int nThread);

  ~PredictBox();
};


class Predict {
  const bool useBag;
  unsigned int noLeaf; // Inattainable leaf index value.
  const class FramePredict *framePredict;
  const class Forest *forest;
  const unsigned int nTree;
  const unsigned int nRow;
  const vector<size_t> treeOrigin;
  unique_ptr<unsigned int[]> predictLeaves; // Tree-relative leaf indices.


  /**
     @brief Assigns a true leaf index at the prediction coordinates passed.

     @return void.
   */
  inline void predictLeaf(unsigned int blockRow,
                      unsigned int tc,
                      unsigned int leafIdx) {
    predictLeaves[nTree * blockRow + tc] = leafIdx;
  }


  /**
     @brief Manages row-blocked prediction across trees.

     @param leaf records the predicted values.

     @param bag summarizes the bagged rows.

     @param quant is non-null iff quantile prediction specified.
   */
  void predictAcross(class LeafFrame* leaf,
                     const class BitMatrix *bag,
                     class Quant *quant = nullptr);

  
  /**
     @brief Dispatches prediction on a block of rows, by predictor type.

     @param rowStart is the starting row over which to predict.

     @param rowEnd is the final row over which to predict.

     @param bag is the packed in-bag representation, if validating.
  */
  void predictBlock(unsigned int rowStart,
                    unsigned int rowEnd,
                    const class BitMatrix *bag);

  /**
     @brief Multi-row prediction with predictors of both numeric and factor type.
     @param rowStart is the first row in the block.

     @param rowEnd is the first row beyond the block.
     
     @param bag indicates whether prediction is restricted to out-of-bag data.
 */
  void predictBlockMixed(unsigned int rowStart,
                    unsigned int rowEnd,
                    const class BitMatrix *bag);
  

  /**
     @brief Multi-row prediction with predictors of only numeric.

     Parameters as with mixed case, above.
  */
  void predictBlockNum(unsigned int rowStart,
                    unsigned int rowEnd,
                    const class BitMatrix *bag);

/**
   @brief Multi-row prediction with predictors of only factor type.

   Parameters as with mixed case, above.
 */
  void predictBlockFac(unsigned int rowStart,
                    unsigned int rowEnd,
                    const class BitMatrix *bag);
  

  /**
     @brief Prediction of single row with mixed predictor types.

     @param row is the absolute row of data over which a prediction is made.

     @param blockRow is the row's block-relative row index.

     @param treeNode is the base of the forest's tree nodes.

     @param facSplit is the base of the forest's split-value vector.

     @param bag indexes out-of-bag rows, and may be null.
  */
  void rowMixed(unsigned int row,
                unsigned int blockRow,
                const class TreeNode *treeNode,
                const class BVJagged *facSplit,
                const class BitMatrix *bag);

  /**
     @brief Prediction over a single row with factor-valued predictors only.

     Parameters as in mixed case, above.
  */
  void rowFac(unsigned int row,
              unsigned int blockRow,
              const class TreeNode *treeNode,
              const class BVJagged *facSplit,
              const class BitMatrix *bag);
  
  /**
     @brief Prediction of a single row with numeric-valued predictors only.

     Parameters as in mixed case, above.
   */
  void rowNum(unsigned int row,
              unsigned int blockRow,
              const class TreeNode *treeNode,
              const class BitMatrix *bag);

 public:  
  static const unsigned int rowBlock = 0x2000; // Block size.
  
  Predict(const PredictBox* box);

  /**
     @brief Quantile prediction entry from bridge.

     @param box summarizes the prediction environment.

     @param quantile specifies the quantiles at which to predict.

     @param nQuant are the number of quantiles specified.

     @param qBin specifies granularity of prediction.

     @return summary of predicted quantiles.
   */
  static unique_ptr<class Quant> predictQuant(const PredictBox* box,
                                              const double* quantile,
                                              unsigned int nQuant,
                                              unsigned int qBin);

  /**
     @brief Generic entry from bridge.

     @param box summarizes the prediction environment.
   */
  static void predict(const PredictBox* box);

  
  /**
     @param[out] termIdx is the predicted tree-relative index.

     @return whether pair is bagged, plus output terminal index.
   */
  inline bool isBagged(unsigned int blockRow,
                       unsigned int tc,
                       unsigned int &termIdx) const {
    termIdx = predictLeaves[nTree * blockRow + tc];
    return termIdx == noLeaf;
  }
};

#endif
