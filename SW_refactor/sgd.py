from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import argparse
import os
import sys
from random import shuffle
import time

import numpy as np
import pandas as pd
from sklearn.linear_model import LogisticRegression
from sklearn.linear_model import SGDClassifier
from sklearn.metrics import log_loss

parser = argparse.ArgumentParser()

parser.add_argument(
	'--train_file',
	type=str,
	required=1,
	help='Absolute path to the raw train file.')
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
mins = np.zeros(X.shape[1])
maxes = np.zeros(X.shape[1])
for j in range(0, X.shape[1]):
	mins[j] = np.min(X[:,j])
	maxes[j] = np.max(X[:,j])
	ranges = maxes[j] - mins[j]
	if ranges > 0.0:
		X_norm[:,j] = np.divide(X[:,j]-mins[j], ranges)

Bias = np.ones((X.shape[0],1))

X_norm = np.concatenate((Bias, X_norm), axis=1)

for i in range(0,2):
	print('X_norm[i]: ' + str(X_norm[i,:]))
	print('y[i]: ' + str(y[i]))

logreg = SGDClassifier(
	loss='log', 
	penalty='l1', 
	alpha=0.001,
	fit_intercept=False, 
	max_iter=10, 
	shuffle=True, 
	verbose=2, 
	n_jobs=1, 
	random_state=1, 
	learning_rate='constant', 
	eta0=0.001, 
	warm_start=True, 
	average=False)

for epoch in range(0,1):
	start = time.time()
	logreg.fit(X_norm, y)
	end = time.time()
	loss = log_loss(y, logreg.predict_proba(X_norm))
	print('epoch' + str(epoch) + ', loss: ' + str(loss))
	print('time per epoch: ' + str(end-start))
