// This file is part of ArboristCore.

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
   @file splitnux.cc

   @brief Methods belonging to the minimal splitting representation.

   @author Mark Seligman
 */

#include "frontier.h"
#include "splitnux.h"
#include "splitcand.h"
#include "summaryframe.h"

double SplitNux::minRatio = minRatioDefault;

void SplitNux::immutables(double minRatio) {
  SplitNux::minRatio = minRatio;
}

void SplitNux::deImmutables() {
  minRatio = minRatioDefault;
}


PredictorT SplitNux::getCardinality(const SummaryFrame* frame) const {
  return frame->getCardinality(splitCoord.predIdx);
}


void SplitNux::consume(IndexSet* iSet) const {
  iSet->consumeCriterion(minRatio * info, lhSCount, lhExtent);
}
