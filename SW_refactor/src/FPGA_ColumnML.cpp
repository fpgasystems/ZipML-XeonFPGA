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

#include "FPGA_ColumnML.h"

using namespace std;

uint32_t FPGA_ColumnML::FPGA_PrintMemory() {

	uint32_t address32 = 0;
	
	for (uint32_t i = 0; i < m_residualAddress[0]*m_numValuesPerLine; i++) {
		cout << i << ": " << m_interfaceFPGA->readFromMemoryFloat('i', address32++) << endl;
	}

	exit(1);
}


uint32_t FPGA_ColumnML::FPGA_CopyDataIntoMemory(
	uint32_t numMinibatches, 
	uint32_t minibatchSize, 
	uint32_t numMinibatchesToAssign[],
	uint32_t numEpochs, 
	bool useEncrypted)
{
	uint32_t address32 = NUM_FINSTANCES*m_pagesToAllocate*m_numValuesPerLine;
	uint32_t numMinibatchesAssigned = 0;
	
	// Space for offsets
	for (uint32_t n = 0; n < NUM_FINSTANCES; n++) {
		m_samplesAddress[n] = address32/m_numValuesPerLine;
		for (uint32_t i = 0; i < (m_cstore->m_numFeatures/m_numValuesPerLine + (m_cstore->m_numFeatures%m_numValuesPerLine > 0))*m_numValuesPerLine; i++) {
			m_interfaceFPGA->writeToMemoryFloat('i', 0, address32++);
		}
	}

	// Write labels
	numMinibatchesAssigned = 0;
	for (uint32_t n = 0; n < NUM_FINSTANCES; n++) {
		m_labelsAddress[n] = address32/m_numValuesPerLine;

		for (uint32_t i = 0; i < numMinibatchesToAssign[n]*minibatchSize; i++) {
			m_interfaceFPGA->writeToMemoryFloat('i', m_cstore->m_labels[numMinibatchesAssigned*minibatchSize + i], address32++);
		}
		if (address32%m_numValuesPerLine > 0) {
			uint32_t padding = (m_numValuesPerLine - address32%m_numValuesPerLine);
			for (uint32_t i = 0; i < padding; i++) {
				m_interfaceFPGA->writeToMemory32('i', 0, address32++);
			}
		}
		numMinibatchesAssigned += numMinibatchesToAssign[n];
	}
	
	// Write samples
	for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
		
		numMinibatchesAssigned = 0;
		for (uint32_t n = 0; n < NUM_FINSTANCES; n++) {
			m_interfaceFPGA->writeToMemory32('i', address32/m_numValuesPerLine, m_samplesAddress[n]*m_numValuesPerLine + j);

			for (uint32_t i = 0; i < numMinibatchesToAssign[n]*minibatchSize; i++) {
				if (useEncrypted) {
					m_interfaceFPGA->writeToMemory32('i', m_cstore->m_encryptedSamples[j][numMinibatchesAssigned*minibatchSize + i], address32++);
				}
				else {
					m_interfaceFPGA->writeToMemoryFloat('i', m_cstore->m_samples[j][numMinibatchesAssigned*minibatchSize + i], address32++);
				}
			}
			if (address32%m_numValuesPerLine > 0) {
				uint32_t padding = (m_numValuesPerLine - address32%m_numValuesPerLine);
				for (uint32_t i = 0; i < padding; i++) {
					m_interfaceFPGA->writeToMemoryFloat('i', 0, address32++);
				}
			}

			numMinibatchesAssigned += numMinibatchesToAssign[n];
		}		
	}

	// Residual addresses
	for (uint32_t n = 0; n < NUM_FINSTANCES; n++) {
		m_residualAddress[n] = address32/m_numValuesPerLine;
		for (uint32_t i = 0; i < numMinibatchesToAssign[n]*minibatchSize; i++) {
			m_interfaceFPGA->writeToMemoryFloat('i', 0, address32++);
		}
	}

	// Step addresses
	for (uint32_t n = 0; n < NUM_FINSTANCES; n++) {
		m_stepAddress[n] = address32/m_numValuesPerLine;
		address32 += numEpochs*numMinibatchesToAssign[n]*(m_cstore->m_numFeatures/m_numValuesPerLine + (m_cstore->m_numFeatures%m_numValuesPerLine > 0))*m_numValuesPerLine;
	}

	return m_stepAddress[0]*m_numValuesPerLine;
}

uint32_t FPGA_ColumnML::FPGA_CopyCompressedDataIntoMemory(
	uint32_t numMinibatches,
	uint32_t minibatchSize,
	uint32_t numMinibatchesToAssign[],
	uint32_t numEpochs,
	bool useEncrypted)
{
	uint32_t address32 = NUM_FINSTANCES*m_pagesToAllocate*m_numValuesPerLine;
	uint32_t numMinibatchesAssigned = 0;

	// Space for offsets
	for (uint32_t n = 0; n < NUM_FINSTANCES; n++) {
		m_samplesAddress[n] = address32/m_numValuesPerLine;
		for (uint32_t i = 0; i < (m_cstore->m_numFeatures/m_numValuesPerLine + (m_cstore->m_numFeatures%m_numValuesPerLine > 0))*m_numValuesPerLine; i++) {
			m_interfaceFPGA->writeToMemoryFloat('i', 0, address32++);
		}
	}
	// cout << address32 << endl;

	// Write labels
	numMinibatchesAssigned = 0;
	for (uint32_t n = 0; n < NUM_FINSTANCES; n++) {
		m_labelsAddress[n] = address32/m_numValuesPerLine;

		for (uint32_t i = 0; i < numMinibatchesToAssign[n]*minibatchSize; i++) {
			m_interfaceFPGA->writeToMemoryFloat('i', m_cstore->m_labels[numMinibatchesAssigned*minibatchSize + i], address32++);
		}
		if (address32%m_numValuesPerLine > 0) {
			uint32_t padding = (m_numValuesPerLine - address32%m_numValuesPerLine);
			for (uint32_t i = 0; i < padding; i++) {
				m_interfaceFPGA->writeToMemory32('i', 0, address32++);
			}
		}

		numMinibatchesAssigned += numMinibatchesToAssign[n];
	}
	// cout << address32 << endl;

	// Write samples
	for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
		
		numMinibatchesAssigned = 0;
		for (uint32_t n = 0; n < NUM_FINSTANCES; n++) {
			m_interfaceFPGA->writeToMemory32('i', address32/m_numValuesPerLine, m_samplesAddress[n]*m_numValuesPerLine + j);

			for (uint32_t m = numMinibatchesAssigned; m < numMinibatchesAssigned+numMinibatchesToAssign[n]; m++) {
				uint32_t compressedSamplesOffset = 0;
				if (m > 0) {
					compressedSamplesOffset = m_cstore->m_compressedSamplesSizes[j][m-1];
				}

				uint32_t numWordsInBatch = m_cstore->m_compressedSamplesSizes[j][m] - compressedSamplesOffset;

				// Size for the current compressed mini batch, +1 for the first line which contains how many further lines to read
				// cout << address32 << endl;
				m_interfaceFPGA->writeToMemory32('i', numWordsInBatch/m_numValuesPerLine + (numWordsInBatch%m_numValuesPerLine > 0) + 1, address32++);
				if (address32%m_numValuesPerLine > 0) {
					uint32_t padding = (m_numValuesPerLine - address32%m_numValuesPerLine);
					for (uint32_t i = 0; i < padding; i++) {
						m_interfaceFPGA->writeToMemory32('i', 0, address32++);
					}
				}
				// cout << address32 << endl;

				for (uint32_t i = 0; i < numWordsInBatch; i++) {
					if (useEncrypted) {
						m_interfaceFPGA->writeToMemory32('i', m_cstore->m_encryptedSamples[j][compressedSamplesOffset + i], address32++);
					}
					else {
						m_interfaceFPGA->writeToMemory32('i', m_cstore->m_compressedSamples[j][compressedSamplesOffset + i], address32++);
					}
				}
				if (address32%m_numValuesPerLine > 0) {
					uint32_t padding = (m_numValuesPerLine - address32%m_numValuesPerLine);
					for (uint32_t i = 0; i < padding; i++) {
						m_interfaceFPGA->writeToMemory32('i', 0, address32++);
					}
				}
				// cout << address32 << endl;
			}
			numMinibatchesAssigned += numMinibatchesToAssign[n];
		}		
	}

	// Residual addresses
	for (uint32_t n = 0; n < NUM_FINSTANCES; n++) {
		m_residualAddress[n] = address32/m_numValuesPerLine;
		for (uint32_t i = 0; i < numMinibatchesToAssign[n]*minibatchSize; i++) {
			m_interfaceFPGA->writeToMemoryFloat('i', 0, address32++);
		}
	}

	// Step addresses
	for (uint32_t n = 0; n < NUM_FINSTANCES; n++) {
		m_stepAddress[n] = address32/m_numValuesPerLine;
		address32 += numEpochs*numMinibatchesToAssign[n]*(m_cstore->m_numFeatures/m_numValuesPerLine + (m_cstore->m_numFeatures%m_numValuesPerLine > 0))*m_numValuesPerLine;
	}

	return m_stepAddress[0]*m_numValuesPerLine;	
}


double FPGA_ColumnML::FPGA_SCD(
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
	bool enableStaleness,
	uint32_t toIntegerScaler,
	AdditionalArguments* args,
	uint32_t numInstancesToUse)
{
	cout << "SCD ---------------------------------------" << endl;
	uint32_t numMinibatches = args->m_numSamples/minibatchSize;
	cout << "numMinibatches: " << numMinibatches << endl;
	uint32_t rest = args->m_numSamples - numMinibatches*minibatchSize;
	cout << "rest: " << rest << endl;

	bool enableMultiLine = true;

	if (doRealSCD) {
		residualUpdatePeriod = 0xFFFFFFFF;
		enableStaleness = 0;
		numInstancesToUse = 1;
	}

	uint32_t numMinibatchesAssigned = 0;
	uint32_t numMinibatchesToAssign[NUM_FINSTANCES];
	for (uint32_t n = 0; n < NUM_FINSTANCES; n++) {
		if (numMinibatchesAssigned < numMinibatches) {
			uint32_t temp = numMinibatches/numInstancesToUse + (numMinibatches%numInstancesToUse > 0);
			if (temp > numMinibatches - numMinibatchesAssigned) {
				numMinibatchesToAssign[n] = numMinibatches - numMinibatchesAssigned;
			}
			else {
				numMinibatchesToAssign[n] = temp;
			}
		}
		else {
			numMinibatchesToAssign[n] = 0;
		}
		cout << "numMinibatchesToAssign[" << n << "]: " << numMinibatchesToAssign[n] << endl; 
		numMinibatchesAssigned += numMinibatchesToAssign[n];
	}

	uint32_t address32 = 0;
	if (useCompressed) {
		address32 = FPGA_CopyCompressedDataIntoMemory(numMinibatches, minibatchSize, numMinibatchesToAssign, numEpochs, useEncrypted);
	}
	else {
		address32 = FPGA_CopyDataIntoMemory(numMinibatches, minibatchSize, numMinibatchesToAssign, numEpochs, useEncrypted);
	}

	float* xHistoryLocal = (float*)aligned_alloc(64, numEpochs*m_cstore->m_numFeatures*sizeof(float));
	memset(xHistoryLocal, 0, numEpochs*m_cstore->m_numFeatures*sizeof(float));
	float* x = (float*)aligned_alloc(64, m_cstore->m_numFeatures*sizeof(float));
	memset(x, 0, m_cstore->m_numFeatures*sizeof(float));
	
#ifdef PRINT_LOSS
	cout << "Initial loss: " << Loss(type, x, lambda, args) << endl;
#endif

	float tempStepSize;
	if (doRealSCD ){
		tempStepSize = stepSize/(float)args->m_numSamples;
	}
	else {
		tempStepSize = stepSize/(float)minibatchSize;
	}
	uint32_t* tempStepSizeAddr = (uint32_t*) &tempStepSize;

	uint64_t config[NUM_FINSTANCES][5];
	for (uint32_t n = 0; n < NUM_FINSTANCES; n++) {
		m_interfaceFPGA->m_pALIMMIOService->mmioWrite32(CSR_NUM_LINES, n);
		m_interfaceFPGA->m_pALIMMIOService->mmioWrite32(CSR_READ_OFFSET, 0);
		m_interfaceFPGA->m_pALIMMIOService->mmioWrite32(CSR_WRITE_OFFSET, 0);

		config[n][0] = 0;
		config[n][0] = ((uint64_t)m_labelsAddress[n] << 32) | ((uint64_t)m_samplesAddress[n]);
		
		config[n][1] = 0;
		config[n][1] = ((uint64_t)m_residualAddress[n] << 32) | ((uint64_t)m_stepAddress[n]);
		
		config[n][2] = 0;
		config[n][2] = ((uint64_t)(minibatchSize/m_numValuesPerLine) << 48) | ((uint64_t)numMinibatchesToAssign[n] << 32) | ((uint64_t)m_cstore->m_numFeatures);
		
		config[n][3] = 0;
		config[n][3] = ((uint64_t)numEpochs << 32) | ((uint64_t)*tempStepSizeAddr);

		config[n][4] = 0;
		config[n][4] = ((uint64_t)doRealSCD << 25) | ((uint64_t)15 << 20) | ((uint64_t)useEncrypted << 19) | ((uint64_t)enableMultiLine << 18) | ((uint64_t)toIntegerScaler << 2) | ((uint64_t)useCompressed << 1) | (uint64_t)enableStaleness;	
	}
	if (useEncrypted) {
		uint64_t temp = 0;
		for (uint32_t i = 0; i < 15; i++) {
			uint64_t temp = ((uint64_t)i << 20);
			m_interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG5, temp);
			m_interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG6, ((uint64_t*)m_cstore->m_KEYS_dec)[2*i]);
			m_interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG7, ((uint64_t*)m_cstore->m_KEYS_dec)[2*i+1]);
		}
		temp = ((uint64_t)15 << 20);
		m_interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG5, temp);
		uint64_t* temp_ptr = (uint64_t*)m_cstore->m_ivec;
		m_interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG6, temp_ptr[0]);
		m_interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG7, temp_ptr[1]);
	}

	uint32_t index[NUM_FINSTANCES];
	
	double start = get_time();

	uint32_t epoch = 0;
	if (numEpochs/residualUpdatePeriod > 0) {
		for (epoch = 0; epoch+residualUpdatePeriod < numEpochs; epoch += residualUpdatePeriod) {
			// Do normal epochs
			for (uint32_t n = 0; n < NUM_FINSTANCES; n++) {
				m_interfaceFPGA->m_pALIMMIOService->mmioWrite32(CSR_NUM_LINES, n);
				m_interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG1, config[n][0]);
				m_interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG2, config[n][1]);
				m_interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG3, config[n][2]);
				uint64_t temp1 = ((uint64_t)residualUpdatePeriod << 32) | (config[n][3] & 0xFFFFFFFF);
				m_interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG4, temp1);
				m_interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG5, config[n][4]);
			}

			m_interfaceFPGA->startTransaction();
			m_interfaceFPGA->joinTransaction();

			memset(index, 0, NUM_FINSTANCES*sizeof(uint32_t));
			for (uint32_t e = 0; e < residualUpdatePeriod; e++) {
				for (uint32_t n = 0; n < numInstancesToUse; n++) {
					for (uint32_t m = 0; m < numMinibatchesToAssign[n]; m++) {
						for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
							float value = m_interfaceFPGA->readFromMemoryFloat('i', m_stepAddress[n]*m_numValuesPerLine+index[n]);
							m_interfaceFPGA->writeToMemoryFloat('i', 0, m_stepAddress[n]*m_numValuesPerLine+index[n]);
							x[j] -= value;
							index[n]++;
						}
						if (index[n]%m_numValuesPerLine > 0) {
							index[n] += (m_numValuesPerLine - index[n]%m_numValuesPerLine);
						}
					}
				}
				for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
					xHistoryLocal[(epoch+e)*m_cstore->m_numFeatures+j] = x[j]/numMinibatches;
				}
			}

			for (uint32_t n = 0; n < numInstancesToUse; n++) {
				for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
					m_interfaceFPGA->writeToMemoryFloat('i', x[j]/numMinibatches, m_stepAddress[n]*m_numValuesPerLine+j);
				}
			}

			// Do residual update
			if (epoch + residualUpdatePeriod < numEpochs) {
				for (uint32_t n = 0; n < NUM_FINSTANCES; n++) {
					m_interfaceFPGA->m_pALIMMIOService->mmioWrite32(CSR_NUM_LINES, n);
					m_interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG1, config[n][0]);
					m_interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG2, config[n][1]);
					m_interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG3, config[n][2]);
					uint64_t temp1 = ((uint64_t)1 << 32) | (config[n][3] & 0xFFFFFFFF);
					m_interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG4, temp1);
					uint64_t temp2 = ((uint64_t)1 << 24) | config[n][4];
					m_interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG5, temp2);
				}
				m_interfaceFPGA->startTransaction();
				m_interfaceFPGA->joinTransaction();
			}
		}
	}
	cout << "epoch: " << epoch << endl;

	if (epoch < numEpochs) {
		for (uint32_t n = 0; n < NUM_FINSTANCES; n++) {
			m_interfaceFPGA->m_pALIMMIOService->mmioWrite32(CSR_NUM_LINES, n);
			m_interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG1, config[n][0]);
			m_interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG2, config[n][1]);
			m_interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG3, config[n][2]);
			uint64_t temp = ((uint64_t)(numEpochs - epoch) << 32) | (config[n][3] & 0xFFFFFFFF);
			m_interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG4, temp);
			m_interfaceFPGA->m_pALIMMIOService->mmioWrite64(CSR_MY_CONFIG5, config[n][4]);
		}
		m_interfaceFPGA->startTransaction();
		m_interfaceFPGA->joinTransaction();
	}

	double end = get_time();
	cout << "Time for all epochs on the FPGA: " << end-start << endl;
	cout << "Time for one epoch on the FPGA: " << (end-start)/numEpochs << endl;
	double epoch_time = (end-start)/numEpochs;

	memset(index, 0, NUM_FINSTANCES*sizeof(uint32_t));
	if (doRealSCD == false) {
		if (epoch < numEpochs) {
			for (uint32_t e = epoch; e < numEpochs; e++) {
				for (uint32_t n = 0; n < numInstancesToUse; n++) {
					for (uint32_t m = 0; m < numMinibatchesToAssign[n]; m++) {
						for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
							float value = m_interfaceFPGA->readFromMemoryFloat('i', m_stepAddress[n]*m_numValuesPerLine+index[n]);
							x[j] -= value;
							index[n]++;
						}
						if (index[n]%m_numValuesPerLine > 0) {
							index[n] += (m_numValuesPerLine - index[n]%m_numValuesPerLine);
						}
					}
				}
				for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
					xHistoryLocal[e*m_cstore->m_numFeatures + j] = x[j]/numMinibatches;
				}
			}
		}
		for(uint32_t e = 0; e < numEpochs; e++) {
			float* xEnd = xHistoryLocal + e*m_cstore->m_numFeatures;
			if (xHistory != nullptr) {
				for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
					xHistory[e*m_cstore->m_numFeatures + j] = xEnd[j];
				}
			}
			else {
#ifdef PRINT_LOSS
				cout << Loss(type, xEnd, lambda, args) << endl;
#endif
			}
		}
	}
	else {
		uint32_t n = 0;
		for (uint32_t e = epoch; e < numEpochs; e++) {
			for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
				float value = m_interfaceFPGA->readFromMemoryFloat('i', m_stepAddress[n]*m_numValuesPerLine+index[n]);
				x[j] -= value;
				index[n]++;
			}
			if (index[n]%m_numValuesPerLine > 0) {
				index[n] += (m_numValuesPerLine - index[n]%m_numValuesPerLine);
			}
			if (xHistory != nullptr) {
				for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
					xHistory[e*m_cstore->m_numFeatures + j] = x[j];
				}
			}
			else {
#ifdef PRINT_LOSS
				cout << Loss(type, x, lambda, args) << endl;
#endif
			}
		}
	}

	free(xHistoryLocal);

	return epoch_time;
}
