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

#include <stdio.h>
#include <stdint.h>
#include <pthread.h>

#include "../src/scd.h"

using namespace std;

#define NUM_VALUES_PER_LINE 16
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
	char* pathToTestDataset;
	uint32_t numSamples;
	uint32_t numTestSamples;
	uint32_t numFeatures;
	uint32_t miniBatchSize = 512;
	uint32_t numInstances = 1;
	uint32_t useEncryption = 0;
	uint32_t useCompression = 0;
	if (argc != 10) {
		cout << "Usage: ./app <pathToDataset> <pathToTestDataset> <numSamples> <numTestSamples> <numFeatures> <miniBatchSize> <f_instances> <encrypt> <compress>" << endl;
		return 0;
	}
	else {
		pathToDataset = argv[1];
		pathToTestDataset = argv[2];
		numSamples = atoi(argv[3]);
		numTestSamples = atoi(argv[4]);
		numFeatures = atoi(argv[5]);
		miniBatchSize = atoi(argv[6]);
		numInstances = atoi(argv[7]);
		useEncryption = atoi(argv[8]);
		useCompression = atoi(argv[9]);
	}

	uint32_t SGD_stepSizeShifter = 2;
	uint32_t SCD_stepSizeShifter = 2;
	uint32_t numEpochs = 3;
	uint32_t numMinibatchesAtATime = 1;
	float lambda = 0.0005;

	scd scd_app(0);
	
	scd_app.load_raw_data(pathToDataset, numSamples, numFeatures, 1);

	float a_min[scd_app.numFeatures];
	float a_range[scd_app.numFeatures];
	scd_app.a_normalize(0, 'c', a_min, a_range);
	uint32_t numPositive = scd_app.b_normalize(0, 1, 1.0);
	cout << "numPositive: " << numPositive << " out of " << scd_app.numSamples << endl;

	scd_app.print_samples(3);


	uint32_t numModels = 10;
	float xM[numModels*scd_app.numFeatures];
	scd_app.gentleAdaBoost(xM, numModels, numEpochs, miniBatchSize, 1.0/(1 << SGD_stepSizeShifter), lambda);

	// scd_app.float_logreg_SGD(x_history, numEpochs, miniBatchSize,  1.0/(1 << SGD_stepSizeShifter), lambda);

	// scd_app.float_logreg_SCD(NULL, numEpochs, numSamples, 100, 1.0/(1 << SCD_stepSizeShifter), lambda, 0, 0, VALUE_TO_INT_SCALER);

	// scd_app.float_logreg_SCD(NULL, numEpochs, miniBatchSize, 100, 1.0/(1 << SCD_stepSizeShifter), lambda, 0, 0, VALUE_TO_INT_SCALER);

	// scd_app.load_raw_data(pathToTestDataset, numTestSamples, numFeatures, 0);
	// scd_app.print_samples(3);
	// scd_app.write_logreg_predictions(((float*)x_history) + (numEpochs-1)*scd_app.numFeatures, a_min, a_range);
}
