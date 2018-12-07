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
#include <string>
#include <fstream>
#include <iostream>
#include <limits>
#include <sys/time.h>

#include "aes.h"

using namespace std;

static double get_time()
{
	struct timeval t;
	gettimeofday(&t, NULL);
	return t.tv_sec + t.tv_usec*1e-6;
}

enum NormType {ZeroToOne, MinusOneToOne};
enum NormDirection {row, column};

class ColumnStore {
public:
	float** m_samples;
	float* m_labels;

	uint32_t** m_compressedSamples;
	uint32_t** m_compressedSamplesSizes;
	uint32_t** m_encryptedSamples;

	uint32_t m_numSamples;
	uint32_t m_numFeatures;
	bool m_samplesBiased;

	NormType m_samplesNorm;
	NormType m_labelsNorm;

	float* m_samplesRange;
	float* m_samplesMin;
	float m_labelsRange;
	float m_labelsMin;

	ColumnStore() {
		m_samples = nullptr;
		m_labels = nullptr;

		m_compressedSamples = nullptr;
		m_compressedSamplesSizes = nullptr;
		m_encryptedSamples = nullptr;

		for (uint32_t i = 0; i < 32; i++) {
			m_initKey[i] = (unsigned char)i;
		}
		m_KEYS_enc = (unsigned char*)malloc(16*15);
		m_KEYS_dec = (unsigned char*)malloc(16*15);
		AES_256_Key_Expansion(m_initKey, m_KEYS_enc);
		AES_256_Decryption_Keys(m_KEYS_enc, m_KEYS_dec);
	}

	~ColumnStore() {
		deallocData();
		deallocCompressed();
		deallocEncrypted();

		free(m_KEYS_enc);
		free(m_KEYS_dec);
	}

	void PrintSamples(uint32_t num) {
		for (uint32_t i = 0; i < num; i++) {
			cout << "sample " << i << ": " << endl;
			for (uint32_t j = 0; j < m_numFeatures; j++) {
				cout << m_samples[j][i] << " ";
			}
			cout << endl;
			cout << "label " << i << ": " << m_labels[i] << endl;
		}
	}

	// Data loading functions
	void LoadLibsvmData(char* pathToFile, uint32_t numSamples, uint32_t numFeatures, bool samplesBiased);
	void LoadRawData(char* pathToFile, uint32_t numSamples, uint32_t numFeatures, bool labelPresent);
	void GenerateSyntheticData(uint32_t numSamples, uint32_t numFeatures, bool labelBinary, NormType labelsNorm);

	// Normalization and data shaping
	void NormalizeSamples(NormType norm, NormDirection direction);
	void NormalizeLabels(NormType norm, bool binarizeLabels, float labelsToBinarizeTo);
	float CompressSamples(uint32_t minibatchSize, uint32_t toIntegerScaler);
	void EncryptSamples(uint32_t minibatchSize, bool useCompressed);

	static uint32_t decompressColumn(uint32_t* compressedColumn, uint32_t inNumWords, float* decompressedColumn, uint32_t toIntegerScaler);
	static uint32_t compressColumn(float* originalColumn, uint32_t inNumWords, uint32_t* compressedColumn, uint32_t toIntegerScaler);
	void decryptColumn(uint32_t* encryptedColumn, uint32_t inNumWords, float* decryptedColumn);
	void encryptColumn(float* originalColumn, uint32_t inNumWords, uint32_t* encryptedColumn);

	inline void ReturnDecompressedAndDecrypted(
		float* transformedColumn1,
		float* &transformedColumn2,
		uint32_t coordinate,
		uint32_t* minibatchIndex,
		uint32_t numMinibatchesAtATime,
		uint32_t minibatchSize,
		bool useEncrypted, 
		bool useCompressed,
		uint32_t toIntegerScaler,
		double &decryptionTime,
		double &decompressionTime)
	{
		double timeStamp1, timeStamp2, timeStamp3;
		for (uint32_t l = 0; l < numMinibatchesAtATime; l++) {
			if (useEncrypted && useCompressed) {
				int32_t compressedSamplesOffset = 0;
				if (minibatchIndex[l] > 0) {
					compressedSamplesOffset = m_compressedSamplesSizes[coordinate][minibatchIndex[l]-1];
				}
				timeStamp1 = get_time();
				decryptColumn(m_encryptedSamples[coordinate] + compressedSamplesOffset, m_compressedSamplesSizes[coordinate][minibatchIndex[l]] - compressedSamplesOffset, transformedColumn1 + l*minibatchSize);
				timeStamp2 = get_time();
				decryptionTime += (timeStamp2-timeStamp1);
				ColumnStore::decompressColumn((uint32_t*)transformedColumn1 + l*minibatchSize, m_compressedSamplesSizes[coordinate][minibatchIndex[l]] - compressedSamplesOffset, transformedColumn2 + l*minibatchSize, toIntegerScaler);
				timeStamp3 = get_time();
				decompressionTime += (timeStamp3-timeStamp2);
			}
			else if (useEncrypted) {
				timeStamp1 = get_time();
				decryptColumn(m_encryptedSamples[coordinate] + minibatchIndex[l]*minibatchSize, minibatchSize, transformedColumn2 + l*minibatchSize);
				timeStamp2 = get_time();
				decryptionTime += (timeStamp2-timeStamp1);
			}
			else if (useCompressed) {
				timeStamp1 = get_time();
				int32_t compressedSamplesOffset = 0;
				if (minibatchIndex[l] > 0) {
					compressedSamplesOffset = m_compressedSamplesSizes[coordinate][minibatchIndex[l]-1];
				}
				ColumnStore::decompressColumn(m_compressedSamples[coordinate] + compressedSamplesOffset, m_compressedSamplesSizes[coordinate][minibatchIndex[l]] - compressedSamplesOffset, transformedColumn2 + l*minibatchSize, toIntegerScaler);
				timeStamp2 = get_time();
				decompressionTime += (timeStamp2-timeStamp1);
			}
			else if (numMinibatchesAtATime > 1) {
				for (uint32_t i = 0; i < minibatchSize; i++) {
					transformedColumn2[l*minibatchSize + i] = m_samples[coordinate][minibatchIndex[l]*minibatchSize + i];
				}
			}
			else {
				transformedColumn2 = m_samples[coordinate] + minibatchIndex[l]*minibatchSize;
			}
		}
	}

	unsigned char m_initKey[32];
	unsigned char* m_KEYS_enc;
	unsigned char* m_KEYS_dec;
	unsigned char m_ivec[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
private:
	void reallocData() {
		deallocData();
		
		// Allocate memory
		m_samples = (float**)malloc(m_numFeatures*sizeof(float*));
		for (uint32_t j = 0; j < m_numFeatures; j++) {
			m_samples[j] = (float*)aligned_alloc(64, m_numSamples*sizeof(float));
		}
		m_labels = (float*)aligned_alloc(64, m_numSamples*sizeof(float));
	}

	void deallocData() {
		// dealloc if not nullptr
		if (m_samples != nullptr) {
			cout << "Freeing m_samples..." << endl;
			for (uint32_t j = 0; j < m_numFeatures; j++) {
				free(m_samples[j]);
			}
			free(m_samples);
		}
		if (m_labels != nullptr) {
			cout << "Freeing m_labels..." << endl;
			free(m_labels);
		}
	}

	void reallocCompressed(uint32_t numMinibatches) {
		deallocCompressed();

		m_compressedSamples = (uint32_t**)malloc(m_numFeatures*sizeof(uint32_t*));
		m_compressedSamplesSizes = (uint32_t**)malloc(m_numFeatures*sizeof(uint32_t*));
		for (uint32_t j = 0; j < m_numFeatures; j++) {
			m_compressedSamples[j] = (uint32_t*)aligned_alloc(64, m_numSamples*sizeof(uint32_t));
			m_compressedSamplesSizes[j] = (uint32_t*)aligned_alloc(64, numMinibatches*sizeof(uint32_t));
		}
	}

	void deallocCompressed() {
		if (m_compressedSamples != nullptr) {
			for (uint32_t j = 0; j < m_numFeatures; j++) {
				free(m_compressedSamples[j]);
			}
			free(m_compressedSamples);
		}
		if (m_compressedSamplesSizes != nullptr) {
			for (uint32_t j = 0; j < m_numFeatures; j++) {
				free(m_compressedSamplesSizes[j]);
			}
			free(m_compressedSamplesSizes);
		}
	}

	void reallocEncrypted() {
		deallocEncrypted();

		m_encryptedSamples = (uint32_t**)malloc(m_numFeatures*sizeof(uint32_t*));
		for (uint32_t j = 0; j < m_numFeatures; j++) {
			m_encryptedSamples[j] = (uint32_t*)aligned_alloc(64, m_numSamples*sizeof(uint32_t));
		}
	}

	void deallocEncrypted() {
		if (m_encryptedSamples != nullptr) {
			for (uint32_t j = 0; j < m_numFeatures; j++) {
				free(m_encryptedSamples[j]);
			}
			free(m_encryptedSamples);
		}
	}
};