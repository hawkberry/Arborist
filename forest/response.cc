// This file is part of ArboristCore.

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
   @file response.cc

   @brief Methods involving response type.

   @author Mark Seligman
 */

#include "predict.h"
#include "response.h"
#include "sample.h"
#include "sampler.h"

#include <algorithm>


Response::Response() {
}


ResponseReg::ResponseReg(const vector<double>& y_) :
  Response(),
  yTrain(y_),
  defaultPrediction(meanTrain()) {
}


ResponseCtg::ResponseCtg(const vector<PredictorT>& yCtg_,
		 PredictorT nCtg_,
		 const vector<double>& classWeight_) :
  Response(),
  yCtg(yCtg_),
  nCtg(nCtg_),
  classWeight(classWeight_),
  defaultPrediction(ctgDefault()) {
}


ResponseCtg::ResponseCtg(const vector<PredictorT>& yCtg_,
		 PredictorT nCtg_) :
  Response(),
  yCtg(yCtg_),
  nCtg(nCtg_),
  classWeight(vector<double>(0)),
  defaultPrediction(ctgDefault()) {
}


unique_ptr<ResponseCtg> Response::factoryCtg(const vector<PredictorT>& yCtg,
				     PredictorT nCtg,
				     const vector<double>& classWeight) {
  return make_unique<ResponseCtg>(yCtg, nCtg, classWeight);
}


unique_ptr<ResponseCtg> Response::factoryCtg(const vector<PredictorT>& yCtg,
				     PredictorT nCtg) {
  return make_unique<ResponseCtg>(yCtg, nCtg);
}


unique_ptr<ResponseReg> Response::factoryReg(const vector<double>& yTrain) {
  return make_unique<ResponseReg>(yTrain);
}

  
PredictorT ResponseCtg::ctgDefault() const {
  vector<double> probDefault = defaultProb();
  return max_element(probDefault.begin(), probDefault.end()) - probDefault.begin();
}

  

unique_ptr<Sample> ResponseReg::rootSample(const Sampler* sampler) const {
  return Sample::factoryReg(sampler, this, yTrain);
}


unique_ptr<Sample> ResponseCtg::rootSample(const Sampler* sampler) const {
  return Sample::factoryCtg(sampler, this, classWeight, yCtg);
}


double ResponseReg::predictObs(const Predict* predict, size_t row) const {
  double sumScore = 0.0;
  unsigned int nEst = 0;
  for (unsigned int tIdx = 0; tIdx < predict->getNTree(); tIdx++) {
    double score;
    if (predict->isLeafIdx(row, tIdx, score)) {
      nEst++;
      sumScore += score;
    }
  }
  return nEst > 0 ? sumScore / nEst : defaultPrediction;
}


PredictorT ResponseCtg::predictObs(const Predict* predict, size_t row, PredictorT* census) const {
  unsigned int nEst = 0;
  vector<double> ctgJitter(nCtg);
  for (unsigned int tIdx = 0; tIdx < predict->getNTree(); tIdx++) {
    double score;
    if (predict->isLeafIdx(row, tIdx, score)) {
      nEst++;
      PredictorT ctg = floor(score); // Truncates jittered score for indexing.
      census[ctg]++;
      ctgJitter[ctg] += score - ctg; // Accumulates category jitters.
    }
  }
  if (nEst == 0) { // Default category unity, all others zero.
    census[defaultPrediction] = 1;
  }

  return argMaxJitter(census, &ctgJitter[0]);
}


PredictorT ResponseCtg::argMaxJitter(const IndexT* census,
				 const double* ctgJitter) const {
  PredictorT argMax = 0;
  IndexT countMax = 0;
  // Assumes at least one slot has nonzero count.
  for (PredictorT ctg = 0; ctg < nCtg; ctg++) {
    IndexT count = census[ctg];
    if (count == 0)
      continue;
    else if (count > countMax) {
      countMax = count;
      argMax = ctg;
    }
    else if (count == countMax) {
      if (ctgJitter[ctg] > ctgJitter[argMax]) {
	argMax = ctg;
      }
    }
  }
  return argMax;
}


CtgProb::CtgProb(const Predict* predict,
		 const ResponseCtg* response,
		 const class Sampler* sampler,
		 bool doProb) :
  nCtg(response->getNCtg()),
  probDefault(response->defaultProb()),
  probs(vector<double>(doProb ? predict->getNRow() * nCtg : 0)) {
}


void CtgProb::predictRow(const Predict* predict, size_t row, PredictorT* ctgRow) {
  unsigned int nEst = accumulate(ctgRow, ctgRow + nCtg, 0ul);
  double* probRow = &probs[row * nCtg];
  if (nEst == 0) {
    applyDefault(probRow);
  }
  else {
    double scale = 1.0 / nEst;
    for (PredictorT ctg = 0; ctg < nCtg; ctg++)
      probRow[ctg] = ctgRow[ctg] * scale;
  }
}


vector<double> ResponseCtg::defaultProb() const {
  // Uses the ECDF as the default distribution.
  vector<IndexT> ctgTot(nCtg);
  for (auto ctg : yCtg) {
    ctgTot[ctg]++;
  }

  vector<double> ctgDefault(nCtg);
  double scale = 1.0 / yCtg.size();
  for (PredictorT ctg = 0; ctg < nCtg; ctg++) {
    ctgDefault[ctg] = ctgTot[ctg] * scale;
  }
  return ctgDefault;
}


void CtgProb::applyDefault(double probPredict[]) const {
  for (PredictorT ctg = 0; ctg < nCtg; ctg++) {
    probPredict[ctg] = probDefault[ctg];
  }
}


void CtgProb::dump() const {
  // TODO
}
