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

#include "ColumnStore.h"

using namespace std;

void ColumnStore::LoadLibsvmData(char* pathToFile, uint32_t numSamples, uint32_t numFeatures, bool samplesBiased) {
	cout << "LoadLibsvmData is reading " << pathToFile << endl;

	m_samplesBiased = samplesBiased;
	m_numSamples = numSamples;
	m_numFeatures = m_samplesBiased ? numFeatures + 1 : numFeatures;

	reallocData();

	string line;
	ifstream f(pathToFile);

	uint32_t index = 0;
	if (f.is_open()) {
		while( index < m_numSamples ) {
			getline(f, line);
			int pos0 = 0;
			int pos1 = 0;
			int pos2 = 0;
			int column = 0;
			while ( pos2 < (int)line.length()+1 ) {
				if (pos2 == 0) {
					pos2 = line.find(" ", pos1);
					float temp = stof(line.substr(pos1, pos2-pos1), NULL);
					m_labels[index] = temp;
				}
				else {
					pos0 = pos2;
					pos1 = line.find(":", pos1)+1;
					pos2 = line.find(" ", pos1);
					column = stof(line.substr(pos0+1, pos1-pos0-1));
					column = m_samplesBiased ? column : column - 1;
					if (pos2 == -1) {
						pos2 = line.length()+1;
						m_samples[column][index] = stof(line.substr(pos1, pos2-pos1), NULL);
					}
					else {
						m_samples[column][index] = stof(line.substr(pos1, pos2-pos1), NULL);
					}
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

	if (m_samplesBiased) {
		for (uint32_t i = 0; i < m_numSamples; i++) { // Bias term
			m_samples[0][i] = 1.0;
		}
	}
	
	cout << "m_numSamples: " << m_numSamples << endl;
	cout << "m_numFeatures: " << m_numFeatures << endl;
}


void ColumnStore::LoadRawData(char* pathToFile, uint32_t numSamples, uint32_t numFeatures, bool labelPresent) {
	cout << "LoadRawData is reading " << pathToFile << endl;

	m_samplesBiased = true;
	m_numSamples = numSamples;
	m_numFeatures = numFeatures+1; // For the bias term
	
	reallocData();

	FILE* f = fopen(pathToFile, "r");
	if (f == NULL) {
		cout << "Can't find files at pathToFile" << endl;
		exit(1);
	}

	double* temp;
	
	uint32_t numFeaturesWithoutBias = m_numFeatures-1;

	if (labelPresent) {
		temp = (double*)malloc(m_numSamples*(numFeaturesWithoutBias+1)*sizeof(double));
		size_t readsize = fread(temp, sizeof(double), m_numSamples*(numFeaturesWithoutBias+1), f);
		for (uint32_t i = 0; i < m_numSamples; i++) {
			m_labels[i] = (float)temp[i*(numFeaturesWithoutBias+1)];
			for (uint32_t j = 0; j < numFeaturesWithoutBias; j++) {
				m_samples[j+1][i] = (float)temp[i*(numFeaturesWithoutBias+1) + j + 1];
			}
		}
	}
	else {
		temp = (double*)malloc(m_numSamples*numFeaturesWithoutBias*sizeof(double));
		size_t readsize = fread(temp, sizeof(double), m_numSamples*numFeaturesWithoutBias, f);
		for (uint32_t i = 0; i < m_numSamples; i++) {
			m_labels[i] = 0;
			for (uint32_t j = 0; j < numFeaturesWithoutBias; j++) {
				m_samples[j+1][i] = (float)temp[i*numFeaturesWithoutBias + j];
			}
		}
	}

	for (uint32_t i = 0; i < m_numSamples; i++) { // Bias term
		m_samples[0][i] = 1.0;
	}

	free(temp);
	fclose(f);

	cout << "m_numSamples: " << m_numSamples << endl;
	cout << "m_numFeatures: " << m_numFeatures << endl;
}

void ColumnStore::GenerateSyntheticData(uint32_t numSamples, uint32_t numFeatures, bool labelBinary, NormType labelsNorm) {
	
	m_numSamples = numSamples;
	m_numFeatures = numFeatures;
	m_samplesBiased = false;
	m_labelsNorm = labelsNorm;

	reallocData();
	
	srand(7);
	float* x = (float*)malloc(m_numFeatures*sizeof(float));
	for (uint32_t j = 0; j < m_numFeatures; j++) {
		x[j] = ((float)rand())/RAND_MAX;
	}

	for (uint32_t i = 0; i < m_numSamples; i++) {
		if (labelBinary) {
			float temp = ((float)rand())/RAND_MAX;
			if (temp > 0.5) {
				m_labels[i] = 1.0;
			}
			else {
				if (labelsNorm == MinusOneToOne) {
					m_labels[i] = -1.0;
				}
				else {
					m_labels[i] = 0.0;
				}
			}
		}
		else {
			m_labels[i] = (float)rand()/RAND_MAX;
		}
		for (uint32_t j = 0; j < m_numFeatures; j++) {
			m_samples[j][i] = m_labels[i]*x[j] + 0.001*(float)rand()/(RAND_MAX);
		}
	}

	free(x);

	cout << "m_numSamples: " << m_numSamples << endl;
	cout << "m_numFeatures: " << m_numFeatures << endl;
}

void ColumnStore::NormalizeSamples(NormType norm, NormDirection direction) {
	m_samplesNorm = norm;

	if (direction == row) {
		m_samplesRange = (float*)realloc(m_samplesRange, m_numSamples*sizeof(float));
		m_samplesMin = (float*)realloc(m_samplesMin, m_numSamples*sizeof(float));

		for (uint32_t i = 0; i < m_numSamples; i++) {
			float samplesMin = numeric_limits<float>::max();
			float samplesMax = -numeric_limits<float>::max();
			for (uint32_t j = 0; j < m_numFeatures; j++) {
				if (m_samples[j][i] > samplesMax) {
					samplesMax = m_samples[j][i];
				}
				if (m_samples[j][i] < samplesMin) {
					samplesMin = m_samples[j][i];
				}
			}
			float samplesRange = samplesMax - samplesMin;
			if (samplesRange > 0) {
				if (m_samplesNorm == MinusOneToOne) {
					for (uint32_t j = 0; j < m_numFeatures; j++) {
						m_samples[j][i] = ((m_samples[j][i] - samplesMin)/samplesRange)*2.0-1.0;
					}
				}
				else {
					for (uint32_t j = 0; j < m_numFeatures; j++) {
						m_samples[j][i] = ((m_samples[j][i] - samplesMin)/samplesRange);
					}
				}
			}
			m_samplesRange[i] = samplesRange;
			m_samplesMin[i] = samplesMin;
		}
	}
	else {
		m_samplesRange = (float*)realloc(m_samplesRange, m_numFeatures*sizeof(float));
		m_samplesMin = (float*)realloc(m_samplesMin, m_numFeatures*sizeof(float));

		uint32_t startCoordinate = m_samplesBiased ? 1 : 0;
		m_samplesRange[0] = 0.0;
		m_samplesMin[0] = 0.0;
		for (uint32_t j = startCoordinate; j < m_numFeatures; j++) {
			float samplesMin = numeric_limits<float>::max();
			float samplesMax = -numeric_limits<float>::max();
			for (uint32_t i = 0; i < m_numSamples; i++) {
				if (m_samples[j][i] > samplesMax) {
					samplesMax = m_samples[j][i];;
				}
				if (m_samples[j][i] < samplesMin) {
					samplesMin = m_samples[j][i];;
				}
			}
			float samplesRange = samplesMax - samplesMin;
			if (samplesRange > 0) {
				if (m_samplesNorm == MinusOneToOne) {
					for (uint32_t i = 0; i < m_numSamples; i++) {
						m_samples[j][i] = ((m_samples[j][i] - samplesMin)/samplesRange)*2.0-1.0;
					}
				}
				else {
					for (uint32_t i = 0; i < m_numSamples; i++) {
						m_samples[j][i] = ((m_samples[j][i] - samplesMin)/samplesRange);
					}
				}
			}
			m_samplesRange[j] = samplesRange;
			m_samplesMin[j] = samplesMin;
		}
	}
}

void ColumnStore::NormalizeLabels(NormType norm, bool binarizeLabels, float labelsToBinarizeTo) {
	m_labelsNorm = norm;

	if (!binarizeLabels) {
		float labelsMin = numeric_limits<float>::max();
		float labelsMax = -numeric_limits<float>::max();
		for (uint32_t i = 0; i < m_numSamples; i++) {
			if (m_labels[i] > labelsMax) {
				labelsMax = m_labels[i];
			}
			if (m_labels[i] < labelsMin) {
				labelsMin = m_labels[i];
			}
		}
		
		float labelsRange = labelsMax - labelsMin;
		if (labelsRange > 0) {
			if (m_labelsNorm == MinusOneToOne) {
				for (uint32_t i = 0; i < m_numSamples; i++) {
					m_labels[i] = ((m_labels[i] - labelsMin)/labelsRange)*2.0 - 1.0;
				}
			}
			else {
				for (uint32_t i = 0; i < m_numSamples; i++) {
					m_labels[i] = (m_labels[i] - labelsMin)/labelsRange;
				}
			}
		}
		m_labelsMin = labelsMin;
		m_labelsRange = labelsRange;
	}
	else {
		for (uint32_t i = 0; i < m_numSamples; i++) {
			if(m_labels[i] == labelsToBinarizeTo) {
				m_labels[i] = 1.0;
			}
			else {
				if (m_labelsNorm == MinusOneToOne) {
					m_labels[i] = -1.0;
					m_labelsMin = -1.0;
					m_labelsRange = 2.0;
				}
				else {
					m_labels[i] = 0.0;
					m_labelsMin = 0.0;
					m_labelsRange = 1.0;
				}
			}
		}
	}
}

float ColumnStore::CompressSamples(uint32_t minibatchSize, uint32_t toIntegerScaler) {
	uint32_t numMinibatches = m_numSamples/minibatchSize;
	cout << "numMinibatches: " << numMinibatches << endl;
	uint32_t rest = m_numSamples - numMinibatches*minibatchSize;
	cout << "rest: " << rest << endl;

	reallocCompressed(numMinibatches);

	for (uint32_t m = 0; m < numMinibatches; m++) {
		for (uint32_t j = 0; j < m_numFeatures; j++) {
			uint32_t compressedSamplesOffset = 0;
			if (m > 0) {
				compressedSamplesOffset = m_compressedSamplesSizes[j][m-1];
			}
			uint32_t numWordsInBatch = compressColumn(m_samples[j] + m*minibatchSize, minibatchSize, m_compressedSamples[j] + compressedSamplesOffset, toIntegerScaler);
			if (numWordsInBatch%4 > 0) {
				numWordsInBatch += (4 - numWordsInBatch%4);
			}
			m_compressedSamplesSizes[j][m] = compressedSamplesOffset + numWordsInBatch;
		}
	}

	uint32_t numWordsAfterCompression = 0;
	for (uint32_t j = 0; j < m_numFeatures; j++) {
		numWordsAfterCompression += m_compressedSamplesSizes[j][numMinibatches-1];
	}

	float compressionRate = (float)(numMinibatches*minibatchSize*m_numFeatures)/(float)numWordsAfterCompression;

	return compressionRate;
}

void ColumnStore::EncryptSamples(uint32_t minibatchSize, bool useCompressed) {
	uint32_t numMinibatches = m_numSamples/minibatchSize;
	cout << "numMinibatches: " << numMinibatches << endl;
	uint32_t rest = m_numSamples - numMinibatches*minibatchSize;
	cout << "rest: " << rest << endl;

	reallocEncrypted();

	if (useCompressed) {
		for (uint32_t j = 0; j < m_numFeatures; j++) {
			for (uint32_t m = 0; m < numMinibatches; m++) {
				int32_t compressedSamplesOffset = 0;
				if (m > 0) {
					compressedSamplesOffset = m_compressedSamplesSizes[j][m-1];
				}
				encryptColumn((float*)(m_compressedSamples[j] + compressedSamplesOffset), m_compressedSamplesSizes[j][m] - compressedSamplesOffset, m_encryptedSamples[j] + compressedSamplesOffset);
			}
		}
	}
	else {
		for (uint32_t j = 0; j < m_numFeatures; j++) {
			for (uint32_t m = 0; m < numMinibatches; m++) {
				encryptColumn(m_samples[j] + m*minibatchSize, minibatchSize, m_encryptedSamples[j] + m*minibatchSize);
			}
		}
	}
}

uint32_t ColumnStore::decompressColumn(uint32_t* compressedColumn, uint32_t inNumWords, float* decompressedColumn, uint32_t toIntegerScaler) {
	uint32_t outNumWords = 0;
	int delta[31];
	uint32_t CL[8];

	for (uint32_t i = 0; i < inNumWords; i+=8) {
		uint32_t meta = (compressedColumn[i+7] >> 24) & 0xFC;
		int base = (int)compressedColumn[i];
		decompressedColumn[outNumWords++] = ((float)base)/((float)(1 << toIntegerScaler));

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
				else {
					decompressedColumn[outNumWords++] = ((float)(base+delta[k]))/((float)(1 << toIntegerScaler));
				}
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
				else {
					decompressedColumn[outNumWords++] = ((float)(base+delta[k]))/((float)(1 << toIntegerScaler));
				}
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
				else {
					decompressedColumn[outNumWords++] = ((float)(base+delta[k]))/((float)(1 << toIntegerScaler));
				}
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

			for (uint32_t k = 0; k < numProcessed; k++) {
				if ((delta[k] >> 30) == 0x1) {
					delta[k] = delta[k] - (1 << 31);
					decompressedColumn[outNumWords++] = ((float)(base+delta[k]))/((float)(1 << toIntegerScaler));
				}
				else {
					decompressedColumn[outNumWords++] = ((float)(base+delta[k]))/((float)(1 << toIntegerScaler));
				}
			}
		}
	}
	return outNumWords;
}

uint32_t ColumnStore::compressColumn(float* originalColumn, uint32_t inNumWords, uint32_t* compressedColumn, uint32_t toIntegerScaler) {
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

void ColumnStore::decryptColumn(uint32_t* encryptedColumn, uint32_t inNumWords, float* decryptedColumn) {
	AES_CBC_decrypt((unsigned char*)encryptedColumn, (unsigned char*)decryptedColumn, m_ivec, inNumWords*sizeof(float), m_KEYS_dec, 14);
}

void ColumnStore::encryptColumn(float* originalColumn, uint32_t inNumWords, uint32_t* encryptedColumn) {
	AES_CBC_encrypt((unsigned char*)originalColumn, (unsigned char*)encryptedColumn, m_ivec, inNumWords*sizeof(float), m_KEYS_enc, 14);
}