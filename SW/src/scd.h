#ifndef SCD
#define SCD

#include <iostream>
#include <string>
#include <fstream>
#include <stdint.h>
#include <stdio.h>
#include <vector>
#include <limits>
#include <cmath>

#include "../driver/iFPGA.h"

#ifdef AVX2
#include "immintrin.h"
#endif

using namespace std;

#define NUM_THREADS 14

class scd {

public:
	float** a;	// Data set features matrix: numSamples x numFeatures
	float* b;	// Data set labels vector: numSamples

	uint32_t numFeatures;
	uint32_t numSamples;

	char a_normalizedToMinus1_1;
	char b_normalizedToMinus1_1;
	float b_range;
	float b_min;
	uint32_t b_toIntegerScaler;

	scd(uint32_t _b_toIntegerScaler){
		a = NULL;
		b = NULL;

		b_toIntegerScaler = _b_toIntegerScaler;
	}
	~scd() {
		if (a != NULL) {
			for (uint32_t j = 0; j < numFeatures; j++) {
				free(a[j]);
			}
			free(a);
		}
		if (b != NULL)
			free(b);
	}

	void print_samples(uint32_t num) {
		for (uint32_t i = 0; i < num; i++) {
			cout << "a" << i << ": " << endl;
			for (uint32_t j = 0; j < numFeatures; j++) {
				cout << a[j][i] << " ";
			}
			cout << endl;
			cout << "b" << i << ": " << b[i] << endl;
		}
	}

	// Normalization and data shaping
	void a_normalize(char toMinus1_1, char rowOrColumnWise);
	void b_normalize(char toMinus1_1, char binarize_b, float b_toBinarizeTo);
	void compress_column(float* ptr);

	// Data loading functions
	void load_libsvm_data(char* pathToFile, uint32_t _numSamples, uint32_t _numFeatures);
	void load_raw_data(char* pathToFile, uint32_t _numSamples, uint32_t _numFeatures);
	void generate_synthetic_data(uint32_t _numSamples, uint32_t _numFeatures, char binary);

	float calculate_loss(float* x);

	void float_linreg_SGD(float* x_history, uint32_t numEpochs, uint32_t minibatchSize, float stepSize);
	void float_linreg_SCD(float* x_history, uint32_t numEpochs, uint32_t minibatchSize, float stepSize);
#ifdef AVX2
	void AVX_float_linreg_SCD(float* x_history, uint32_t numEpochs, uint32_t minibatchSize, float stepSize);
	void AVXmulti_float_linreg_SCD(float* x_history, uint32_t numEpochs, uint32_t minibatchSize, float stepSize);
#endif
};



void scd::load_libsvm_data(char* pathToFile, uint32_t _numSamples, uint32_t _numFeatures) {
	cout << "Reading " << pathToFile << endl;

	numSamples = _numSamples;
	numFeatures = _numFeatures+1; // For the bias term

	if (a != NULL)
		free(a);
	a = (float**)malloc(numFeatures*sizeof(float*));
	for (uint32_t j = 0; j < numFeatures; j++) {
		a[j] = (float*)aligned_alloc(64, numSamples*sizeof(float));
	}
	if (b != NULL)
		free(b);
	b = (float*)aligned_alloc(64, numSamples*sizeof(float));

	string line;
	ifstream f(pathToFile);

	uint32_t index = 0;
	if (f.is_open()) {
		while( index < numSamples ) {
			getline(f, line);
			int pos0 = 0;
			int pos1 = 0;
			int pos2 = 0;
			int column = 0;
			//while ( column < numFeatures-1 ) {
			while ( pos2 < (int)line.length()+1 ) {
				if (pos2 == 0) {
					pos2 = line.find(" ", pos1);
					float temp = stof(line.substr(pos1, pos2-pos1), NULL);
					b[index] = temp;
				}
				else {
					pos0 = pos2;
					pos1 = line.find(":", pos1)+1;
					pos2 = line.find(" ", pos1);
					column = stof(line.substr(pos0+1, pos1-pos0-1));
					if (pos2 == -1) {
						pos2 = line.length()+1;
						a[column][index] = stof(line.substr(pos1, pos2-pos1), NULL);
					}
					else
						a[column][index] = stof(line.substr(pos1, pos2-pos1), NULL);
				}
			}
			index++;
		}
		f.close();
	}
	else
		cout << "Unable to open file " << pathToFile << endl;

	for (uint32_t i = 0; i < numSamples; i++) { // Bias term
		a[0][i] = 1.0;
	}
	
	cout << "numSamples: " << numSamples << endl;
	cout << "numFeatures: " << numFeatures << endl;
}

void scd::generate_synthetic_data(uint32_t _numSamples, uint32_t _numFeatures, char binary) {
	numSamples = _numSamples;
	numFeatures = _numFeatures;

	if (a != NULL)
		free(a);
	a = (float**)malloc(numFeatures*sizeof(float*));
	for (uint32_t j = 0; j < numFeatures; j++) {
		a[j] = (float*)aligned_alloc(64, numSamples*sizeof(float));
	}
	if (b != NULL)
		free(b);
	b = (float*)aligned_alloc(64, numSamples*sizeof(float));

	srand(7);
	float* x = (float*)malloc(numFeatures*sizeof(float));
	for (uint32_t j = 0; j < numFeatures; j++) {
		x[j] = ((float)rand())/RAND_MAX;
	}

	for (uint32_t i = 0; i < numSamples; i++) {
		if (binary == 1) {
			float temp = ((float)rand())/RAND_MAX;
			if (temp > 0.5)
				b[i] = 1.0;
			else
				b[i] = -1.0;
		}
		else
			b[i] = (float)rand()/RAND_MAX;

		for (uint32_t j = 0; j < numFeatures; j++) {
			a[j][i] = b[i]*x[j] + 0.001*(float)rand()/(RAND_MAX);
		}
	}

	free(x);

	cout << "numSamples: " << numSamples << endl;
	cout << "numFeatures: " << numFeatures << endl;
}


void scd::a_normalize(char toMinus1_1, char rowOrColumnWise) {
	a_normalizedToMinus1_1 = toMinus1_1;
	if (rowOrColumnWise == 'r') {
		for (uint32_t i = 0; i < numSamples; i++) {
			float amin = numeric_limits<float>::max();
			float amax = numeric_limits<float>::min();
			for (uint32_t j = 0; j < numFeatures; j++) {
				float a_here = a[j][i];
				if (a_here > amax)
					amax = a_here;
				if (a_here < amin)
					amin = a_here;
			}
			float arange = amax - amin;
			if (arange > 0) {
				if (toMinus1_1 == 1) {
					for (uint32_t j = 0; j < numFeatures; j++) {
						a[j][i] = ((a[j][i] - amin)/arange)*2.0-1.0;
					}
				}
				else {
					for (uint32_t j = 0; j < numFeatures; j++) {
						a[j][i] = ((a[j][i] - amin)/arange);
					}
				}
			}
		}
	}
	else {
		for (uint32_t j = 1; j < numFeatures; j++) { // Don't normalize bias
			float amin = numeric_limits<float>::max();
			float amax = numeric_limits<float>::min();
			for (uint32_t i = 0; i < numSamples; i++) {
				float a_here = a[j][i];
				if (a_here > amax)
					amax = a_here;
				if (a_here < amin)
					amin = a_here;
			}
			float arange = amax - amin;
			if (arange > 0) {
				if (toMinus1_1 == 1) {
					for (uint32_t i = 0; i < numSamples; i++) {
						a[j][i] = ((a[j][i] - amin)/arange)*2.0-1.0;
					}
				}
				else {
					for (uint32_t i = 0; i < numSamples; i++) {
						a[j][i] = ((a[j][i] - amin)/arange);
					}
				}
			}
		}
	}
}

void scd::b_normalize(char toMinus1_1, char binarize_b, float b_toBinarizeTo) {
	b_normalizedToMinus1_1 = toMinus1_1;
	if (binarize_b == 0) {
		float bmin = numeric_limits<float>::max();
		float bmax = numeric_limits<float>::min();
		for (uint32_t i = 0; i < numSamples; i++) {
			if (b[i] > bmax)
				bmax = b[i];
			if (b[i] < bmin)
				bmin = b[i];
		}
		cout << "bmax: " << bmax << ", bmin: " << bmin << endl;
		float brange = bmax - bmin;
		if (brange > 0) {
			if (toMinus1_1 == 1) {
				for (uint32_t i = 0; i < numSamples; i++) {
					b[i] = ((b[i]-bmin)/brange)*2.0 - 1.0;
				}
			}
			else {
				for (uint32_t i = 0; i < numSamples; i++) {
					b[i] = (b[i]-bmin)/brange;
				}
			}
		}
		b_min = bmin;
		b_range = brange;
	}
	else {
		for (uint32_t i = 0; i < numSamples; i++) {
			if(b[i] == b_toBinarizeTo)
				b[i] = 1.0;
			else
				b[i] = -1.0;
		}
		b_min = -1.0;
		b_range = 2.0;
	}
}

static void shuffle(uint32_t* indexes, uint32_t size) {
	for (uint32_t j = 0; j < size; j++) {
		uint32_t rand_j = size*((float)(rand()-1)/(float)RAND_MAX);

		uint32_t temp = indexes[j];
		indexes[j] = indexes[rand_j];
		indexes[rand_j] = temp;
	}
}

void scd::float_linreg_SGD(float* x_history, uint32_t numEpochs, uint32_t minibatchSize, float stepSize) {
	float* x = (float*)aligned_alloc(64, numFeatures*sizeof(float));
	memset(x, 0, numFeatures*sizeof(float));
	float* gradient = (float*)aligned_alloc(64, numFeatures*sizeof(float));
	memset(gradient, 0, numFeatures*sizeof(float));

	cout << "Initial loss: " << calculate_loss(x) << endl;

	uint32_t numMinibatches = numSamples/minibatchSize;
	cout << "numMinibatches: " << numMinibatches << endl;
	uint32_t rest = numSamples - numMinibatches*minibatchSize;
	cout << "rest: " << rest << endl;

	for(uint32_t epoch = 0; epoch < numEpochs; epoch++) {

		double start = get_time();

		for (uint32_t m = 0; m < numMinibatches; m++) {
			for (uint32_t i = 0; i < minibatchSize; i++) {
				float dot = 0;
				for (uint32_t j = 0; j < numFeatures; j++) {
					dot += x[j]*a[j][m*minibatchSize + i];
				}
				for (uint32_t j = 0; j < numFeatures; j++) {
					gradient[j] += (dot - b[m*minibatchSize + i])*a[j][m*minibatchSize + i];
				}
			}
			// cout << "gradient[0]: " << gradient[0] << endl;
			for (uint32_t j = 0; j < numFeatures; j++) {
				x[j] -= stepSize*(gradient[j]/minibatchSize);
				gradient[j] = 0.0;
			}
		}
		// Handle the rest
		if (rest > 0) {
			for (uint32_t i = numSamples-rest; i < numSamples; i++) {
				float dot = 0;
				for (uint32_t j = 0; j < numFeatures; j++) {
					dot += x[j]*a[j][i];
				}
				for (uint32_t j = 0; j < numFeatures; j++) {
					gradient[j] += (dot - b[i])*a[j][i];
				}
			}
			for (uint32_t j = 0; j < numFeatures; j++) {
				x[j] -= stepSize*gradient[j];
				gradient[j] = 0.0;
			}
		}

		double end = get_time();
		cout << "Time for one epoch: " << end-start << endl;

		if (x_history != NULL) {
			for (uint32_t j = 0; j < numFeatures; j++) {
				x_history[epoch*numFeatures + j] = x[j];
			}
		}
		else
			cout << "Loss " << epoch << ": " << calculate_loss(x) << endl;

		cout << epoch << endl;
	}

	free(x);
	free(gradient);
}

void scd::float_linreg_SCD(float* x_history, uint32_t numEpochs, uint32_t minibatchSize, float stepSize) {
	float* residual = (float*)aligned_alloc(64, numSamples*sizeof(float));
	memset(residual, 0, numSamples*sizeof(float));

	uint32_t numMinibatches = numSamples/minibatchSize;
	cout << "numMinibatches: " << numMinibatches << endl;
	uint32_t rest = numSamples - numMinibatches*minibatchSize;
	cout << "rest: " << rest << endl;

	float* x = (float*)aligned_alloc(64, (numMinibatches + numSamples%minibatchSize)*numFeatures*sizeof(float));
	memset(x, 0, (numMinibatches + numSamples%minibatchSize)*numFeatures*sizeof(float));

	cout << "Initial loss: " << calculate_loss(x) << endl;

	for(uint32_t epoch = 0; epoch < numEpochs; epoch++) {

		double start = get_time();

		for (uint32_t m = 0; m < numMinibatches; m++) {
			
			for (uint32_t j = 0; j < numFeatures; j++) {

				float gradient = 0;
				for (uint32_t i = 0; i < minibatchSize; i++) {
					gradient += (residual[m*minibatchSize + i] - b[m*minibatchSize + i])*a[j][m*minibatchSize + i];
				}
				float step = stepSize*(gradient/minibatchSize);

				x[m*numFeatures + j] -= step;

				for (uint32_t i = 0; i < minibatchSize; i++) {
					residual[m*minibatchSize + i] -= step*a[j][m*minibatchSize + i];
				}

			}
		}

		for (uint32_t m = 1; m < numMinibatches; m++) {
			for (uint32_t j = 0; j < numFeatures; j++) {
				x[j] += x[m*numFeatures + j];
			}
		}
		for (uint32_t j = 0; j < numFeatures; j++) {
			x[j] = x[j]/numMinibatches;
		}

		double end = get_time();
		cout << "Time for one epoch: " << end-start << endl;

		if (x_history != NULL) {
			for (uint32_t j = 0; j < numFeatures; j++) {
				x_history[epoch*numFeatures + j] = x[j];
			}
		}
		else
			cout << "Loss " << epoch << ": " << calculate_loss(x) << endl;

		cout << epoch << endl;
	}

	free(x);
	free(residual);
}

#ifdef AVX2
void scd::AVX_float_linreg_SCD(float* x_history, uint32_t numEpochs, uint32_t minibatchSize, float stepSize) {
	if (minibatchSize%8 == 8) {
		cout << "For AVX minibatchSize%8 must be 0!" << endl;
		exit(1);
	}

	float scaledStepSize = -stepSize/(float)minibatchSize;
	__m256 scaledStepSize_temp = _mm256_set1_ps(scaledStepSize);

	float* residual = (float*)aligned_alloc(64, numSamples*sizeof(float));
	memset(residual, 0, numSamples*sizeof(float));

	uint32_t numMinibatches = numSamples/minibatchSize;
	cout << "numMinibatches: " << numMinibatches << endl;
	uint32_t rest = numSamples - numMinibatches*minibatchSize;
	cout << "rest: " << rest << endl;

	float* x = (float*)aligned_alloc(64, (numMinibatches + numSamples%minibatchSize)*numFeatures*sizeof(float));
	memset(x, 0, (numMinibatches + numSamples%minibatchSize)*numFeatures*sizeof(float));

	cout << "Initial loss: " << calculate_loss(x) << endl;

	for(uint32_t epoch = 0; epoch < numEpochs; epoch++) {

		double start = get_time();

		__m256 residual_temp;
		__m256 a_temp;
		__m256 b_temp;

		for (uint32_t m = 0; m < numMinibatches; m++) {
			
			for (uint32_t j = 0; j < numFeatures; j++) {
				__m256 gradient = _mm256_setzero_ps();

				for (uint32_t i = 0; i < minibatchSize; i+=8) {
					a_temp = _mm256_load_ps(a[j] + m*minibatchSize + i);
					b_temp = _mm256_load_ps(b + m*minibatchSize + i);
					residual_temp = _mm256_load_ps(residual + m*minibatchSize + i);

					__m256 error_temp = _mm256_sub_ps(residual_temp, b_temp);
					gradient = _mm256_fmadd_ps(a_temp, error_temp, gradient);
				}
				__m256 step = _mm256_mul_ps(gradient, scaledStepSize_temp);

				float stepReduce[8];
				_mm256_store_ps(stepReduce, step);
				stepReduce[0] = (stepReduce[0] + 
								stepReduce[1] + 
								stepReduce[2] + 
								stepReduce[3] + 
								stepReduce[4] + 
								stepReduce[5] + 
								stepReduce[6] + 
								stepReduce[7]);


				__m256 stepReduce_temp = _mm256_set1_ps(stepReduce[0]);

				x[m*numFeatures + j] += stepReduce[0];

				for (uint32_t i = 0; i < minibatchSize; i+=8) {
					a_temp = _mm256_load_ps(a[j] + m*minibatchSize + i);
					residual_temp = _mm256_load_ps(residual + m*minibatchSize + i);
					residual_temp = _mm256_fmadd_ps(a_temp, stepReduce_temp, residual_temp);

					_mm256_store_ps(residual + m*minibatchSize + i, residual_temp);
				}
			}
		}

		for (uint32_t m = 1; m < numMinibatches; m++) {
			for (uint32_t j = 0; j < numFeatures; j++) {
				x[j] += x[m*numFeatures + j];
			}
		}
		for (uint32_t j = 0; j < numFeatures; j++) {
			x[j] = x[j]/numMinibatches;
		}

		double end = get_time();
		cout << "Time for one epoch: " << end-start << endl;

		if (x_history != NULL) {
			for (uint32_t j = 0; j < numFeatures; j++) {
				x_history[epoch*numFeatures + j] = x[j];
			}
		}
		else
			cout << "Loss " << epoch << ": " << calculate_loss(x) << endl;

		cout << epoch << endl;
	}

	free(x);
	free(residual);
}

typedef struct {
	pthread_barrier_t* barrier;
	uint32_t tid;
	float* x;
	float** a;
	float* b;
	float* residual;
	float stepSize;
	uint32_t startingBatch;
	uint32_t numBatchesToProcess;
	uint32_t minibatchSize;
	uint32_t numMinibatches;
	uint32_t numEpochs;
	uint32_t numFeatures;
	uint32_t numSamples;
	float* x_history;
	scd* app;
} batch_thread_data;

void* batch_thread(void* args) {
	batch_thread_data* r = (batch_thread_data*)args;

	float scaledStepSize = -r->stepSize/(float)r->minibatchSize;
	__m256 scaledStepSize_temp = _mm256_set1_ps(scaledStepSize);

	double start, end;

	for(uint32_t epoch = 0; epoch < r->numEpochs; epoch++) {

		pthread_barrier_wait(r->barrier);
		double start = get_time();

		__m256 residual_temp;
		__m256 a_temp;
		__m256 b_temp;

		for (uint32_t m = r->startingBatch; m < r->startingBatch + r->numBatchesToProcess; m++) {
			for (uint32_t j = 0; j < r->numFeatures; j++) {
				__m256 gradient = _mm256_setzero_ps();

				for (uint32_t i = 0; i < r->minibatchSize; i+=8) {
					a_temp = _mm256_load_ps(r->a[j] + m*r->minibatchSize + i);
					b_temp = _mm256_load_ps(r->b + m*r->minibatchSize + i);
					residual_temp = _mm256_load_ps(r->residual + m*r->minibatchSize + i);

					__m256 error_temp = _mm256_sub_ps(residual_temp, b_temp);
					gradient = _mm256_fmadd_ps(a_temp, error_temp, gradient);
				}
				__m256 step = _mm256_mul_ps(gradient, scaledStepSize_temp);

				float stepReduce[8];
				_mm256_store_ps(stepReduce, step);
				stepReduce[0] = (stepReduce[0] + 
								stepReduce[1] + 
								stepReduce[2] + 
								stepReduce[3] + 
								stepReduce[4] + 
								stepReduce[5] + 
								stepReduce[6] + 
								stepReduce[7]);


				__m256 stepReduce_temp = _mm256_set1_ps(stepReduce[0]);

				r->x[m*r->numFeatures + j] += stepReduce[0];

				for (uint32_t i = 0; i < r->minibatchSize; i+=8) {
					a_temp = _mm256_load_ps(r->a[j] + m*r->minibatchSize + i);
					residual_temp = _mm256_load_ps(r->residual + m*r->minibatchSize + i);
					residual_temp = _mm256_fmadd_ps(a_temp, stepReduce_temp, residual_temp);

					_mm256_store_ps(r->residual + m*r->minibatchSize + i, residual_temp);
				}
			}
		}

		pthread_barrier_wait(r->barrier);

		if (r->tid == 0) {
			for (uint32_t m = 1; m < r->numMinibatches; m++) {
				for (uint32_t j = 0; j < r->numFeatures; j++) {
					r->x[j] += r->x[m*r->numFeatures + j];
				}
			}
			for (uint32_t j = 0; j < r->numFeatures; j++) {
				r->x[j] = r->x[j]/r->numMinibatches;
			}

			end = get_time();

			cout << "Time for one epoch: " << end-start << endl;
			if (r->x_history != NULL) {
				for (uint32_t j = 0; j < r->numFeatures; j++) {
					r->x_history[epoch*r->numFeatures + j] = r->x[j];
				}
			}
			else
				cout << "Loss " << epoch << ": " << r->app->calculate_loss(r->x) << endl;
			cout << epoch << endl;
		}
	}

	return NULL;
}

void scd::AVXmulti_float_linreg_SCD(float* x_history, uint32_t numEpochs, uint32_t minibatchSize, float stepSize) {
	if (minibatchSize%8 == 8) {
		cout << "For AVX minibatchSize%8 must be 0!" << endl;
		exit(1);
	}

	pthread_barrier_t barrier;
	pthread_attr_t attr;
	pthread_t threads[NUM_THREADS];
	batch_thread_data args[NUM_THREADS];
	cpu_set_t set;

	float* residual = (float*)aligned_alloc(64, numSamples*sizeof(float));
	memset(residual, 0, numSamples*sizeof(float));

	uint32_t numMinibatches = numSamples/minibatchSize;
	cout << "numMinibatches: " << numMinibatches << endl;
	uint32_t rest = numSamples - numMinibatches*minibatchSize;
	cout << "rest: " << rest << endl;

	float* x = (float*)aligned_alloc(64, (numMinibatches + numSamples%minibatchSize)*numFeatures*sizeof(float));
	memset(x, 0, (numMinibatches + numSamples%minibatchSize)*numFeatures*sizeof(float));

	cout << "Initial loss: " << calculate_loss(x) << endl;

	uint32_t startingBatch = 0;
	pthread_barrier_init(&barrier, NULL, NUM_THREADS);
	pthread_attr_init(&attr);
	CPU_ZERO(&set);
	for (uint32_t n = 0; n < NUM_THREADS; n++) {
		CPU_SET(n, &set);
		pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &set);

		args[n].barrier = &barrier;
		args[n].tid = n;
		args[n].x = x;
		args[n].a = a;
		args[n].b = b;
		args[n].residual = residual;
		args[n].stepSize = stepSize;
		args[n].startingBatch = startingBatch;
		if (n == NUM_THREADS-1)
			args[n].numBatchesToProcess = numMinibatches - startingBatch;
		else
			args[n].numBatchesToProcess = numMinibatches/NUM_THREADS;
		args[n].minibatchSize = minibatchSize;
		args[n].numMinibatches = numMinibatches;
		args[n].numEpochs = numEpochs;
		args[n].numFeatures = numFeatures;
		args[n].numSamples = numSamples;
		args[n].x_history = x_history;
		args[n].app = this;

		pthread_create(&threads[n], &attr, batch_thread, (void*)&args[n]);
		startingBatch += args[n].numBatchesToProcess;
	}
	for (uint32_t n = 0; n < NUM_THREADS; n++) {
		pthread_join(threads[n], NULL);
	}	

	free(x);
	free(residual);
}
#endif

float scd::calculate_loss(float* x) {
	float loss = 0;
	for(uint32_t i = 0; i < numSamples; i++) {
		float dot = 0.0;
		for (uint32_t j = 0; j < numFeatures; j++) {
			dot += x[j]*a[j][i];
		}
		loss += (dot - b[i])*(dot - b[i]);
	}

	loss /= (float)(2*numSamples);
	return loss;
}

// void decompress_column(uint32_t* compressed_column, uint32_t inNumWords, float* decompressed_column){
// 	uint32_t outNumWords = 0;

// 	for (uint32_t i = 0; i < inNumWords; i+=8) {
// 		uint32_t meta = (compressed_column[i*8-1] >> 25);
// 		if ( meta == 0x40 ) {
// 			int base = (int)compressed_column[i];
// 			decompressed_column[outNumWords++] = ((float)base)/((float)b_toIntegerScaler);
			
// 			int delta[31];
// 			delta[0] = (int)(CL[1] & 0x7F);
// 			delta[1] = (int)((CL[1] >> 7 ) & 0x7F);
// 			delta[2] = (int)((CL[1] >> 14 ) & 0x7F);
// 			delta[3] = (int)((CL[1] >> 21 ) & 0x7F);
// 			delta[4] = (int)(((CL[2] & 0x7) << 4) + (CL[1] >> 28));
// 			delta[5] = (int)((CL[2] >> 3) & 0x7F);
// 			delta[6] = (int)((CL[2] >> 10) & 0x7F);
// 			delta[7] = (int)((CL[2] >> 17) & 0x7F);
// 			delta[8] = (int)((CL[2] >> 24) & 0x7F);
// 			delta[9] = (int)(((CL[3] & 0x3F) << 1) + (CL[2] >> 31));
// 			delta[10] = (int)((CL[3] >> 6) & 0x7F);
// 			delta[11] = (int)((CL[3] >> 13) & 0x7F);
// 			delta[12] = (int)((CL[3] >> 20) & 0x7F);
// 			delta[13] = (int)(((CL[4] & 0x3) << 5) + (CL[3] >> 27));
// 			delta[14] = (int)((CL[4] >> 2) & 0x7F);
// 			delta[15] = (int)((CL[4] >> 9) & 0x7F);
// 			delta[16] = (int)((CL[4] >> 16) & 0x7F);
// 			delta[17] = (int)((CL[4]))


// 			for (uint32_t k = 0; k < 31; k++) {
// 				decompressed_column[outNumWords++] = ((float)(base+delta[k]))/((float)b_toIntegerScaler);
// 			}
			


// 		}
// 		else if (meta == 0x20) {
// 			int base = (int)compressed_column[i];
// 			decompressed_column[outNumWords++] = ((float)base)/((float)b_toIntegerScaler);


// 		}

// 	}


// }

// void compress_column(float* original_column, uint32_t* compressed_column) {
// 	// 0x40 31 7bit delta, 0x30 24 9bit delta, 0x20 15 14bit delta, 0x10 7 31-bit delta
// 	uint32_t numWords = 0;

// 	uint32_t CL[8];
//   	uint32_t keys[31];

// 	uint32_t numProcessed = 0;
// 	for (uint32_t i = 0; i < numSamples; i=i+num_processed+1) {
// 		int base = (int)(original_column[i]*b_toIntegerScaler);

// 		CL[0] = (uint32_t)base;

// 		uint32_t search_for = 31;
// 		uint32_t j = i + 1;
// 		numProcessed = 0;

// 		while (1) {
// 			int next = (int)(original_column[j]*b_toIntegerScaler);
// 			int delta = next - base;

// 			if (delta >= -64 && delta <= 63 && i+31 < numSamples && search_for >= 31)
// 				search_for = 31;
// 			else if (delta >= -256 && delta <= 255 && i+24 < numSamples && search_for >= 24)
// 				search_for = 24;
// 			else if (delta >= -8192 && delta <= 8191 && i+15 < numSamples && search_for >= 15)
// 				search_for = 15;
// 			else
// 				search_for = 7;

// 			keys[numProcessed] = (uint32_t)delta;
// 			numProcessed++;
// 			if (numProcessed >= search_for) {
// 				numProcessed = search_for;
// 				break;
// 			}
// 			j++;
// 		}

// 		if (numProcessed == 31) {
		
// 			CL[1] =
// 			((keys[4] & 0xF) << 28) |
// 			((keys[3] & 0x7F) << 21) |
// 			((keys[2] & 0x7F) << 14) |
// 			((keys[1] & 0x7F) << 7) |
// 			((keys[0] & 0x7F));

// 			CL[2] =
// 			((keys[9] & 0x01) << 31) |
// 			((keys[8] & 0x7F) << 24) |
// 			((keys[7] & 0x7F) << 17) |
// 			((keys[6] & 0x7F) << 10) |
// 			((keys[5] & 0x7F) << 3) |
// 			((keys[4] & 0x7F) >> 4);

// 			CL[3] =
// 			((keys[13] & 0x1F) << 27) |
// 			((keys[12] & 0x7F) << 20) |
// 			((keys[11] & 0x7F) << 13) |
// 			((keys[10] & 0x7F) << 6) |
// 			((keys[9] & 0x7F) >> 1);

// 			CL[4] =
// 			((keys[18] & 0x03) << 30) |
// 			((keys[17] & 0x7F) << 23) |
// 			((keys[16] & 0x7F) << 16) |
// 			((keys[15] & 0x7F) << 9) |
// 			((keys[14] & 0x7F) << 2) |
// 			((keys[13] & 0x7F) >> 5);

// 			CL[5] =
// 			((keys[22] & 0x3F) << 26)
// 			((keys[21] & 0x7F) << 19) |
// 			((keys[20] & 0x7F) << 12) |
// 			((keys[19] & 0x7F) << 5) |
// 			((keys[18] & 0x7F) >> 2);

// 			CL[6] =
// 			((keys[27] & 0x07) << 29) |
// 			((keys[26] & 0x7F) << 22) |
// 			((keys[25] & 0x7F) << 15) |
// 			((keys[24] & 0x7F) << 8) |
// 			((keys[23] & 0x7F) << 1) |
// 			((keys[22] & 0x7F) >> 6);

// 			CL[7] =
// 			((keys[30] & 0x7F) << 18) |
// 			((keys[29] & 0x7F) << 11) |
// 			((keys[28] & 0x7F) << 4) |
// 			((keys[27] & 0x7F) >> 3);

// 			uint32_t meta = 0x40;
// 			meta = meta << 24;
// 			CL[7] |= meta;
// 		}
// 		else if (numProcessed == 24) {

// 			CL[1] =
// 			((keys[3] & 0x1F) << 27) |
// 			((keys[2] & 0x1FF) << 18) |
// 			((keys[1] & 0x1FF) << 9) |
// 			((keys[0] & 0x1FF));

// 			CL[2] =
// 			((keys[7] & 0x1) << 31) |
// 			((keys[6] & 0x1FF) << 22) |
// 			((keys[5] & 0x1FF) << 13) |
// 			((keys[4] & 0x1FF) << 4) |
// 			((keys[3] & 0x1FF) >> 5)

// 			CL[3] =
// 			((keys[10] & 0x3F) << 26) |
// 			((keys[9] & 0x1FF) << 17) |
// 			((keys[8] & 0x1FF) << 8) |
// 			((keys[7] & 0x1FF) >> 1);

// 			CL[4] =
// 			((keys[14] & 0x3) << 30) |
// 			((keys[13] & 0x1FF) << 21) |
// 			((keys[12] & 0x1FF) << 12) |
// 			((keys[11] & 0x1FF) << 3) |
// 			((keys[10] & 0x1FF) >> 6);

// 			CL[5] =
// 			((keys[17] & 0x7F) << 25) |
// 			((keys[16] & 0x1FF) << 16) |
// 			((keys[15] & 0x1FF) << 7) |
// 			((keys[14] & 0x1FF) >> 2);

// 			CL[6] =
// 			((keys[21] & 0x7) << 29) |
// 			((keys[20] & 0x1FF) << 20) |
// 			((keys[19] & 0x1FF) << 11) |
// 			((keys[18] & 0x1FF) << 2) |
// 			((keys[17] & 0x1FF) >> 7);

// 			CL[7] =
// 			((keys[23] & 0x1FF) << 15) |
// 			((keys[22] & 0x1FF) << 6) |
// 			((keys[21] & 0x1FF) >> 3);

// 			uint32_t meta = 0x30;
// 			meta = meta << 24;
// 			CL[7] |= meta;
// 		}
// 		else if (numProcessed == 15) {

// 			CL[1] =
// 			((keys[2] & 0xF) << 28) |
// 			((keys[1] & 0x3FFF) << 14) |
// 			((keys[0] & 0x3FFF));

// 			CL[2] =
// 			((keys[4] & 0xFF) << 24) |
// 			((keys[3] & 0x3FFF) << 10) |
// 			((keys[2] & 0x3FFF) >> 4);

// 			CL[3] =
// 			((keys[6] & 0x3FFF) << 20)
// 			((keys[5] & 0x3FFF) << 6) |
// 			((keys[4] & 0x3FFF) >> 8);

// 			CL[4] =
// 			((keys[9] & 0x3) << 30) |
// 			((keys[8] & 0x3FFF) << 16) |
// 			((keys[7] & 0x3FFF) << 2) |
// 			((keys[6] & 0x3FFF) >> 12);

// 			CL[5] =
// 			((keys[11] & 0x3F) << 26) |
// 			((keys[10] & 0x3FFF) << 12) |
// 			((keys[9] & 0x3FFF) >> 2);

// 			CL[6] =
// 			((keys[13] & 0x3FF) << 22) |
// 			((keys[12] & 0x3FFF) << 8) |
// 			((keys[11] & 0x3FFF) >> 6);

// 			CL[7] =
// 			((keys[14] & 0x3FFF) << 4) |
// 			((keys[13] & 0x3FFF) >> 10);

// 			uint32_t meta = 0x20;
// 			meta = meta << 24;
// 			CL[7] |= meta;
// 		}
// 		else {
// 			CL[1] = (keys[0] & 0x7FFFFFFF);
// 			CL[2] = (keys[1] & 0x7FFFFFFF);
// 			CL[3] = (keys[2] & 0x7FFFFFFF);
// 			CL[4] = (keys[3] & 0x7FFFFFFF);
// 			CL[5] = (keys[4] & 0x7FFFFFFF);
// 			CL[6] = (keys[5] & 0x7FFFFFFF);
// 			CL[7] = (keys[6] & 0x7FFFFFFF);

// 			uint32_t meta = 0x80;
// 			meta = meta << 24;
// 			CL[7] |= meta;
// 		}

// 		for (uint32_t k = 0; k < 8; k++) {
// 			compressed_column[numWords++] = CL[k];
// 		}
// 	}

// 	return numWords;
// }

#endif