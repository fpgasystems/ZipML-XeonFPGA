// Copyright (C) 2017 Kaan Kara - Systems Group, ETH Zurich

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

#ifndef ZIPML_SGD
#define ZIPML_SGD

#include <iostream>
#include <string>
#include <fstream>
#include <stdint.h>
#include <stdio.h>
#include <vector>
#include <limits>
#include <cmath>

#include "iFPGA.h"

using namespace std;

class zipml_sgd {
public:
	float* a;	// Data set features matrix: numSamples x numFeatures
	float* b;	// Data set labels vector: numSamples
	int* bi;	// Integer version of b

	uint32_t numberOfIndices;

	uint32_t numFeatures;
	uint32_t numSamples;

	char gotFPGA;
	RuntimeClient runtimeClient;
	iFPGA* interfaceFPGA;
	uint32_t numCacheLines;

	char a_normalizedToMinus1_1;
	char b_normalizedToMinus1_1;
	float b_range;
	float b_min;
	uint32_t b_toIntegerScaler;

	zipml_sgd(char getFPGA, uint32_t _b_toIntegerScaler);
	~zipml_sgd();

	float calculate_loss(float x[]);

	// Data loading functions
	void load_tsv_data(char* pathToFile, uint32_t _numSamples, uint32_t _numFeatures);
	void load_libsvm_data(char* pathToFile, uint32_t _numSamples, uint32_t _numFeatures);

	// Normalization and data shaping
	void a_normalize(char toMinus1_1, char rowOrColumnWise);
	void b_normalize(char toMinus1_1, char binarize_b, float b_toBinarizeTo);

	// Return how many cache lines needed
	uint32_t copy_data_into_FPGA_memory();
	uint32_t copy_data_into_FPGA_memory_after_quantization(int quantizationBits, int _numberOfIndices, uint32_t address32offset);
	uint32_t get_number_of_CLs_needed_for_one_index(int quantizationBits);

	// Quantization function
	void quantize_data_integer(int aiq[], uint32_t numBits);

	// Linear Regression
	void float_linreg_SGD(float x_history[], uint32_t numberOfIterations, float stepSize);
	void Qfixed_linreg_SGD(float x_history[], uint32_t numberOfIterations, int stepSizeShifter, int quantizationBits);

	// FPGA-based SGD (solves either linear regression of L2 SVM, depending on what is loaded)
	void floatFSGD(float x[], uint32_t numberOfIterations, float stepSize, int binarize_b, float b_toBinarizeTo);
	void qFSGD(float x[], uint32_t numberOfIterations, int stepSizeShifter, int quantizationBits, int binarize_b, int bi_toBinarizeTo);

	// Calculate loss and log into file with detailed experiment information
	void log_history(char SWorFPGA, char fileOutput, int quantizationBits, float stepSize, int numberOfIterations, double time, float* x_history);

	// Perform inference
	void inference(float result[], float* x);
	void multi_classification(float* xs[], uint32_t numClasses);
};

zipml_sgd::zipml_sgd(char getFPGA, uint32_t _b_toIntegerScaler) {
	srand(7);

	a = NULL;
	b = NULL;
	bi = NULL;

	numFeatures = 0;
	numSamples = 0;

	b_toIntegerScaler = _b_toIntegerScaler;

	if (getFPGA == 1) {
		interfaceFPGA = new iFPGA(&runtimeClient);
		if(!runtimeClient.isOK()){
			cout << "FPGA runtime failed to start" << endl;
			exit(1);
		}
		gotFPGA = 1;
	}
	else
		gotFPGA = 0;
}

zipml_sgd::~zipml_sgd() {
	if (gotFPGA == 1)
		delete interfaceFPGA;

	if (a != NULL)
		free(a);
	if (b != NULL)
		free(b);
	if (bi != NULL)
		free(bi);
}

void zipml_sgd::load_tsv_data(char* pathToFile, uint32_t _numSamples, uint32_t _numFeatures) {
	cout << "Reading " << pathToFile << endl;

	numSamples = _numSamples;
	numFeatures = _numFeatures+1; // For the bias term
	a = (float*)malloc(numSamples*numFeatures*sizeof(float)); 
	b = (float*)malloc(numSamples*sizeof(float));
	bi = (int*)malloc(numSamples*sizeof(int));

	FILE* f;
	f = fopen(pathToFile, "r");

	uint32_t sample;
	uint32_t feature;
	float value;
	while(fscanf(f, "%d\t%d\t%f", &sample, &feature, &value) != EOF) {
		if (feature == -2) {
			b[sample] = value;
			bi[sample] = (int)(value*(float)b_toIntegerScaler);
		}
		else
			a[sample*numFeatures + (feature+1)] = value;
	}
	fclose(f);

	for (int i = 0; i < numSamples; i++) { // Bias term
		a[i*numFeatures] = 1.0;
	}

	cout << "numSamples: " << numSamples << endl;
	cout << "numFeatures: " << numFeatures << endl;
}

void zipml_sgd::load_libsvm_data(char* pathToFile, uint32_t _numSamples, uint32_t _numFeatures) {
	cout << "Reading " << pathToFile << endl;

	numSamples = _numSamples;
	numFeatures = _numFeatures+1; // For the bias term
	a = (float*)malloc(numSamples*numFeatures*sizeof(float)); 
	b = (float*)malloc(numSamples*sizeof(float));
	bi = (int*)malloc(numSamples*sizeof(int));

	string line;
	ifstream f(pathToFile);

	int index = 0;
	if (f.is_open()) {
		while( index < numSamples ) {
			getline(f, line);
			int pos0 = 0;
			int pos1 = 0;
			int pos2 = 0;
			int column = 0;
			//while ( column < numFeatures-1 ) {
			while ( pos2 < line.length()+1 ) {
				if (pos2 == 0) {
					pos2 = line.find(" ", pos1);
					float temp = stof(line.substr(pos1, pos2-pos1), NULL);
					b[index] = temp;
					bi[index] = (int)(temp*(float)b_toIntegerScaler);
				}
				else {
					pos0 = pos2;
					pos1 = line.find(":", pos1)+1;
					pos2 = line.find(" ", pos1);
					column = stof(line.substr(pos0+1, pos1-pos0-1));
					if (pos2 == -1) {
						pos2 = line.length()+1;
						a[index*numFeatures + column] = stof(line.substr(pos1, pos2-pos1), NULL);
					}
					else
						a[index*numFeatures + column] = stof(line.substr(pos1, pos2-pos1), NULL);
				}
			}
			index++;
		}
		f.close();
	}
	else
		cout << "Unable to open file " << pathToFile << endl;

	for (int i = 0; i < numSamples; i++) { // Bias term
		a[i*numFeatures] = 1.0;
	}
	
	cout << "numSamples: " << numSamples << endl;
	cout << "numFeatures: " << numFeatures << endl;
}

void zipml_sgd::a_normalize(char toMinus1_1, char rowOrColumnWise) {
	a_normalizedToMinus1_1 = toMinus1_1;
	if (rowOrColumnWise == 'r') {
		for (int i = 0; i < numSamples; i++) {
			float amin = numeric_limits<float>::max();
			float amax = numeric_limits<float>::min();
			for (int j = 0; j < numFeatures; j++) {
				float a_here = a[i*numFeatures + j];
				if (a_here > amax)
					amax = a_here;
				if (a_here < amin)
					amin = a_here;
			}
			float arange = amax - amin;
			if (arange > 0) {
				if (toMinus1_1 == 1) {
					for (int j = 0; j < numFeatures; j++) {
						a[i*numFeatures + j] = ((a[i*numFeatures + j] - amin)/arange)*2.0-1.0;
					}
				}
				else {
					for (int j = 0; j < numFeatures; j++) {
						a[i*numFeatures + j] = ((a[i*numFeatures + j] - amin)/arange);
					}
				}
			}
		}
	}
	else {
		for (int j = 1; j < numFeatures; j++) { // Don't normalize bias
			float amin = numeric_limits<float>::max();
			float amax = numeric_limits<float>::min();
			for (int i = 0; i < numSamples; i++) {
				float a_here = a[i*numFeatures + j];
				if (a_here > amax)
					amax = a_here;
				if (a_here < amin)
					amin = a_here;
			}
			float arange = amax - amin;
			if (arange > 0) {
				if (toMinus1_1 == 1) {
					for (int i = 0; i < numSamples; i++) {
						a[i*numFeatures + j] = ((a[i*numFeatures + j] - amin)/arange)*2.0-1.0;
					}
				}
				else {
					for (int i = 0; i < numSamples; i++) {
						a[i*numFeatures + j] = ((a[i*numFeatures + j] - amin)/arange);
					}
				}
			}
		}
	}
}

void zipml_sgd::b_normalize(char toMinus1_1, char binarize_b, float b_toBinarizeTo) {
	b_normalizedToMinus1_1 = toMinus1_1;
	if (binarize_b == 0) {
		float bmin = numeric_limits<float>::max();
		float bmax = numeric_limits<float>::min();
		for (int i = 0; i < numSamples; i++) {
			if (b[i] > bmax)
				bmax = b[i];
			if (b[i] < bmin)
				bmin = b[i];
		}
		cout << "bmax: " << bmax << ", bmin: " << bmin << endl;
		float brange = bmax - bmin;
		if (brange > 0) {
			if (toMinus1_1 == 1) {
				for (int i = 0; i < numSamples; i++) {
					b[i] = ((b[i]-bmin)/brange)*2.0 - 1.0;
					bi[i] = (int)(b[i]*(float)b_toIntegerScaler);
				}
			}
			else {
				for (int i = 0; i < numSamples; i++) {
					b[i] = (b[i]-bmin)/brange;
					bi[i] = (int)(b[i]*(float)b_toIntegerScaler);
				}
			}
		}
		b_min = bmin;
		b_range = brange;
	}
	else {
		for (int i = 0; i < numSamples; i++) {
			if(b[i] == b_toBinarizeTo)
				b[i] = 1.0;
			else
				b[i] = -1.0;

			bi[i] = (int)(b[i]*(float)b_toIntegerScaler);
		}
		b_min = -1.0;
		b_range = 2.0;
	}
}

uint32_t zipml_sgd::copy_data_into_FPGA_memory() {
	uint32_t address32 = 0;
	// Copy data to FPGA shared memory
	for (int i = 0; i < numSamples; i++) {
		for (int j = 0; j < numFeatures; j++) {
			interfaceFPGA->writeToMemoryFloat('i', a[i*numFeatures + j], address32);
			address32++;
		}
		for (int k = 0; k < 15-numFeatures%16; k++) {
			interfaceFPGA->writeToMemoryFloat('i', 0, address32);
			address32++;
		}
		interfaceFPGA->writeToMemoryFloat('i', b[i], address32);
		address32++;
	}
	cout << "address32: " << address32 << endl;
	uint32_t cacheLines = address32/16;
	numCacheLines = cacheLines;
	return cacheLines;
}

uint32_t zipml_sgd::copy_data_into_FPGA_memory_after_quantization(int quantizationBits, int _numberOfIndices, uint32_t address32offset) {
	numberOfIndices = _numberOfIndices;

	uint32_t address32 = address32offset;
	uint32_t address16 = 0;
	uint32_t address8 = 0;
	uint32_t address4 = 0;
	uint32_t address2 = 0;
	int aiq1[numSamples*numFeatures];
	int aiq2[numSamples*numFeatures];
	for (int i = 0; i < _numberOfIndices; i++) {
		quantize_data_integer(aiq1, quantizationBits);
		quantize_data_integer(aiq2, quantizationBits);

		for (int i = 0; i < numSamples; i++) {
			uint32_t temp = 0;
			for (int j = 0; j < numFeatures; j++) {
				int q1 = aiq1[i*numFeatures + j];
				int q2 = aiq2[i*numFeatures + j];
				
				if (quantizationBits == 1) {
					temp = temp | ( (((q2&0x1)<<1 | (q1&0x1))&0x3) << (2*(address2%16)) ) ;
					address2++;
					if (address2%16 == 0) {
						interfaceFPGA->writeToMemory32('i', temp, address32);
						address32++;
						temp = 0;
					}
				}
				else if (quantizationBits == 2) {
					temp = temp | ( (((q2&0x3)<<2 | (q1&0x3))&0xF) << (4*(address4%8)) ) ;
					address4++;
					if (address4%8 == 0) {
						interfaceFPGA->writeToMemory32('i', temp, address32);
						address32++;
						temp = 0;
					}
				}
				else if (quantizationBits == 4) {
					temp = temp | ( (((q2&0xF)<<4 | (q1&0xF))&0xFF) << (8*(address8%4)) ) ;
					address8++;
					if (address8%4 == 0) {
						interfaceFPGA->writeToMemory32('i', temp, address32);
						address32++;
						temp = 0;
					}
				}
				else if (quantizationBits == 8) {
					temp = temp | ( (((q2&0xFF)<<8 | (q1&0xFF))&0xFFFF) << (16*(address16%2)) ) ;
					address16++;
					if (address16%2 == 0) {
						interfaceFPGA->writeToMemory32('i', temp, address32);
						address32++;
						temp = 0;
					}
				}
				else {
					cout << "FPGA can only handle 1, 2, 4, 8 bit quantization." << endl;
					return 0;
				}
			}
			if (quantizationBits == 1) {
				if (address2%16 != 0) {
					interfaceFPGA->writeToMemory32('i', temp, address32);
					address32++;
					address2 += 16-address2%16;
					temp = 0;
				}
			}
			else if (quantizationBits == 2) {
				if (address4%8 != 0) {
					interfaceFPGA->writeToMemory32('i', temp, address32);
					address32++;
					address4 += 8-address4%8;
					temp = 0;
				}
			}
			else if (quantizationBits == 4) {
				if (address8%4 != 0) {
					interfaceFPGA->writeToMemory32('i', temp, address32);
					address32++;
					address8 += 4-address8%4;
					temp = 0;
				}
			}
			else if (quantizationBits == 8) {
				if (address16%2 != 0) {
					interfaceFPGA->writeToMemory32('i', temp, address32);
					address32++;
					address16 += 2-address16%2;
					temp = 0;
				}
			}
			uint32_t address32untilNow = address32;
			for (int k = 0; k < 15-address32untilNow%16; k++) {
				//interfaceFPGA->writeToMemory32('i', 0, address32);
				address32++;
				address2 += 16;
				address4 += 8;
				address8 += 4;
				address16 += 2;
			}
			interfaceFPGA->writeToMemory32('i', bi[i], address32);
			address32++;
			address2 += 16;
			address4 += 8;
			address8 += 4;
			address16 += 2;	
		}
	}
	/*cout << "address32: " << address32 << endl;
	cout << "address2: " << address2 << endl;
	cout << "address4: " << address4 << endl;
	cout << "address8: " << address8 << endl;
	cout << "address16: " << address16 << endl;*/
	uint32_t cacheLines = address32/16;
	return cacheLines/numberOfIndices;
}

uint32_t zipml_sgd::get_number_of_CLs_needed_for_one_index(int quantizationBits) {
	uint32_t address32 = 0;
	uint32_t address16 = 0;
	uint32_t address8 = 0;
	uint32_t address4 = 0;
	uint32_t address2 = 0;

	for (int i = 0; i < numSamples; i++) {
		for (int j = 0; j < numFeatures; j++) {
			if (quantizationBits == 1) {
				address2++;
				if (address2%16 == 0) {
					address32++;
				}
			}
			else if (quantizationBits == 2) {
				address4++;
				if (address4%8 == 0) {
					address32++;
				}
			}
			else if (quantizationBits == 4) {
				address8++;
				if (address8%4 == 0) {
					address32++;
				}
			}
			else if (quantizationBits == 8) {
				address16++;
				if (address16%2 == 0) {
					address32++;
				}
			}
			else {
				cout << "FPGA can only handle 1, 2, 4, 8 bit quantization." << endl;
				return 0;
			}
		}
		if (quantizationBits == 1) {
			if (address2%16 != 0) {
				address32++;
				address2 += 16-address2%16;
			}
		}
		else if (quantizationBits == 2) {
			if (address4%8 != 0) {
				address32++;
				address4 += 8-address4%8;
			}
		}
		else if (quantizationBits == 4) {
			if (address8%4 != 0) {
				address32++;
				address8 += 4-address8%4;
			}
		}
		else if (quantizationBits == 8) {
			if (address16%2 != 0) {
				address32++;
				address16 += 2-address16%2;
			}
		}
		uint32_t address32untilNow = address32;
		for (int k = 0; k < 15-address32untilNow%16; k++) {
			address32++;
			address2 += 16;
			address4 += 8;
			address8 += 4;
			address16 += 2;
		}
		address32++;
		address2 += 16;
		address4 += 8;
		address8 += 4;
		address16 += 2;
	}
	uint32_t cacheLines = address32/16;
	return cacheLines;
}

// Provide: int aiq[numSamples*numFeatures]
void zipml_sgd::quantize_data_integer(int aiq[], uint32_t numBits) {
	int numLevels = (1 << (numBits-1)) + 1;

	if (a_normalizedToMinus1_1 == 0) {
		for (int j = 0; j < numFeatures; j++) { // For every feature
			for (int i = 0; i < numSamples; i++) { // For every sample

				float scaledElement = a[i*numFeatures+j]*(numLevels-1);
				int baseLevel = (int)scaledElement;
				
				float toBaseLevelProbability = 1.0 - (scaledElement - (float)baseLevel);

				float probability = ((float)rand())/RAND_MAX; //0 to 1
				//float probability = 0.5;
				if (toBaseLevelProbability > probability)
					aiq[i*numFeatures + j] = (int)(baseLevel);
				else
					aiq[i*numFeatures + j] = (int)(baseLevel+1);
			}
		}
	}
	else {
		for (int j = 0; j < numFeatures; j++) { // For every feature
			for (int i = 0; i < numSamples; i++) { // For every sample

				float a_here = a[i*numFeatures + j];
				if (a_here > 0) {
					float scaledElement = a_here*((numLevels-1)/2);
					int baseLevel = (int)scaledElement;
					
					float toBaseLevelProbability = 1.0 - (scaledElement - (float)baseLevel);

					float probability = ((float)rand())/RAND_MAX; //0 to 1
					//float probability = 0.5;
					if (toBaseLevelProbability > probability)
						aiq[i*numFeatures + j] = (int)(baseLevel);
					else
						aiq[i*numFeatures + j] = (int)(baseLevel+1);
				}
				else {
					float temp = -a_here;

					float scaledElement = temp*((numLevels-1)/2);
					int baseLevel = (int)scaledElement;

					float toBaseLevelProbability = 1.0 - (scaledElement - (float)baseLevel);

					float probability = ((float)rand())/RAND_MAX; //0 to 1
					//float probability = 0.5;
					if (toBaseLevelProbability > probability)
						aiq[i*numFeatures + j] = (int)(-(baseLevel));
					else
						aiq[i*numFeatures + j] = (int)(-(baseLevel+1));
				}
			}
		}
	}
}

// Provide: float x_history[numberOfIterations*numFeatures]
void zipml_sgd::float_linreg_SGD(float x_history[], uint32_t numberOfIterations, float stepSize) {
	float x[numFeatures];
	for (int j = 0; j < numFeatures; j++) {
		x[j] = 0.0;
	}

	for(int epoch = 0; epoch < numberOfIterations; epoch++) {
		for (int i = 0; i < numSamples; i++) {
			float dot = 0;
			for (int j = 0; j < numFeatures; j++) {
				dot += x[j]*a[i*numFeatures + j];
			}
			for (int j = 0; j < numFeatures; j++) {
				x[j] -= stepSize*(dot - b[i])*a[i*numFeatures + j];
			}
		}
		for (int j = 0; j < numFeatures; j++) {
			x_history[epoch*numFeatures + j] = x[j];
		}
		cout << epoch << endl;
	}
}

// Provide: float x_history[numberOfIterations*numFeatures]
void zipml_sgd::Qfixed_linreg_SGD(float x_history[], uint32_t numberOfIterations, int stepSizeShifter, int quantizationBits) {
	int xi[numFeatures];
	for (int j = 0; j < numFeatures; j++) {
		xi[j] = 0;
	}

	int numBitsToShift;
	if (a_normalizedToMinus1_1 == 0)
		numBitsToShift = quantizationBits-1;
	else
		numBitsToShift = quantizationBits-2;

	int* aiq1 = (int*)malloc(numSamples*numFeatures*sizeof(int));
	int* aiq2 = (int*)malloc(numSamples*numFeatures*sizeof(int));
	for(int epoch = 0; epoch < numberOfIterations; epoch++) {
		quantize_data_integer(aiq1, quantizationBits);
		quantize_data_integer(aiq2, quantizationBits);

		for (int i = 0; i < numSamples; i++) {
			int dot = 0;
			for (int j = 0; j < numFeatures; j++) {
				dot += (xi[j]*aiq1[i*numFeatures + j]) >> numBitsToShift;
			}
			for (int j = 0; j < numFeatures; j++) {
				xi[j] -= ( ((dot - bi[i])*aiq2[i*numFeatures + j]) >> (stepSizeShifter + numBitsToShift) );
			}
		}
		for (int j = 0; j < numFeatures; j++) {
			x_history[epoch*numFeatures + j] = ((float)xi[j]/(float)b_toIntegerScaler);
		}
		cout << epoch << endl;
	}
	free(aiq1);
	free(aiq2);
}

// Provide: float x[numFeatures]
void zipml_sgd::floatFSGD(float x[], uint32_t numberOfIterations, float stepSize, int binarize_b, float b_toBinarizeTo) {
	cout << "numCacheLines: " << numCacheLines << endl;

	int minibatch_size = 36;

	interfaceFPGA->m_AFUService->CSRWrite(CSR_READ_OFFSET, 0);
	interfaceFPGA->m_AFUService->CSRWrite(CSR_WRITE_OFFSET, 0);
	interfaceFPGA->m_AFUService->CSRWrite(CSR_NUM_LINES, numCacheLines);
	uint32_t* b_to_binarize_toAddr = (uint32_t*) &b_toBinarizeTo;
	interfaceFPGA->m_AFUService->CSRWrite(CSR_MY_CONFIG5, *b_to_binarize_toAddr);
	interfaceFPGA->m_AFUService->CSRWrite(CSR_MY_CONFIG4, numSamples);
	interfaceFPGA->m_AFUService->CSRWrite(CSR_MY_CONFIG3, numberOfIterations << 18 /*Number of epochs*/ | numFeatures); // Samples
	interfaceFPGA->m_AFUService->CSRWrite(CSR_MY_CONFIG2, ((minibatch_size&0xFFFF) << 10) | (binarize_b << 1) );
	uint32_t* stepSizeAddr = (uint32_t*) &stepSize;
	interfaceFPGA->m_AFUService->CSRWrite(CSR_MY_CONFIG1, *stepSizeAddr);

	interfaceFPGA->doTransaction();

	int numCLsForX = numFeatures/16 + 1;

	uint32_t offset = (numberOfIterations-1)*numCLsForX*16;
	for (int j = 0; j < numFeatures; j++) {
		int32_t temp = interfaceFPGA->readFromMemory32('o', j + offset);
		x[j] = (float)temp;
		x[j] = x[j]/b_toIntegerScaler;
	}
}

// Provide: float x[numFeatures]
void zipml_sgd::qFSGD(float x[], uint32_t numberOfIterations, int stepSizeShifter, int quantizationBits, int binarize_b, int bi_toBinarizeTo) {
	cout << "numCacheLines: " << numCacheLines << endl;
	cout << "numberOfIndices: " << numberOfIndices << endl;

	int minibatch_size = 16;
	int stepSizeDeclineInterval = 128-1;

	interfaceFPGA->m_AFUService->CSRWrite(CSR_READ_OFFSET, 0);
	interfaceFPGA->m_AFUService->CSRWrite(CSR_WRITE_OFFSET, 0);
	interfaceFPGA->m_AFUService->CSRWrite(CSR_NUM_LINES, numCacheLines);
	interfaceFPGA->m_AFUService->CSRWrite(CSR_MY_CONFIG5, bi_toBinarizeTo);
	interfaceFPGA->m_AFUService->CSRWrite(CSR_MY_CONFIG4, numSamples);
	interfaceFPGA->m_AFUService->CSRWrite(CSR_MY_CONFIG3, numberOfIterations << 18 /*Number of epochs*/ | numFeatures); // Samples
	interfaceFPGA->m_AFUService->CSRWrite(CSR_MY_CONFIG2, ((minibatch_size&0xFFFF) << 10) | ((numberOfIndices&0xFF) << 2) | (binarize_b << 1) | a_normalizedToMinus1_1);
	interfaceFPGA->m_AFUService->CSRWrite(CSR_MY_CONFIG1, ((stepSizeDeclineInterval&0x3FFF) << 6) | stepSizeShifter&0x3F);

	interfaceFPGA->doTransaction();

	int numCLsForX = numFeatures/16 + 1;
	if (quantizationBits == 1 && numCLsForX%16 != 0)
		numCLsForX = numCLsForX + (16-numCLsForX%16);
	else if (quantizationBits == 2 && numCLsForX%8 != 0)
		numCLsForX = numCLsForX + (8-numCLsForX%8);
	else if (quantizationBits == 4 && numCLsForX%4 != 0)
		numCLsForX = numCLsForX + (4-numCLsForX%4);
	else if (quantizationBits == 8 && numCLsForX%2 != 0)
		numCLsForX = numCLsForX + 1;

	uint32_t offset = (numberOfIterations-1)*numCLsForX*16;
	for (int j = 0; j < numFeatures; j++) {
		int32_t temp = interfaceFPGA->readFromMemory32('o', j + offset);
		x[j] = (float)temp;
		x[j] = x[j]/b_toIntegerScaler;
	}
}

float zipml_sgd::calculate_loss(float x[]) {
	float loss = 0;
	for(int i = 0; i < numSamples; i++) {
		float dot = 0.0;
		for (int j = 0; j < numFeatures; j++) {
			dot += x[j]*a[i*numFeatures + j];
		}
		loss += (dot - b[i])*(dot - b[i]);
	}

	loss /= (float)(2*numSamples);
	return loss;
}

void zipml_sgd::log_history(char SWorFPGA, char fileOutput, int quantizationBits, float stepSize, int numberOfIterations, double time, float* x_history) {
	char* fileName = (char*)malloc(200);
	FILE* f;

	cout << "Time: " << time << endl;

	if (SWorFPGA == 's') {
		if (fileOutput == 1) {
			sprintf(fileName, "logs/SW_SGDhistory_%d_%d_%.6f_%d.log", numSamples, numFeatures, stepSize, numberOfIterations);
			cout << "fileName:" << fileName << endl;
			f = fopen(fileName, "w");
			fprintf(f, "a_normalizedToMinus1_1\t%d\n", a_normalizedToMinus1_1);
			fprintf(f, "b_normalizedToMinus1_1\t%d\n", b_normalizedToMinus1_1);
			fprintf(f, "b_toIntegerScaler\t%x\n", b_toIntegerScaler);
			fprintf(f, "numSamples\t%d\n", numSamples);
			fprintf(f, "numFeatures\t%d\n", numFeatures);
			fprintf(f, "numIterations\t%d\n", numberOfIterations);
			fprintf(f, "stepSize\t%.10f\n", stepSize);
			fprintf(f, "time\t%.10f\n", time);
		}
		
		// Calculate initial loss
		float x_zero[numFeatures];
		for (int j = 0; j < numFeatures; j++) {
			x_zero[j] = 0.0;
		}
		float J0 = calculate_loss(x_zero);
		cout << J0 << endl;
		if (fileOutput == 1)
			fprintf(f, "J\t%d\t%d\t%.10f\n", -1, 0, J0);

		double epoch_time = time/numberOfIterations;

		for(int epoch = 0; epoch < numberOfIterations; epoch++) {
			float J = calculate_loss(x_history + epoch*numFeatures);
			cout << J << endl;
			if (fileOutput == 1)
				fprintf(f, "J\t%d\t%.10f\t%.10f\n", epoch, epoch_time*(epoch+1), J);
		}
		if (fileOutput == 1)
			fclose(f);
	}
	else if (SWorFPGA == 'h') {
		if (fileOutput == 1) {
			sprintf(fileName, "logs/Q%dfixedSGDhistory_%d_%d_%.6f_%d.log", quantizationBits, numSamples, numFeatures, stepSize, numberOfIterations);
			cout << "fileName:" << fileName << endl;
			f = fopen(fileName, "w");
			fprintf(f, "numberOfIndices\t%d\n", numberOfIndices);
			fprintf(f, "a_normalizedToMinus1_1\t%d\n", a_normalizedToMinus1_1);
			fprintf(f, "b_normalizedToMinus1_1\t%d\n", b_normalizedToMinus1_1);
			fprintf(f, "b_toIntegerScaler\t%x\n", b_toIntegerScaler);
			fprintf(f, "numCacheLines\t%d\n", numCacheLines);
			fprintf(f, "quantizationBits\t%d\n", quantizationBits);
			fprintf(f, "numSamples\t%d\n", numSamples);
			fprintf(f, "numFeatures\t%d\n", numFeatures);
			fprintf(f, "numIterations\t%d\n", numberOfIterations);
			fprintf(f, "stepSize\t%.10f\n", stepSize);
			fprintf(f, "time\t%.10f\n", time);
		}

		float x[numFeatures];

		int numCLsForX = numFeatures/16 + 1;
		if (quantizationBits == 1 && numCLsForX%16 != 0)
			numCLsForX = numCLsForX + (16-numCLsForX%16);
		else if (quantizationBits == 2 && numCLsForX%8 != 0)
			numCLsForX = numCLsForX + (8-numCLsForX%8);
		else if (quantizationBits == 4 && numCLsForX%4 != 0)
			numCLsForX = numCLsForX + (4-numCLsForX%4);
		else if (quantizationBits == 8 && numCLsForX%2 != 0)
			numCLsForX = numCLsForX + 1;

		cout << "numCLsForX: " << numCLsForX << endl;

		// Calculate initial loss
		float x_zero[numFeatures];
		for (int j = 0; j < numFeatures; j++) {
			x_zero[j] = 0.0;
		}
		float J0 = calculate_loss(x_zero);
		cout << J0 << endl;
		if (fileOutput == 1)
			fprintf(f, "J\t%d\t%d\t%.10f\n", -1, 0, J0);

		double epoch_time = time/numberOfIterations;

		for(int epoch = 0; epoch < numberOfIterations; epoch++) {
			float x[numFeatures];
			uint32_t offset = epoch*numCLsForX*16;
			for (int j = 0; j < numFeatures; j++) {
				int32_t temp = interfaceFPGA->readFromMemory32('o', offset + j);
				x[j] = (float)temp;
				x[j] = x[j]/b_toIntegerScaler;
			}

			float J = calculate_loss(x);
			cout << J << endl;
			if (fileOutput == 1)
				fprintf(f, "J\t%d\t%.10f\t%.10f\n", epoch, epoch_time*(epoch+1), J);
		}
		if (fileOutput == 1)
			fclose(f);
	}

	free(fileName);
}

void zipml_sgd::inference(float result[], float* x) {
	//float result[numSamples];

	int count_trues = 0;
	for (int i = 0; i < numSamples; i++) {
		float dot = 0;
		for (int j = 0; j < numFeatures; j++) {
			dot += x[j]*a[i*numFeatures + j];
		}
		if (b_normalizedToMinus1_1 == 0) {
			dot = dot*b_range + b_min;
		}
		else if (b_normalizedToMinus1_1 == 1) {
			dot = (dot+1.0)*(b_range/2.0) + b_min;
		}
		result[i] = dot;
		int prediction = (int)(dot+0.5);
		if((int)b[i] == prediction)
			count_trues++;
	}
	cout << "True predictions: " << count_trues << " out of " << numSamples << " samples." << endl;
}

void zipml_sgd::multi_classification(float* xs[], uint32_t numClasses) {
	int count_trues = 0;
	for (int i = 0; i < numSamples; i++) {
		float max = 0.0;
		int matched_class = -1;
		for (int c = 0; c < numClasses; c++) {
			float dot = 0;
			for (int j = 0; j < numFeatures; j++) {
				dot += xs[c][j]*a[i*numFeatures + j];
			}
			if (dot > max) {
				max = dot;
				matched_class = c;
			}
		}
		if ((int)b[i] == matched_class)
			count_trues++;
	}
	cout << "True predictions: " << count_trues << " out of " << numSamples << " samples." << endl;
}


#endif