// This file is part of ArboristCore.

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
   @file path.h

   @brief Definitions for the classes managing paths from index sets
   and to indiviual indices.

   @author Mark Seligman
 */

#ifndef CORE_PATH_H
#define CORE_PATH_H

#include <cstdint>
#include <vector>

#include "typeparam.h"

/**
   @brief Records index, start and extent for path reached from MRRA.
 */
class NodePath {
  static constexpr unsigned int logPathMax = 8 * sizeof(PathT) - 1;
  // Maximal path length is also an inattainable path index.
  static constexpr unsigned int noPath = 1 << logPathMax;

  unsigned int splitIdx; // < noIndex iff path extinct.
  unsigned int idxStart; // Target offset for path.
  unsigned int extent;
  unsigned int relBase; // Dense starting position.
 public:

  /**
     @return maximal path length.
   */
  inline static constexpr unsigned int pathMax() {
    return noPath;
  }


  /**
     @brief Determines whether a path size is representable within
     container.

     @param pathSize is the path size in question.

     @return true iff path size does not exceed maximum representable.
   */
  inline static bool isRepresentable(unsigned int pathSize) {
    return pathSize <= logPathMax;
  }

  
  /**
     @brief Determines whether a path is active.

     @param path is the path in in question.

     @return true iff path is active.
   */
  static inline bool isActive(unsigned int path) {
    return path != noPath;
  }
  
  /**
     @brief Sets to non-extinct path coordinates.
   */
  inline void init(unsigned int _splitIdx,
                   unsigned int _idxStart,
                   unsigned int _extent,
                   unsigned int _relBase) {
    splitIdx = _splitIdx;
    idxStart = _idxStart;
    extent = _extent;
    relBase = _relBase;
  }
  

  /**
     @brief Multiple accessor for path coordinates.
   */
  inline void getCoords(unsigned int &_splitIdx,
                        unsigned int &_idxStart,
                        unsigned int &_extent) const {
    _splitIdx = splitIdx;
    _idxStart = idxStart;
    _extent = extent;
  }

  
  inline unsigned int IdxStart() const {
    return idxStart;
  }


  inline unsigned int Extent() const {
    return extent;
  }
  

  inline unsigned int RelBase() const {
    return relBase;
  }


  inline unsigned int Idx() const {
    return splitIdx;
  }
};


class IdxPath {
  const unsigned int idxLive; // Inattainable index.
  static constexpr unsigned int noPath = NodePath::pathMax();
  static constexpr unsigned int maskExtinct = noPath;
  static constexpr unsigned int maskLive = maskExtinct - 1;
  static constexpr unsigned int relMax = 1 << 15;
  vector<unsigned int> relFront;
  vector<unsigned char> pathFront;

  /**
     @brief Setter for path reaching an index.

     @param idx is the index in question.

     @param path is the reaching path.
   */
  inline void set(unsigned int idx, unsigned int path = maskExtinct) {
    pathFront[idx] = path;
  }

  /**
   */
  inline void set(unsigned int idx,
                  unsigned int path,
                  unsigned int relThis,
                  unsigned int ndOff = 0) {
    pathFront[idx] = path;
    relFront[idx] = relThis;
    offFront[idx] = ndOff;
  }


  inline PathT PathSucc(unsigned int idx,
                        unsigned int pathMask,
                        bool &isLive_) const {
    isLive_ = isLive(idx);
    return isLive_ ? pathFront[idx] & pathMask : noPath;
  }
  
  
  /**
     @brief Determines whether indexed path is live and looks up
     corresponding front index.

     @param idx is the element index.

     @param front outputs the front index, if live.

     @return true iff path live.
   */
  inline bool frontLive(unsigned int idx, unsigned int &front) const {
    if (!isLive(idx)) {
      return false;
    }
    else {
      front = relFront[idx];
      return true;
    }
  }

  
  /**
     @brief Determines a sample's coordinates with respect to the front level.

     @param idx is the node-relative index of the sample.

     @param path outputs the path offset of the sample in the front level.

     @return true iff contents copied.
   */
  inline bool copyLive(IdxPath *backRef,
                       unsigned int idx,
                       unsigned int backIdx) const {
    if (!isLive(idx)) {
      return false;
    }
    else {
      backRef->set(backIdx, pathFront[idx], relFront[idx], offFront[idx]);
      return true;
    }
  }

  
  // Only defined for enclosing Levels employing node-relative indexing.
  //
  // Narrow for data locality, but wide enough to be useful.  Can
  // be generalized to multiple sizes to accommodate more sophisticated
  // hierarchies.
  //
  vector<uint_least16_t> offFront;
  
 public:

  IdxPath(unsigned int _idxLive);

  /**
     @brief When appropriate, introduces node-relative indexing at the
     cost of trebling span of memory accesses:  char vs. char + uint16.

     @return True iff node-relative indexing expected to be profitable.
   */
  static inline bool localizes(unsigned int bagCount, unsigned int idxMax) {
    return idxMax > relMax || bagCount <= 3 * relMax ? false : true;
  }

  
  /**
     @brief Setter for path reaching an index.

     @param idx is the index in question.

     @param path is the reaching path.

     @param doesReach indicates whether the path reaches an actual successor.
   */
  inline void setSuccessor(unsigned int idx,
                           unsigned int pathSucc,
                           bool doesReach) {
    set(idx, doesReach ? pathSucc : noPath);
  }


  /**
     @brief Accumulates a path bit vector for a live reference.

     @return shift-stamped path if live else fixed extinct mask.
   */
  inline static unsigned int pathNext(unsigned int pathPrev, bool isLeft) {
    return maskLive & ((pathPrev << 1) | (isLeft ? 0 : 1));
  }
  

  /**
     @brief Revises path and target for live index.

     @param idx is the current index value.

     @param path is the revised path.

     @param targIdx is the revised relative index.
  */
  inline void setLive(unsigned int idx,
                      unsigned int path,
                      unsigned int targIdx) {
    set(idx, path, targIdx);
  }

  
  /**
     @brief Revises path and target for potentially node-relative live index.

     @param idx is the current index value.

     @param path

     @param targIdx is the revised index.
  */
  inline void setLive(unsigned int idx,
                      unsigned int path,
                      unsigned int targIdx,
                      unsigned int ndOff) {
    set(idx, path, targIdx, ndOff);
  }

  
  /**
     @brief Marks path as extinct, sets front index to inattainable value.
     Other values undefined.

     @param idx is the index in question.
   */
  inline void setExtinct(unsigned int idx) {
    set(idx, maskExtinct, idxLive);
  }


  /**
     @brief Indicates whether path reaching index is live.

     @param idx is the index in question.

     @return true iff reaching path is not extinct.
   */
  inline bool isLive(unsigned idx) const {
    return (pathFront[idx] & maskExtinct) == 0;
  }


  /**
     @brief Looks up the path leading to the front level and updates
     the index, if either in a switching to a node-relative regime.

     @param idx inputs the path vector index and outputs the index to
     be used in the next level.

     @return path to input index.
   */
  inline PathT update(unsigned int &idx, unsigned int pathMask, const unsigned int reachBase[], bool idxUpdate) const {
    bool isLive;
    PathT path = PathSucc(idx, pathMask, isLive);
    if (isLive) {
      // Avoids irregular update unless necessary:
      idx = reachBase != nullptr ? (reachBase[path] + offFront[idx]) : (idxUpdate ? relFront[idx] : idx);
    }

    return path;
  }


  /**
     @brief Resets front coordinates using first level's map.

     @param one2Front maps first level's coordinates to front.
   */
  inline void backdate(const IdxPath *one2Front) {
    for (unsigned int idx = 0; idx < idxLive; idx++) {
      unsigned int oneIdx;
      if (frontLive(idx, oneIdx)) {
        if (!one2Front->copyLive(this, oneIdx, idx)) {
	  setExtinct(idx);
	}
      }
    }
  }

};

#endif
