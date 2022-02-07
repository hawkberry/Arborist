// This file is part of ArboristCore.

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
   @file trainbridge.h

   @brief Training methods exportable to front end.

   @author Mark Seligman
 */


#ifndef RF_BRIDGE_TRAINBRIDGE_H
#define RF_BRIDGE_TRAINBRIDGE_H

#include<vector>
#include<memory>

using namespace std;

struct TrainBridge {
  TrainBridge(const struct RLEFrame* rleFrame,
	      double autoCompress,
	      bool enableCoproc,
	      vector<string>& diag);
  
  ~TrainBridge();

  /**
     @brief Copies internal-to-external predictor map.

     @return copy of trainFrame's predMap.
   */
  vector<unsigned int> getPredMap() const;

  
  /**
     @brief Main entry for training.
   */
  unique_ptr<struct TrainedChunk> train(const class ForestBridge& forest,
					const class SamplerBridge* sampler,
					const class LeafBridge* leafBridge) const;


  /**
     @brief Registers training tree-block count.

     @param trainBlock_ is the number of trees by which to block.
  */
  static void initBlock(unsigned int trainBlock);


  static void initProb(unsigned int predFixed,
                       const vector<double> &predProb);

  /**
     @brief Registers tree-shape parameters.
  */
  static void initTree(unsigned int leafMax);

  /**
     @brief Initializes static OMP thread state.

     @param nThread is a user-specified thread request.
   */
  static void initOmp(unsigned int nThread);

  
  /**
     @brief Registers parameters governing splitting.
     
     @param minNode is the mininal number of sample indices represented by a tree node.

     @param totLevels is the maximum tree depth to train.

     @param minRatio is the minimum information ratio of a node to its parent.
     
     @param splitQuant is a per-predictor quantile specification.
  */
  static void initSplit(unsigned int minNode,
                        unsigned int totLevels,
                        double minRatio,
			const vector<double>& feSplitQuant);
  
  /**
     @brief Registers monotone specifications for regression.

     @param regMono has length equal to the predictor count.  Only
     numeric predictors may have nonzero entries.
  */
  void initMono(const vector<double>& regMono);

  /**
     @brief Static de-initializer.
   */
  static void deInit();

private:
  unique_ptr<class TrainFrame> trainFrame;
};


struct TrainedChunk {
  TrainedChunk(unique_ptr<class Train>);

  ~TrainedChunk();
  
  
  /**
     @brief Getter for splitting information values.

     @return reference to per-preditor information vector.
   */
  const vector<double>& getPredInfo() const;


private:

    unique_ptr<class Train> train;
};


#endif
