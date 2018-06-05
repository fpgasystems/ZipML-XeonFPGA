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
	uint32_t numSamples;
	uint32_t numFeatures;
	uint32_t minibatchSize = 512;
	uint32_t numMinibatchesAtATime = 1;
	uint32_t numInstances = 1;
	uint32_t useEncryption = 0;
	uint32_t useCompression = 0;
	char doRealSCD = 0;
	if (argc != 6) {
		cout << "Usage: ./app <pathToDataset> <numSamples> <numFeatures> <miniBatchSize> <numMinibatchesAtATime>" << endl;
		return 0;
	}
	else {
		pathToDataset = argv[1];
		numSamples = atoi(argv[2]);
		numFeatures = atoi(argv[3]);
		minibatchSize = atoi(argv[4]);
		numMinibatchesAtATime = atoi(argv[5]);
	}

	uint32_t stepSizeShifter = 12;
	uint32_t numEpochs = 20;
	uint32_t residualUpdatePeriod = 100;
	float lambda = 0.1;

	scd scd_app(0);

	scd_app.load_raw_data(pathToDataset, numSamples, numFeatures, 1);

	scd_app.print_samples(2);

	float x_history[numEpochs*scd_app.numFeatures];
	scd_app.float_logreg_blockwise_SGD(x_history, numEpochs, minibatchSize, numMinibatchesAtATime, 1.0/(1 << stepSizeShifter), lambda);
	cout << "-----------------------------------------" << endl;
	cout << "Loss: " << endl;
	for (uint32_t e = 0; e < numEpochs; e++) {
		cout << scd_app.calculate_logreg_loss(x_history + e*scd_app.numFeatures, lambda, NULL) << endl;
	}

	// for (uint32_t p = 0; p < 7; p++) {	
	// 	scd_app.float_logreg_blockwise_SGD(x_history, numEpochs, minibatchSize, (1 << p), 1.0/(1 << stepSizeShifter), lambda);
	// 	cout << "-----------------------------------------" << endl;
	// 	cout << "p: " << p << endl;
	// 	cout << "Loss: " << endl;
	// 	for (uint32_t e = 0; e < numEpochs; e++) {
	// 		cout << scd_app.calculate_logreg_loss(x_history + e*scd_app.numFeatures, lambda, NULL) << endl;
	// 	}
	// }

	return 0;
}
