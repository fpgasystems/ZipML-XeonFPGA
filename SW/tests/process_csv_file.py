from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import argparse
import os
import sys
from random import shuffle

import numpy as np

parser = argparse.ArgumentParser()

parser.add_argument(
	'--file',
	type=str,
	default='',
	help='Absolute path to the csv file.')
parser.add_argument(
	'--label_index',
	type=int,
	default=0,
	help='index of label value in csv sample')

args = parser.parse_args()

samples = []

f_in = open(args.file, 'r')

index = 0
for line in f_in:
	if index == 0: # First line feature names
		num_features = line.count(',')
	else:
		sample = np.array(line.split(','))
		sample = sample.astype(np.float)

		temp = sample[0]
		sample[0] = sample[args.label_index]
		sample[args.label_index] = temp

		samples.append(sample)
	index += 1

num_samples = index-1

samples = np.asarray(samples)

print('samples.shape: ' + str(samples.shape) )

f_out = open(args.file + '_raw_' + str(samples.shape[0]) + '_' + str(samples.shape[1]), 'w');
samples.tofile(f_out)
f_out.close()