from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import argparse
import os
import sys
from random import shuffle

import numpy as np

def reformat_features(directory, file_numbers, label):
	samples = []

	processed_samples = 0

	for i in file_numbers:
		filename = 'features' + str(i)
		f_in = open(directory + '/' + filename, 'r')

		for line in f_in:
			if 'synset_id' not in line and 'label' not in line:
				features = np.array(line.split(';')[2].split(' '))
				features = features[1:2049]
				sample = np.insert(features, 0, label)
				sample = sample.astype(np.float)

				samples.append(sample)

				# print(len(samples))
				# print(samples)
				# if processed_samples == 100:
				# 	break

				print(processed_samples)
				processed_samples += 1
		f_in.close()

	return samples

parser = argparse.ArgumentParser()

parser.add_argument(
	'--features_dir',
	type=str,
	default='',
	help='Absolute path to features directory.')

args = parser.parse_args()

samples = []

cats = [
282
# ,281
]
samples.extend( reformat_features(args.features_dir, cats, 0) )

dogs = [
235
# ,153
]
samples.extend( reformat_features(args.features_dir, dogs, 1) )

# print('Shuffling...')
# shuffle(samples)

samples = np.asarray(samples)

print('samples.shape: ' + str(samples.shape) )

f_out = open(args.features_dir + '/cats_vs_dogs_' + str(samples.shape[0]) + '_' + str(samples.shape[1]), 'w');
samples.tofile(f_out)
f_out.close()
