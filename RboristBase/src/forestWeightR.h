// Copyright (C)  2012-2023  Mark Seligman
//
// This file is part of rf.
//
// rfR is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// rfR is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with rfR.  If not, see <http://www.gnu.org/licenses/>.

/**
   @file weightingR.h

   @brief C++ interface to R entry for forest weightings.

   @author Mark Seligman
 */

#ifndef CORE_WEIGHTING_R_H
#define CORE_WEIGHTING_R_H

#include <Rcpp.h>
using namespace Rcpp;

/**
   @brief Entry from R.
 */
RcppExport SEXP forestWeightRcpp(const SEXP sTrain,
				 const SEXP sSampler,
				 const SEXP sPredict,
				 const SEXP sArgs);

struct ForestWeightR {
  /**
     @brief Meinshausen's (2006) forest weight.
   */
  static List forestWeight(const List& lTrain,
			   const List& lSampler,
			   const NumericMatrix& indices,
			   const List& lArgs);
  };
#endif
