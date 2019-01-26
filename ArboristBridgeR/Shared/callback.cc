// Copyright (C)  2012-2019   Mark Seligman
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
   @file callback.cc

   @brief Implements sampling utitlities by means of calls to front end.

   @author Mark Seligman
 */

#include "rcppSample.h"
#include "callback.h"

using namespace std;
vector<unsigned int> CallBack::sampleRows(unsigned int nSamp) {
  IntegerVector rowSample(RcppSample::sampleRows(nSamp));

  vector<unsigned int> rowOut(rowSample.begin(), rowSample.end());
  return rowOut;
}


vector<double> CallBack::rUnif(size_t len) {
  RNGScope scope;
  NumericVector rn(runif(len));

  vector<double> rnOut(rn.begin(), rn.end());
  return rnOut;
}
