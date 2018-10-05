from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import argparse
import os
import sys
from random import shuffle
import time

import numpy as np
import tensorflow as tf

tf.enable_eager_execution()
tfe = tf.contrib.eager

class LogReg(tf.keras.layers.Layer):
	def __init__(self, num_features):
		super(LogReg, self).__init__()
		self.tf_w = self.add_variable("kernel", shape=[num_features,1], dtype=tf.float32, initializer='zeros')

	def call(self, x):
		return tf.nn.sigmoid(tf.matmul(x, self.tf_w))

def loss(model, x, y):
	num_samples = y.shape[0]
	ones = np.ones((num_samples,1)).astype(np.float32)

	predictions = model(x)

	positiveLoss = tf.multiply( y, tf.log(predictions) )
	negativeLoss = tf.multiply( tf.subtract(ones, y), tf.log(tf.subtract(ones, predictions)) )

	loss = -tf.reduce_sum( tf.add(positiveLoss, negativeLoss) )
	loss = tf.divide(loss, num_samples)
	return loss

def grad(model, x, y):
	with tf.GradientTape() as tape:
		loss_value = loss(model, x, y)
	return loss_value, tape.gradient(loss_value, model.trainable_variables)

def train(model, x, y, num_epochs, minibatch_size, learning_rate, regularization):
	num_samples = y.shape[0]
	optimizer = tf.train.GradientDescentOptimizer(learning_rate=learning_rate)
	global_step = tf.train.get_or_create_global_step()

	initial_loss = loss(model, x, y)
	print('initial_loss: ' + str(initial_loss))

	for epoch in range(0, num_epochs):
		for i in range(0, int(num_samples/minibatch_size)):
			_, grads = grad(model, x[i*minibatch_size:(i+1)*minibatch_size,:], y[i*minibatch_size:(i+1)*minibatch_size,:])
			optimizer.apply_gradients(zip(grads, model.variables), global_step)

		loss_value = loss(model, x, y)
		print(loss_value)



def tf_logreg(num_features):
	tf.set_random_seed(7)

	learning_rate = tf.placeholder("float", name='learning_rate')
	regularization = tf.placeholder("float", name='regularization')

	tf_x = tf.placeholder("float", [None, num_features], name='x')
	tf_y = tf.placeholder("float", [None], name='y')

	x = tf.reshape(tf_x, [-1, num_features])
	y = tf.reshape(tf_y, [-1, 1])

	tf_w = tf.get_variable("model", [num_features, 1], dtype=tf.float32, initializer=tf.zeros_initializer)

	loss = -tf.reduce_sum(y*tf.log(tf.nn.sigmoid(tf.matmul(x, tf_w))) + (1-y)*tf.log(1 - tf.nn.sigmoid(tf.matmul(x, tf_w))))
	loss = tf.divide(loss, y.get_shape().as_list()[1])

	loss = loss + regularization*tf.reduce_sum(tf.abs(tf_w))
	loss = tf.identity(loss, name="loss")

	optimizer = tf.train.GradientDescentOptimizer(learning_rate).minimize(loss)

	return tf_x, tf_y, tf_w, loss, optimizer, learning_rate, regularization

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

minibatch_size = 512
lr = 0.1/minibatch_size
reg = 0.001


num_features = X_norm.shape[1]
num_samples = y.shape[0]

tf_x = X_norm.astype(np.float32)
tf_y = np.reshape(y,(num_samples, 1)).astype(np.float32)

model = LogReg(num_features)

train(model, tf_x, tf_y, 10, minibatch_size, lr, reg)


# tf.reset_default_graph()
# tf_x, tf_y, tf_w, loss, optimizer, learning_rate, regularization = tf_logreg(X_norm.shape[1])
# init = tf.global_variables_initializer()

# with tf.Session() as sess:
# 	sess.run(init)
# 	initial_loss = sess.run(loss, feed_dict={tf_x:X_norm, tf_y:y, learning_rate:0, regularization:reg})
# 	initial_loss = initial_loss/y.shape[0]
# 	print('Inital loss: ' + str(initial_loss))

# 	for epoch in range(0, 10):
# 		start = time.time()
# 		for i in range(0, int(y.shape[0]/minibatch_size) ):
# 			_ = sess.run(optimizer, feed_dict={tf_x:X_norm[i:i+minibatch_size, :].reshape(minibatch_size,X_norm.shape[1]), tf_y:y[i:i+minibatch_size].reshape(minibatch_size), learning_rate:lr, regularization:reg})

# 		end = time.time()
# 		print('time per epoch: ' + str(end-start))

# 		epoch_loss = sess.run(loss, feed_dict={tf_x:X_norm, tf_y:y, learning_rate:0, regularization:reg})
# 		epoch_loss = epoch_loss/y.shape[0]
# 		print('epoch ' + str(epoch) + ': ' + str(epoch_loss))

# 	weights = sess.run(tf_w)
# 	print(weights)

# 	f_out = open('model_' + str(weights.shape[0]), 'w');
# 	weights.tofile(f_out)
# 	f_out.close()