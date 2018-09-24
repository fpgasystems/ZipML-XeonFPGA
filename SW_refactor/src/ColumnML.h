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
#include <iostream>
#include <cmath>
#include <pthread.h>

#include "ColumnStore.h"
#include "../driver/iFPGA.h"

#ifdef AVX2
#include "immintrin.h"
#include "avx_mathfun.h"
#endif

using namespace std;

// #define PRINT_TIMING
#define PRINT_LOSS
// #define PRINT_ACCURACY

#define MAX_NUM_THREADS 14
#define NUM_FINSTANCES 4

enum ModelType {l2svm, logreg, linreg};

struct AdditionalArguments
{
	// l2svm
	float costPos;
	float costNeg;

	// logreg
	uint32_t startIndex;
	uint32_t length;

	// l2svm, linreg
	float decisionBoundary;
	float trueLabel;
	float falseLabel;
	
};

class ColumnML {
public:
	ColumnStore* m_cstore;

	ColumnML(bool getFPGA) {
		m_cstore = new ColumnStore();

		m_pageSizeInCacheLines = 65536; // 65536 x 64B = 4 MB
		m_pagesToAllocate = 1024;
		m_numValuesPerLine = 16;

		m_gotFPGA = false;
		if (getFPGA) {
			m_interfaceFPGA = new iFPGA(&m_runtimeClient, m_pagesToAllocate, m_pageSizeInCacheLines, 0);
			if(!m_runtimeClient.isOK()){
				cout << "FPGA runtime failed to start" << endl;
				exit(1);
			}
			m_gotFPGA = true;
		}	
	}

	~ColumnML() {
		delete m_cstore;
		if (m_gotFPGA) {
			delete m_interfaceFPGA;
		}
	}

	void printTimeout();

	void WriteLogregPredictions(float* x);

	float L2regularization(float* x, float lambda);
	float L1regularization(float* x, float lambda);
	float L2svmLoss(float* x, float costPos, float costNeg, float lambda);
	float LogregLoss(float* x, float lambda);
	float LinregLoss(float* x, float lambda);
	uint32_t LogregAccuracy(float* x, uint32_t startIndex, uint32_t length);
	uint32_t LinregAccuracy(float* x, float decisionBoundary, int trueLabel, int falseLabel);
	
	float Loss(ModelType type, float* x, float lambda, AdditionalArguments* args) {
		float result = 0.0;
		switch(type) {
			case l2svm:
				result = L2svmLoss(x, args->costPos, args->costNeg, lambda);
				break;
			case logreg:
				result = LogregLoss(x, lambda);
				break;
			case linreg:
				result = LinregLoss(x, lambda);
				break;
		}
		return result;
	}
	uint32_t Accuracy(ModelType type, float* x, AdditionalArguments* args) {
		uint32_t result = 0;
		switch(type) {
			case l2svm:
				result = LinregAccuracy(x, args->decisionBoundary, args->trueLabel, args->falseLabel);
				break;
			case logreg:
				result = LogregAccuracy(x, args->startIndex, args->length);
				break;
			case linreg:
				result = LinregAccuracy(x, args->decisionBoundary, args->trueLabel, args->falseLabel);
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

private:
	inline float getDot(float* x, uint32_t sampleIndex) {
		float dot = 0.0;
		for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
			dot += x[j]*m_cstore->m_samples[j][sampleIndex];
		}
		return dot;
	}

	inline float AVX_getDot(float* x, uint32_t sampleIndex) {
		float dot = 0.0;
		if (m_cstore->m_numFeatures >= 8) {
			__m256 dot_AVX = _mm256_set1_ps(0.0);
			for (uint32_t j = 0; j < m_cstore->m_numFeatures; j+=8) {
				dot_AVX = _mm256_fmadd_ps(AVX_x, AVX_samples, dot);
				dot += x[j]*m_cstore->m_samples[j][sampleIndex];
			}
		}
		
		return dot;
	}

	void updateL2svmGradient(float* gradient, float* x, uint32_t sampleIndex, AdditionalArguments* args) {
		float dot = getDot(x, sampleIndex);
		float temp = 1 - m_cstore->m_labels[sampleIndex]*dot;
		if (temp > 0) {
			for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
				if (m_cstore->m_labels[sampleIndex] > 0) {
					gradient[j] += args->costPos*(dot - m_cstore->m_labels[sampleIndex])*m_cstore->m_samples[j][sampleIndex];
				}
				else {
					gradient[j] += args->costNeg*(dot - m_cstore->m_labels[sampleIndex])*m_cstore->m_samples[j][sampleIndex];
				}
			}
		}
	}

	void updateLogregGradient(float* gradient, float* x, uint32_t sampleIndex) {
		float dot = getDot(x, sampleIndex);
		for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
			gradient[j] += ((1/(1+exp(-dot))) - m_cstore->m_labels[sampleIndex])*m_cstore->m_samples[j][sampleIndex];
		}
	}

	void updateLinregGradient(float* gradient, float* x, uint32_t sampleIndex) {
		float dot = getDot(x, sampleIndex);
		for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
			gradient[j] += (dot - m_cstore->m_labels[sampleIndex])*m_cstore->m_samples[j][sampleIndex];
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

	uint32_t m_pageSizeInCacheLines;
	uint32_t m_pagesToAllocate;
	uint32_t m_numValuesPerLine;

	bool m_gotFPGA;
	RuntimeClient m_runtimeClient;
	iFPGA* m_interfaceFPGA;
	uint32_t m_samplesAddress[NUM_FINSTANCES];
	uint32_t m_labelsAddress[NUM_FINSTANCES];
	uint32_t m_residualAddress[NUM_FINSTANCES];
	uint32_t m_stepAddress[NUM_FINSTANCES];
};