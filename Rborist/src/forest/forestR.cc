// Copyright (C)  2012-2022   Mark Seligman
//
// This file is part of rfR.
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
   @file forestR.cc

   @brief C++ interface to R entry for Forest methods.

   @author Mark Seligman
 */

#include "resizeR.h"
#include "forestR.h"
#include "forestbridge.h"


const string FBTrain::strNTree = "nTree";
const string FBTrain::strNode = "node";
const string FBTrain::strExtent = "extent";
const string FBTrain::strTreeNode = "treeNode";
const string FBTrain::strScores= "scores";
const string FBTrain::strFactor = "factor";
const string FBTrain::strFacSplit = "facSplit";
const string FBTrain::strObserved = "observed";


FBTrain::FBTrain(unsigned int nTree_) :
  nTree(nTree_),
  nodeExtent(NumericVector(nTree)),
  nodeTop(0),
  scores(NumericVector(0)),
  facExtent(NumericVector(nTree)),
  facTop(0),
  facRaw(RawVector(0)) {
}


void FBTrain::bridgeConsume(const ForestBridge& bridge,
			    unsigned int tIdx,
			    double scale) {
  nodeConsume(bridge, tIdx, scale);
  factorConsume(bridge, tIdx, scale);
}


void FBTrain::nodeConsume(const ForestBridge& bridge,
			  unsigned int tIdx,
			  double scale) {
  const vector<size_t>&nExtents = bridge.getNodeExtents();
  unsigned int fromIdx = 0;
  for (unsigned int toIdx = tIdx; toIdx < tIdx + nExtents.size(); toIdx++) {
    nodeExtent[toIdx] = nExtents[fromIdx++];
  }

  size_t nodeCount = bridge.getNodeCount();
  if (nodeTop + nodeCount > static_cast<size_t>(cNode.length())) {
    cNode = move(ResizeR::resize<ComplexVector>(cNode, nodeTop, nodeCount, scale));
    scores = move(ResizeR::resize<NumericVector>(scores, nodeTop, nodeCount, scale));
  }
  bridge.dumpTree((complex<double>*)&cNode[nodeTop]);
  bridge.dumpScore(&scores[nodeTop]);
  nodeTop += nodeCount;
}


void FBTrain::factorConsume(const ForestBridge& bridge,
			    unsigned int tIdx,
			    double scale) {
  const vector<size_t>& fExtents = bridge.getFacExtents();
  unsigned int fromIdx = 0;
  for (unsigned int toIdx = tIdx; toIdx < tIdx + fExtents.size(); toIdx++) {
    facExtent[toIdx] = fExtents[fromIdx++];
  }
 
  size_t facBytes = bridge.getFactorBytes();
  if (facTop + facBytes > static_cast<size_t>(facRaw.length())) {
    facRaw = move(ResizeR::resize<RawVector>(facRaw, facTop, facBytes, scale));
    facObserved = move(ResizeR::resize<RawVector>(facObserved, facTop, facBytes, scale));
  }
  bridge.dumpFactorRaw(&facRaw[facTop]);
  bridge.dumpFactorObserved(&facObserved[facTop]);
  facTop += facBytes;
}


List FBTrain::wrapNode() {
  BEGIN_RCPP
  List wrappedNode = List::create(_[strTreeNode] = move(cNode),
				  _[strExtent] = move(nodeExtent)
				  );
  wrappedNode.attr("class") = "Node";
  return wrappedNode;
  END_RCPP
}


List FBTrain::wrapFactor() {
  BEGIN_RCPP
    List wrappedFactor = List::create(_[strFacSplit] = move(facRaw),
				      _[strExtent] = move(facExtent),
				      _[strObserved] = move(facObserved)
				      );
  wrappedFactor.attr("class") = "Factor";

  return wrappedFactor;
  END_RCPP
}


List FBTrain::wrap() {
  BEGIN_RCPP
  List forest =
    List::create(_[strNTree] = nTree,
		 _[strNode] = move(wrapNode()),
		 _[strScores] = move(scores),
		 _[strFactor] = move(wrapFactor())
                 );
  cNode = ComplexVector(0);
  scores = NumericVector(0);
  facRaw = RawVector(0);
  facObserved = RawVector(0);
  forest.attr("class") = "Forest";

  return forest;
  END_RCPP
}


unique_ptr<ForestBridge> ForestRf::unwrap(const List& lTrain) {
  List lForest(checkForest(lTrain));
  List lNode((SEXP) lForest[FBTrain::strNode]);
  List lFactor((SEXP) lForest[FBTrain::strFactor]);
  return make_unique<ForestBridge>(as<unsigned int>(lForest[FBTrain::strNTree]),
				   as<NumericVector>(lNode[FBTrain::strExtent]).begin(),
				   (complex<double>*) as<ComplexVector>(lNode[FBTrain::strTreeNode]).begin(),
				   as<NumericVector>(lForest[FBTrain::strScores]).begin(),
				   as<NumericVector>(lFactor[FBTrain::strExtent]).begin(),
				   as<RawVector>(lFactor[FBTrain::strFacSplit]).begin());
}


List ForestRf::checkForest(const List& lTrain) {
  BEGIN_RCPP

  List lForest((SEXP) lTrain["forest"]);
  if (!lForest.inherits("Forest")) {
    stop("Expecting Forest");
  }
  return lForest;
  
  END_RCPP
}


unique_ptr<ForestExport> ForestExport::unwrap(const List& lTrain,
                                              const IntegerVector& predMap) {
  (void) ForestRf::checkForest(lTrain);
  return make_unique<ForestExport>(lTrain, predMap);
}


ForestExport::ForestExport(const List &lTrain,
                           const IntegerVector &predMap) :
  forestBridge(ForestRf::unwrap(lTrain)),
  predTree(vector<vector<unsigned int> >(forestBridge->getNTree())),
  bumpTree(vector<vector<double> >(forestBridge->getNTree())),
  splitTree(vector<vector<double > >(forestBridge->getNTree())),
  facSplitTree(vector<vector<unsigned char> >(forestBridge->getNTree())) {
  forestBridge->dump(predTree, splitTree, bumpTree, facSplitTree);
  predExport(predMap.begin());
}


unsigned int ForestExport::getNTree() const {
  return forestBridge->getNTree();
}


/**
   @brief Recasts 'pred' field of nonterminals to front-end facing values.

   @return void.
 */
void ForestExport::treeExport(const int predMap[],
                            vector<unsigned int> &pred,
                            const vector<double>& bump) {
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
void ForestExport::predExport(const int predMap[]) {
  for (unsigned int tIdx = 0; tIdx < predTree.size(); tIdx++) {
    treeExport(predMap, predTree[tIdx], bumpTree[tIdx]);
  }
}
