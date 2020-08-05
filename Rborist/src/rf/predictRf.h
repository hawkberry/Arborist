// Copyright (C)  2012-2019   Mark Seligman
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
   @file predictRf.h

   @brief C++ interface to R entry for prediction.

   @author Mark Seligman
 */


#ifndef RF_PREDICT_RF_H
#define RF_PREDICT_RF_H

#include <Rcpp.h>
using namespace Rcpp;


RcppExport SEXP ValidateReg(const SEXP sFrame,
                            const SEXP sTrain,
                            SEXP sYTest,
			    SEXP sImportance,
                            SEXP sNThread);


RcppExport SEXP TestReg(const SEXP sFrame,
                        const SEXP sTrain,
                        SEXP sYTest,
                        SEXP sOOB,
                        SEXP sNThread);


RcppExport SEXP ValidateVotes(const SEXP sFrame,
                              const SEXP sTrain,
                              SEXP sYTest,
			      SEXP sImportance,
                              SEXP sNThread);


RcppExport SEXP ValidateProb(const SEXP sFrame,
                             const SEXP sTrain,
                             SEXP sYTest,
			     SEXP sImportance,
                             SEXP sNThread);


RcppExport SEXP ValidateQuant(const SEXP sFrame,
                              const SEXP sTrain,
                              SEXP sYTest,
			      SEXP sImportance,
                              SEXP sQuantVec,
                              SEXP sNThread);


RcppExport SEXP TestQuant(const SEXP sFrame,
                          const SEXP sTrain,
                          SEXP sQuantVec,
                          SEXP sYTest,
                          SEXP sOOB,
                          SEXP sNThread);

/**
   @brief Predicts with class votes.

   @param sFrame contains the blocked observations.

   @param sTrain contains the trained object.

   @param sYTest is the vector of test values.

   @param sOOB indicates whether testing is out-of-bag.

   @return wrapped predict object.
 */
RcppExport SEXP TestProb(const SEXP sFrame,
                         const SEXP sTrain,
                         SEXP sYTest,
                         SEXP sOOB,
                         SEXP sNThread);


/**
   @brief Predicts with class votes.

   @param sFrame contains the blocked observations.

   @param sTrain contains the trained object.

   @param sYTest contains the test vector.

   @param sOOB indicates whether testing is out-of-bag.

   @return wrapped predict object.
 */
RcppExport SEXP TestVotes(const SEXP sFrame,
                          const SEXP sTrain,
                          SEXP sYTest,
                          SEXP sOOB,
                          SEXP sNThread);

/**
   @brief Bridge-variant PredictBridge pins unwrapped front-end structures.
 */
struct PBRf {

  static List predictCtg(const List& lDeframe,
			 const List& lTrain,
			 SEXP sYTest,
			 bool oob,
			 bool doProb,
			 bool importance,
			 unsigned int nThread);

  
  /**
     @brief Prediction for regression.  Parameters as above.
   */
  static List predictReg(const List& lDeframe,
                         const List& lTrain,
                         SEXP sYTest,
                         bool oob,
			 bool importance,
                         unsigned int nThread);


  /**
  @brief Prediction with quantiles.

    @param sFrame contains the blocked observations.

    @param sTrain contains the trained object.

    @param sQuantVec is a vector of quantile training data.
   
    @param sYTest is the test vector.

    @param oob is true iff testing restricted to out-of-bag.

    @param importance is true iff permutation testing is specified.

    @return wrapped prediction list.
 */
  static List predictQuant(const List& lDeframe,
			   const List& sTrain,
			   SEXP sQuantVec,
			   SEXP sYTest,
			   bool oob,
			   bool importance,
			   unsigned int nThread);

  /**
     @brief Unwraps regression data structurs and moves to box.

     @return unique pointer to bridge-variant PredictBridge. 
   */
  static unique_ptr<struct PredictBridge> unwrapReg(const List& lDeframe,
                                                   const List& lTrain,
                                                   bool oob,
						    bool importance,
                                                   unsigned int nThread,
                                                   const vector<double>& quantile);

  /**
     @brief Unwraps regression data structurs and moves to box.

     @return unique pointer to bridge-variant PredictBridge. 
   */
  static unique_ptr<struct PredictBridge> unwrapReg(const List& lDeframe,
						    const List& lTrain,
						    bool oob,
						    bool importance,
						    unsigned int nThread);


  /**
     @brief Instantiates core prediction object and predicts quantiles.

     @return wrapped predictions.
   */
  List predict(SEXP sYTest,
               const vector<double>& quantile) const;

  /**
     @brief Unwraps regression data structurs and moves to box.

     @return unique pointer to bridge-variant PredictBridge. 
   */
  static unique_ptr<struct PredictBridge> unwrapCtg(const List& lDeframe,
						    const List& lTrain,
						    bool oob,
						    bool doProb,
						    bool importance,
						    unsigned int nThread);

private:
  /**
     @brief Instantiates core prediction object and predicts means.

     @return wrapped predictions.
   */
  static List predictReg(SEXP sYTest);

  /**
     @brief Instantiates core PredictRf object, driving prediction.

     @return wrapped prediction.
   */
  static List predictCtg(SEXP sYTest, const List& lTrain, const List& sFrame);
};
#endif
