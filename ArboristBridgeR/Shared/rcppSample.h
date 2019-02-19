// Copyright (C)  2012-2019  Mark Seligman
//
// This file is part of ArboristBridgeR.
//
// ArboristBridgeR is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// ArboristBridgeR is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with ArboristBridgeR.  If not, see <http://www.gnu.org/licenses/>.

/**
   @file rcppSample.h

   @brief C++ class definitions for invocation of R methods implementing response sampling.   Can be extended for other instances of sampling.

   @author Mark Seligman

 */


#ifndef ARBORIST_RCPP_SAMPLE_H
#define ARBORIST_RCPP_SAMPLE_H

#include <Rcpp.h>
using namespace Rcpp;


/**
   @brief Row-sampling parameters supplied by the front end are invariant, so can be cached as static.
 */
class RcppSample {
  static bool withRepl; // Whether sampling employs replacement.
  static NumericVector &weight; // Pinned vector[nRow] of weights.
  static IntegerVector &rowSeq; // Pinned sequence from 0 to nRow - 1.
public:

  /**
   @brief Caches row sampling parameters as static values.

   @param feWeight is user-specified weighting of row samples.

   @param withRepl_ is true iff sampling with replacement.

   @return void.
 */
  static void init(const NumericVector &feWeight,
                   bool withRepl_);

  /**
   @brief Samples row indices either with or without replacement using methods from RccpArmadillo.

   @param nSamp is the number of samples to draw.

   @return vector of sampled row indices.
 */
  static IntegerVector sampleRows(unsigned int nSamp);
};

#endif
