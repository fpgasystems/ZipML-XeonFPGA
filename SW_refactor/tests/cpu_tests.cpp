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

void SweepP(ColumnML* obj, ModelType type, uint32_t numEpochs, float lambda, AdditionalArguments args);
void MultiCoreSCDPerformance(ColumnML* obj, ModelType type, uint32_t numEpochs, float lambda, AdditionalArguments args);
void Convergence(ColumnML* obj, uint32_t numEpochs);
void StepSizeSweepSGD(ColumnML* obj, ModelType type, uint32_t numEpochs, uint32_t minibatchSize, float lambda, AdditionalArguments args);
void StepSizeSweepSCD(ColumnML* obj, ModelType type, uint32_t numEpochs, uint32_t minibatchSize, float lambda, AdditionalArguments args);
void SGDvsSCDPerformance(ColumnML* obj, ModelType type, uint32_t numEpochs, float lambda, AdditionalArguments args);
void PredictionSCD(
	ColumnML* obj,
	uint32_t numEpochs,
	uint32_t minibatchSize,
	float stepSize,
	float lambda,
	AdditionalArguments args,
	char* pathToTestDataset,
	uint32_t numTestSamples,
	uint32_t numFeatures,
	bool WritePredictions);

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

	ColumnML* columnML = new ColumnML();

	ModelType type;
	if ( strcmp(pathToDataset, "syn") == 0) {
		columnML->m_cstore->GenerateSyntheticData(numSamples, numFeatures, false, MinusOneToOne);
		type = linreg;
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

	// // All tests

	// SweepP(columnML, type, numEpochs, lambda, args);

	// MultiCoreSCDPerformance(columnML, type, numEpochs, lambda, args);

	// Convergence(columnML, numEpochs);

	// StepSizeSweepSGD(columnML, type, numEpochs, minibatchSize, lambda, args);

	// StepSizeSweepSCD(columnML, type, numEpochs, numSamples, lambda, args);

	// SGDvsSCDPerformance(columnML, type, numEpochs, lambda, args);
	
	// PredictionSCD(
	// 	columnML,
	// 	numEpochs,
	// 	minibatchSize,
	// 	stepSize,
	// 	lambda,
	// 	args,
	// 	pathToTestDataset,
	// 	numTestSamples,
	// 	numFeatures,
	// 	true);

	// PredictionSCD(
	// 	columnML,
	// 	numEpochs,
	// 	minibatchSize,
	// 	stepSize,
	// 	lambda,
	// 	args,
	// 	pathToTestDataset,
	// 	numTestSamples,
	// 	numFeatures,
	// 	false);

	delete columnML;

	return 0;
}

#ifdef AVX2
void SweepP(ColumnML* obj, ModelType type, uint32_t numEpochs, float lambda, AdditionalArguments args) {
	obj->AVXmulti_SCD(type, false, nullptr, numEpochs, 16384, 4, lambda, 1, false, false, VALUE_TO_INT_SCALER, &args, 14);
	obj->AVXmulti_SCD(type, false, nullptr, numEpochs, 16384, 4, lambda, 2, false, false, VALUE_TO_INT_SCALER, &args, 14);
	obj->AVXmulti_SCD(type, false, nullptr, numEpochs, 16384, 4, lambda, 4, false, false, VALUE_TO_INT_SCALER, &args, 14);
	obj->AVXmulti_SCD(type, false, nullptr, numEpochs, 16384, 4, lambda, 8, false, false, VALUE_TO_INT_SCALER, &args, 14);
	obj->AVXmulti_SCD(type, false, nullptr, numEpochs, 16384, 4, lambda, 16, false, false, VALUE_TO_INT_SCALER, &args, 14);

	obj->m_cstore->CompressSamples(16384, VALUE_TO_INT_SCALER);
	obj->m_cstore->EncryptSamples(16384, true);

	obj->AVXmulti_SCD(type, false, nullptr, numEpochs, 16384, 4, lambda, 1, true, true, VALUE_TO_INT_SCALER, &args, 14);
	obj->AVXmulti_SCD(type, false, nullptr, numEpochs, 16384, 4, lambda, 2, true, true, VALUE_TO_INT_SCALER, &args, 14);
	obj->AVXmulti_SCD(type, false, nullptr, numEpochs, 16384, 4, lambda, 4, true, true, VALUE_TO_INT_SCALER, &args, 14);
	obj->AVXmulti_SCD(type, false, nullptr, numEpochs, 16384, 4, lambda, 8, true, true, VALUE_TO_INT_SCALER, &args, 14);
	obj->AVXmulti_SCD(type, false, nullptr, numEpochs, 16384, 4, lambda, 16, true, true, VALUE_TO_INT_SCALER, &args, 14);
}

void MultiCoreSCDPerformance(ColumnML* obj, ModelType type, uint32_t numEpochs, float lambda, AdditionalArguments args) {
	obj->AVXmulti_SCD(type, false, nullptr, numEpochs, 16384, 4, lambda, 10, false, false, VALUE_TO_INT_SCALER, &args, 1);
	obj->AVXmulti_SCD(type, false, nullptr, numEpochs, 16384, 4, lambda, 10, false, false, VALUE_TO_INT_SCALER, &args, 2);
	obj->AVXmulti_SCD(type, false, nullptr, numEpochs, 16384, 4, lambda, 10, false, false, VALUE_TO_INT_SCALER, &args, 4);
	obj->AVXmulti_SCD(type, false, nullptr, numEpochs, 16384, 4, lambda, 10, false, false, VALUE_TO_INT_SCALER, &args, 8);
	obj->AVXmulti_SCD(type, false, nullptr, numEpochs, 16384, 4, lambda, 10, false, false, VALUE_TO_INT_SCALER, &args, 14);

	obj->AVXmulti_SCD(type, true, nullptr, numEpochs, 16384, 4, lambda, 10, false, false, VALUE_TO_INT_SCALER, &args, 1);
	obj->AVXmulti_SCD(type, true, nullptr, numEpochs, 16384, 4, lambda, 10, false, false, VALUE_TO_INT_SCALER, &args, 2);
	obj->AVXmulti_SCD(type, true, nullptr, numEpochs, 16384, 4, lambda, 10, false, false, VALUE_TO_INT_SCALER, &args, 4);
	obj->AVXmulti_SCD(type, true, nullptr, numEpochs, 16384, 4, lambda, 10, false, false, VALUE_TO_INT_SCALER, &args, 8);
	obj->AVXmulti_SCD(type, true, nullptr, numEpochs, 16384, 4, lambda, 10, false, false, VALUE_TO_INT_SCALER, &args, 14);
}

void Convergence(ColumnML* obj, uint32_t numEpochs) {

	ModelType type = logreg;

	AdditionalArguments args;
	args.m_firstSample = 0;
	args.m_numSamples = obj->m_cstore->m_numSamples;
	args.m_constantStepSize = false;

	float lambda = 0.00001;

	float* xHistory_rowSGD = new float[numEpochs*obj->m_cstore->m_numFeatures];
	obj->AVXrowwise_SGD(type, xHistory_rowSGD, numEpochs, 1, 0.01, lambda, &args);

	float* xHistory_SGD8 = new float[numEpochs*obj->m_cstore->m_numFeatures];
	obj->AVX_SGD(type, xHistory_SGD8, numEpochs, 8, 0.1, lambda, &args);

	float* xHistory_SGD64 = new float[numEpochs*obj->m_cstore->m_numFeatures];
	obj->AVX_SGD(type, xHistory_SGD64, numEpochs, 64, 0.5, lambda, &args);

	float* xHistory_SGD512 = new float[numEpochs*obj->m_cstore->m_numFeatures];
	obj->AVX_SGD(type, xHistory_SGD512, numEpochs, 512, 0.9, lambda, &args);

	float* xHistory_SCD = new float[numEpochs*obj->m_cstore->m_numFeatures];
	obj->AVXmulti_SCD(type, true, xHistory_SCD, numEpochs, 16384, 128, lambda, 10000, false, false, VALUE_TO_INT_SCALER, &args, 14);

	float* xHistory_pSCD = new float[numEpochs*obj->m_cstore->m_numFeatures];
	obj->AVXmulti_SCD(type, false, xHistory_pSCD, numEpochs, 16384, 128, lambda, 10, false, false, VALUE_TO_INT_SCALER, &args, 14);

	cout << "xHistory_rowSGD: " << endl;
	for (uint32_t e = 0; e < numEpochs; e+=10) {
		cout << obj->Loss(type, xHistory_rowSGD + e*obj->m_cstore->m_numFeatures, lambda, &args) << endl;
	}
	cout << "xHistory_SGD8 loss: " << endl;
	for (uint32_t e = 0; e < numEpochs; e+=10) {
		cout << obj->Loss(type, xHistory_SGD8 + e*obj->m_cstore->m_numFeatures, lambda, &args) << endl;
	}
	cout << "xHistory_SGD64 loss: " << endl;
	for (uint32_t e = 0; e < numEpochs; e+=10) {
		cout << obj->Loss(type, xHistory_SGD64 + e*obj->m_cstore->m_numFeatures, lambda, &args) << endl;
	}
	cout << "xHistory_SGD512 loss: " << endl;
	for (uint32_t e = 0; e < numEpochs; e+=10) {
		cout << obj->Loss(type, xHistory_SGD512 + e*obj->m_cstore->m_numFeatures, lambda, &args) << endl;
	}
	cout << "xHistory_SCD loss: " << endl;
	for (uint32_t e = 0; e < numEpochs; e+=10) {
		cout << obj->Loss(type, xHistory_SCD + e*obj->m_cstore->m_numFeatures, lambda, &args) << endl;
	}
	cout << "xHistory_pSCD loss: " << endl;
	for (uint32_t e = 0; e < numEpochs; e+=10) {
		cout << obj->Loss(type, xHistory_pSCD + e*obj->m_cstore->m_numFeatures, lambda, &args) << endl;
	}
}

void StepSizeSweepSGD(ColumnML* obj, ModelType type, uint32_t numEpochs, uint32_t minibatchSize, float lambda, AdditionalArguments args) {
	float stepSize = 1;
	for (uint32_t n = 0; n < 5; n++) {
		stepSize = stepSize/((float)10);
		cout << "stepSize: " << stepSize << endl;
		
		obj->AVX_SGD(type, nullptr, numEpochs, minibatchSize, stepSize, lambda, &args);
	}
}

void StepSizeSweepSCD(ColumnML* obj, ModelType type, uint32_t numEpochs, uint32_t minibatchSize, float lambda, AdditionalArguments args) {
	float stepSizes[5] = {1, 4, 10, 20, 30};
	for (uint32_t n = 0; n < 5; n++) {
		cout << "stepSize: " << stepSizes[n] << endl;
		obj->AVX_SCD(type, nullptr, numEpochs, minibatchSize, stepSizes[n], lambda, 10000, false, false, VALUE_TO_INT_SCALER, &args);
	}
}

void SGDvsSCDPerformance(ColumnML* obj, ModelType type, uint32_t numEpochs, float lambda, AdditionalArguments args) {

	obj->AVXrowwise_SGD(type, nullptr, numEpochs, 1, 0.001, lambda, &args);

	obj->AVX_SGD(type, nullptr, numEpochs, 1, 0.01, lambda, &args);

	obj->AVX_SGD(type, nullptr, numEpochs, 8, 0.1, lambda, &args);

	obj->AVX_SGD(type, nullptr, numEpochs, 64, 0.5, lambda, &args);

	obj->AVX_SGD(type, nullptr, numEpochs, 512, 0.9, lambda, &args);

	obj->AVX_SCD(type, nullptr, numEpochs, obj->m_cstore->m_numSamples - (obj->m_cstore->m_numSamples%8), 4, lambda, 10000, false, false, VALUE_TO_INT_SCALER, &args);

	obj->AVX_SCD(type, nullptr, numEpochs, 16384, 4, lambda, 10000, false, false, VALUE_TO_INT_SCALER, &args);
}

void PredictionSCD(
	ColumnML* obj,
	uint32_t numEpochs,
	uint32_t minibatchSize,
	float stepSize,
	float lambda,
	AdditionalArguments args,
	char* pathToTestDataset,
	uint32_t numTestSamples,
	uint32_t numFeatures,
	bool WritePredictions)
{
	ModelType type = logreg;

	if (!WritePredictions) {
		args.m_firstSample = 0;
		args.m_numSamples = obj->m_cstore->m_numSamples * 0.8;
	}

	float* xHistory_SCD = new float[numEpochs*obj->m_cstore->m_numFeatures];
	obj->AVXmulti_SCD(type, true, xHistory_SCD, numEpochs, minibatchSize, stepSize, lambda, 10000, false, false, VALUE_TO_INT_SCALER, &args, 14);

	float* xHistory_pSCD_Pinf = new float[numEpochs*obj->m_cstore->m_numFeatures];
	obj->AVXmulti_SCD(type, false, xHistory_pSCD_Pinf, numEpochs, minibatchSize, stepSize, lambda, 10000, false, false, VALUE_TO_INT_SCALER, &args, 14);

	float* xHistory_pSCD_P100 = new float[numEpochs*obj->m_cstore->m_numFeatures];
	obj->AVXmulti_SCD(type, false, xHistory_pSCD_P100, numEpochs, minibatchSize, stepSize, lambda, 100, false, false, VALUE_TO_INT_SCALER, &args, 14);

	float* xHistory_pSCD_P10 = new float[numEpochs*obj->m_cstore->m_numFeatures];
	obj->AVXmulti_SCD(type, false, xHistory_pSCD_P10, numEpochs, minibatchSize, stepSize, lambda, 10, false, false, VALUE_TO_INT_SCALER, &args, 14);

	cout << "SCD loss: " << endl;
	for (uint32_t e = 0; e < numEpochs; e+=10) {
		cout << obj->Loss(type, xHistory_SCD + e*obj->m_cstore->m_numFeatures, lambda, &args) << endl;
	}
	cout << "pSCD_Pinf loss: " << endl;
	for (uint32_t e = 0; e < numEpochs; e+=10) {
		cout << obj->Loss(type, xHistory_pSCD_Pinf + e*obj->m_cstore->m_numFeatures, lambda, &args) << endl;
	}
	cout << "pSCD_P100 loss: " << endl;
	for (uint32_t e = 0; e < numEpochs; e+=10) {
		cout << obj->Loss(type, xHistory_pSCD_P100 + e*obj->m_cstore->m_numFeatures, lambda, &args) << endl;
	}
	cout << "pSCD_P10 loss: " << endl;
	for (uint32_t e = 0; e < numEpochs; e+=10) {
		cout << obj->Loss(type, xHistory_pSCD_P10 + e*obj->m_cstore->m_numFeatures, lambda, &args) << endl;
	}

	if (WritePredictions) {
		obj->m_cstore->LoadRawData(pathToTestDataset, numTestSamples, numFeatures, false);
		obj->m_cstore->PrintSamples(2);
		obj->WriteLogregPredictions((char*)"SCD_predictions.txt", xHistory_SCD + (numEpochs-1)*obj->m_cstore->m_numFeatures);
		obj->WriteLogregPredictions((char*)"pSCDinf_predictions.txt", xHistory_pSCD_Pinf + (numEpochs-1)*obj->m_cstore->m_numFeatures);
		obj->WriteLogregPredictions((char*)"pSCD100_predictions.txt", xHistory_pSCD_P100 + (numEpochs-1)*obj->m_cstore->m_numFeatures);
		obj->WriteLogregPredictions((char*)"pSCD10_predictions.txt", xHistory_pSCD_P10 + (numEpochs-1)*obj->m_cstore->m_numFeatures);
	}
	else {
		args.m_firstSample = obj->m_cstore->m_numSamples * 0.8;
		args.m_numSamples = obj->m_cstore->m_numSamples - args.m_firstSample;

		uint32_t SCD_corrects = obj->Accuracy(type, xHistory_SCD + (numEpochs-1)*obj->m_cstore->m_numFeatures, &args);
		uint32_t pSCDinf_corrects = obj->Accuracy(type, xHistory_pSCD_Pinf + (numEpochs-1)*obj->m_cstore->m_numFeatures, &args);
		uint32_t pSCD100_corrects = obj->Accuracy(type, xHistory_pSCD_P100 + (numEpochs-1)*obj->m_cstore->m_numFeatures, &args);
		uint32_t pSCD10_corrects = obj->Accuracy(type, xHistory_pSCD_P10 + (numEpochs-1)*obj->m_cstore->m_numFeatures, &args);

		cout << "SCD accuracy: " << endl;
		cout << SCD_corrects << " corrects out of " << args.m_numSamples << ". " << (float)SCD_corrects/(float)args.m_numSamples << "%" << endl;
		cout << "pSCDinf accuracy: " << endl;
		cout << pSCDinf_corrects << " corrects out of " << args.m_numSamples << ". " << (float)pSCDinf_corrects/(float)args.m_numSamples << "%" << endl;
		cout << "pSCD100 accuracy: " << endl;
		cout << pSCD100_corrects << " corrects out of " << args.m_numSamples << ". " << (float)pSCD100_corrects/(float)args.m_numSamples << "%" << endl;
		cout << "pSCD10 accuracy: " << endl;
		cout << pSCD10_corrects << " corrects out of " << args.m_numSamples << ". " << (float)pSCD10_corrects/(float)args.m_numSamples << "%" << endl;
	}
}
#endif