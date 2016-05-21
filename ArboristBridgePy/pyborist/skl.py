import numpy as np

from .cyrowrank import PyRowRank
from .cytrain import PyTrain
from .cypredict import PyPredict

__all__ = ['PyboristModel']



class PyboristModel(object):
    """The scikit-learn API for Pyborist model.

    Parameters
    ----------
    n_estimators: int, optional (default=10)
        The number of trees to train.

    bootstrap: bool, optional (defualt=True)
        Thether row sampling is by replacement.

    class_weight: str or None or array-like, optional (default=None)
        Proportional weighting of classification categories.
        Could use None or `balance`, or an array

    min_info_ratio: float, optional (default=0.01)
        Information ratio with parent below which node does not split.

    min_samples_split: int, optional (default=2 for regression or 5 for classification)
        Minimum number of distinct row references to split a node.

    max_depth: int or None, optional (default=0)
        Maximum number of tree levels to train. Zero denotes no limit.

    no_validate: bool, optional (default=True)
        Whether to train without validation.

    n_to_sample: int, optional (default=0)
        Number of rows to sample, per tree.

    pred_fixed: int, optional (default=0)
        Number of trial predictors for a split (`mtry`).

    pred_prob: float, optional (default=0.0)
        Probability of selecting individual predictor as trial splitter.
        Causes each predictor to be selected as a splitting candidate with distribution Bernoulli(pred_prob).

    quantiles_arr: array-like or None (dafault=None)
        Quantile levels to validate.

    q_bin: int, optional (default=5000)
        Bin size for facilating quantiles at large sample count.

    reg_mono: array-like or None, optional (default=None)
        Signed probability constraint for monotonic regression.

    tree_block: int, optional (default=1)
        Maximum number of trees to train during a single level (e.g., coprocessor computing).

    pvt_block: int, optional (default=8)
        Maximum number of trees to train in a block (e.g., cluster computing).

    is_classify_task: bool, optional (default = False)
        Regression or Classification?

    Attributes
    ----------
    n_features_ : int
        The number of features.

    n_outputs_ : int
        The number of outputs.

    """
    def __init__(self,
        n_estimators = 10,
        bootstrap = True,
        class_weight = None,
        min_info_ratio = 0.01,
        min_samples_split = 0,
        max_depth = 0,
        no_validate = True,
        n_to_sample = 0,
        pred_fixed = 0,
        pred_prob = 0.0,
        quantiles_arr = None,
        q_bin = 5000,
        reg_mono = None,
        tree_block = 1,
        pvt_block = 8,
        is_classify_task = False):
        # a trick to save everything into self...
        for k, v in locals().items():
            if k == 'self':
                continue
            setattr(self, k, v)

        if self.min_samples_split == 0:
            if self.is_classify_task:
                self.min_samples_split = 2
            else:
                self.min_samples_split = 5
        if self.min_samples_split <= 0:
            raise ValueError('Invalid min_samples_split.')

        if self.is_classify_task and self.quantiles_arr is not None:
            raise ValueError('Quantiles are for regression only.')
        if self.quantiles_arr is not None:
            if np.any(self.quantiles_arr < 0.0) or np.any(self.quantiles_arr > 1.0):
                raise ValueError('Quantiles shoule be inside 0 and 1.')
            if np.any(np.diff(self.quantiles_arr) < 0.0):
                raise ValueError('Quantiles should be increasing.')

        if self.pred_prob < 0.0 or self.pred_prob > 1.0:
            raise ValueError('pred_prob should be inside [0.0, 1.0].')


    def fit(self,
        X,
        y,
        sample_weight = None,
        feature_weight = None):
        """Fit estimator.

        Parameters
        ----------
        X : array-like, shape=(n_samples, n_features)
            The input samples.

        y: array-like, shape=(n_samples)

        Returns
        -------
        self : object
            Returns self.
        """
        X = X.astype(np.double, copy=False)
        if self.is_classify_task:
            y = y.astype(np.uintc, copy=False)
        else:
            y = y.astype(np.double, copy=False)

        self._init_basic_attrbutes(X, y, sample_weight, feature_weight)
        self._init_row_rank()
        self._adjust_model_params()

        if self.is_classify_task:
            self._train_classification()
        else:
            self._train_regression()

        return self


    def _init_basic_attrbutes(self,
        X,
        y,
        sample_weight = None,
        feature_weight = None):
        """
        Parameters
        ----------
        X : array-like, shape=(n_samples, n_features)
            The input samples.

        y: array-like, shape=(n_samples)

        Attributes
        ----------
        X : array-like, shape=(n_samples, n_features)
            The input samples.

        y: array-like, shape=(n_samples)

        sample_weight: array-like, shape=(n_samples) or None

        feature_weight: array-like, shape=(n_features) or None
            Relative weighting of individual predictors as trial splitters.

        n_samples: int (defualt = 0)
            Number of rows

        n_features: int (default = 0)
            The number of predictors.

        Returns
        -------
        self : object
            Returns self.
        """
        self.X = X
        self.y = y
        self.sample_weight = sample_weight
        self.feature_weight = feature_weight
        self.n_samples = X.shape[0]
        self.n_features = X.shape[1]

        if self.n_features <= 0 or self.n_samples <= 0:
            raise ValueError('Invalid design matrix.')

        if self.sample_weight is None:
            self.sample_weight = np.ones(self.n_samples)
        else:
            if len(self.sample_weight) != self.n_samples:
                raise ValueError('Sample weight should equal to column number.')
            if np.any(self.sample_weight < 0.0):
                raise ValueError('Sample weight should larger than zero.')
            if np.sum(self.sample_weight) == 0.0:
                raise ValueError('Sample weight could not be all zero.')

        if self.feature_weight is None:
            self.feature_weight = np.ones(self.n_features)
        else:
            if len(self.feature_weight) != self.n_features:
                raise ValueError('Predictor weight should equal to column number.')
            if np.any(self.feature_weight < 0.0):
                raise ValueError('Predictor weight should larger than zero.')
            if np.sum(self.feature_weight) == 0.0:
                raise ValueError('Predictor weight could not be all zero.')

        return self


    def _init_row_rank(self):
        """Call the backend to generate the rowrank. Similar to the R verson.

        Attributes
        ----------
        row: array-like, shape=(n_samples * n_features)

        rank: array-like, shape=(n_samples * n_features)

        inv_num: array-like, shape=(n_samples * n_features)

        Returns
        -------
        self : object
            Returns self.
        """
        n_samples = self.n_samples
        n_features = self.n_features
        rank = np.empty([n_samples * n_features], dtype=np.intc)
        row = np.empty([n_samples * n_features], dtype=np.intc)
        inv_num = np.zeros([n_samples * n_features], dtype=np.intc)
        PyRowRank.PreSortNum(np.ascontiguousarray(self.X.transpose().reshape(self.X.size)), # transpose() to become consistent with rcpp reuslt
            n_features,
            n_samples,
            row,
            rank,
            inv_num)
        self.row = row
        self.rank = rank
        self.inv_num = inv_num
        return self


    def _adjust_model_params(self):
        """Regenerate some parameters of model based on input.

        Attributes
        ----------
        prob_arr: array-like, shape=(n_features)

        Returns
        -------
        self : object
            Returns self.
        """
        #TODO params should be invariant
        if self.n_to_sample == 0:
            if self.bootstrap:
                self.n_to_sample = self.n_samples
            else:
                self.n_to_sample = np.round((1-np.exp(-1)) * self.n_samples)

        if self.pred_fixed == 0 \
            and self.pred_prob == 0 and self.n_features < 16:
            if not self.is_classify_task:
                self.pred_fixed = np.max([np.floor(self.n_features/3), 1])
            else:
                self.pred_fixed = np.floor(np.sqrt(self.n_features))

        if self.pred_prob == 0.0 and self.pred_fixed == 0:
            if not self.is_classify_task:
                self.pred_prob = 0.4
            else:
                self.pred_prob = np.ceil(np.sqrt(self.n_features)) / self.n_features

        if self.pred_fixed > self.n_features:
            raise ValueError('pred_fixed should be no more than n_features.')

        if self.is_classify_task:
            ctg_width = np.max(self.y) + 1 # how many categories
            if self.class_weight is None:
                self.class_weight = np.ones(ctg_width)
            elif self.class_weight == 'balanced':
                uniqueClasses, occurrences = np.unique(y, return_counts=True)
                assert(len(uniqueClasses) == ctgWidth)
                self.class_weight = 1.0 / occurrences
                self.class_weight[np.isinf(self.class_weight)] = 0.0
            elif len(self.class_weight) != class_weight:
                raise ValueError('Invalid class weighting.')
            elif np.any(self.class_weight < 0.0):
                raise ValueError('Invalid class weighting.')
            elif np.sum(self.class_weight) == 0.0:
                raise ValueError('Invalid class weighting.')
            else:
                raise ValueError('Invalid class weighting.')
            self.class_weight = self.class_weight / np.sum(self.class_weight)
            self.class_weight_jittered = (self.class_weight[self.y] + 
                (np.random.uniform(size=self.n_samples) - 0.5) * 
                0.5 / self.n_samples / self.n_samples).astype(np.double)
        else:
            #TODO maybe move some code into __init__
            if self.class_weight is not None:
                raise ValueError('Class Weight should be used in classification.')

        mean_weight = 1.0 if self.pred_prob == 0.0 else self.pred_prob
        self.prob_arr = self.feature_weight * (self.n_features * mean_weight) / np.sum(self.feature_weight)

        if self.min_samples_split > self.n_samples:
            raise ValueError('Invalid min_samples_split.')

        if self.reg_mono is None:
            self.reg_mono = np.zeros(self.n_features)
        if self.is_classify_task:
            if np.any(self.reg_mono != 0.0):
                raise ValueError('Categorical response could not have reg_mono.')

        return self


    def _train_regression(self):
        """
        Returns
        -------
        self : object
            Returns self.
        """
        result = PyTrain.Regression(
            np.ascontiguousarray(self.X.transpose().reshape(self.X.size)),
            np.ascontiguousarray(np.reshape(self.y, self.n_samples)),
            self.n_samples,
            self.n_features,
            np.ascontiguousarray(self.row),
            np.ascontiguousarray(self.rank),
            np.ascontiguousarray(self.inv_num),
            self.n_estimators,
            self.n_to_sample,
            np.ascontiguousarray(np.reshape(self.sample_weight, self.n_samples)),
            self.bootstrap,
            self.tree_block,
            self.min_samples_split,
            self.min_info_ratio,
            self.max_depth,
            self.pred_fixed,
            np.ascontiguousarray(np.reshape(self.prob_arr, self.n_features)),
            np.ascontiguousarray(np.reshape(self.reg_mono, self.n_features))
        )
        self.trained_result = result
        return self

    def _train_classification(self):
        """
        Returns
        -------
        self : object
            Returns self.
        """
        result = PyTrain.Classification(
            np.ascontiguousarray(self.X.transpose().reshape(self.X.size)),
            np.ascontiguousarray(np.reshape(self.y, self.n_samples)),
            self.n_samples,
            self.n_features,
            np.ascontiguousarray(self.row),
            np.ascontiguousarray(self.rank),
            np.ascontiguousarray(self.inv_num),
            self.n_estimators,
            self.n_to_sample,
            np.ascontiguousarray(np.reshape(self.sample_weight, self.n_samples)),
            self.bootstrap,
            self.tree_block,
            self.min_samples_split,
            self.min_info_ratio,
            self.max_depth,
            self.pred_fixed,
            np.ascontiguousarray(np.reshape(self.prob_arr, self.n_features)),
            np.ascontiguousarray(self.class_weight_jittered)
        )
        self.trained_result = result
        return self


    def _valid_trained_result(self):
        """
        validate the training result

        Returns
        -------
        self : object
            Returns self.
        """
        if self.no_validate:
            return self
        return self


    def predict(self, X):
        """Fit estimator.

        Parameters
        ----------
        X : array-like, shape=(n_samples, n_features)
            The input samples.

        Returns
        -------
        y_pred : array-like, shape=(n_samples)
            Returns the predicted result.
        """
        if self.is_classify_task:
            self.y_pred, self.y_pred_votes, self.y_pred_proba = self._predict_classification(X)
            return self.y_pred
        else:
            self.y_pred = self._predict_regression(X)
            return self.y_pred

    def predict_proba(self, X, return_votes=False):
        """Fit estimator for classification.

        Parameters
        ----------
        X : array-like, shape=(n_samples, n_features)
            The input samples.

        return_votes: bool, optional (defualt=False)
            Whether return votes or probilities.

        Returns
        -------
        y_pred_proba or y_pred_votes: array-like, shape=(n_samples, n_classes)
            Returns the predicted result, in probilities or votes.
        """
        if not self.is_classify_task:
            raise AttributeError('This function is used in classification.')
        self.predict(X)
        if return_votes:
            return self.y_pred_votes
        return self.y_pred_proba


    def _predict_regression(self, X):
        result = PyPredict.Regression(np.ascontiguousarray(X.reshape(X.size)),
            X.shape[0],
            X.shape[1],
            self.trained_result['forest']['origin'],
            self.trained_result['forest']['facOrig'],
            self.trained_result['forest']['facSplit'],
            self.trained_result['forest']['forestNode'],
            self.trained_result['leaf']['yRanked'],
            self.trained_result['leaf']['leafOrigin'],
            self.trained_result['leaf']['leafNode'],
            self.trained_result['leaf']['bagRow'],
            self.trained_result['leaf']['nRow'],
            self.trained_result['leaf']['rank']
        )
        return result


    def _predict_classification(self, X):
        result = PyPredict.Classification(np.ascontiguousarray(X.reshape(X.size)),
            X.shape[0],
            X.shape[1],
            np.max(self.y) + 1, # how many categories
            self.trained_result['forest']['origin'],
            self.trained_result['forest']['facOrig'],
            self.trained_result['forest']['facSplit'],
            self.trained_result['forest']['forestNode'],
            self.trained_result['leaf']['yLevels'],
            self.trained_result['leaf']['leafOrigin'],
            self.trained_result['leaf']['leafNode'],
            self.trained_result['leaf']['bagRow'],
            self.trained_result['leaf']['nRow'],
            self.trained_result['leaf']['weight']
        )
        return result
