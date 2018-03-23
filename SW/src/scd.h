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
#include "aes.h"

#ifdef AVX2
#include "immintrin.h"
#endif

using namespace std;

#define NUM_THREADS 14
#define NUM_FINSTANCES 4

class scd {

public:
	float** a;	// Data set features matrix: numSamples x numFeatures
	float* b;	// Data set labels vector: numSamples

	uint32_t** compressed_a;
	uint32_t** compressed_a_sizes;
	uint32_t** encrypted_a;

	uint32_t numFeatures;
	uint32_t numSamples;

	char a_normalizedToMinus1_1;
	char b_normalizedToMinus1_1;
	float b_range;
	float b_min;
	
	char gotFPGA;
	RuntimeClient runtimeClient;
	iFPGA* interfaceFPGA;
	uint32_t a_address[NUM_FINSTANCES];
	uint32_t b_address[NUM_FINSTANCES];
	uint32_t residual_address[NUM_FINSTANCES];
	uint32_t step_address[NUM_FINSTANCES];

	uint32_t page_size_in_cache_lines;
	uint32_t pages_to_allocate;
	uint32_t numValuesPerLine;

	unsigned char initKey[32];
	unsigned char* KEYS_enc;
	unsigned char* KEYS_dec;
	unsigned char ivec[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

	scd(char getFPGA){
		page_size_in_cache_lines = 65536; // 65536 x 64B = 4 MB
		pages_to_allocate = 8;
		numValuesPerLine = 16;

		if (getFPGA == 1) {
			interfaceFPGA = new iFPGA(&runtimeClient, pages_to_allocate, page_size_in_cache_lines, 0);
			if(!runtimeClient.isOK()){
				cout << "FPGA runtime failed to start" << endl;
				exit(1);
			}
			gotFPGA = 1;

			cout << "Zeroing allocated space.." << endl;
			for (uint32_t i = 0; i < numValuesPerLine*page_size_in_cache_lines*pages_to_allocate; i++) {
				interfaceFPGA->writeToMemory32('i', 0, i);
			}
		}
		else
			gotFPGA = 0;

		a = NULL;
		b = NULL;

		compressed_a = NULL;
		encrypted_a = NULL;

		for (uint32_t i = 0; i < 32; i++) {
			initKey[i] = (unsigned char)i;
		}
		KEYS_enc = (unsigned char*)malloc(16*15);
		KEYS_dec = (unsigned char*)malloc(16*15);
		AES_256_Key_Expansion(initKey, KEYS_enc);
		AES_256_Decryption_Keys(KEYS_enc, KEYS_dec);
	}
	~scd() {
		if (gotFPGA == 1)
			delete interfaceFPGA;

		if (a != NULL) {
			for (uint32_t j = 0; j < numFeatures; j++) {
				free(a[j]);
			}
			free(a);
		}
		if (b != NULL)
			free(b);

		if (compressed_a != NULL) {
			for (uint32_t j = 0; j < numFeatures; j++) {
				free(compressed_a[j]);
				free(compressed_a_sizes[j]);
			}
			free(compressed_a);
			free(compressed_a_sizes);
		}
		if (encrypted_a != NULL) {
			for (uint32_t j = 0; j < numFeatures; j++) {
				free(encrypted_a[j]);
			}
			free(encrypted_a);
		}

		free(KEYS_enc);
		free(KEYS_dec);
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

	void print_timeout() {
		uint32_t NumberOfRequestedReads = interfaceFPGA->readFromMemory32('i', 0);
		uint32_t NumberOfReceivedReads = interfaceFPGA->readFromMemory32('i', 1);
		uint32_t residual_NumberOfReceivedReads = interfaceFPGA->readFromMemory32('i', 2);
		uint32_t b_NumberOfReceivedReads = interfaceFPGA->readFromMemory32('i', 3);
		uint32_t a_NumberOfReceivedReads = interfaceFPGA->readFromMemory32('i', 4);
		uint32_t residual_NumberOfWriteRequests = interfaceFPGA->readFromMemory32('i', 5);
		uint32_t step_NumberOfWriteRequests = interfaceFPGA->readFromMemory32('i', 6);
		uint32_t NumberOfWriteResponses = interfaceFPGA->readFromMemory32('i', 7);
		uint32_t completed_epochs = interfaceFPGA->readFromMemory32('i', 8);
		uint32_t reorder_free_count = interfaceFPGA->readFromMemory32('i', 9);
		uint32_t feature_update_index = interfaceFPGA->readFromMemory32('i', 10);
		uint32_t write_batch_index = interfaceFPGA->readFromMemory32('i', 11);
		uint32_t decompressor_out_fifo_free_count = interfaceFPGA->readFromMemory32('i', 12);

		cout << "NumberOfRequestedReads: " << NumberOfRequestedReads << endl;
		cout << "NumberOfReceivedReads: " << NumberOfReceivedReads << endl;
		cout << "residual_NumberOfReceivedReads: " << residual_NumberOfReceivedReads << endl;
		cout << "b_NumberOfReceivedReads: " << b_NumberOfReceivedReads << endl;
		cout << "a_NumberOfReceivedReads: " << a_NumberOfReceivedReads << endl;
		cout << "residual_NumberOfWriteRequests: " << residual_NumberOfWriteRequests << endl;
		cout << "step_NumberOfWriteRequests: " << step_NumberOfWriteRequests << endl;
		cout << "NumberOfWriteResponses: " << NumberOfWriteResponses << endl;
		cout << "completed_epochs: " << completed_epochs << endl;
		cout << "reorder_free_count: " << reorder_free_count << endl;
		cout << "feature_update_index: " << feature_update_index << endl;
		cout << "write_batch_index: " << write_batch_index << endl;
		cout << "decompressor_out_fifo_free_count: " << decompressor_out_fifo_free_count << endl;
	}

	// Data loading functions
	void load_libsvm_data(char* pathToFile, uint32_t _numSamples, uint32_t _numFeatures);
	void load_raw_data(char* path_to_file, uint32_t _numSamples, uint32_t _numFeatures);
	void generate_synthetic_data(uint32_t _numSamples, uint32_t _numFeatures, char binary);

	// Normalization and data shaping
	void a_normalize(char toMinus1_1, char rowOrColumnWise);
	void b_normalize(char toMinus1_1, char binarize_b, float b_toBinarizeTo);
	float compress_a(uint32_t minibatchSize, uint32_t toIntegerScaler);
	void encrypt_a(uint32_t minibatchSize, char useCompressed);

	float calculate_loss(float* x);
	uint32_t calculate_accuracy(float* x);

	void float_linreg_SGD(float* x_history, uint32_t numEpochs, uint32_t minibatchSize, float stepSize);
	void float_linreg_SCD(float* x_history, uint32_t numEpochs, uint32_t minibatchSize, uint32_t numMinibatchesAtATime, uint32_t residualUpdatePeriod, float stepSize, char useEncrypted, char useCompressed, uint32_t toIntegerScaler);
#ifdef AVX2
	void AVX_float_linreg_SCD(float* x_history, uint32_t numEpochs, uint32_t minibatchSize, float stepSize, char useEncrypted, char useCompressed, uint32_t toIntegerScaler);
	void AVXmulti_float_linreg_SCD(float* x_history, uint32_t numEpochs, uint32_t minibatchSize, float stepSize, char useEncrypted, char useCompressed, uint32_t toIntegerScaler);
	double AVXmulti_justread(uint32_t minibatchSize, char useEncrypted, char useCompressed);
#endif

	uint32_t print_FPGA_memory();
	uint32_t copy_data_into_FPGA_memory(uint32_t numMinibatches, uint32_t minibatchSize, uint32_t numMinibatchesToAssign[], uint32_t numEpochs, char useEncrypted);
	uint32_t copy_compressed_data_into_FPGA_memory(uint32_t numMinibatches, uint32_t minibatchSize, uint32_t numMinibatchesToAssign[], uint32_t numEpochs, char useEncrypted);
	void float_linreg_FSCD(float* x_history, uint32_t numEpochs, uint32_t minibatchSize, uint32_t residualUpdatePeriod, float stepSize, char enableStaleness, char useEncrypted, char useCompressed, uint32_t toIntegerScaler, uint32_t numInstancesToUse);
	
	uint32_t decompress_column(uint32_t* compressedColumn, uint32_t inNumWords, float* decompressedColumn, uint32_t toIntegerScaler);
	uint32_t compress_column(float* originalColumn, uint32_t inNumWords, uint32_t* compressedColumn, uint32_t toIntegerScaler);

	void decrypt_column(uint32_t* encryptedColumn, uint32_t inNumWords, float* decryptedColumn);
	void encrypt_column(float* originalColumn, uint32_t inNumWords, uint32_t* encryptedColumn);
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
	else {
		cout << "Unable to open file " << pathToFile << endl;
		exit(1);
	}

	for (uint32_t i = 0; i < numSamples; i++) { // Bias term
		a[0][i] = 1.0;
	}
	
	cout << "numSamples: " << numSamples << endl;
	cout << "numFeatures: " << numFeatures << endl;
}

void scd::load_raw_data(char* pathToFile, uint32_t _numSamples, uint32_t _numFeatures) {
	cout << "Reading " << pathToFile << endl;

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

	FILE* f = fopen(pathToFile, "r");
	if (f == NULL) {
		cout << "Can't find files at pathToFile" << endl;
		exit(1);
	}

	double* temp;
	temp = (double*)malloc(numSamples*(numFeatures+1)*sizeof(double));
	size_t readsize = fread(temp, sizeof(double), numSamples*(numFeatures+1), f);
	for (uint32_t i = 0; i < numSamples; i++) {
		b[i] = (float)temp[i*(numFeatures+1)];
		for (uint32_t j = 0; j < numFeatures; j++) {
			a[j][i] = (float)temp[i*(numFeatures+1) + j + 1];
		}
	}
	free(temp);
	fclose(f);

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

float scd::compress_a(uint32_t minibatchSize, uint32_t toIntegerScaler) {
	uint32_t numMinibatches = numSamples/minibatchSize;
	cout << "numMinibatches: " << numMinibatches << endl;
	uint32_t rest = numSamples - numMinibatches*minibatchSize;
	cout << "rest: " << rest << endl;

	compressed_a = (uint32_t**)malloc(numFeatures*sizeof(uint32_t*));
	compressed_a_sizes = (uint32_t**)malloc(numFeatures*sizeof(uint32_t*));
	for (uint32_t j = 0; j < numFeatures; j++) {
		compressed_a[j] = (uint32_t*)aligned_alloc(64, numSamples*sizeof(uint32_t));
		compressed_a_sizes[j] = (uint32_t*)aligned_alloc(64, numMinibatches*sizeof(uint32_t));
	}

	for (uint32_t m = 0; m < numMinibatches; m++) {
		for (uint32_t j = 0; j < numFeatures; j++) {
			uint32_t compressed_a_offset = 0;
			if (m > 0)
				compressed_a_offset = compressed_a_sizes[j][m-1];
			uint32_t numWordsInBatch = compress_column(a[j] + m*minibatchSize, minibatchSize, compressed_a[j] + compressed_a_offset, toIntegerScaler);
			if (numWordsInBatch%4 > 0)
				numWordsInBatch += (4 - numWordsInBatch%4);
			compressed_a_sizes[j][m] = compressed_a_offset + numWordsInBatch;
			// cout << "compressed_a_sizes[" << j << "][" << m << "]: " << numWordsInBatch << endl;
		}
	}

	uint32_t numWordsAfterCompression = 0;
	for (uint32_t j = 0; j < numFeatures; j++) {
		numWordsAfterCompression += compressed_a_sizes[j][numMinibatches-1];
	}

	float compressionRate = (float)(numMinibatches*minibatchSize*numFeatures)/(float)numWordsAfterCompression;

	return compressionRate;
}

void scd::encrypt_a(uint32_t minibatchSize, char useCompressed) {
	uint32_t numMinibatches = numSamples/minibatchSize;
	cout << "numMinibatches: " << numMinibatches << endl;
	uint32_t rest = numSamples - numMinibatches*minibatchSize;
	cout << "rest: " << rest << endl;

	encrypted_a = (uint32_t**)malloc(numFeatures*sizeof(uint32_t*));
	for (uint32_t j = 0; j < numFeatures; j++) {
		encrypted_a[j] = (uint32_t*)aligned_alloc(64, numSamples*sizeof(uint32_t));
	}

	if (useCompressed == 1) {
		for (uint32_t j = 0; j < numFeatures; j++) {
			for (uint32_t m = 0; m < numMinibatches; m++) {
				int32_t compressed_a_offset = 0;
				if (m > 0)
					compressed_a_offset = compressed_a_sizes[j][m-1];
				encrypt_column((float*)(compressed_a[j] + compressed_a_offset), compressed_a_sizes[j][m] - compressed_a_offset, encrypted_a[j] + compressed_a_offset);
			}
		}
	}
	else {
		for (uint32_t j = 0; j < numFeatures; j++) {
			for (uint32_t m = 0; m < numMinibatches; m++) {
				encrypt_column(a[j] + m*minibatchSize, minibatchSize, encrypted_a[j] + m*minibatchSize);
			}
		}
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

		for (uint32_t k = 0; k < numMinibatches; k++) {
			uint32_t m = numMinibatches*((float)(rand()-1)/(float)RAND_MAX);
			for (uint32_t i = 0; i < minibatchSize; i++) {
				float dot = 0;
				for (uint32_t j = 0; j < numFeatures; j++) {
					dot += x[j]*a[j][m*minibatchSize + i];
				}
				for (uint32_t j = 0; j < numFeatures; j++) {
					gradient[j] += (dot - b[m*minibatchSize + i])*a[j][m*minibatchSize + i];
				}
			}
			for (uint32_t j = 0; j < numFeatures; j++) {
				x[j] -= stepSize*(gradient[j]/minibatchSize);
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

void scd::float_linreg_SCD(float* x_history, uint32_t numEpochs, uint32_t minibatchSize, uint32_t numMinibatchesAtATime, uint32_t residualUpdatePeriod, float stepSize, char useEncrypted, char useCompressed, uint32_t toIntegerScaler) {
	float* residual = (float*)aligned_alloc(64, numSamples*sizeof(float));
	memset(residual, 0, numSamples*sizeof(float));

	cout << "---------------------------------------" << endl;
	uint32_t numMinibatches = numSamples/minibatchSize;
	cout << "numMinibatches: " << numMinibatches << endl;
	uint32_t rest = numSamples - numMinibatches*minibatchSize;
	cout << "rest: " << rest << endl;

	float* x = (float*)aligned_alloc(64, (numMinibatches + numSamples%minibatchSize)*numFeatures*sizeof(float));
	memset(x, 0, (numMinibatches + numSamples%minibatchSize)*numFeatures*sizeof(float));
	float* x_end = (float*)aligned_alloc(64, numFeatures*sizeof(float));
	memset(x_end, 0, numFeatures*sizeof(float));
	
	cout << "Initial loss: " << calculate_loss(x_end) << endl;

	float* transformedColumn1 = NULL;
	float* transformedColumn2 = NULL;
	if (useEncrypted == 1 && useCompressed == 1) {
		transformedColumn1 = (float*)aligned_alloc(64, numMinibatchesAtATime*minibatchSize*sizeof(float));
		transformedColumn2 = (float*)aligned_alloc(64, numMinibatchesAtATime*minibatchSize*sizeof(float));
	}
	else {
		transformedColumn2 = (float*)aligned_alloc(64, numMinibatchesAtATime*minibatchSize*sizeof(float));
	}

	for(uint32_t epoch = 0; epoch < numEpochs + (numEpochs/residualUpdatePeriod); epoch++) {

		double decryption_time = 0;
		double decompression_time = 0;
		double dot_product_time = 0;
		double residual_update_time = 0;
		double temp_time1, temp_time2, temp_time3;
		double start = get_time();

		for (uint32_t k = 0; k < numMinibatches/numMinibatchesAtATime; k++) {

			if (numMinibatchesAtATime > 1) {

				uint32_t m[numMinibatchesAtATime];
				for (uint32_t l = 0; l < numMinibatchesAtATime; l++) {
					m[l] = l*(numMinibatches/numMinibatchesAtATime)+k;
				}

				for (uint32_t j = 0; j < numFeatures; j++) {

					for (uint32_t l = 0; l < numMinibatchesAtATime; l++) {
						if (useEncrypted == 1 && useCompressed == 1) {
							int32_t compressed_a_offset = 0;
							if (m[l] > 0)
								compressed_a_offset = compressed_a_sizes[j][m[l]-1];
							
							temp_time1 = get_time();
							decrypt_column(encrypted_a[j] + compressed_a_offset, compressed_a_sizes[j][m[l]] - compressed_a_offset, transformedColumn1 + l*minibatchSize);
							temp_time2 = get_time();
							decryption_time += (temp_time2-temp_time1);
							decompress_column((uint32_t*)transformedColumn1 + l*minibatchSize, compressed_a_sizes[j][m[l]] - compressed_a_offset, transformedColumn2 + l*minibatchSize, toIntegerScaler);
							temp_time3 = get_time();
							decompression_time += (temp_time3-temp_time2);
						}
						else if (useEncrypted == 1) {
							temp_time1 = get_time();
							decrypt_column(encrypted_a[j] + m[l]*minibatchSize, minibatchSize, transformedColumn2 + l*minibatchSize);
							temp_time2 = get_time();
							decryption_time += (temp_time2-temp_time1);
						}
						else if (useCompressed == 1) {
							temp_time1 = get_time();
							int32_t compressed_a_offset = 0;
							if (m[l] > 0)
								compressed_a_offset = compressed_a_sizes[j][m[l]-1];

							decompress_column(compressed_a[j] + compressed_a_offset, compressed_a_sizes[j][m[l]] - compressed_a_offset, transformedColumn2 + l*minibatchSize, toIntegerScaler);
							temp_time2 = get_time();
							decompression_time += (temp_time2-temp_time1);
						}
						else {
							for (uint32_t i = 0; i < minibatchSize; i++) {
								transformedColumn2[l*minibatchSize + i] = a[j][m[l]*minibatchSize + i];
							}
						}
					}

					if ( (epoch+1)%(residualUpdatePeriod+1) == 0 ) {
						for (uint32_t l = 0; l < numMinibatchesAtATime; l++) {
							for (uint32_t i = 0; i < minibatchSize; i++) {
								if (j == 0)
									residual[m[l]*minibatchSize + i] = x_end[j]*transformedColumn2[l*minibatchSize + i];
								else
									residual[m[l]*minibatchSize + i] += x_end[j]*transformedColumn2[l*minibatchSize + i];
							}
						}
					}
					else {
						temp_time1 = get_time();
						float gradient = 0;
						for (uint32_t l = 0; l < numMinibatchesAtATime; l++) {
							for (uint32_t i = 0; i < minibatchSize; i++) {
								gradient += (residual[m[l]*minibatchSize + i] - b[m[l]*minibatchSize + i])*transformedColumn2[l*minibatchSize + i];
							}
						}
						temp_time2 = get_time();
						dot_product_time += (temp_time2-temp_time1);

						float step = stepSize*(gradient/minibatchSize);
						x[k*numFeatures + j] -= step;

						for (uint32_t l = 0; l < numMinibatchesAtATime; l++) {
							for (uint32_t i = 0; i < minibatchSize; i++) {
								residual[m[l]*minibatchSize + i] -= step*transformedColumn2[l*minibatchSize + i];
							}
						}
						temp_time3 = get_time();
						residual_update_time += (temp_time3-temp_time2);
					}
				}
			}
			else {
				uint32_t m = k;

				for (uint32_t j = 0; j < numFeatures; j++) {

					if (useEncrypted == 1 && useCompressed == 1) {
						int32_t compressed_a_offset = 0;
						if (m > 0)
							compressed_a_offset = compressed_a_sizes[j][m-1];

						temp_time1 = get_time();
						decrypt_column(encrypted_a[j] + compressed_a_offset, compressed_a_sizes[j][m] - compressed_a_offset, transformedColumn1);
						temp_time2 = get_time();
						decryption_time += (temp_time2-temp_time1);
						decompress_column((uint32_t*)transformedColumn1, compressed_a_sizes[j][m] - compressed_a_offset, transformedColumn2, toIntegerScaler);
						temp_time3 = get_time();
						decompression_time += (temp_time3-temp_time2);
					}
					else if (useEncrypted == 1) {
						temp_time1 = get_time();
						decrypt_column(encrypted_a[j] + m*minibatchSize, minibatchSize, transformedColumn2);
						temp_time2 = get_time();
						decryption_time += (temp_time2-temp_time1);
					}
					else if (useCompressed == 1) {
						temp_time1 = get_time();
						int32_t compressed_a_offset = 0;
						if (m > 0)
							compressed_a_offset = compressed_a_sizes[j][m-1];
						decompress_column(compressed_a[j] + compressed_a_offset, compressed_a_sizes[j][m] - compressed_a_offset, transformedColumn2, toIntegerScaler);
						temp_time2 = get_time();
						decompression_time += (temp_time2-temp_time1);
					}
					else {
						transformedColumn2 = a[j] + m*minibatchSize;
					}

					if ( (epoch+1)%(residualUpdatePeriod+1) == 0 ) {
						for (uint32_t i = 0; i < minibatchSize; i++) {
							if (j == 0)
								residual[m*minibatchSize + i] = x_end[j]*transformedColumn2[i];
							else
								residual[m*minibatchSize + i] += x_end[j]*transformedColumn2[i];
						}
						// cout << "residual[0]:" << residual[0] << endl;
					}
					else {
						temp_time1 = get_time();
						float gradient = 0;
						for (uint32_t i = 0; i < minibatchSize; i++) {
							gradient += (residual[m*minibatchSize + i] - b[m*minibatchSize + i])*transformedColumn2[i];
						}
						temp_time2 = get_time();
						dot_product_time += (temp_time2-temp_time1);

						// cout << "gradient: " << gradient << endl;
						float step = stepSize*(gradient/minibatchSize);
						x[m*numFeatures + j] -= step;
						// cout << "step : " << step << endl;

						for (uint32_t i = 0; i < minibatchSize; i++) {
							residual[m*minibatchSize + i] -= step*transformedColumn2[i];
						}
						temp_time3 = get_time();
						residual_update_time += (temp_time3-temp_time2);
					}
				}
			}
		}

		if ( (epoch+1)%(residualUpdatePeriod+1) == 0 ) {
			cout << "--> PERFORMED RESIDUAL UPDATE !!!" << endl;
		}
		else {
			temp_time1 = get_time();
			for (uint32_t j = 0; j < numFeatures; j++) {
				x_end[j] = 0;
			}
			for (uint32_t k = 0; k < numMinibatches/numMinibatchesAtATime; k++) {
				for (uint32_t j = 0; j < numFeatures; j++) {
					x_end[j] += x[k*numFeatures + j];
				}
			}
			for (uint32_t j = 0; j < numFeatures; j++) {
				x_end[j] = x_end[j]/(numMinibatches/numMinibatchesAtATime);
			}

			double end = get_time();
			cout << "decryption_time: " << decryption_time << endl;
			cout << "decompression_time: " << decompression_time << endl;
			cout << "dot_product_time: " << dot_product_time << endl;
			cout << "residual_update_time: " << residual_update_time << endl;
			cout << "x_average_time: " << end-temp_time1 << endl;
			cout << "Time for one epoch: " << end-start << endl;

			if (x_history != NULL) {
				for (uint32_t j = 0; j < numFeatures; j++) {
					x_history[epoch*numFeatures + j] = x_end[j];
				}
			}
			else {
				cout << "Loss " << epoch << ": " << calculate_loss(x_end) << endl;
				cout << calculate_accuracy(x_end) << " corrects out of " << numSamples << endl;
			}

			cout << epoch << endl;
		}
	}

	if (useEncrypted == 1 && useCompressed == 1) {
		free(transformedColumn1);
		free(transformedColumn2);
	}
	else if (useEncrypted == 1 || useCompressed == 1) {
		free(transformedColumn2);
	}
	free(x);
	free(x_end);
	free(residual);
}

#ifdef AVX2
void scd::AVX_float_linreg_SCD(float* x_history, uint32_t numEpochs, uint32_t minibatchSize, float stepSize, char useEncrypted, char useCompressed, uint32_t toIntegerScaler) {
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
	float* x_end = (float*)aligned_alloc(64, numFeatures*sizeof(float));
	memset(x_end, 0, numFeatures*sizeof(float));

	cout << "---------------------------------------" << endl;
	cout << "Initial loss: " << calculate_loss(x_end) << endl;

	float* transformedColumn1 = NULL;
	float* transformedColumn2 = NULL;
	if (useEncrypted == 1 && useCompressed == 1) {
		transformedColumn1 = (float*)aligned_alloc(64, minibatchSize*sizeof(float));
		transformedColumn2 = (float*)aligned_alloc(64, minibatchSize*sizeof(float));
	}
	else if (useEncrypted == 1 || useCompressed == 1) {
		transformedColumn2 = (float*)aligned_alloc(64, minibatchSize*sizeof(float));
	}

	for(uint32_t epoch = 0; epoch < numEpochs; epoch++) {

		double decryption_time = 0;
		double decompression_time = 0;
		double dot_product_time = 0;
		double residual_update_time = 0;
		double temp_time1, temp_time2, temp_time3;
		double start = get_time();

		__m256 residual_temp;
		__m256 a_temp;
		__m256 b_temp;

		for (uint32_t m = 0; m < numMinibatches; m++) {
			for (uint32_t j = 0; j < numFeatures; j++) {

				if (useEncrypted == 1 && useCompressed == 1) {
					int32_t compressed_a_offset = 0;
					if (m > 0)
						compressed_a_offset = compressed_a_sizes[j][m-1];

					temp_time1 = get_time();
					decrypt_column(encrypted_a[j] + compressed_a_offset, compressed_a_sizes[j][m] - compressed_a_offset, transformedColumn1);
					temp_time2 = get_time();
					decryption_time += (temp_time2-temp_time1);
					decompress_column((uint32_t*)transformedColumn1, compressed_a_sizes[j][m] - compressed_a_offset, transformedColumn2, toIntegerScaler);
					temp_time3 = get_time();
					decompression_time += (temp_time3-temp_time2);
				}
				else if (useEncrypted == 1) {
					temp_time1 = get_time();
					decrypt_column(encrypted_a[j] + m*minibatchSize, minibatchSize, transformedColumn2);
					temp_time2 = get_time();
					decryption_time += (temp_time2-temp_time1);
				}
				else if (useCompressed == 1) {
					temp_time1 = get_time();
					int32_t compressed_a_offset = 0;
					if (m > 0)
						compressed_a_offset = compressed_a_sizes[j][m-1];
					decompress_column(compressed_a[j] + compressed_a_offset, compressed_a_sizes[j][m] - compressed_a_offset, transformedColumn2, toIntegerScaler);
					temp_time2 = get_time();
					decompression_time += (temp_time2-temp_time1);
				}
				else {
					transformedColumn2 = a[j] + m*minibatchSize;
				}

				temp_time1 = get_time();
				__m256 gradient = _mm256_setzero_ps();
				for (uint32_t i = 0; i < minibatchSize; i+=8) {
					a_temp = _mm256_load_ps(transformedColumn2 + i);
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

				temp_time2 = get_time();
				dot_product_time += (temp_time2-temp_time1);

				for (uint32_t i = 0; i < minibatchSize; i+=8) {
					a_temp = _mm256_load_ps(transformedColumn2 + i);
					residual_temp = _mm256_load_ps(residual + m*minibatchSize + i);
					residual_temp = _mm256_fmadd_ps(a_temp, stepReduce_temp, residual_temp);

					_mm256_store_ps(residual + m*minibatchSize + i, residual_temp);
				}

				temp_time3 = get_time();
				residual_update_time += (temp_time3-temp_time2);
			}
		}

		temp_time1 = get_time();
		for (uint32_t j = 0; j < numFeatures; j++) {
			x_end[j] = 0;
		}
		for (uint32_t m = 0; m < numMinibatches; m++) {
			for (uint32_t j = 0; j < numFeatures; j++) {
				x_end[j] += x[m*numFeatures + j];
			}
		}
		for (uint32_t j = 0; j < numFeatures; j++) {
			x_end[j] = x_end[j]/numMinibatches;
		}

		double end = get_time();
		cout << "decryption_time: " << decryption_time << endl;
		cout << "decompression_time: " << decompression_time << endl;
		cout << "dot_product_time: " << dot_product_time << endl;
		cout << "residual_update_time: " << residual_update_time << endl;
		cout << "x_average_time: " << end-temp_time1 << endl;
		cout << "Time for one epoch: " << end-start << endl;

		if (x_history != NULL) {
			for (uint32_t j = 0; j < numFeatures; j++) {
				x_history[epoch*numFeatures + j] = x_end[j];
			}
		}
		else
			cout << "Loss " << epoch << ": " << calculate_loss(x_end) << endl;

		cout << epoch << endl;
	}

	if (useEncrypted == 1 && useCompressed == 1) {
		free(transformedColumn1);
		free(transformedColumn2);
	}
	else if (useEncrypted == 1 || useCompressed == 1) {
		free(transformedColumn2);
	}
	free(x);
	free(x_end);
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
	char useCompressed;
	char useEncrypted;
	uint32_t toIntegerScaler;
	double decryption_time;
	double decompression_time;
	double dot_product_time;
	double residual_update_time;
} batch_thread_data;

void* batch_thread(void* args) {
	batch_thread_data* r = (batch_thread_data*)args;

	float scaledStepSize = -r->stepSize/(float)r->minibatchSize;
	__m256 scaledStepSize_temp = _mm256_set1_ps(scaledStepSize);

	float* transformedColumn1 = NULL;
	float* transformedColumn2 = NULL;
	if (r->useEncrypted == 1 && r->useCompressed == 1) {
		transformedColumn1 = (float*)aligned_alloc(64, r->minibatchSize*sizeof(float));
		transformedColumn2 = (float*)aligned_alloc(64, r->minibatchSize*sizeof(float));
	}
	else if (r->useEncrypted == 1 || r->useCompressed == 1) {
		transformedColumn2 = (float*)aligned_alloc(64, r->minibatchSize*sizeof(float));
	}

	double start, end;

	for(uint32_t epoch = 0; epoch < r->numEpochs; epoch++) {

		r->decryption_time = 0;
		r->decompression_time = 0;
		r->dot_product_time = 0;
		r->residual_update_time = 0;
		
		double temp_time1, temp_time2, temp_time3;
		pthread_barrier_wait(r->barrier);
		double start = get_time();

		__m256 residual_temp;
		__m256 a_temp;
		__m256 b_temp;

		for (uint32_t m = r->startingBatch; m < r->startingBatch + r->numBatchesToProcess; m++) {
			for (uint32_t j = 0; j < r->numFeatures; j++) {

				if (r->useEncrypted == 1 && r->useCompressed == 1) {
					int32_t compressed_a_offset = 0;
					if (m > 0)
						compressed_a_offset = r->app->compressed_a_sizes[j][m-1];

					temp_time1 = get_time();
					r->app->decrypt_column(r->app->encrypted_a[j] + compressed_a_offset, r->app->compressed_a_sizes[j][m] - compressed_a_offset, transformedColumn1);
					temp_time2 = get_time();
					r->decryption_time += (temp_time2-temp_time1);
					r->app->decompress_column((uint32_t*)transformedColumn1, r->app->compressed_a_sizes[j][m] - compressed_a_offset, transformedColumn2, r->toIntegerScaler);
					temp_time3 = get_time();
					r->decompression_time += (temp_time3-temp_time2);
				}
				else if (r->useEncrypted == 1) {
					temp_time1 = get_time();
					r->app->decrypt_column(r->app->encrypted_a[j] + m*r->minibatchSize, r->minibatchSize, transformedColumn2);
					temp_time2 = get_time();
					r->decryption_time += (temp_time2-temp_time1);
				}
				else if (r->useCompressed == 1) {
					temp_time1 = get_time();
					int32_t compressed_a_offset = 0;
					if (m > 0)
						compressed_a_offset = r->app->compressed_a_sizes[j][m-1];
					r->app->decompress_column(r->app->compressed_a[j] + compressed_a_offset, r->app->compressed_a_sizes[j][m] - compressed_a_offset, transformedColumn2, r->toIntegerScaler);
					temp_time2 = get_time();
					r->decompression_time += (temp_time2-temp_time1);
				}
				else {
					transformedColumn2 = r->a[j] + m*r->minibatchSize;
				}

				temp_time1 = get_time();
				__m256 gradient = _mm256_setzero_ps();
				for (uint32_t i = 0; i < r->minibatchSize; i+=8) {
					a_temp = _mm256_load_ps(transformedColumn2 + i);
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

				temp_time2 = get_time();
				r->dot_product_time += (temp_time2-temp_time1);

				for (uint32_t i = 0; i < r->minibatchSize; i+=8) {
					a_temp = _mm256_load_ps(transformedColumn2 + i);
					residual_temp = _mm256_load_ps(r->residual + m*r->minibatchSize + i);
					residual_temp = _mm256_fmadd_ps(a_temp, stepReduce_temp, residual_temp);

					_mm256_store_ps(r->residual + m*r->minibatchSize + i, residual_temp);
				}

				temp_time3 = get_time();
				r->residual_update_time += (temp_time3-temp_time2);
			}
		}

		pthread_barrier_wait(r->barrier);
		temp_time1 = get_time();

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

	if (r->useEncrypted == 1 && r->useCompressed == 1) {
		free(transformedColumn1);
		free(transformedColumn2);
	}
	else if (r->useEncrypted == 1 || r->useCompressed == 1) {
		free(transformedColumn2);
	}
	return NULL;
}

void scd::AVXmulti_float_linreg_SCD(float* x_history, uint32_t numEpochs, uint32_t minibatchSize, float stepSize, char useEncrypted, char useCompressed, uint32_t toIntegerScaler) {
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

	cout << "---------------------------------------" << endl;
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
		args[n].useCompressed = useCompressed;
		args[n].useEncrypted = useEncrypted;
		args[n].toIntegerScaler = toIntegerScaler;

		pthread_create(&threads[n], &attr, batch_thread, (void*)&args[n]);
		startingBatch += args[n].numBatchesToProcess;
	}
	for (uint32_t n = 0; n < NUM_THREADS; n++) {
		pthread_join(threads[n], NULL);
	}

	double decryption_time = 0;
	double decompression_time = 0;
	double dot_product_time = 0;
	double residual_update_time = 0;
	for (uint32_t n = 0; n < NUM_THREADS; n++) {
		decryption_time += args[n].decryption_time;
		decompression_time += args[n].decompression_time;
		dot_product_time += args[n].dot_product_time;
		residual_update_time += args[n].residual_update_time;
	}
	cout << "decryption_time: " << decryption_time/NUM_THREADS << endl;
	cout << "decompression_time: " << decompression_time/NUM_THREADS << endl;
	cout << "dot_product_time: " << dot_product_time/NUM_THREADS << endl;
	cout << "residual_update_time: " << residual_update_time/NUM_THREADS << endl;

	free(x);
	free(residual);
}


void* justread_thread(void* args) {
	batch_thread_data* r = (batch_thread_data*)args;

	double start = get_time();

	float* transformedColumn1 = NULL;
	float* transformedColumn2 = NULL;
	if (r->useEncrypted == 1 && r->useCompressed == 1) {
		transformedColumn1 = (float*)aligned_alloc(64, r->minibatchSize*sizeof(float));
		transformedColumn2 = (float*)aligned_alloc(64, r->minibatchSize*sizeof(float));
	}
	else if (r->useEncrypted == 1 || r->useCompressed == 1) {
		transformedColumn2 = (float*)aligned_alloc(64, r->minibatchSize*sizeof(float));
	}

	double temp_time1, temp_time2, temp_time3;
	r->decryption_time = 0;
	r->decompression_time = 0;

	__m256 sum = _mm256_setzero_ps();
	for (uint32_t m = r->startingBatch; m < r->startingBatch + r->numBatchesToProcess; m++) {
		for (uint32_t j = 0; j < r->app->numFeatures; j++) {

			if (r->useEncrypted == 1 && r->useCompressed == 1) {
				int32_t compressed_a_offset = 0;
				if (m > 0)
					compressed_a_offset = r->app->compressed_a_sizes[j][m-1];
				
				temp_time1 = get_time();
				r->app->decrypt_column(r->app->encrypted_a[j] + compressed_a_offset, r->app->compressed_a_sizes[j][m] - compressed_a_offset, transformedColumn1);
				temp_time2 = get_time();
				r->decryption_time += (temp_time2-temp_time1);
				r->app->decompress_column((uint32_t*)transformedColumn1, r->app->compressed_a_sizes[j][m] - compressed_a_offset, transformedColumn2, r->toIntegerScaler);
				temp_time3 = get_time();
				r->decompression_time += (temp_time3-temp_time2);
			}
			else if (r->useEncrypted == 1) {
				temp_time1 = get_time();
				r->app->decrypt_column(r->app->encrypted_a[j] + m*r->minibatchSize, r->minibatchSize, transformedColumn2);
				temp_time2 = get_time();
				r->decryption_time += (temp_time2-temp_time1);
			}
			else if (r->useCompressed == 1) {
				temp_time1 = get_time();
				int32_t compressed_a_offset = 0;
				if (m > 0)
					compressed_a_offset = r->app->compressed_a_sizes[j][m-1];
				r->app->decompress_column(r->app->compressed_a[j] + compressed_a_offset, r->app->compressed_a_sizes[j][m] - compressed_a_offset, transformedColumn2, r->toIntegerScaler);
				temp_time2 = get_time();
				r->decompression_time += (temp_time2-temp_time1);
			}
			else {
				transformedColumn2 = r->a[j] + m*r->minibatchSize;
				for (uint32_t i = 0; i < r->minibatchSize; i+=8) {
					__m256 a_temp = _mm256_load_ps(transformedColumn2 + i);
					sum = _mm256_add_ps(a_temp, sum);
					_mm256_store_ps(transformedColumn2+i, sum);
				}
			}
		}
	}
	
	float temp[8];
	_mm256_store_ps(temp, sum);

	cout << temp[0] << endl;

	if (r->useEncrypted == 1 && r->useCompressed == 1) {
		free(transformedColumn1);
		free(transformedColumn2);
	}
	else if (r->useEncrypted == 1 || r->useCompressed == 1) {
		free(transformedColumn2);
	}

	// cout << "Time per thread: " << get_time()-start << endl;

	return NULL;
}

double scd::AVXmulti_justread(uint32_t minibatchSize, char useEncrypted, char useCompressed) {
	pthread_attr_t attr;
	pthread_t threads[NUM_THREADS];
	batch_thread_data args[NUM_THREADS];
	cpu_set_t set;

	uint32_t numMinibatches = numSamples/minibatchSize;
	cout << "numMinibatches: " << numMinibatches << endl;
	uint32_t rest = numSamples - numMinibatches*minibatchSize;
	cout << "rest: " << rest << endl;

	uint32_t startingBatch = 0;
	pthread_attr_init(&attr);
	CPU_ZERO(&set);
	for (uint32_t n = 0; n < NUM_THREADS; n++) {
		CPU_SET(n, &set);
		pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &set);

		args[n].tid = n;
		args[n].a = a;
		args[n].app = this;
		args[n].startingBatch = startingBatch;
		if (n == NUM_THREADS-1)
			args[n].numBatchesToProcess = numMinibatches - startingBatch;
		else
			args[n].numBatchesToProcess = numMinibatches/NUM_THREADS + (numMinibatches%NUM_THREADS > 0);
		args[n].minibatchSize = minibatchSize;
		args[n].useEncrypted = useEncrypted;
		args[n].useCompressed = useCompressed;

		cout << "args[n].numBatchesToProcess: " << args[n].numBatchesToProcess << endl;
		startingBatch += args[n].numBatchesToProcess;
	}
	double start = get_time();
	for (uint32_t n = 0; n < NUM_THREADS; n++) {
		pthread_create(&threads[n], &attr, justread_thread, (void*)&args[n]);
	}
	for (uint32_t n = 0; n < NUM_THREADS; n++) {
		pthread_join(threads[n], NULL);
	}
	double total = get_time() - start;
	cout << "total time: " << total << endl;
	double rate = (numMinibatches*minibatchSize*numFeatures*4)/total;
	cout << "just read rate: " << rate << endl;

	double decryption_time = 0;
	double decompression_time = 0;
	double dot_product_time = 0;
	double residual_update_time = 0;
	for (uint32_t n = 0; n < NUM_THREADS; n++) {
		decryption_time += args[n].decryption_time;
		decompression_time += args[n].decompression_time;
		dot_product_time += args[n].dot_product_time;
		residual_update_time += args[n].residual_update_time;
	}
	cout << "decryption_time: " << decryption_time/NUM_THREADS << endl;
	cout << "decompression_time: " << decompression_time/NUM_THREADS << endl;

	return total;
}
#endif

uint32_t scd::print_FPGA_memory() {

	uint32_t address32 = 0;
	
	for (uint32_t i = 0; i < residual_address[0]*numValuesPerLine; i++) {
		cout << i << ": " << interfaceFPGA->readFromMemoryFloat('i', address32++) << endl;
	}

	exit(0);
}

uint32_t scd::copy_data_into_FPGA_memory(uint32_t numMinibatches, uint32_t minibatchSize, uint32_t numMinibatchesToAssign[], uint32_t numEpochs, char useEncrypted) {
	uint32_t address32 = NUM_FINSTANCES*pages_to_allocate*numValuesPerLine;
	uint32_t numMinibatchesAssigned = 0;
	
	// Space for offsets
	for (uint32_t n = 0; n < NUM_FINSTANCES; n++) {
		a_address[n] = address32/numValuesPerLine;
		for (uint32_t i = 0; i < (numFeatures/numValuesPerLine + (numFeatures%numValuesPerLine > 0))*numValuesPerLine; i++) {
			interfaceFPGA->writeToMemoryFloat('i', 0, address32++);
		}
	}

	// Write b
	numMinibatchesAssigned = 0;
	for (uint32_t n = 0; n < NUM_FINSTANCES; n++) {
		b_address[n] = address32/numValuesPerLine;

		for (uint32_t i = 0; i < numMinibatchesToAssign[n]*minibatchSize; i++) {
			interfaceFPGA->writeToMemoryFloat('i', b[numMinibatchesAssigned*minibatchSize + i], address32++);
		}
		if (address32%numValuesPerLine > 0) {
			uint32_t padding = (numValuesPerLine - address32%numValuesPerLine);
			for (uint32_t i = 0; i < padding; i++) {
				interfaceFPGA->writeToMemory32('i', 0, address32++);
			}
		}

		numMinibatchesAssigned += numMinibatchesToAssign[n];
	}
	
	// Write a
	for (uint32_t j = 0; j < numFeatures; j++) {
		
		numMinibatchesAssigned = 0;
		for (uint32_t n = 0; n < NUM_FINSTANCES; n++) {
			interfaceFPGA->writeToMemory32('i', address32/numValuesPerLine, a_address[n]*numValuesPerLine + j);

			for (uint32_t i = 0; i < numMinibatchesToAssign[n]*minibatchSize; i++) {
				if (useEncrypted == 1)
					interfaceFPGA->writeToMemory32('i', encrypted_a[j][numMinibatchesAssigned*minibatchSize + i], address32++);
				else
					interfaceFPGA->writeToMemoryFloat('i', a[j][numMinibatchesAssigned*minibatchSize + i], address32++);
			}
			if (address32%numValuesPerLine > 0) {
				uint32_t padding = (numValuesPerLine - address32%numValuesPerLine);
				for (uint32_t i = 0; i < padding; i++) {
					interfaceFPGA->writeToMemoryFloat('i', 0, address32++);
				}
			}

			numMinibatchesAssigned += numMinibatchesToAssign[n];
		}		
	}

	// Residual addresses
	for (uint32_t n = 0; n < NUM_FINSTANCES; n++) {
		residual_address[n] = address32/numValuesPerLine;
		for (uint32_t i = 0; i < numMinibatchesToAssign[n]*minibatchSize; i++) {
			interfaceFPGA->writeToMemoryFloat('i', 0, address32++);
		}
	}

	// Step addresses
	for (uint32_t n = 0; n < NUM_FINSTANCES; n++) {
		step_address[n] = address32/numValuesPerLine;
		address32 += numEpochs*numMinibatchesToAssign[n]*(numFeatures/numValuesPerLine + (numFeatures%numValuesPerLine > 0))*numValuesPerLine;
	}

	return step_address[0]*numValuesPerLine;
}

uint32_t scd::copy_compressed_data_into_FPGA_memory(uint32_t numMinibatches, uint32_t minibatchSize, uint32_t numMinibatchesToAssign[], uint32_t numEpochs, char useEncrypted) {
	uint32_t address32 = NUM_FINSTANCES*pages_to_allocate*numValuesPerLine;
	uint32_t numMinibatchesAssigned = 0;

	// Space for offsets
	for (uint32_t n = 0; n < NUM_FINSTANCES; n++) {
		a_address[n] = address32/numValuesPerLine;
		for (uint32_t i = 0; i < (numFeatures/numValuesPerLine + (numFeatures%numValuesPerLine > 0))*numValuesPerLine; i++) {
			interfaceFPGA->writeToMemoryFloat('i', 0, address32++);
		}
	}
	// cout << address32 << endl;

	// Write b
	numMinibatchesAssigned = 0;
	for (uint32_t n = 0; n < NUM_FINSTANCES; n++) {
		b_address[n] = address32/numValuesPerLine;

		for (uint32_t i = 0; i < numMinibatchesToAssign[n]*minibatchSize; i++) {
			interfaceFPGA->writeToMemoryFloat('i', b[numMinibatchesAssigned*minibatchSize + i], address32++);
		}
		if (address32%numValuesPerLine > 0) {
			uint32_t padding = (numValuesPerLine - address32%numValuesPerLine);
			for (uint32_t i = 0; i < padding; i++) {
				interfaceFPGA->writeToMemory32('i', 0, address32++);
			}
		}

		numMinibatchesAssigned += numMinibatchesToAssign[n];
	}
	// cout << address32 << endl;

	// Write a
	for (uint32_t j = 0; j < numFeatures; j++) {
		
		numMinibatchesAssigned = 0;
		for (uint32_t n = 0; n < NUM_FINSTANCES; n++) {
			interfaceFPGA->writeToMemory32('i', address32/numValuesPerLine, a_address[n]*numValuesPerLine + j);

			for (uint32_t m = numMinibatchesAssigned; m < numMinibatchesAssigned+numMinibatchesToAssign[n]; m++) {
				uint32_t compressed_a_offset = 0;
				if (m > 0)
					compressed_a_offset = compressed_a_sizes[j][m-1];

				uint32_t numWordsInBatch = compressed_a_sizes[j][m] - compressed_a_offset;

				// Size for the current compressed mini batch, +1 for the first line which contains how many further lines to read
				// cout << address32 << endl;
				interfaceFPGA->writeToMemory32('i', numWordsInBatch/numValuesPerLine + (numWordsInBatch%numValuesPerLine > 0) + 1, address32++);
				if (address32%numValuesPerLine > 0) {
					uint32_t padding = (numValuesPerLine - address32%numValuesPerLine);
					for (uint32_t i = 0; i < padding; i++) {
						interfaceFPGA->writeToMemory32('i', 0, address32++);
					}
				}
				// cout << address32 << endl;

				for (uint32_t i = 0; i < numWordsInBatch; i++) {
					if (useEncrypted == 1)
						interfaceFPGA->writeToMemory32('i', encrypted_a[j][compressed_a_offset + i], address32++);
					else
						interfaceFPGA->writeToMemory32('i', compressed_a[j][compressed_a_offset + i], address32++);
				}
				if (address32%numValuesPerLine > 0) {
					uint32_t padding = (numValuesPerLine - address32%numValuesPerLine);
					for (uint32_t i = 0; i < padding; i++) {
						interfaceFPGA->writeToMemory32('i', 0, address32++);
					}
				}
				// cout << address32 << endl;
			}
			numMinibatchesAssigned += numMinibatchesToAssign[n];
		}		
	}

	// Residual addresses
	for (uint32_t n = 0; n < NUM_FINSTANCES; n++) {
		residual_address[n] = address32/numValuesPerLine;
		for (uint32_t i = 0; i < numMinibatchesToAssign[n]*minibatchSize; i++) {
			interfaceFPGA->writeToMemoryFloat('i', 0, address32++);
		}
	}

	// Step addresses
	for (uint32_t n = 0; n < NUM_FINSTANCES; n++) {
		step_address[n] = address32/numValuesPerLine;
		address32 += numEpochs*numMinibatchesToAssign[n]*(numFeatures/numValuesPerLine + (numFeatures%numValuesPerLine > 0))*numValuesPerLine;
	}

	return step_address[0]*numValuesPerLine;
}

void scd::float_linreg_FSCD(float* x_history, uint32_t numEpochs, uint32_t minibatchSize, uint32_t residualUpdatePeriod, float stepSize, char enableStaleness, char useEncrypted, char useCompressed, uint32_t toIntegerScaler, uint32_t numInstancesToUse) {
	uint32_t numMinibatches = numSamples/minibatchSize;
	cout << "numMinibatches: " << numMinibatches << endl;
	uint32_t rest = numSamples - numMinibatches*minibatchSize;
	cout << "rest: " << rest << endl;

	uint32_t enableMultiline = 1;

	uint32_t numMinibatchesAssigned = 0;
	uint32_t numMinibatchesToAssign[NUM_FINSTANCES];
	for (uint32_t n = 0; n < NUM_FINSTANCES; n++) {
		if (numMinibatchesAssigned < numMinibatches) {
			uint32_t temp = numMinibatches/numInstancesToUse + (numMinibatches%numInstancesToUse > 0);
			if (temp > numMinibatches - numMinibatchesAssigned)
				numMinibatchesToAssign[n] = numMinibatches - numMinibatchesAssigned;
			else
				numMinibatchesToAssign[n] = temp;
		}
		else
			numMinibatchesToAssign[n] = 0;
		cout << "numMinibatchesToAssign[" << n << "]: " << numMinibatchesToAssign[n] << endl; 
		numMinibatchesAssigned += numMinibatchesToAssign[n];
	}

	uint32_t address32 = 0;
	if (useCompressed == 0)
		address32 = copy_data_into_FPGA_memory(numMinibatches, minibatchSize, numMinibatchesToAssign, numEpochs, useEncrypted);
	else
		address32 = copy_compressed_data_into_FPGA_memory(numMinibatches, minibatchSize, numMinibatchesToAssign, numEpochs, useEncrypted);

	float* x_history_local = (float*)aligned_alloc(64, numEpochs*numFeatures*sizeof(float));
	memset(x_history_local, 0, numEpochs*numFeatures*sizeof(float));
	float* x = (float*)aligned_alloc(64, numFeatures*sizeof(float));
	memset(x, 0, numFeatures*sizeof(float));
	
	cout << "Initial loss: " << calculate_loss(x) << endl;

	float tempStepSize = stepSize/(float)minibatchSize;
	uint32_t* tempStepSizeAddr = (uint32_t*) &tempStepSize;

	uint64_t config[NUM_FINSTANCES][5];
	for (uint32_t n = 0; n < NUM_FINSTANCES; n++) {
		interfaceFPGA->m_pALIMMIOService->mmioWrite32(CSR_NUM_LINES, n);

		interfaceFPGA->m_pALIMMIOService->mmioWrite32(CSR_READ_OFFSET, 0);
		interfaceFPGA->m_pALIMMIOService->mmioWrite32(CSR_WRITE_OFFSET, 0);

		config[n][0] = 0;
		config[n][0] = ((uint64_t)b_address[n] << 32) | ((uint64_t)a_address[n]);
		
		config[n][1] = 0;
		config[n][1] = ((uint64_t)residual_address[n] << 32) | ((uint64_t)step_address[n]);
		
		config[n][2] = 0;
		config[n][2] = ((uint64_t)(minibatchSize/numValuesPerLine) << 48) | ((uint64_t)numMinibatchesToAssign[n] << 32) | ((uint64_t)numFeatures);
		
		config[n][3] = 0;
		config[n][3] = ((uint64_t)numEpochs << 32) | ((uint64_t)*tempStepSizeAddr);

		config[n][4] = 0;
		config[n][4] = ((uint64_t)useEncrypted << 19) | ((uint64_t)enableMultiline << 18) | ((uint64_t)toIntegerScaler << 2) | ((uint64_t)useCompressed << 1) | (uint64_t)enableStaleness;	
	}
	if (useEncrypted == 1) {
		for (uint32_t i = 0; i < 15; i++) {
			uint64_t config6 = ((uint64_t)i << 20) | config[0][4];
			interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG5, config6);
			interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG6, ((uint64_t*)KEYS_dec)[2*i]);
			interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG7, ((uint64_t*)KEYS_dec)[2*i+1]);
		}
		uint64_t config7 = ((uint64_t)15 << 20) | config[0][4];
		interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG5, config7);
		uint64_t* temp_ptr = (uint64_t*)ivec;
		interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG6, temp_ptr[0]);
		interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG7, temp_ptr[1]);
	}

	uint32_t index[NUM_FINSTANCES];
	
	double start = get_time();

	uint32_t epoch = 0;
	if (numEpochs/residualUpdatePeriod > 0) {
		for (epoch = 0; epoch+residualUpdatePeriod < numEpochs; epoch += residualUpdatePeriod) {
			// Do normal epochs
			for (uint32_t n = 0; n < NUM_FINSTANCES; n++) {
				interfaceFPGA->m_pALIMMIOService->mmioWrite32(CSR_NUM_LINES, n);
				interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG1, config[n][0]);
				interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG2, config[n][1]);
				interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG3, config[n][2]);
				uint64_t temp1 = ((uint64_t)residualUpdatePeriod << 32) | (config[n][3] & 0xFFFFFFFF);
				interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG4, temp1);
				interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG5, config[n][4]);
			}

			interfaceFPGA->startTransaction();

			memset(index, 0, NUM_FINSTANCES*sizeof(uint32_t));
			for (uint32_t e = 0; e < residualUpdatePeriod; e++) {
				for (uint32_t n = 0; n < numInstancesToUse; n++) {
					for (uint32_t m = 0; m < numMinibatchesToAssign[n]; m++) {
						for (uint32_t j = 0; j < numFeatures; j++) {
							float value = 0;
							while (value == 0) {
								value = interfaceFPGA->readFromMemoryFloat('i', step_address[n]*numValuesPerLine+index[n]);
								SleepNano(100);
							}
							interfaceFPGA->writeToMemoryFloat('i', 0, step_address[n]*numValuesPerLine+index[n]);
							x[j] -= value;
							index[n]++;
						}
						if (index[n]%numValuesPerLine > 0)
							index[n] += (numValuesPerLine - index[n]%numValuesPerLine);
					}
				}
				for (uint32_t j = 0; j < numFeatures; j++) {
					x_history_local[(epoch+e)*numFeatures+j] = x[j]/numMinibatches;
				}
			}

			interfaceFPGA->joinTransaction();

			for (uint32_t n = 0; n < numInstancesToUse; n++) {
				for (uint32_t j = 0; j < numFeatures; j++) {
					interfaceFPGA->writeToMemoryFloat('i', x[j]/numMinibatches, step_address[n]*numValuesPerLine+j);
				}
			}

			// Do residual update
			if (epoch + residualUpdatePeriod < numEpochs) {
				for (uint32_t n = 0; n < NUM_FINSTANCES; n++) {
					interfaceFPGA->m_pALIMMIOService->mmioWrite32(CSR_NUM_LINES, n);
					interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG1, config[n][0]);
					interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG2, config[n][1]);
					interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG3, config[n][2]);
					uint64_t temp1 = ((uint64_t)1 << 32) | (config[n][3] & 0xFFFFFFFF);
					interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG4, temp1);
					uint64_t temp2 = ((uint64_t)1 << 24) | config[n][4];
					interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG5, temp2);
				}
				interfaceFPGA->startTransaction();
				interfaceFPGA->joinTransaction();
			}
		}
	}
	cout << "epoch: " << epoch << endl;

	if (epoch < numEpochs) {
		for (uint32_t n = 0; n < NUM_FINSTANCES; n++) {
			interfaceFPGA->m_pALIMMIOService->mmioWrite32(CSR_NUM_LINES, n);
			interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG1, config[n][0]);
			interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG2, config[n][1]);
			interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG3, config[n][2]);
			uint64_t temp = ((uint64_t)(numEpochs - epoch) << 32) | (config[n][3] & 0xFFFFFFFF);
			interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG4, temp);
			interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG5, config[n][4]);
		}
		interfaceFPGA->startTransaction();
		interfaceFPGA->joinTransaction();
	}

	double end = get_time();
	cout << "Time for all epochs on the FPGA: " << end-start << endl;
	cout << "Time for one epoch on the FPGA: " << (end-start)/numEpochs << endl;

	memset(index, 0, NUM_FINSTANCES*sizeof(uint32_t));
	if (epoch < numEpochs) {
		for (uint32_t e = epoch; e < numEpochs; e++) {
			for (uint32_t n = 0; n < numInstancesToUse; n++) {
				for (uint32_t m = 0; m < numMinibatchesToAssign[n]; m++) {
					for (uint32_t j = 0; j < numFeatures; j++) {
						float value = interfaceFPGA->readFromMemoryFloat('i', step_address[n]*numValuesPerLine+index[n]);
						x[j] -= value;
						index[n]++;
					}
					if (index[n]%numValuesPerLine > 0)
						index[n] += (numValuesPerLine - index[n]%numValuesPerLine);
				}
			}
			for (uint32_t j = 0; j < numFeatures; j++) {
				x_history_local[e*numFeatures + j] = x[j]/numMinibatches;
			}
		}
	}

	for(uint32_t e = 0; e < numEpochs; e++) {
		float* x_end = x_history_local + e*numFeatures;

		if (x_history != NULL) {
			for (uint32_t j = 0; j < numFeatures; j++) {
				x_history[e*numFeatures + j] = x_end[j];
			}
		}
		else
			cout << "Loss " << e << ": " << calculate_loss(x_end) << endl;
	}

	free(x_history_local);
}

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

uint32_t scd::calculate_accuracy(float* x) {
	uint32_t corrects = 0;
	for(uint32_t i = 0; i < numSamples; i++) {
		float dot = 0.0;
		for (uint32_t j = 0; j < numFeatures; j++) {
			dot += x[j]*a[j][i];
		}
		if ( (dot >= 0.5 && b[i] == 1) || (dot < 0.5 && b[i] == 0) )
			corrects++;
	}

	return corrects;
}

uint32_t scd::decompress_column(uint32_t* compressedColumn, uint32_t inNumWords, float* decompressedColumn, uint32_t toIntegerScaler) {
	uint32_t outNumWords = 0;
	int delta[31];
	uint32_t CL[8];

	for (uint32_t i = 0; i < inNumWords; i+=8) {
		uint32_t meta = (compressedColumn[i+7] >> 24) & 0xFC;
		int base = (int)compressedColumn[i];
		decompressedColumn[outNumWords++] = ((float)base)/((float)(1 << toIntegerScaler));

		// cout << "meta: " << hex << meta << dec << endl;

		for (uint32_t j = 1; j < 8; j++){
			CL[j] = compressedColumn[i + j];
		}
		__builtin_prefetch(compressedColumn + i + 16);
		__builtin_prefetch(decompressedColumn + outNumWords + 16);

		if ( meta == 0x40 ) {
			delta[0] = (int)(CL[1] & 0x7F);
			delta[1] = (int)((CL[1] >> 7 ) & 0x7F);
			delta[2] = (int)((CL[1] >> 14 ) & 0x7F);
			delta[3] = (int)((CL[1] >> 21 ) & 0x7F);
			delta[4] = (int)(((CL[2] & 0x7) << 4) + (CL[1] >> 28));
			delta[5] = (int)((CL[2] >> 3) & 0x7F);
			delta[6] = (int)((CL[2] >> 10) & 0x7F);
			delta[7] = (int)((CL[2] >> 17) & 0x7F);
			delta[8] = (int)((CL[2] >> 24) & 0x7F);
			delta[9] = (int)(((CL[3] & 0x3F) << 1) + (CL[2] >> 31));
			delta[10] = (int)((CL[3] >> 6) & 0x7F);
			delta[11] = (int)((CL[3] >> 13) & 0x7F);
			delta[12] = (int)((CL[3] >> 20) & 0x7F);
			delta[13] = (int)(((CL[4] & 0x3) << 5) + (CL[3] >> 27));
			delta[14] = (int)((CL[4] >> 2) & 0x7F);
			delta[15] = (int)((CL[4] >> 9) & 0x7F);
			delta[16] = (int)((CL[4] >> 16) & 0x7F);
			delta[17] = (int)((CL[4] >> 23) & 0x7F);
			delta[18] = (int)(((CL[5] & 0x1F) << 2) + (CL[4] >> 30));
			delta[19] = (int)((CL[5] >> 5) & 0x7F);
			delta[20] = (int)((CL[5] >> 12) & 0x7F);
			delta[21] = (int)((CL[5] >> 19) & 0x7F);
			delta[22] = (int)(((CL[6] & 0x1) << 6) + (CL[5] >> 26));
			delta[23] = (int)((CL[6] >> 1) & 0x7F);
			delta[24] = (int)((CL[6] >> 8) & 0x7F);
			delta[25] = (int)((CL[6] >> 15) & 0x7F);
			delta[26] = (int)((CL[6] >> 22) & 0x7F);
			delta[27] = (int)(((CL[7] & 0xF) << 3) + (CL[6] >> 29));
			delta[28] = (int)((CL[7] >> 4) & 0x7F);
			delta[29] = (int)((CL[7] >> 11) & 0x7F);
			delta[30] = (int)((CL[7] >> 18) & 0x7F);

			for (uint32_t k = 0; k < 31; k++) {
				if ((delta[k] >> 6) == 0x1) {
					delta[k] = delta[k] - (1 << 7);
					decompressedColumn[outNumWords++] = ((float)(base+delta[k]))/((float)(1 << toIntegerScaler));
				}
				else
					decompressedColumn[outNumWords++] = ((float)(base+delta[k]))/((float)(1 << toIntegerScaler));
			}
		}
		else if (meta == 0x30) {
			delta[0] = (int)(CL[1] & 0x1FF);
			delta[1] = (int)((CL[1] >> 9) & 0x1FF);
			delta[2] = (int)((CL[1] >> 18) & 0x1FF);
			delta[3] = (int)(((CL[2] & 0xF) << 5) + (CL[1] >> 27));
			delta[4] = (int)((CL[2] >> 4) & 0x1FF);
			delta[5] = (int)((CL[2] >> 13) & 0x1FF);
			delta[6] = (int)((CL[2] >> 22) & 0x1FF);
			delta[7] = (int)(((CL[3] & 0xFF) << 1) + (CL[2] >> 31));
			delta[8] = (int)((CL[3] >> 8) & 0x1FF);
			delta[9] = (int)((CL[3] >> 17) & 0x1FF);
			delta[10] = (int)(((CL[4] & 0x7) << 6) + (CL[3] >> 26));
			delta[11] = (int)((CL[4] >> 3) & 0x1FF);
			delta[12] = (int)((CL[4] >> 12) & 0x1FF);
			delta[13] = (int)((CL[4] >> 21) & 0x1FF);
			delta[14] = (int)(((CL[5] & 0x7F) << 2) + (CL[4] >> 30));
			delta[15] = (int)((CL[5] >> 7) & 0x1FF);
			delta[16] = (int)((CL[5] >> 16) & 0x1FF);
			delta[17] = (int)(((CL[6] & 0x3) << 7) +  (CL[5] >> 25));
			delta[18] = (int)((CL[6] >> 2) & 0x1FF);
			delta[19] = (int)((CL[6] >> 11) & 0x1FF);
			delta[20] = (int)((CL[6] >> 20) & 0x1FF);
			delta[21] = (int)(((CL[7] & 0x3F) << 3) + (CL[6] >> 29));
			delta[22] = (int)((CL[7] >> 6) & 0x1FF);

			for (uint32_t k = 0; k < 23; k++) {
				if ((delta[k] >> 8) == 0x1) {
					delta[k] = delta[k] - (1 << 9);
					decompressedColumn[outNumWords++] = ((float)(base+delta[k]))/((float)(1 << toIntegerScaler));
				}
				else
					decompressedColumn[outNumWords++] = ((float)(base+delta[k]))/((float)(1 << toIntegerScaler));
			}
		}
		else if (meta == 0x20) {
			delta[0] = (int)(CL[1] & 0x3FFF);
			delta[1] = (int)((CL[1] >> 14) & 0x3FFF);
			delta[2] = (int)(((CL[2] & 0x3FF) << 4) + (CL[1] >> 28));
			delta[3] = (int)((CL[2] >> 10) & 0x3FFF);
			delta[4] = (int)(((CL[3] & 0x3F) << 8) + (CL[2] >> 24));
			delta[5] = (int)((CL[3] >> 6) & 0x3FFF);
			delta[6] = (int)(((CL[4] & 0x3) << 12) + (CL[3] >> 20));
			delta[7] = (int)((CL[4] >> 2) & 0x3FFF);
			delta[8] = (int)((CL[4] >> 16) & 0x3FFF);
			delta[9] = (int)(((CL[5] & 0xFFF) << 2) + (CL[4] >> 30));
			delta[10] = (int)((CL[5] >> 12) & 0x3FFF);
			delta[11] = (int)(((CL[6] & 0xFF) << 6) + (CL[5] >> 26));
			delta[12] = (int)((CL[6] >> 8) & 0x3FFF);
			delta[13] = (int)(((CL[7] & 0xF) << 10) + (CL[6] >> 22));
			delta[14] = (int)((CL[7] >> 4) & 0x3FFF);

			for (uint32_t k = 0; k < 15; k++) {
				if ((delta[k] >> 13) == 0x1) {
					delta[k] = delta[k] - (1 << 14);
					decompressedColumn[outNumWords++] = ((float)(base+delta[k]))/((float)(1 << toIntegerScaler));
				}
				else
					decompressedColumn[outNumWords++] = ((float)(base+delta[k]))/((float)(1 << toIntegerScaler));
			}
		}
		else {
			delta[0] = (int)(CL[1] & 0x7FFFFFFF);
			delta[1] = (int)((CL[2] & 0x3FFFFFFF) << 1) + (CL[1] >> 31);
			delta[2] = (int)((CL[3] & 0x1FFFFFFF) << 2) + (CL[2] >> 30);
			delta[3] = (int)((CL[4] & 0xFFFFFFF) << 3) + (CL[3] >> 29);
			delta[4] = (int)((CL[5] & 0x7FFFFFF) << 4) + (CL[4] >> 28);
			delta[5] = (int)((CL[6] & 0x3FFFFFF) << 5) + (CL[5] >> 27);
			delta[6] = (int)((CL[7] & 0x1FFFFFF) << 6) + (CL[6] >> 26);

			uint32_t numProcessed = meta >> 2;
			// cout << "numProcessed: " << numProcessed << endl;

			for (uint32_t k = 0; k < numProcessed; k++) {
				if ((delta[k] >> 30) == 0x1) {
					delta[k] = delta[k] - (1 << 31);
					decompressedColumn[outNumWords++] = ((float)(base+delta[k]))/((float)(1 << toIntegerScaler));
				}
				else
					decompressedColumn[outNumWords++] = ((float)(base+delta[k]))/((float)(1 << toIntegerScaler));
			}
		}
		// cout << "outNumWords: " << outNumWords << endl;
	}
	return outNumWords;
}

uint32_t scd::compress_column(float* originalColumn, uint32_t inNumWords, uint32_t* compressedColumn, uint32_t toIntegerScaler) {
	// 0x40 31 7bit delta, 0x30 23 9bit delta, 0x20 15 14bit delta, 0x10 7 31-bit delta
	uint32_t numWords = 0;

	uint32_t CL[8];
  	uint32_t keys[31];

	uint32_t numProcessed = 0;
	for (uint32_t i = 0; i < inNumWords; i=i+numProcessed+1) {
		int base = (int)(originalColumn[i]*(1 << toIntegerScaler));

		CL[0] = (uint32_t)base;

		uint32_t search_for = 31;
		uint32_t j = i + 1;
		numProcessed = 0;

		// cout << "------------------" << endl;
		// cout << "j: " << j << endl;

		while (j < inNumWords) {
			int next = (int)(originalColumn[j]*(1 << toIntegerScaler));
			int delta = next - base;

			if (delta >= -64 && delta <= 63 && i+31 < inNumWords && search_for >= 31)
				search_for = 31;
			else if (delta >= -256 && delta <= 255 && i+23 < inNumWords && search_for >= 23)
				search_for = 23;
			else if (delta >= -8192 && delta <= 8191 && i+15 < inNumWords && search_for >= 15)
				search_for = 15;
			else
				search_for = 7;

			keys[numProcessed] = (uint32_t)delta;
			numProcessed++;
			if (numProcessed >= search_for) {
				numProcessed = search_for;
				break;
			}
			j++;
		}

		// cout << "inNumWords: " << inNumWords << endl;
		// cout << "i: " << i << endl;
		// cout << "j: " << j << endl;
		// cout << "numProcessed: " << numProcessed << endl;

		if (numProcessed == 31) {
			CL[1] =
			((keys[4] & 0xF) << 28) |
			((keys[3] & 0x7F) << 21) |
			((keys[2] & 0x7F) << 14) |
			((keys[1] & 0x7F) << 7) |
			((keys[0] & 0x7F));

			CL[2] =
			((keys[9] & 0x01) << 31) |
			((keys[8] & 0x7F) << 24) |
			((keys[7] & 0x7F) << 17) |
			((keys[6] & 0x7F) << 10) |
			((keys[5] & 0x7F) << 3) |
			((keys[4] & 0x7F) >> 4);

			CL[3] =
			((keys[13] & 0x1F) << 27) |
			((keys[12] & 0x7F) << 20) |
			((keys[11] & 0x7F) << 13) |
			((keys[10] & 0x7F) << 6) |
			((keys[9] & 0x7F) >> 1);

			CL[4] =
			((keys[18] & 0x03) << 30) |
			((keys[17] & 0x7F) << 23) |
			((keys[16] & 0x7F) << 16) |
			((keys[15] & 0x7F) << 9) |
			((keys[14] & 0x7F) << 2) |
			((keys[13] & 0x7F) >> 5);

			CL[5] =
			((keys[22] & 0x3F) << 26) |
			((keys[21] & 0x7F) << 19) |
			((keys[20] & 0x7F) << 12) |
			((keys[19] & 0x7F) << 5) |
			((keys[18] & 0x7F) >> 2);

			CL[6] =
			((keys[27] & 0x07) << 29) |
			((keys[26] & 0x7F) << 22) |
			((keys[25] & 0x7F) << 15) |
			((keys[24] & 0x7F) << 8) |
			((keys[23] & 0x7F) << 1) |
			((keys[22] & 0x7F) >> 6);

			CL[7] =
			((keys[30] & 0x7F) << 18) |
			((keys[29] & 0x7F) << 11) |
			((keys[28] & 0x7F) << 4) |
			((keys[27] & 0x7F) >> 3);

			uint32_t meta = 0x40;
			meta = meta << 24;
			CL[7] |= meta;
		}
		else if (numProcessed == 23) {
			CL[1] =
			((keys[3] & 0x1F) << 27) |
			((keys[2] & 0x1FF) << 18) |
			((keys[1] & 0x1FF) << 9) |
			((keys[0] & 0x1FF));

			CL[2] =
			((keys[7] & 0x1) << 31) |
			((keys[6] & 0x1FF) << 22) |
			((keys[5] & 0x1FF) << 13) |
			((keys[4] & 0x1FF) << 4) |
			((keys[3] & 0x1FF) >> 5);

			CL[3] =
			((keys[10] & 0x3F) << 26) |
			((keys[9] & 0x1FF) << 17) |
			((keys[8] & 0x1FF) << 8) |
			((keys[7] & 0x1FF) >> 1);

			CL[4] =
			((keys[14] & 0x3) << 30) |
			((keys[13] & 0x1FF) << 21) |
			((keys[12] & 0x1FF) << 12) |
			((keys[11] & 0x1FF) << 3) |
			((keys[10] & 0x1FF) >> 6);

			CL[5] =
			((keys[17] & 0x7F) << 25) |
			((keys[16] & 0x1FF) << 16) |
			((keys[15] & 0x1FF) << 7) |
			((keys[14] & 0x1FF) >> 2);

			CL[6] =
			((keys[21] & 0x7) << 29) |
			((keys[20] & 0x1FF) << 20) |
			((keys[19] & 0x1FF) << 11) |
			((keys[18] & 0x1FF) << 2) |
			((keys[17] & 0x1FF) >> 7);

			CL[7] =
			((keys[22] & 0x1FF) << 6) |
			((keys[21] & 0x1FF) >> 3);

			uint32_t meta = 0x30;
			meta = meta << 24;
			CL[7] |= meta;
		}
		else if (numProcessed == 15) {
			CL[1] =
			((keys[2] & 0xF) << 28) |
			((keys[1] & 0x3FFF) << 14) |
			((keys[0] & 0x3FFF));

			CL[2] =
			((keys[4] & 0xFF) << 24) |
			((keys[3] & 0x3FFF) << 10) |
			((keys[2] & 0x3FFF) >> 4);

			CL[3] =
			((keys[6] & 0x3FFF) << 20) |
			((keys[5] & 0x3FFF) << 6) |
			((keys[4] & 0x3FFF) >> 8);

			CL[4] =
			((keys[9] & 0x3) << 30) |
			((keys[8] & 0x3FFF) << 16) |
			((keys[7] & 0x3FFF) << 2) |
			((keys[6] & 0x3FFF) >> 12);

			CL[5] =
			((keys[11] & 0x3F) << 26) |
			((keys[10] & 0x3FFF) << 12) |
			((keys[9] & 0x3FFF) >> 2);

			CL[6] =
			((keys[13] & 0x3FF) << 22) |
			((keys[12] & 0x3FFF) << 8) |
			((keys[11] & 0x3FFF) >> 6);

			CL[7] =
			((keys[14] & 0x3FFF) << 4) |
			((keys[13] & 0x3FFF) >> 10);

			uint32_t meta = 0x20;
			meta = meta << 24;
			CL[7] |= meta;
		}
		else {
			CL[1] = ((keys[1] & 0x1) << 31) | (keys[0] & 0x7FFFFFFF);
			CL[2] = ((keys[2] & 0x3) << 30) | ((keys[1] & 0x7FFFFFFF) >> 1);
			CL[3] = ((keys[3] & 0x7) << 29) | ((keys[2] & 0x7FFFFFFF) >> 2);
			CL[4] = ((keys[4] & 0xF) << 28) | ((keys[3] & 0x7FFFFFFF) >> 3);
			CL[5] = ((keys[5] & 0x1F) << 27) | ((keys[4] & 0x7FFFFFFF) >> 4);
			CL[6] = ((keys[6] & 0x3F) << 26) | ((keys[5] & 0x7FFFFFFF) >> 5);
			CL[7] = ((keys[6] & 0x7FFFFFFF) >> 6);

			uint32_t meta = numProcessed;
			meta = meta << 26;
			CL[7] |= meta;
		}

		for (uint32_t k = 0; k < 8; k++) {
			compressedColumn[numWords++] = CL[k];
		}
	}

	return numWords;
}

void scd::decrypt_column(uint32_t* encryptedColumn, uint32_t inNumWords, float* decryptedColumn) {
	AES_CBC_decrypt((unsigned char*)encryptedColumn, (unsigned char*)decryptedColumn, ivec, inNumWords*sizeof(float), KEYS_dec, 14);
}

void scd::encrypt_column(float* originalColumn, uint32_t inNumWords, uint32_t* encryptedColumn) {
	AES_CBC_encrypt((unsigned char*)originalColumn, (unsigned char*)encryptedColumn, ivec, inNumWords*sizeof(float), KEYS_enc, 14);
}

#endif