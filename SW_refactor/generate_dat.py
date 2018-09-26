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
parser.add_argument(
	'--shuffle',
	type=int)

args = parser.parse_args()

samples = []

cats = [
281 	# 'tabby, tabby cat',
,282 	# 'tiger cat',
,283 	# 'Persian cat',
,284 	# 'Siamese cat, Siamese',
,285	# 'Egyptian cat',
,383	# 'Madagascar cat, ring-tailed lemur, Lemur catta',
]
samples.extend( reformat_features(args.features_dir, cats, 0) )

dogs = [
153		# 'Maltese dog, Maltese terrier, Maltese',
,235	# 'German shepherd, German shepherd dog, German police dog, alsatian',
,230	# 'Shetland sheepdog, Shetland sheep dog, Shetland',
,238	# 'Greater Swiss Mountain dog',
,200	# 'Tibetan terrier, chrysanthemum dog',
,230	# 'Shetland sheepdog, Shetland sheep dog, Shetland',
]
samples.extend( reformat_features(args.features_dir, dogs, 1) )

if args.shuffle == 1:
	print('Shuffling...')
	shuffle(samples)

samples = np.asarray(samples)

print('samples.shape: ' + str(samples.shape) )

if args.shuffle == 1:
	file_name = args.features_dir + '/cats_vs_dogs_' + str(samples.shape[0]) + '_' + str(samples.shape[1])
else:
	file_name = args.features_dir + '/cats_vs_dogs_' + str(samples.shape[0]) + '_' + str(samples.shape[1]) + '_nonshuffle'

f_out = open(file_name, 'w');
samples.tofile(f_out)
f_out.close()
