// This file is part of ArboristCore.

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
   @file train.h

   @brief Class definitions for the training entry point.

   @author Mark Seligman
 */

#ifndef ARBORIST_TRAIN_H
#define ARBORIST_TRAIN_H

#include <string>
#include <vector>

#include "typeparam.h"

/**
   @brief Short-lived bundle of objects created for training a block of trees.
 */
typedef pair<shared_ptr<class Sample>, shared_ptr<class PreTree> > TrainSet;

/**
   @brief Interface class for front end.  Holds simulation-specific parameters
   of the data and constructs forest, leaf and diagnostic structures.
*/
class Train {
  static constexpr double slopFactor = 1.2; // Estimates tree growth.
  static unsigned int trainBlock; // Front-end defined buffer size.

  const unsigned int nRow;
  const unsigned int treeChunk;
  unique_ptr<class BitMatrix> bagRow; // treeChunk x nRow
  unique_ptr<class ForestTrain> forest;
  vector<double> predInfo; // E.g., Gini gain:  nPred.

  void trainChunk(const class FrameTrain *frameTrain,
                  const class RankedSet *rankedPair);

  unique_ptr<class LFTrain> leaf;

public:

  /**
     @brief Regression constructor.
  */
  Train(const class FrameTrain *frameTrain,
        const double *y,
        unsigned int treeChunk_);

  
  /**
     @brief Classification constructor.
  */
  Train(const class FrameTrain *frameTrain,
        const unsigned int *yCtg,
        unsigned int nCtg,
        const double *yProxy,
        unsigned int nTree,
        unsigned int treeChunk_);

  ~Train();


  class LFTrain *getLeaf() const {
    return leaf.get();
  }

  const vector<double> &getPredInfo() const {
    return predInfo;
  }


  /**
   @brief Static initializer.

   @return void.
 */
  static void initBlock(unsigned int trainBlock);

  static void initCDF(const vector<double> &splitQuant);

  static void initProb(unsigned int predFixed,
                       const vector<double> &predProb);


  static void initTree(unsigned int nSamp,
                       unsigned int minNode,
                       unsigned int leafMax);

  static void initOmp(unsigned int nThread);
  
  static void initSample(unsigned int nSamp);

  static void initCtgWidth(unsigned int ctgWidth);

  static void initSplit(unsigned int minNode,
                        unsigned int totLevels,
                        double minRatio);
  
  static void initMono(const class FrameTrain* frameTrain,
                       const vector<double> &regMono);

  static void deInit();

  static unique_ptr<Train> regression(
       const class FrameTrain *frameTrain,
       const class RankedSet *rankedPair,
       const double *y,
       unsigned int treeChunk);


  static unique_ptr<Train> classification(
       const class FrameTrain *frameTrain,
       const class RankedSet *rankedPair,
       const unsigned int *yCtg,
       const double *yProxy,
       unsigned int nCtg,
       unsigned int treeChunk,
       unsigned int nTree);
  
  void reserve(vector<TrainSet> &treeBlock);

  /**
     @brief Accumulates block size parameters as clues to forest-wide sizes.
     Estimates improve with larger blocks, at the cost of higher memory footprint.

     @return sum of tree sizes over block.
  */
  unsigned int blockPeek(vector<TrainSet> &treeBlock,
                         size_t &blockFac,
                         size_t &blockBag,
                         size_t &blockLeaf,
                         size_t &maxHeight);

  /**
     @brief Builds segment of decision forest for a block of trees.

     @param treeBlock is a vector of Sample, PreTree pairs.

     @param blockStart is the starting tree index for the block.
  */
  void blockConsume(vector<TrainSet> &treeBlock,
                    unsigned int blockStart);

  /**
     @brief  Creates a block of root samples and trains each one.

     @return Wrapped collection of Sample, PreTree pairs.
  */
  vector<TrainSet> blockProduce(const class FrameTrain *frameTrain,
                                const class RowRank *rowRank,
                                unsigned int tStart,
                                unsigned int tCount);

  class ForestTrain *getForest() const {
    return forest.get();
  }

  /**
     @brief Dumps bag contents as raw characters.

     @param[out] bbRaw
   */
  void cacheBagRaw(unsigned char bbRaw[]) const;
};

#endif
