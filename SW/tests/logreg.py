from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import argparse
import os
import sys
from random import shuffle

import numpy as np
import pandas as pd
from sklearn.linear_model import LogisticRegression
from sklearn.preprocessing import normalize
from sklearn.learning_curve import learning_curve
from sklearn.metrics import log_loss

parser = argparse.ArgumentParser()

parser.add_argument(
	'--train_file',
	type=str,
	required=1,
	help='Absolute path to the raw train file.')
parser.add_argument(
	'--test_file',
	type=str,
	required=1,
	help='Absolute path to the raw test file.')
parser.add_argument(
	'--num_features',
	type=int,
	required=1,
	help='num_features')

args = parser.parse_args()

samples = np.fromfile(args.train_file, dtype=float)

print('len(samples): ' + str(len(samples)) )
print('len(samples)/(args.num_features+1): ' + str(len(samples)/(args.num_features+1)) )
num_samples = int(len(samples)/(args.num_features+1))
print('num_samples: ' + str(num_samples))

samples = np.reshape(samples, (num_samples, args.num_features+1))

X = samples[:,1:]
y = samples[:,0]

print('X.shape: ' + str(X.shape))
print('y.shape: ' + str(y.shape))

X_norm = X
maxes = np.zeros(X.shape[1])
for j in range(0, X.shape[1]):
	maxes[j] = np.max(X[:,j])
	if maxes[j] == 0:
		maxes[j] = 1
	X_norm[:,j] = np.divide(X[:,j], maxes[j])

for i in range(0,5):
	print('X_norm[i]: ' + str(X_norm[i,:]))
	print('y[i]: ' + str(y[i]))

logreg = LogisticRegression(penalty='l2', dual=False, tol=0.001, C=0.1, fit_intercept=True, intercept_scaling=1, class_weight=None, random_state=None, solver='lbfgs', max_iter=10, multi_class='ovr', verbose=0, warm_start=True, n_jobs=1)

for epoch in range(0,100):
	print("Start fitting..., epoch: " + str(epoch))
	logreg.fit(X_norm, y)
	loss = log_loss(y, logreg.predict_proba(X_norm))
	print('loss: ' + str(loss))

train_sizes, train_scores, test_scores = learning_curve(logreg, X_norm, y)
print(str(train_scores))



X_test = np.fromfile(args.test_file, dtype=float)
num_test_samples = int(len(X_test)/args.num_features)
print('num_test_samples: ' + str(num_test_samples))
X_test = np.reshape(X_test, (num_test_samples, args.num_features))

X_test_norm = X_test
for j in range(0, X_test.shape[1]):
	X_test_norm[:,j] = np.divide(X_test[:,j], maxes[j])

for i in range(0,5):
	print('X_test_norm[i]: ' + str(X_test_norm[i,:]))

predictions = logreg.predict_proba(X_test_norm)

outfile = open('predictions.txt', 'w')
for p in predictions:
	outfile.write(str(p[1]) + '\n')
outfile.close()