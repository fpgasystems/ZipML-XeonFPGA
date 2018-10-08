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

#include "../driver/iFPGA.h"
#include "ColumnML.h"

#define NUM_FINSTANCES 4

class FPGA_ColumnML : public ColumnML {
public:

	FPGA_ColumnML(bool getFPGA) {
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

	~FPGA_ColumnML() {
		if (m_gotFPGA) {
			delete m_interfaceFPGA;
		}
	}

	uint32_t FPGA_PrintMemory();
	double FPGA_SCD(
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
		uint32_t numInstancesToUse);

	void FPGA_PrintTimeout() {
		uint32_t numRequestedReads = m_interfaceFPGA->readFromMemory32('i', 0);
		uint32_t numReceivedReads = m_interfaceFPGA->readFromMemory32('i', 1);
		uint32_t residualNumReceivedReads = m_interfaceFPGA->readFromMemory32('i', 2);
		uint32_t labelsNumReceivedReads = m_interfaceFPGA->readFromMemory32('i', 3);
		uint32_t samplesNumReceivedReads = m_interfaceFPGA->readFromMemory32('i', 4);
		uint32_t residualNumWriteRequests = m_interfaceFPGA->readFromMemory32('i', 5);
		uint32_t stepNumWriteRequests = m_interfaceFPGA->readFromMemory32('i', 6);
		uint32_t numWriteResponses = m_interfaceFPGA->readFromMemory32('i', 7);
		uint32_t completedEpochs = m_interfaceFPGA->readFromMemory32('i', 8);
		uint32_t reorderFreeCount = m_interfaceFPGA->readFromMemory32('i', 9);
		uint32_t featureUpdateIndex = m_interfaceFPGA->readFromMemory32('i', 10);
		uint32_t writeBatchIndex = m_interfaceFPGA->readFromMemory32('i', 11);
		uint32_t decompressorOutFifoFreeCount = m_interfaceFPGA->readFromMemory32('i', 12);

		cout << "numRequestedReads: " << numRequestedReads << endl;
		cout << "numReceivedReads: " << numReceivedReads << endl;
		cout << "residualNumReceivedReads: " << residualNumReceivedReads << endl;
		cout << "labelsNumReceivedReads: " << labelsNumReceivedReads << endl;
		cout << "samplesNumReceivedReads: " << samplesNumReceivedReads << endl;
		cout << "residualNumWriteRequests: " << residualNumWriteRequests << endl;
		cout << "stepNumWriteRequests: " << stepNumWriteRequests << endl;
		cout << "numWriteResponses: " << numWriteResponses << endl;
		cout << "completedEpochs: " << completedEpochs << endl;
		cout << "reorderFreeCount: " << reorderFreeCount << endl;
		cout << "featureUpdateIndex: " << featureUpdateIndex << endl;
		cout << "writeBatchIndex: " << writeBatchIndex << endl;
		cout << "decompressorOutFifoFreeCount: " << decompressorOutFifoFreeCount << endl;
	}

private:
	uint32_t FPGA_CopyDataIntoMemory(
		uint32_t numMinibatches, 
		uint32_t minibatchSize, 
		uint32_t numMinibatchesToAssign[],
		uint32_t numEpochs, 
		bool useEncrypted);
	uint32_t FPGA_CopyCompressedDataIntoMemory(
		uint32_t numMinibatches,
		uint32_t minibatchSize,
		uint32_t numMinibatchesToAssign[],
		uint32_t numEpochs,
		bool useEncrypted);

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