// Copyright (C)  2012-2022  Mark Seligman
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
   @file forestRf.H

   @brief Bridge access to core Forest type for Random Forest algorithm.

   @author Mark Seligman

 */

#ifndef RF_FOREST_R_H
#define RF_FOREST_R_H


#include <Rcpp.h>
using namespace Rcpp;

#include <memory>
#include <vector>
using namespace std;

/**
   @brief Front-end access to ForestBridge.
 */
struct ForestRf {

  /**
     @brief Looks up and verifies forest member.

     @return forest member.
   */
  static List checkForest(const List& lTrain);

  
  /**
     @brief Factory incorporating trained forest cached by front end.

     @param sTrain is an R-stye List node containing forest vectors.

     @return bridge specialization of Forest prediction type.
  */
  static unique_ptr<struct ForestBridge> unwrap(const List &sTrain);
};


/**
   @brief As above, but with additional members to facilitate dumping on
   a per-tree basis.
 */
class ForestExport {
  unique_ptr<struct ForestBridge> forestBridge;
  vector<vector<unsigned int> > predTree;
  vector<vector<unsigned int> > bumpTree;
  vector<vector<double > > splitTree;
  vector<vector<unsigned int> > facSplitTree;

  void predExport(const int predMap[]);
  void treeExport(const int predMap[],
                  vector<unsigned int> &pred,
                  const vector<unsigned int> &bump);

 public:
  ForestExport(const List& forestList,
               const IntegerVector& predMap);

  static unique_ptr<ForestExport> unwrap(const List &lTrain,
                                         const IntegerVector &predMap);

  unsigned int getNTree() const;
  
  /**
     @brief Exportation methods for unpacking per-tree node contents
     as separate vectors.

     @param tIdx is the tree index.

     @return vector of unpacked values.
   */
  const vector<unsigned int> &getPredTree(unsigned int tIdx) const {
    return predTree[tIdx];
  }

  const vector<unsigned int> &getBumpTree(unsigned int tIdx) const {
    return bumpTree[tIdx];
  }

  const vector<double> &getSplitTree(unsigned int tIdx) const {
    return splitTree[tIdx];
  }

  const vector<unsigned int> &getFacSplitTree(unsigned int tIdx) const {
    return facSplitTree[tIdx];
  }
};


/**
   @brief Accumulates R-style representation of crescent forest during
   training.
 */
struct FBTrain {
  static const string strNTree;
  static const string strNode;
  static const string strExtent;
  static const string strTreeNode;
  static const string strScores;
  static const string strFactor;
  static const string strFacSplit;
  static const string strObserved;

  const unsigned int nTree; // Total # trees under training.

  // Decision node related:
  NumericVector nodeExtent; // # nodes in respective tree.
  size_t nodeTop; // Next available index in node/score buffers.
  ComplexVector cNode; // Nodes encoded as complex pairs.
  NumericVector scores; // Same indices as nodeRaw.

  // Factor related:
  NumericVector facExtent; // # factor entries in respective tree.
  size_t facTop; // Next available index in factor buffer.
  RawVector facRaw; // Bit-vector representation of factor splits.
  RawVector facObserved; // " " observed levels.

  FBTrain(unsigned int nTree);


  /**
     @brief Decorates trained forest for storage by front end.
   */
  List wrap();


  /**
     @brief Copies core representation of forest components.

     @param bridge caches a crescent forest chunk.

     @param treeOff is the beginning tree index of the trained chunk.

     @param fraction is a scaling factor used to estimate buffer size.
   */
  void bridgeConsume(const struct ForestBridge& bridge,
		     unsigned int treeOff,
		     double fraction);


private:

  List wrapFactor();

  List wrapNode();
  /**
     @brief Copies core representation of a chunk of trained tree nodes.

     @param bridge caches a crescent forest chunk.

     @param treeOff is the beginning tree index of the trained chunk.

     @param fraction is a scaling factor used to estimate buffer size.
   */
  void nodeConsume(const struct ForestBridge& bridge,
		   unsigned int treeOff,
		   double fraction);


  /**
     @brief As above, but collects factor-splitting parameters.
   */
  void factorConsume(const struct ForestBridge& bridge,
		     unsigned int treeOff,
		     double fraction);
};

#endif
