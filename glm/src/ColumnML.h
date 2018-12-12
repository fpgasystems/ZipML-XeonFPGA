// Copyright (C) 2018 Kaan Kara - Systems Group, ETH Zurich

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//*************************************************************************

#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <iostream>
#include <cmath>
#include <pthread.h>

#include "ColumnStore.h"

#ifdef AVX2
#include "immintrin.h"
#include "avx_mathfun.h"
#endif

using namespace std;

// #define PRINT_TIMING
#define PRINT_LOSS
// #define PRINT_ACCURACY
// #define SGD_SHUFFLE
// #define SCD_SHUFFLE

#define MAX_NUM_THREADS 14

enum ModelType {linreg, logreg, l2svm};

struct AdditionalArguments
{
	// l2svm
	float m_costPos;
	float m_costNeg;

	// logreg
	uint32_t m_firstSample;
	uint32_t m_numSamples;

	// l2svm, linreg
	float m_decisionBoundary;
	float m_trueLabel;
	float m_falseLabel;

	bool m_constantStepSize;
};

class ColumnML {
public:
	ColumnStore* m_cstore;

	ColumnML() {
		m_cstore = new ColumnStore();
	}

	~ColumnML() {
		delete m_cstore;
	}

	void WriteLogregPredictions(char* fileName, float* x);
	void LoadModel(char* fileName, float* x, uint32_t numFeatures) {
		cout << "LoadModel from " << fileName << endl;
		FILE* f = fopen(fileName, "r");
		size_t readsize = fread(x, sizeof(double), numFeatures, f);
		cout << "readsize: " << readsize << endl;
		fclose(f);
	}

	float L2regularization(float* x, float lambda, AdditionalArguments* args);
	float L1regularization(float* x, float lambda);
	float L2svmLoss(float* x, float lambda, AdditionalArguments* args);
	float LogregLoss(float* x, float lambda, AdditionalArguments* args);
	float LinregLoss(float* x, float lambda, AdditionalArguments* args);
	uint32_t LogregAccuracy(float* x, AdditionalArguments* args);
	uint32_t LinregAccuracy(float* x, AdditionalArguments* args);
	
	float Loss(ModelType type, float* x, float lambda, AdditionalArguments* args) {
		float result = 0.0;
		switch(type) {
			case l2svm:
				result = L2svmLoss(x, lambda, args);
				break;
			case logreg:
				result = LogregLoss(x, lambda, args);
				break;
			case linreg:
				result = LinregLoss(x, lambda, args);
				break;
		}
		return result;
	}
	uint32_t Accuracy(ModelType type, float* x, AdditionalArguments* args) {
		uint32_t result = 0;
		switch(type) {
			case l2svm:
				result = LinregAccuracy(x, args);
				break;
			case logreg:
				result = LogregAccuracy(x, args);
				break;
			case linreg:
				result = LinregAccuracy(x, args);
				break;
		}
		return result;
	}

	void SGD(
		ModelType type, 
		float* xHistory, 
		uint32_t numEpochs, 
		uint32_t minibatchSize, 
		float stepSize, 
		float lambda, 
		AdditionalArguments* args);
#ifdef AVX2
	void AVX_SGD(
		ModelType type, 
		float* xHistory, 
		uint32_t numEpochs, 
		uint32_t minibatchSize, 
		float stepSize, 
		float lambda, 
		AdditionalArguments* args);
	void AVXrowwise_SGD(
		ModelType type, 
		float* xHistory, 
		uint32_t numEpochs, 
		uint32_t minibatchSize, 
		float stepSize, 
		float lambda, 
		AdditionalArguments* args);
#endif
	void SCD(
		ModelType type, 
		float* xHistory, 
		uint32_t numEpochs, 
		uint32_t minibatchSize, 
		float stepSize, 
		float lambda, 
		uint32_t numMinibatchesAtATime, 
		uint32_t residualUpdatePeriod, 
		bool useEncrypted, 
		bool useCompressed, 
		uint32_t toIntegerScaler,
		AdditionalArguments* args);
#ifdef AVX2
	void AVX_SCD(
		ModelType type,
		float* xHistory, 
		uint32_t numEpochs, 
		uint32_t minibatchSize, 
		float stepSize, 
		float lambda, 
		uint32_t residualUpdatePeriod, 
		bool useEncrypted, 
		bool useCompressed, 
		uint32_t toIntegerScaler,
		AdditionalArguments* args);
	double AVXmulti_SCD (
		ModelType type,
		bool doRealSCD,
		float* xHistory,
		uint32_t numEpochs,
		uint32_t minibatchSize,
		float stepSize,
		float lambda,
		uint32_t residualUpdatePeriod,
		bool useEncrypted,
		bool useCompressed,
		uint32_t toIntegerScaler,
		AdditionalArguments* args,
		uint32_t numThreads);
#endif

	static inline void GetAveragedX (
		uint32_t numMinibatches,
		uint32_t numMinibatchesAtATime,
		ColumnStore* cstore,
		float* xFinal,
		float* x)
	{
		for (uint32_t j = 0; j < cstore->m_numFeatures; j++) {
			xFinal[j] = 0;
		}
		for (uint32_t k = 0; k < numMinibatches/numMinibatchesAtATime; k++) {
			for (uint32_t j = 0; j < cstore->m_numFeatures; j++) {
				xFinal[j] += x[k*cstore->m_numFeatures + j];
			}
		}
		for (uint32_t j = 0; j < cstore->m_numFeatures; j++) {
			xFinal[j] = xFinal[j]/(numMinibatches/numMinibatchesAtATime);
		}
	}

private:
	inline float getDot(float* x, uint32_t sampleIndex) {
		float dot = 0.0;
		for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
			// cout << "x[" << j << "]: " << x[j] << endl;
			// cout << "m_samples[" << j << "]: " << m_cstore->m_samples[j][sampleIndex] << endl;
			dot += x[j]*m_cstore->m_samples[j][sampleIndex];
		}
		return dot;
	}

#ifdef AVX2
	inline float AVX_horizontalGetDot(float* x, uint32_t sampleIndex) {
		float dot = 0.0;

		if (m_cstore->m_numFeatures >= 8) {
			__m256 AVX_dot = _mm256_set1_ps(0.0);
			float gather[8];
			for (uint32_t j = 0; j < m_cstore->m_numFeatures-(m_cstore->m_numFeatures%8); j+=8) {
				__m256 AVX_x = _mm256_load_ps(x + j);

				for (uint32_t k = 0; k < 8; k++) {
					gather[k] = m_cstore->m_samples[j+k][sampleIndex];
				}
				__m256 AVX_samples = _mm256_load_ps(gather);
				AVX_dot = _mm256_fmadd_ps(AVX_x, AVX_samples, AVX_dot);
			}
			_mm256_store_ps(gather, AVX_dot);
			dot = gather[0] + gather[1] + gather[2] + gather[3] + gather[4] + gather[5] + gather[6] + gather[7];
		}
		for (uint32_t j = m_cstore->m_numFeatures-(m_cstore->m_numFeatures%8); j < m_cstore->m_numFeatures; j++) {
			dot += x[j]*m_cstore->m_samples[j][sampleIndex];
		}
		return dot;
	}

	inline __m256 AVX_verticalGetDot(float* x, uint32_t sampleIndex) {
		__m256 AVX_dot = _mm256_set1_ps(0.0);
		float dot[8];
		for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
			__m256 AVX_x = _mm256_set1_ps(x[j]);
			__m256 AVX_samples = _mm256_load_ps(m_cstore->m_samples[j] + sampleIndex);
			AVX_dot = _mm256_fmadd_ps(AVX_x, AVX_samples, AVX_dot);
		}
		
		return AVX_dot;
	}
#endif

	void updateL2svmGradient(float* gradient, float* x, uint32_t sampleIndex, AdditionalArguments* args) {
		float dot = getDot(x, sampleIndex);
		float temp = 1 - m_cstore->m_labels[sampleIndex]*dot;
		if (temp > 0) {
			for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
				if (m_cstore->m_labels[sampleIndex] > 0) {
					gradient[j] += args->m_costPos*(dot - m_cstore->m_labels[sampleIndex])*m_cstore->m_samples[j][sampleIndex];
				}
				else {
					gradient[j] += args->m_costNeg*(dot - m_cstore->m_labels[sampleIndex])*m_cstore->m_samples[j][sampleIndex];
				}
			}
		}
	}

	void updateLogregGradient(float* gradient, float* x, uint32_t sampleIndex) {
		float dot = getDot(x, sampleIndex);
		dot = ((1/(1+exp(-dot))) - m_cstore->m_labels[sampleIndex]);
		for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
			gradient[j] += dot*m_cstore->m_samples[j][sampleIndex];
		}
	}

	void updateLinregGradient(float* gradient, float* x, uint32_t sampleIndex) {
		float dot = getDot(x, sampleIndex);
		// cout << sampleIndex << " dot: " << dot << endl;
		dot -= m_cstore->m_labels[sampleIndex];
		for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
			gradient[j] += dot*m_cstore->m_samples[j][sampleIndex];
		}

	}

	void updateGradient(ModelType type, float* gradient, float* x, uint32_t sampleIndex, AdditionalArguments* args) {
		switch(type) {
			case l2svm:
				updateL2svmGradient(gradient, x, sampleIndex, args);
				break;
			case logreg:
				updateLogregGradient(gradient, x, sampleIndex);
				break;
			case linreg:
				updateLinregGradient(gradient, x, sampleIndex);
				break;
		}
	}
};