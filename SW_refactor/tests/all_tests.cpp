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

#include "../src/ColumnML.h"

using namespace std;

#define VALUE_TO_INT_SCALER 10


void StepSizeSweepSGD(ColumnML* obj, ModelType type, uint32_t numEpochs, uint32_t minibatchSize, float lambda, AdditionalArguments args);
void StepSizeSweepSCD(ColumnML* obj, ModelType type, uint32_t numEpochs, uint32_t minibatchSize, float lambda, AdditionalArguments args);

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
	if (argc != 6) {
		cout << "Usage: ./app <pathToDataset> <numSamples> <numFeatures> <numEpochs> <minibatchSize>" << endl;
		return 0;
	}
	pathToDataset = argv[1];
	numSamples = atoi(argv[2]);
	numFeatures = atoi(argv[3]);
	numEpochs = atoi(argv[4]);
	minibatchSize = atoi(argv[5]);

	uint32_t numMinibatchesAtATime = 1;
	
	float stepSize = 0.1;
	float lambda = 0.001;

	ColumnML* columnML = new ColumnML(false);

	columnML->m_cstore->LoadRawData(pathToDataset, numSamples, numFeatures, true);
	columnML->m_cstore->NormalizeSamples(ZeroToOne, column);
	columnML->m_cstore->NormalizeLabels(ZeroToOne, true, 1);
	columnML->m_cstore->PrintSamples(2);
	
	ModelType type = logreg;

	AdditionalArguments args;
	args.m_firstSample = 0;
	args.m_numSamples = columnML->m_cstore->m_numSamples;



	// StepSizeSweepSGD(columnML, type, numEpochs, minibatchSize, lambda, args);

	// StepSizeSweepSCD(columnML, type, numEpochs, numSamples, lambda, args);

	// columnML->SGD(type, nullptr, numEpochs, minibatchSize, stepSize, lambda, &args);

	// columnML->AVXrowwise_SGD(type, nullptr, numEpochs, 1, stepSize, lambda, &args);

	// columnML->AVX_SGD(type, nullptr, numEpochs, minibatchSize, stepSize, lambda, &args);

	// columnML->SCD(type, nullptr, numEpochs, numSamples - (numSamples%8), 10, lambda, 1, 10000, false, false, VALUE_TO_INT_SCALER, &args);

	// columnML->AVX_SCD(type, nullptr, numEpochs, numSamples - (numSamples%8), 10, lambda, 10000, false, false, VALUE_TO_INT_SCALER, &args);

	columnML->AVXmulti_SCD(type, true, nullptr, numEpochs, minibatchSize, 10, lambda, 10000, false, false, VALUE_TO_INT_SCALER, &args, 14);

	// columnML->m_cstore->CompressSamples(minibatchSize, VALUE_TO_INT_SCALER);
	// columnML->SCD(type, nullptr, numEpochs, minibatchSize, stepSize, lambda, 1, 100, false, true, VALUE_TO_INT_SCALER, &args);

	// columnML->m_cstore->EncryptSamples(minibatchSize, false);
	// columnML->SCD(type, nullptr, numEpochs, minibatchSize, stepSize, lambda, 1, 100, true, false, VALUE_TO_INT_SCALER, &args);

	// columnML->m_cstore->EncryptSamples(minibatchSize, true);
	// columnML->SCD(type, nullptr, numEpochs, minibatchSize, stepSize, lambda, 1, 100, true, true, VALUE_TO_INT_SCALER, &args);


	// columnML->AVXrowwise_SGD(type, nullptr, numEpochs, 1, 0.001, lambda, &args);

	// columnML->AVX_SGD(type, nullptr, numEpochs, 8, 0.01, lambda, &args);

	// columnML->AVX_SGD(type, nullptr, numEpochs, 64, 0.1, lambda, &args);

	// columnML->AVX_SGD(type, nullptr, numEpochs, 512, 0.1, lambda, &args);





	// float* xHistory_SCD = new float[numEpochs*columnML->m_cstore->m_numFeatures];
	// columnML->AVXmulti_SCD(type, true, xHistory_SCD, numEpochs, minibatchSize, 10, lambda, 10000, false, false, VALUE_TO_INT_SCALER, &args, 14);

	// float* xHistory_SGD = new float[numEpochs*columnML->m_cstore->m_numFeatures];
	// columnML->AVX_SGD(type, xHistory_SGD, numEpochs, 64, 0.1, lambda, &args);


	// args.m_firstSample = columnML->m_cstore->m_numSamples * 0.7;
	// args.m_numSamples = columnML->m_cstore->m_numSamples - args.m_firstSample;

	// cout << "SCD validation loss: " << endl;
	// for (uint32_t e = 0; e < numEpochs; e++) {
	// 	cout << columnML->Loss(type, xHistory_SCD + e*columnML->m_cstore->m_numFeatures, lambda, &args) << endl;
	// }

	// cout << "SGD validation loss: " << endl;
	// for (uint32_t e = 0; e < numEpochs; e++) {
	// 	cout << columnML->Loss(type, xHistory_SGD + e*columnML->m_cstore->m_numFeatures, lambda, &args) << endl;
	// }



	
	delete columnML;

	return 0;
}

void StepSizeSweepSGD(ColumnML* obj, ModelType type, uint32_t numEpochs, uint32_t minibatchSize, float lambda, AdditionalArguments args) {
	float stepSize = 1;
	for (uint32_t n = 0; n < 5; n++) {
		stepSize = stepSize/((float)10);
		cout << "stepSize: " << stepSize << endl;
		if (minibatchSize == 1) {
			obj->AVXrowwise_SGD(type, nullptr, numEpochs, minibatchSize, stepSize, lambda, &args);
		}
		else {
			obj->AVX_SGD(type, nullptr, numEpochs, minibatchSize, stepSize, lambda, &args);
		}
	}
}

void StepSizeSweepSCD(ColumnML* obj, ModelType type, uint32_t numEpochs, uint32_t minibatchSize, float lambda, AdditionalArguments args) {
	float stepSizes[5] = {1, 10, 20, 30, 40};
	for (uint32_t n = 0; n < 5; n++) {
		cout << "stepSize: " << stepSizes[n] << endl;
		obj->AVX_SCD(type, nullptr, numEpochs, minibatchSize, stepSizes[n], lambda, 10000, false, false, VALUE_TO_INT_SCALER, &args);
	}
}