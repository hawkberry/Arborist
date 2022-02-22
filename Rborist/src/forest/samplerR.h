// Copyright (C)  2012-2022   Mark Seligman
//
// This file is part of rf.
//
// rf is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// rf is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with rfR.  If not, see <http://www.gnu.org/licenses/>.

/**
   @file samplerR.h

   @brief C++ interface to R entry for sampled obsservations.

   @author Mark Seligman
 */

#ifndef FOREST_SAMPLER_R_H
#define FOREST_SAMPLER_R_H

#include <Rcpp.h>
using namespace Rcpp;

#include <memory>
#include <vector>
using namespace std;

RcppExport SEXP rootSample(const SEXP sDeframe,
			  const SEXP sArgList);


/**
   @brief Summary of bagged rows, by tree.

   Recast as namespace?
 */
struct SamplerR {
  static const string strYTrain;
  static const string strNSamp;
  static const string strNTree;
  static const string strSamples; // Output field name of sample.
  

  /**
     @brief Samples according to specification.

     @return wrapped list of sample records.
   */
  static List sample(const List& lDeframe,
		     const List& argList);


  /**
     @brief Constructs a bridge Sampler object from wrapped record list.

  static struct SampleBridge unwrapTrain(const List& lSampler,
					 const List& argList);
  */


  /**
     @return Core-ready vector of class weights.
   */
  static vector<double> weightVec(const List& lSampler,
				  const List& argList);


  /**
      @brief Class weighting.

      Class weighting constructs a proxy response based on category
      frequency.  In the absence of class weighting, proxy values are
      identical for all clasess.  The technique of class weighting is
      not without controversy.

      @param yZero is the (zero-based) categorical response vector.

      @param classWeight are user-suppled weightings of the categories.

      @return vector of scaled class weights.
  */
  static NumericVector ctgWeight(const IntegerVector &yZero,
				 const NumericVector &classWeight);



  /**
     @brief Getter for tree count.

  const auto getNTree() const {
    return nTree;
  }
  */
  
  /**
     @brief Bundles trained bag into format suitable for R.

     The wrap functions are called at TrainR::summary, following which
     'this' is deleted.  There is therefore no need to reinitialize
     the block state.

     @return list containing raw data and summary information.
   */
  static List wrap(const struct SamplerBridge* sb,
		   const IntegerVector& yTrain);
  
  
  static List wrap(const struct SamplerBridge* sb,
		   const NumericVector& yTrain);
  
  
  /**
     @brief Consumes a block of samples following training.

     @param scale is a fudge-factor for resizing.
   */
  static NumericVector bridgeConsume(const struct SamplerBridge* sb);

  
  /**
     @brief Checks that bag and prediction data set have conforming rows.

     @param lBag is the training bag.
   */
  static SEXP checkOOB(const List& lBag,
                       const size_t nRow);
  

  /**
     @brief Reads bundled sampler in R form.

     @param lSampler contains the R sampler summary.

     @param lArgs is the argument list, contains sample-weighting values.

     @param return instantiation suitable for training.
   */
  static unique_ptr<struct SamplerBridge> unwrapTrain(const List& lSampler,
						      const List& lArgs);


  /**
     @brief Reads bundled bag information in front-end format.

     @param lSampler contains the R sampler summary.

     @param lDeframe contains the deframed observations.

     @param bagging indicates whether a non-null bag is requested.

     @return instantiation suitable for prediction.
   */
  static unique_ptr<struct SamplerBridge> unwrapPredict(const List& lSampler,
							const List& lDeframe,
							bool bagging);

  /**
     @brief Lower-level call precipitated by above.
   */  
  static unique_ptr<struct SamplerBridge> unwrapPredict(const List& lSampler,
							bool bagging = false);


  /**
     @brief Specialization for numeric response.
   */
  static unique_ptr<struct SamplerBridge> unwrapNum(const List& lSampler,
						    bool bagging = false);

  /**
     @brief Specialization for factor=valued response, training.
   */
  static unique_ptr<struct SamplerBridge> unwrapFac(const List& lSampler,
						    vector<double> weights);


  /**
     @brief Specialization for factor=valued response, prediction.
   */
  static unique_ptr<struct SamplerBridge> unwrapFac(const List& lSampler,
						    bool bagging = false);


  /**
     @return core-ready vector of zero-based factor codes.
   */
  static vector<unsigned int> coreCtg(const IntegerVector& yTrain);
};

#endif
