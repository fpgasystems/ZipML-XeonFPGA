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
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include <iostream>
#include <string>
#include <atomic>
#include <vector>
#include <memory>

using namespace std;

#include "opae_svc_wrapper.h"
#include "csr_mgr.h"

using namespace opae::fpga::types;
using namespace opae::fpga::bbb::mpf::types;

#include "ColumnML.h"

class Instruction {
public:
	static const uint32_t NUM_WORDS = 16;
	static const uint32_t NUM_BYTES = NUM_WORDS*4;
	uint32_t m_data[NUM_WORDS];

	Instruction() {
		for (unsigned i = 0; i < 16; i++) {
			m_data[i] = 0;
		}
		// Maintain indexes by default
		m_data[0] = 0xFFFFFFFF;
		m_data[1] = 0xFFFFFFFF;
		m_data[2] = 0xFFFFFFFF;
	}

	void ResetUpdateIndex() {
		m_data[0] = 0;
	}

	void ResetPartitionIndex() {
		m_data[1] = 0;
	}

	void ResetEpochIndex() {
		m_data[2] = 0;
	}

	void IncrementUpdateIndex() {
		m_data[0] = 0xFFFFFFF;
	}

	void IncrementPartitionIndex() {
		m_data[1] = 0xFFFFFFF;
	}

	void IncrementEpochIndex() {
		m_data[2] = 0xFFFFFFF;
	}

	void MakeNonBlocking() {
		m_data[15] |= (1 << 8);
	}

	void JustLoad1(
		uint32_t loadOffsetDRAM,
		uint32_t loadLengthDRAM,
		uint32_t storeOffsetMemory1,
		uint32_t offsetByUpdate,
		uint32_t offsetByPartition)
	{
		m_data[15] = 0;
		m_data[3] = loadOffsetDRAM;
		m_data[4] = loadLengthDRAM;
		m_data[5] = (loadLengthDRAM << 16) | (storeOffsetMemory1 & 0xFFFF);
		m_data[6] = 0;
		m_data[7] = 0;
		m_data[10] = offsetByUpdate;
		m_data[11] = offsetByPartition;
	}

	void JustLoad2(
		uint32_t loadOffsetDRAM,
		uint32_t loadLengthDRAM,
		uint32_t storeOffsetMemory2,
		uint32_t offsetByUpdate,
		uint32_t offsetByPartition)
	{
		m_data[15] = 0;
		m_data[3] = loadOffsetDRAM;
		m_data[4] = loadLengthDRAM;
		m_data[5] = 0;
		m_data[6] = (loadLengthDRAM << 16) | (storeOffsetMemory2 & 0xFFFF);
		m_data[7] = 0;
		m_data[10] = offsetByUpdate;
		m_data[11] = offsetByPartition;
	}

	void LoadToPrefetch(
		uint32_t loadOffsetDRAM,
		uint32_t loadLengthDRAM,
		uint32_t offsetByUpdate,
		uint32_t offsetByPartition)
	{
		m_data[15] = 0;
		m_data[3] = loadOffsetDRAM;
		m_data[4] = loadLengthDRAM;
		m_data[5] = 0;
		m_data[6] = 0;
		m_data[7] = (loadLengthDRAM << 16) | (loadLengthDRAM & 0xFFFF);
		m_data[10] = offsetByUpdate;
		m_data[11] = offsetByPartition;
	}

	void WriteBack(
		uint32_t storeOffsetDRAM,
		uint32_t storeLengthDRAM,
		uint32_t loadOffsetMemory,
		uint32_t whichMemory,
		uint32_t whichDRAMBuffer,
		uint32_t offsetByPartition,
		uint32_t offsetByEpoch)
	{
		m_data[15] = 4; // opcode
		m_data[3] = storeOffsetDRAM;
		m_data[4] = storeLengthDRAM;
		m_data[5] = loadOffsetMemory;
		m_data[6] = (whichDRAMBuffer << 1) | whichMemory;
		m_data[10] = offsetByEpoch;
		m_data[11] = offsetByPartition;
	}

	void Dot(
		uint32_t readLength,
		bool usePrefetchFIFO,
		bool useModelForwardFIFO,
		uint32_t loadOffsetMemory1,
		uint32_t loadOffsetMemory2)
	{
		uint32_t tempUsePrefetchFIFO = (uint32_t)usePrefetchFIFO;
		uint32_t tempUseModelForwardFIFO = (uint32_t)useModelForwardFIFO;
		m_data[15] = 1; // opcode
		m_data[3] = (tempUseModelForwardFIFO << 17) | (tempUsePrefetchFIFO << 16) | readLength & 0xFFFF;
		m_data[4] = (loadOffsetMemory2 << 16) | (loadOffsetMemory1 & 0xFFFF);
	}

	void Modify(
		uint32_t loadOffsetMemory2,
		uint32_t storeOffsetMemory2,
		uint32_t type,
		uint32_t algo,
		float stepSize,
		float lambda)
	{
		m_data[15] = 2; // opcode
		m_data[3] = (storeOffsetMemory2 << 16) | (loadOffsetMemory2 & 0xFFFF);
		m_data[4] = (algo << 2) | (type & 0x3);
		uint32_t* temp = (uint32_t*)&stepSize;
		m_data[5] = *temp;
		uint32_t* temp2 = (uint32_t*)&lambda;
		m_data[6] = *temp2;
	}

	void Update(
		uint32_t loadStoreOffsetMemory1,
		uint32_t loadLength,
		bool modelForward)
	{
		m_data[15] = 3; // opcode
		m_data[3] = (loadLength << 16) | (loadStoreOffsetMemory1 & 0xFFFF);
		m_data[4] = (uint32_t)modelForward;
	}

/*
	void SubtractDot(
		uint32_t loadOffsetDRAM,
		uint32_t loadLengthDRAM,
		uint32_t loadOffsetMemory1,
		uint32_t loadOffsetMemory2,
		uint32_t offsetByUpdate,
		uint32_t offsetByPartition)
	{
		m_data[15] = 0; // opcode
		m_data[2] = loadOffsetDRAM;
		m_data[3] = loadLengthDRAM;
		m_data[4] = (loadOffsetMemory2 << 16) | (loadOffsetMemory1 & 0xFFFF);
		m_data[5] = (loadLengthDRAM << 16) | (loadLengthDRAM & 0xFFFF);
		m_data[9] = loadLengthDRAM & 0xFFFF;
		m_data[10] = offsetByUpdate;
		m_data[12] = offsetByPartition;
	}

	void DotUpdate(
		uint32_t loadOffsetDRAM,
		uint32_t loadLengthDRAM,
		uint32_t loadOffsetMemory1,
		uint32_t storeOffsetMemory1,
		uint32_t offsetByUpdate,
		uint32_t offsetByPartition)
	{
		m_data[15] = 0; // opcode
		m_data[2] = loadOffsetDRAM;
		m_data[3] = loadLengthDRAM;
		m_data[4] = loadOffsetMemory1 & 0xFFFF;
		m_data[5] = loadLengthDRAM & 0xFFFF;
		m_data[6] = storeOffsetMemory1 & 0xFFFF;
		m_data[7] = loadLengthDRAM & 0xFFFF;
		m_data[8] = loadLengthDRAM & 0xFFFF;
		m_data[9] = loadLengthDRAM & 0xFFFF;
		m_data[10] = offsetByUpdate;
		m_data[12] = offsetByPartition;
	}

	void SubtractDotUpdate(
		uint32_t loadOffsetDRAM,
		uint32_t loadLengthDRAM,
		uint32_t loadOffsetMemory1,
		uint32_t loadOffsetMemory2,
		uint32_t storeOffsetMemory1,
		uint32_t offsetByUpdate,
		uint32_t offsetByPartition)
	{
		m_data[15] = 0; // opcode
		m_data[2] = loadOffsetDRAM;
		m_data[3] = loadLengthDRAM;
		m_data[4] = (loadOffsetMemory2 << 16) | (loadOffsetMemory1 & 0xFFFF);
		m_data[5] = (loadLengthDRAM << 16) | (loadLengthDRAM & 0xFFFF);
		m_data[6] = storeOffsetMemory1 & 0xFFFF;
		m_data[7] = loadLengthDRAM & 0xFFFF;
		m_data[8] = loadLengthDRAM & 0xFFFF;
		m_data[9] = loadLengthDRAM & 0xFFFF;
		m_data[10] = offsetByUpdate;
		m_data[12] = offsetByPartition;
	}
*/
	void Jump0(
		uint32_t predicate,
		uint32_t nextPCFalse,
		uint32_t nextPCTrue)
	{
		m_data[15] = 10; // opcode
		m_data[12] = predicate;
		m_data[14] = nextPCFalse;
		m_data[13] = nextPCTrue;
	}

	void Jump1(
		uint32_t predicate,
		uint32_t nextPCFalse,
		uint32_t nextPCTrue)
	{
		m_data[15] = 11; // opcode
		m_data[12] = predicate;
		m_data[14] = nextPCFalse;
		m_data[13] = nextPCTrue;
	}

	void Jump2(
		uint32_t predicate,
		uint32_t nextPCFalse,
		uint32_t nextPCTrue)
	{
		m_data[15] = 12; // opcode
		m_data[12] = predicate;
		m_data[14] = nextPCFalse;
		m_data[13] = nextPCTrue;
	}

	void Copy(volatile uint32_t* data) {
		for (uint32_t i = 0; i < NUM_WORDS; i++) {
			data[i] = m_data[i];
		}
	}
};

struct MemoryChunk {
	uint32_t m_offsetInCL;
	uint32_t m_lenghtInCL;
};

enum MemoryFormat {FormatSGD, FormatSCD};

class FPGA_ColumnML : public ColumnML {
public:
	uint32_t m_numSamplesInCL;
	uint32_t m_numFeaturesInCL;
	uint32_t m_alignedNumSamples;
	uint32_t m_alignedNumFeatures;
	uint32_t m_partitionSize;
	uint32_t m_partitionSizeInCL;
	uint32_t m_alignedPartitionSize;
	uint32_t m_numPartitions;

	shared_buffer::ptr_t m_handle;
	volatile float* m_memory = nullptr;
	volatile float* m_model = nullptr;
	volatile float* m_residual = nullptr;
	volatile float* m_labels = nullptr;
	volatile float* m_samples = nullptr;
	
	MemoryChunk m_modelChunk;
	MemoryChunk m_residualChunk;
	MemoryChunk m_labelChunk;
	MemoryChunk m_samplesChunk;

	MemoryFormat m_currentMemoryFormat;

	OPAE_SVC_WRAPPER* m_fpga;
	CSR_MGR* m_csrs;


	FPGA_ColumnML(const char* accel_uuid) {
		m_fpga = new OPAE_SVC_WRAPPER(accel_uuid);
		assert(m_fpga->isOk());
		m_csrs = new CSR_MGR(*m_fpga);
		m_handle = NULL;
	};

	~FPGA_ColumnML() {
		cout << "m_handle->release()" << endl;
		if (m_handle != NULL) {
			m_handle->release();
			m_handle = NULL;
		}

		cout << "delete m_csrs" << endl;
		delete m_csrs;
		cout << "delete m_fpga" << endl;
		delete m_fpga;
	}

	uint32_t CreateMemoryLayout(MemoryFormat format, uint32_t partitionSize) {
		m_currentMemoryFormat = format;

		m_numSamplesInCL = (m_cstore->m_numSamples >> 4) + ((m_cstore->m_numSamples&0xF) > 0);
		m_numFeaturesInCL = (m_cstore->m_numFeatures >> 4) + ((m_cstore->m_numFeatures&0xF) > 0);
		m_partitionSize = (partitionSize > m_cstore->m_numSamples) ? m_cstore->m_numSamples : partitionSize;
		m_partitionSizeInCL = (m_partitionSize >> 4) + ((m_partitionSize & 0xF) > 0);
		m_alignedNumSamples = m_numSamplesInCL*16;
		m_alignedNumFeatures = m_numFeaturesInCL*16;
		m_alignedPartitionSize = m_partitionSizeInCL*16;
		m_numPartitions = m_numSamplesInCL/m_partitionSizeInCL + (m_numSamplesInCL%m_partitionSizeInCL > 0);

		std::cout << "m_numSamplesInCL: " << m_numSamplesInCL << std::endl;
		std::cout << "m_numFeaturesInCL: " << m_numFeaturesInCL << std::endl;
		std::cout << "m_partitionSize: " << m_partitionSize << std::endl;
		std::cout << "m_partitionSizeInCL: " << m_partitionSizeInCL << std::endl;
		std::cout << "m_alignedNumSamples: " << m_alignedNumSamples << std::endl;
		std::cout << "m_alignedNumFeatures: " << m_alignedNumFeatures << std::endl;
		std::cout << "m_alignedPartitionSize: " << m_alignedPartitionSize << std::endl;
		std::cout << "m_numPartitions: " << m_numPartitions << std::endl;

		uint32_t countCL = 0;

		if (format == FormatSGD) {
			// Model
			m_modelChunk.m_offsetInCL = countCL;
			countCL += m_numFeaturesInCL;
			m_modelChunk.m_lenghtInCL = countCL - m_modelChunk.m_offsetInCL;

			// Labels
			m_labelChunk.m_offsetInCL = countCL;
			countCL += m_numSamplesInCL;
			m_labelChunk.m_lenghtInCL = countCL - m_labelChunk.m_offsetInCL;

			// Samples
			m_samplesChunk.m_offsetInCL = countCL;
			countCL += m_cstore->m_numSamples*m_numFeaturesInCL;
			m_samplesChunk.m_lenghtInCL = countCL - m_samplesChunk.m_offsetInCL;

			// No residual used
			m_residualChunk.m_offsetInCL = 0;
			m_residualChunk.m_lenghtInCL = 0;
		}
		else if (format == FormatSCD) {
			// Residual
			m_residualChunk.m_offsetInCL = countCL;
			countCL += m_numSamplesInCL;
			m_residualChunk.m_lenghtInCL = countCL - m_residualChunk.m_offsetInCL;

			// Labels
			m_labelChunk.m_offsetInCL = countCL;
			countCL += m_numSamplesInCL;
			m_labelChunk.m_lenghtInCL = countCL - m_labelChunk.m_offsetInCL;

			// Samples
			m_samplesChunk.m_offsetInCL = countCL;
			countCL += m_cstore->m_numFeatures*m_numSamplesInCL;
			m_samplesChunk.m_lenghtInCL = countCL - m_samplesChunk.m_offsetInCL;

			// Model
			m_modelChunk.m_offsetInCL = countCL;
			countCL += m_numPartitions*m_numFeaturesInCL;
			m_modelChunk.m_lenghtInCL = countCL - m_modelChunk.m_offsetInCL;
		}

		if (m_handle != NULL) {
			m_handle->release();
			m_handle = NULL;
		}
		m_handle = m_fpga->allocBuffer(countCL*64);
		m_memory = reinterpret_cast<volatile float*>(m_handle->c_type());
		assert(NULL != m_memory);

		memset((void*)m_memory, 0, 16*countCL*sizeof(float));

		m_model = m_memory + m_modelChunk.m_offsetInCL*16;
		m_residual = m_memory + m_residualChunk.m_offsetInCL*16;
		m_labels = m_memory + m_labelChunk.m_offsetInCL*16;
		m_samples = m_memory + m_samplesChunk.m_offsetInCL*16;

		if (format == FormatSGD) {
			for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
				m_model[j] = 0;
			}
			for (uint32_t i = 0; i < m_cstore->m_numSamples; i++) {
				m_labels[i] = m_cstore->m_labels[i];
				for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
					m_samples[i*m_alignedNumFeatures + j] = m_cstore->m_samples[j][i];
				}
			}
		}
		else if (format == FormatSCD) {
			for (uint32_t i = 0; i < m_cstore->m_numSamples; i++) {
				m_residual[i] = 0;
				m_labels[i] = m_cstore->m_labels[i];
			}
			for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
				for (uint32_t i = 0; i < m_cstore->m_numSamples; i++) {
					m_samples[j*m_alignedNumSamples + i] = m_cstore->m_samples[j][i];
				}
			}
			for (uint32_t p = 0; p < m_numPartitions; p++) {
				for (uint32_t j = 0; j < m_alignedNumFeatures; j++) {
					m_model[p*m_alignedNumFeatures + j] = 0;
				}
			}
		}

		return countCL;
	}

	void CopyDataToFPGAMemory(MemoryFormat format, uint32_t partitionSize) {
		uint32_t countCL = CreateMemoryLayout(format, partitionSize);

		cout << "CopyDataToFPGAMemory END" << endl;
	}

	void fSGD(
		ModelType type, 
		float* xHistory, 
		uint32_t numEpochs, 
		uint32_t minibatchSize, 
		float stepSize, 
		float lambda, 
		AdditionalArguments* args)
	{
		if (m_memory == nullptr) {
			return;
		}

		const uint32_t numInstructions = 14;
		Instruction inst[numInstructions];

		uint32_t modelOffsetInMemory1 = 0;
		uint32_t labelOffsetInMemory2 = 0;

		// Load model
		inst[0].JustLoad1(
			m_modelChunk.m_offsetInCL,
			m_modelChunk.m_lenghtInCL,
			modelOffsetInMemory1,
			0,
			0);
		inst[0].ResetUpdateIndex();
		inst[0].ResetPartitionIndex();
		inst[0].ResetEpochIndex();

		// Load labels in partition
		inst[1].JustLoad2(
			m_labelChunk.m_offsetInCL,
			m_partitionSizeInCL,
			labelOffsetInMemory2,
			0,
			m_partitionSizeInCL);

		inst[2].LoadToPrefetch(
			m_samplesChunk.m_offsetInCL,
			m_numFeaturesInCL,
			m_numFeaturesInCL,
			m_numFeaturesInCL*m_partitionSize);
		inst[2].MakeNonBlocking();

		inst[3].Dot(
			m_numFeaturesInCL,
			true,
			false,
			modelOffsetInMemory1,
			0);
		
		// Innermost loop
		inst[4].Modify(
			labelOffsetInMemory2,
			0xFFFF,
			type,
			0,
			stepSize,
			lambda);
		inst[4].IncrementUpdateIndex();
		inst[4].MakeNonBlocking();

		inst[5].Update(
			modelOffsetInMemory1,
			m_numFeaturesInCL,
			true);
		inst[5].MakeNonBlocking();

		inst[6].LoadToPrefetch(
			m_samplesChunk.m_offsetInCL,
			m_numFeaturesInCL,
			m_numFeaturesInCL,
			m_numFeaturesInCL*m_partitionSize);
		inst[6].MakeNonBlocking();

		inst[7].Dot(
			m_numFeaturesInCL,
			true,
			true,
			0,
			0);

		// End of samples
		inst[8].Jump0(m_partitionSize-1, 4, 9);

		inst[9].Modify(
			labelOffsetInMemory2,
			0xFFFF,
			type,
			0,
			stepSize,
			lambda);

		inst[10].Update(
			modelOffsetInMemory1,
			m_numFeaturesInCL,
			false);
		inst[10].ResetUpdateIndex();
		inst[10].IncrementPartitionIndex();

		inst[11].Jump1(m_numPartitions, 1, 12);

		inst[12].WriteBack(
			1,
			m_numFeaturesInCL,
			modelOffsetInMemory1,
			0,
			0,
			0,
			m_numFeaturesInCL);
		inst[12].IncrementEpochIndex();

		// End of epochs
		inst[13].Jump2(numEpochs, 1, 0xFFFFFFFF);
		inst[13].ResetUpdateIndex();
		inst[13].ResetPartitionIndex();

		std::vector<Instruction> instructions;
		for (uint32_t i = 0; i < numInstructions; i++) {
			instructions.push_back(inst[i]);
		}

		// Copy program to FPGA memory
		auto programMemoryHandle = m_fpga->allocBuffer(instructions.size()*64);
		auto programMemory = reinterpret_cast<volatile uint32_t*>(programMemoryHandle->c_type());
		uint32_t k = 0;
		for (Instruction i: instructions) {
			i.Copy(programMemory + k*Instruction::NUM_WORDS);
			k++;
		}

		auto outputHandle = m_fpga->allocBuffer(numEpochs*m_numFeaturesInCL*64);
		auto output = reinterpret_cast<volatile float*>(outputHandle->c_type());
		assert(NULL != output);


		m_csrs->writeCSR(0, intptr_t(m_memory));
		m_csrs->writeCSR(1, intptr_t(output));
		m_csrs->writeCSR(2, intptr_t(programMemory));
		m_csrs->writeCSR(3, (uint64_t)instructions.size());

		// Spin, waiting for the value in memory to change to something non-zero.
		struct timespec pause;
		// Longer when simulating
		pause.tv_sec = (m_fpga->hwIsSimulated() ? 1 : 0);
		pause.tv_nsec = 2500000;


		output[0] = 0;
		while (0 == output[0]) {
			nanosleep(&pause, NULL);
		};


		xHistory = (float*)(output + 16);
		for (uint32_t e = 0; e < numEpochs; e++) {
			float loss = Loss(type, xHistory + e*m_alignedNumFeatures, lambda, args);
			std::cout << "loss " << e << ": " << loss << std::endl;
		}


		// Reads CSRs to get some statistics
		cout	<< "# List length: " << m_csrs->readCSR(0) << endl
				<< "# Linked list data entries read: " << m_csrs->readCSR(1) << endl;

		cout	<< "#" << endl
				<< "# AFU frequency: " << m_csrs->getAFUMHz() << " MHz"
				<< (m_fpga->hwIsSimulated() ? " [simulated]" : "")
				<< endl;

		// MPF VTP (virtual to physical) statistics
		mpf_handle::ptr_t mpf = m_fpga->mpf;
		if (mpfVtpIsAvailable(*mpf))
		{
			mpf_vtp_stats vtp_stats;
			mpfVtpGetStats(*mpf, &vtp_stats);

			cout << "#" << endl;
			if (vtp_stats.numFailedTranslations)
			{
				cout << "# VTP failed translating VA: 0x" << hex << uint64_t(vtp_stats.ptWalkLastVAddr) << dec << endl;
			}
			cout	<< "# VTP PT walk cycles: " << vtp_stats.numPTWalkBusyCycles << endl
					<< "# VTP L2 4KB hit / miss: " << vtp_stats.numTLBHits4KB << " / "
					<< vtp_stats.numTLBMisses4KB << endl
					<< "# VTP L2 2MB hit / miss: " << vtp_stats.numTLBHits2MB << " / "
					<< vtp_stats.numTLBMisses2MB << endl;
		}
	}

/*
	void fSCD(
		ModelType type, 
		float* xHistory, 
		uint32_t numEpochs, 
		uint32_t partitionSize, 
		float stepSize, 
		float lambda, 
		AdditionalArguments* args)
	{
		cout << "fSCD ---------------------------------------" << endl;

		if (m_memory == nullptr) {
			return;
		}

		uint32_t rest = args->m_numSamples - m_numPartitions*m_partitionSizeInCL*16;
		std::cout << "rest: " << rest << std::endl;

		float scaledStepSize = stepSize/partitionSize;
		float scaledLambda = stepSize*lambda;

		const uint32_t numInstructions = 14;
		Instruction inst[numInstructions];

		uint32_t residualOffsetInMemory1 = 0;
		uint32_t labelOffsetInMemory2 = 0;
		uint32_t modelOffsetInMemory2 = labelOffsetInMemory2 + m_partitionSizeInCL;



		inst[0].ResetUpdateIndex();
		inst[0].ResetPartitionIndex();
		inst[0].ResetEpochIndex();

		inst[1].JustLoad1(
			m_residualChunk.m_offsetInCL,
			m_partitionSizeInCL,
			residualOffsetInMemory1,
			0,
			m_partitionSizeInCL);

		inst[2].JustLoad2(
			m_labelChunk.m_offsetInCL,
			m_partitionSizeInCL,
			labelOffsetInMemory2,
			0,
			m_partitionSizeInCL);

		inst[3].JustLoad2(
			m_modelChunk.m_offsetInCL,
			m_numFeaturesInCL,
			modelOffsetInMemory2,
			0,
			m_numFeaturesInCL);

		inst[4].SubtractDot(
			m_samplesChunk.m_offsetInCL,
			m_partitionSizeInCL,
			residualOffsetInMemory1,
			labelOffsetInMemory2,
			m_numSamplesInCL,
			m_partitionSizeInCL);

		inst[5].DotModify(
			modelOffsetInMemory2,
			modelOffsetInMemory2,
			type,
			1,
			scaledStepSize,
			scaledLambda);
		inst[5].IncrementUpdateIndex();

		inst[6].SubtractDotUpdate(
			m_samplesChunk.m_offsetInCL,
			m_partitionSizeInCL,
			residualOffsetInMemory1,
			labelOffsetInMemory2,
			residualOffsetInMemory1,
			m_numSamplesInCL,
			m_partitionSizeInCL);

		inst[7].Jump0(m_cstore->m_numFeatures-1, 5, 8);

		inst[8].DotModify(
			modelOffsetInMemory2,
			modelOffsetInMemory2,
			type,
			1,
			scaledStepSize,
			scaledLambda);

		inst[9].Update(
			m_samplesChunk.m_offsetInCL,
			m_partitionSizeInCL,
			residualOffsetInMemory1,
			residualOffsetInMemory1,
			m_numSamplesInCL,
			m_partitionSizeInCL);
		
		inst[10].WriteBack(
			m_residualChunk.m_offsetInCL,
			residualOffsetInMemory1,
			m_partitionSizeInCL,
			m_partitionSizeInCL,
			0,
			0,
			1);

		inst[11].WriteBack(
			m_modelChunk.m_offsetInCL,
			modelOffsetInMemory2,
			m_numFeaturesInCL,
			m_numFeaturesInCL,
			0,
			1,
			1);
		inst[11].IncrementPartitionIndex();

		inst[12].Jump11(m_numPartitions, 1, 13);
		inst[12].ResetUpdateIndex();

		inst[13].Jump1(numEpochs-1, 1, 0xFFFFFFFF);
		inst[13].IncrementEpochIndex();
		inst[13].ResetPartitionIndex();

		std::vector<Instruction> instructions;
		for (uint32_t i = 0; i < numInstructions; i++) {
			instructions.push_back(inst[i]);
		}

	}
*/

	void TestProgram() {

		const uint32_t numInstructions = 5;
		Instruction inst[numInstructions];

		inst[0].JustLoad1(
			m_modelChunk.m_offsetInCL,
			m_modelChunk.m_lenghtInCL,
			0,
			0,
			0);
		inst[0].ResetUpdateIndex();
		inst[0].ResetPartitionIndex();
		inst[0].ResetEpochIndex();

		inst[1].LoadToPrefetch(
			m_samplesChunk.m_offsetInCL,
			m_numFeaturesInCL,
			0,
			0);
		inst[1].MakeNonBlocking();

		inst[2].Dot(
			m_numFeaturesInCL,
			true,
			false,
			0,
			0);

		inst[3].Modify(
			0,
			0xFFFF,
			0,
			0,
			0.1,
			0.1);

		inst[4].Jump0(0, 0, 0xFFFFFFFF);

		std::vector<Instruction> instructions;
		for (uint32_t i = 0; i < numInstructions; i++) {
			instructions.push_back(inst[i]);
		}

		// Copy program to FPGA memory
		auto programMemoryHandle = m_fpga->allocBuffer(instructions.size()*64);
		auto programMemory = reinterpret_cast<volatile uint32_t*>(programMemoryHandle->c_type());
		uint32_t k = 0;
		for (Instruction i: instructions) {
			i.Copy(programMemory + k*Instruction::NUM_WORDS);
			k++;
		}

		auto outputHandle = m_fpga->allocBuffer(getpagesize());
		auto output = reinterpret_cast<volatile float*>(outputHandle->c_type());
		assert(NULL != output);


		m_csrs->writeCSR(0, intptr_t(m_memory));
		m_csrs->writeCSR(1, intptr_t(output));
		m_csrs->writeCSR(2, intptr_t(programMemory));
		m_csrs->writeCSR(3, (uint64_t)instructions.size());

		// Spin, waiting for the value in memory to change to something non-zero.
		struct timespec pause;
		// Longer when simulating
		pause.tv_sec = (m_fpga->hwIsSimulated() ? 1 : 0);
		pause.tv_nsec = 2500000;


		output[0] = 0;
		while (0 == output[0]) {
			nanosleep(&pause, NULL);
		};

		for (unsigned i = 0; i < m_alignedNumSamples; i++) {
			cout << "output[" << i << "]: " << output[16+i] << endl;
		}

		// Reads CSRs to get some statistics
		cout	<< "# List length: " << m_csrs->readCSR(0) << endl
				<< "# Linked list data entries read: " << m_csrs->readCSR(1) << endl;

		cout	<< "#" << endl
				<< "# AFU frequency: " << m_csrs->getAFUMHz() << " MHz"
				<< (m_fpga->hwIsSimulated() ? " [simulated]" : "")
				<< endl;

		// MPF VTP (virtual to physical) statistics
		mpf_handle::ptr_t mpf = m_fpga->mpf;
		if (mpfVtpIsAvailable(*mpf))
		{
			mpf_vtp_stats vtp_stats;
			mpfVtpGetStats(*mpf, &vtp_stats);

			cout << "#" << endl;
			if (vtp_stats.numFailedTranslations)
			{
				cout << "# VTP failed translating VA: 0x" << hex << uint64_t(vtp_stats.ptWalkLastVAddr) << dec << endl;
			}
			cout	<< "# VTP PT walk cycles: " << vtp_stats.numPTWalkBusyCycles << endl
					<< "# VTP L2 4KB hit / miss: " << vtp_stats.numTLBHits4KB << " / "
					<< vtp_stats.numTLBMisses4KB << endl
					<< "# VTP L2 2MB hit / miss: " << vtp_stats.numTLBHits2MB << " / "
					<< vtp_stats.numTLBMisses2MB << endl;
		}
	}
};