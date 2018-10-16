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

#include <iostream>

#include "../src/FPGA_ColumnML.h"

using namespace std;

#define VALUE_TO_INT_SCALER 10

int main(int argc, char* argv[]) {
	cpu_set_t cpuset;
	pthread_t thread = pthread_self();
	pthread_attr_t attr;
	pthread_getattr_np(thread, &attr);
	CPU_ZERO(&cpuset);
	CPU_SET(0, &cpuset);
	pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset);

	char* pathToDataset;
	uint32_t numSamples = 0;
	uint32_t numFeatures = 0;
	uint32_t numEpochs = 10;
	uint32_t minibatchSize = 512;
	char* pathToTestDataset;
	char* pathPreTrainedModel;
	uint32_t numTestSamples = 0;
	if (!(argc == 6 || argc == 8 || argc == 9)) {
		cout << "Usage: ./app <pathToDataset> <numSamples> <numFeatures> <numEpochs> <minibatchSize> <pathToTestDataset> <numTestSamples> <pathPreTrainedModel>" << endl;
		return 0;
	}
	pathToDataset = argv[1];
	numSamples = atoi(argv[2]);
	numFeatures = atoi(argv[3]);
	numEpochs = atoi(argv[4]);
	minibatchSize = atoi(argv[5]);
	pathToTestDataset = argv[6];
	numTestSamples = atoi(argv[7]);
	pathPreTrainedModel = argv[8];

	uint32_t numMinibatchesAtATime = 1;
	float stepSize = 4;
	float lambda = 0.00001;

	FPGA_ColumnML* columnML = new FPGA_ColumnML(true);

	ModelType type;
	if ( strcmp(pathToDataset, "syn") == 0) {
		columnML->m_cstore->GenerateSyntheticData(numSamples, numFeatures, false, MinusOneToOne);
		type = logreg;
	}
	else {
		columnML->m_cstore->LoadRawData(pathToDataset, numSamples, numFeatures, true);
		columnML->m_cstore->NormalizeSamples(ZeroToOne, column);
		columnML->m_cstore->NormalizeLabels(ZeroToOne, true, 1);
		type = logreg;
	}
	columnML->m_cstore->PrintSamples(2);

	AdditionalArguments args;
	args.m_firstSample = 0;
	args.m_numSamples = columnML->m_cstore->m_numSamples;
	args.m_constantStepSize = false;

	columnML->m_cstore->EncryptSamples(minibatchSize, false);

	columnML->SCD(type, nullptr, numEpochs, minibatchSize, stepSize, lambda, 1, 10, false, false, VALUE_TO_INT_SCALER, &args);

	columnML->FPGA_SCD(
		type,
		false,
		nullptr,
		numEpochs,
		minibatchSize,
		stepSize,
		lambda,
		10,
		true,
		false,
		false,
		VALUE_TO_INT_SCALER,
		&args,
		4);

	return 0;
}