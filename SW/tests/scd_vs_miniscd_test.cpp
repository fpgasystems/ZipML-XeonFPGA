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
	char raw_or_libsvm = 0;
	uint32_t numSamples;
	uint32_t numFeatures;
	uint32_t miniBatchSize = 512;
	if (argc != 6) {
		cout << "Usage: ./app <pathToDataset> <raw_or_libsvm> <numSamples> <numFeatures> <miniBatchSize>" << endl;
		return 0;
	}
	else {
		pathToDataset = argv[1];
		raw_or_libsvm = (char)atoi(argv[2]);
		numSamples = atoi(argv[3]);
		numFeatures = atoi(argv[4]);
		miniBatchSize = atoi(argv[5]);
	}
	uint32_t stepSizeShifter = 1;
	uint32_t numEpochs = 100;
	uint32_t numMinibatchesAtATime = 1;
	float lambda = 0.01;

	scd scd_app(0);

	if (raw_or_libsvm == 1) {
		scd_app.load_libsvm_data(pathToDataset, numSamples, numFeatures);

		scd_app.a_normalize(0, 'c', NULL, NULL);
		// scd_app.b_normalize(0, 0, 0.0);

		scd_app.print_samples(3);

		float* x_history = (float*)malloc(numEpochs*scd_app.numFeatures*sizeof(float));
		memset(x_history, 0, numEpochs*scd_app.numFeatures*sizeof(float));
		float initial_loss = scd_app.calculate_linreg_loss(x_history);

		scd_app.float_linreg_SCD(x_history, numEpochs, scd_app.numSamples, 1, numEpochs+1, 1.0/(1 << stepSizeShifter), 0, 0, VALUE_TO_INT_SCALER);
		ofstream ofs1 ("temp/SCD_loss.txt", ofstream::out);
		ofs1 << initial_loss << endl;
		for (uint32_t i = 0; i < numEpochs; i++) {
			ofs1 << scd_app.calculate_linreg_loss(((float*)x_history) + i*scd_app.numFeatures) << endl;
		}
		ofs1.close();

		scd_app.float_linreg_SCD(x_history, numEpochs, miniBatchSize, 1, numEpochs+1, 1.0/(1 << stepSizeShifter), 0, 0, VALUE_TO_INT_SCALER);
		ofstream ofs2 ("temp/miniSCD_no_loss.txt", ofstream::out);
		ofs2 << initial_loss << endl;
		for (uint32_t i = 0; i < numEpochs; i++) {
			ofs2 << scd_app.calculate_linreg_loss(((float*)x_history) + i*scd_app.numFeatures) << endl;
		}
		ofs2.close();

		scd_app.float_linreg_SCD(x_history, numEpochs, miniBatchSize, 1, 100, 1.0/(1 << stepSizeShifter), 0, 0, VALUE_TO_INT_SCALER);
		ofstream ofs3 ("temp/miniSCD_100_loss.txt", ofstream::out);
		ofs3 << initial_loss << endl;
		for (uint32_t i = 0; i < numEpochs; i++) {
			ofs3 << scd_app.calculate_linreg_loss(((float*)x_history) + i*scd_app.numFeatures) << endl;
		}
		ofs3.close();

		scd_app.float_linreg_SCD(x_history, numEpochs, miniBatchSize, 1, 10, 1.0/(1 << stepSizeShifter), 0, 0, VALUE_TO_INT_SCALER);
		ofstream ofs4 ("temp/miniSCD_10_loss.txt", ofstream::out);
		ofs4 << initial_loss << endl;
		for (uint32_t i = 0; i < numEpochs; i++) {
			ofs4 << scd_app.calculate_linreg_loss(((float*)x_history) + i*scd_app.numFeatures) << endl;
		}
		ofs4.close();

		scd_app.float_linreg_SCD(x_history, numEpochs, miniBatchSize, 1, 5, 1.0/(1 << stepSizeShifter), 0, 0, VALUE_TO_INT_SCALER);
		ofstream ofs5 ("temp/miniSCD_5_loss.txt", ofstream::out);
		ofs5 << initial_loss << endl;
		for (uint32_t i = 0; i < numEpochs; i++) {
			ofs5 << scd_app.calculate_linreg_loss(((float*)x_history) + i*scd_app.numFeatures) << endl;
		}
		ofs5.close();

		free(x_history);
	}
	else {
		scd_app.load_raw_data(pathToDataset, numSamples, numFeatures, 1);

		float a_min[scd_app.numFeatures];
		float a_range[scd_app.numFeatures];
		scd_app.a_normalize(0, 'c', a_min, a_range);
		uint32_t numPositive = scd_app.b_normalize(0, 0, 0.0);
		cout << "numPositive: " << numPositive << " out of " << scd_app.numSamples << endl;

		uint32_t originalNumSamples = scd_app.numSamples;
		scd_app.numSamples = scd_app.numSamples*0.8;
		uint32_t testNumSamples = originalNumSamples - scd_app.numSamples;
		cout << "Train numSamples: " << scd_app.numSamples << endl;
		cout << "Test numSamples: " << testNumSamples << endl; 

		scd_app.print_samples(3);

		float* x_history = (float*)malloc(numEpochs*scd_app.numFeatures*sizeof(float));
		memset(x_history, 0, numEpochs*scd_app.numFeatures*sizeof(float));
		float initial_loss = scd_app.calculate_logreg_loss(x_history, lambda, NULL);


		scd_app.float_logreg_SCD(x_history, numEpochs, scd_app.numSamples, numEpochs+1, 1.0/(1 << stepSizeShifter), lambda, 0, 0, VALUE_TO_INT_SCALER);
		ofstream ofs1 ("temp/SCD_loss.txt", ofstream::out);
		ofs1 << initial_loss << endl;
		for (uint32_t i = 0; i < numEpochs; i++) {
			ofs1 << scd_app.calculate_logreg_loss(((float*)x_history) + i*scd_app.numFeatures, lambda, NULL) << "\t";
			ofs1 << scd_app.calculate_logreg_accuracy(((float*)x_history) + i*scd_app.numFeatures, 0, scd_app.numSamples) << "\t";
			ofs1 << scd_app.calculate_logreg_accuracy(((float*)x_history) + i*scd_app.numFeatures, scd_app.numSamples, testNumSamples) << endl;
		}
		ofs1.close();

		scd_app.float_logreg_SCD(x_history, numEpochs, miniBatchSize, numEpochs+1, 1.0/(1 << stepSizeShifter), lambda, 0, 0, VALUE_TO_INT_SCALER);
		ofstream ofs2 ("temp/miniSCD_no_loss.txt", ofstream::out);
		ofs2 << initial_loss << endl;
		for (uint32_t i = 0; i < numEpochs; i++) {
			ofs2 << scd_app.calculate_logreg_loss(((float*)x_history) + i*scd_app.numFeatures, lambda, NULL) << "\t";
			ofs2 << scd_app.calculate_logreg_accuracy(((float*)x_history) + i*scd_app.numFeatures, 0, scd_app.numSamples) << "\t";
			ofs2 << scd_app.calculate_logreg_accuracy(((float*)x_history) + i*scd_app.numFeatures, scd_app.numSamples, testNumSamples) << endl;
		}
		ofs2.close();

		scd_app.float_logreg_SCD(x_history, numEpochs, miniBatchSize, 20, 1.0/(1 << stepSizeShifter), lambda, 0, 0, VALUE_TO_INT_SCALER);
		ofstream ofs3 ("temp/miniSCD_20_loss.txt", ofstream::out);
		ofs3 << initial_loss << endl;
		for (uint32_t i = 0; i < numEpochs; i++) {
			ofs3 << scd_app.calculate_logreg_loss(((float*)x_history) + i*scd_app.numFeatures, lambda, NULL) << "\t";
			ofs3 << scd_app.calculate_logreg_accuracy(((float*)x_history) + i*scd_app.numFeatures, 0, scd_app.numSamples) << "\t";
			ofs3 << scd_app.calculate_logreg_accuracy(((float*)x_history) + i*scd_app.numFeatures, scd_app.numSamples, testNumSamples) << endl;
		}
		ofs3.close();

		scd_app.float_logreg_SCD(x_history, numEpochs, miniBatchSize, 10, 1.0/(1 << stepSizeShifter), lambda, 0, 0, VALUE_TO_INT_SCALER);
		ofstream ofs4 ("temp/miniSCD_10_loss.txt", ofstream::out);
		ofs4 << initial_loss << endl;
		for (uint32_t i = 0; i < numEpochs; i++) {
			ofs4 << scd_app.calculate_logreg_loss(((float*)x_history) + i*scd_app.numFeatures, lambda, NULL) << "\t";
			ofs4 << scd_app.calculate_logreg_accuracy(((float*)x_history) + i*scd_app.numFeatures, 0, scd_app.numSamples) << "\t";
			ofs4 << scd_app.calculate_logreg_accuracy(((float*)x_history) + i*scd_app.numFeatures, scd_app.numSamples, testNumSamples) << endl;
		}
		ofs4.close();

		memset(x_history, 0, numEpochs*scd_app.numFeatures*sizeof(float));
		scd_app.float_logreg_SCD(x_history, numEpochs, miniBatchSize, 5, 1.0/(1 << stepSizeShifter), lambda, 0, 0, VALUE_TO_INT_SCALER);
		ofstream ofs5 ("temp/miniSCD_5_loss.txt", ofstream::out);
		ofs5 << initial_loss << endl;
		for (uint32_t i = 0; i < numEpochs; i++) {
			ofs5 << scd_app.calculate_logreg_loss(((float*)x_history) + i*scd_app.numFeatures, lambda, NULL) << "\t";
			ofs5 << scd_app.calculate_logreg_accuracy(((float*)x_history) + i*scd_app.numFeatures, 0, scd_app.numSamples) << "\t";
			ofs5 << scd_app.calculate_logreg_accuracy(((float*)x_history) + i*scd_app.numFeatures, scd_app.numSamples, testNumSamples) << endl;
		}
		ofs5.close();
	}

	return 0;
}
