// Copyright (C)  2012-2018   Mark Seligman
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
   @file forestBridge.cc

   @brief C++ interface to R entry for Forest methods.

   @author Mark Seligman
 */

#include "forestBridge.h"
#include "forest.h"

List ForestBridge::Wrap(const ForestTrain *forestTrain) {
  RawVector nodeRaw(forestTrain->NodeBytes());
  RawVector facRaw(forestTrain->FacBytes());
  forestTrain->NodeRaw(&nodeRaw[0]);
  forestTrain->FacRaw(&facRaw[0]);
  
  List forest = List::create(
     _["forestNode"] = nodeRaw,
     _["origin"] = forestTrain->TreeOrigin(),
     _["facOrig"] = forestTrain->FacOrigin(),
     _["facSplit"] = facRaw);
  forest.attr("class") = "Forest";

  return forest;
}


unique_ptr<ForestBridge> ForestBridge::Unwrap(SEXP sForest) {
  List forest = Legal(sForest);
  return make_unique<ForestBridge>(IntegerVector((SEXP) forest["origin"]),
			  RawVector((SEXP) forest["facSplit"]),
			  IntegerVector((SEXP) forest["facOrig"]),
			  RawVector((SEXP) forest["forestNode"]));
}


List ForestBridge::Legal(SEXP sForest) {
  BEGIN_RCPP
  List forest(sForest);

  if (!forest.inherits("Forest")) {
    stop("Expecting Forest");
  }

  return forest;
  
  END_RCPP
}


// Alignment should be sufficient to guarantee safety of
// the casted loads.
ForestBridge::ForestBridge(const IntegerVector &_feOrigin,
			   const RawVector &_feFacSplit,
			   const IntegerVector &_feFacOrig,
			   const RawVector &_feNode) :
  feOrigin(_feOrigin),
  feFacSplit(_feFacSplit),
  feFacOrig(_feFacOrig),
  feNode(_feNode),
  forest(move(make_unique<Forest>((ForestNode*) &feNode[0],
				  feNode.length() / sizeof(ForestNode),
				  (unsigned int *) &feOrigin[0],
				  feOrigin.length(),
				  (unsigned int *) &feFacSplit[0],
				  feFacSplit.length() / sizeof(unsigned int),
				  (unsigned int*) &feFacOrig[0],
				  feFacOrig.length()
				  ))) {
}


unique_ptr<ForestExport> ForestExport::Unwrap(SEXP sForest,
					      IntegerVector &predMap) {
  List forestList = Legal(sForest);
  return make_unique<ForestExport>(forestList, predMap);
}


ForestExport::ForestExport(List &forestList, IntegerVector &predMap) :
  ForestBridge(IntegerVector((SEXP) forestList["origin"]),
	       RawVector((SEXP) forestList["facSplit"]),
	       IntegerVector((SEXP) forestList["facOrig"]),
	       RawVector((SEXP) forestList["forestNode"])),
  nTree(as<unsigned int>(forestList["nTree"])),
  predTree(vector<vector<unsigned int> >(nTree)),
  bumpTree(vector<vector<unsigned int> >(nTree)),
  splitTree(vector<vector<double > >( nTree)),
  facSplitTree(vector<vector<unsigned int> >(nTree)) {
  forest->Export(predTree, splitTree, bumpTree, facSplitTree);
    PredExport(predMap.begin());
}


/**
   @brief Recasts 'pred' field of nonterminals to front-end facing values.

   @return void.
 */
void ForestExport::PredTree(const int predMap[],
			    vector<unsigned int> &pred,
			    const vector<unsigned int> &bump) {
  for (unsigned int i = 0; i < pred.size(); i++) {
    if (bump[i] > 0) { // terminal 'pred' values do not reference predictors.
      unsigned int predCore = pred[i];
      pred[i] = predMap[predCore];
    }
  }
}


/**
   @brief Prepares predictor field for export by remapping to front-end indices.
 */
void ForestExport::PredExport(const int predMap[]) {
  for (unsigned int tIdx = 0; tIdx < predTree.size(); tIdx++) {
    PredTree(predMap, predTree[tIdx], bumpTree[tIdx]);
  }
}
