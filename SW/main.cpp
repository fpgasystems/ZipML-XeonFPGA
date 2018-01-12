// Copyright (C) 2017 Kaan Kara - Systems Group, ETH Zurich

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

#include "zipml_sgd.h"

using namespace std;

#define VALUE_TO_INT_SCALER 0x00800000
#define NUM_VALUES_PER_LINE 16

int main(int argc, char* argv[]) {

	char* pathToDataset;
	if (argc != 2) {
		cout << "Usage: ./ZipML.exe <pathToDataset>" << endl;
		return 0;
	}
	else {
		pathToDataset = argv[1];
	}

	// Set parameters
	int stepSizeShifter = 9;
	int numEpochs = 2;
	int quantizationBits = 0;
	int numberOfIndices = numEpochs;

	// Instantiate
	zipml_sgd app(1, VALUE_TO_INT_SCALER, NUM_VALUES_PER_LINE);

	// Load data
	// app.load_raw_data(pathToDataset, 10, 2048);
	app.generate_synthetic_data(100, 370, 0);
	
	double start, end;

	// Do normalization
	// app.a_normalize(0, 'c');
	// app.b_normalize(0, 0, 0.0);

	app.print_samples(1);

	// Full precision linear regression in SW
	float x_history1[numEpochs*app.numFeatures];
	start = get_time();
	app.float_linreg_SGD( x_history1, numEpochs, 1.0/(1 << stepSizeShifter) );
	end = get_time();
	app.log_history('s', 0, quantizationBits, 1.0/(1 << stepSizeShifter), numEpochs, end-start, x_history1);

/*
	// Quantized linear regression in SW
	float x_history2[numEpochs*app.numFeatures];
	start = get_time();
	app.Qfixed_linreg_SGD( x_history2, numEpochs, stepSizeShifter, quantizationBits );
	end = get_time();
	app.log_history('s', 0, quantizationBits, 1.0/(1 << stepSizeShifter), numEpochs, end-start, x_history2);
*/

	// Full precision linear regression on FPGA
	float x1[numEpochs*app.numFeatures];
	app.numCacheLines = app.copy_data_into_FPGA_memory();
	start = get_time();
	app.floatFSGD( x1, numEpochs, 1.0/(1 << stepSizeShifter), 0, 0.0 );
	end = get_time();
	app.log_history('h', 0, quantizationBits, 1.0/(1 << stepSizeShifter), numEpochs, end-start, NULL);

/*
	// Quantized linear regression on FPGA
	float x2[numEpochs*app.numFeatures];
	app.numCacheLines = app.copy_data_into_FPGA_memory_after_quantization(quantizationBits, numberOfIndices, 0);
	start = get_time();
	app.qFSGD( x2, numEpochs, stepSizeShifter, quantizationBits, 0, 0.0);
	end = get_time();
	app.log_history('h', 0, quantizationBits, 1.0/(1 << stepSizeShifter), numEpochs, end-start, NULL);
*/
/*
	// Multi-class training for MNIST
	app.load_libsvm_data((char*)"../Datasets/mnist", 60000, 780);

	app.a_normalize(0, 'r');

	float* xs[10];

	app.numCacheLines = app.copy_data_into_FPGA_memory_after_quantization(quantizationBits, numberOfIndices, 0);
	start = get_time();
	for (int digit = 0; digit < 10; digit++) {
		xs[digit] = (float*)malloc(app.numFeatures*sizeof(float));
		app.qFSGD( xs[digit], numEpochs, stepSizeShifter, quantizationBits, 1, digit*value_to_integer_scaler);
	}
	end = get_time();
	cout << "Total training time: " << end-start << endl;

	app.load_libsvm_data((char*)"./Datasets/mnist.t", 10000, 780);

	app.a_normalize(0, 'r');

	app.multi_classification(xs, 10);

	for (int digit = 0; digit < 10; digit++) {
		free(xs[digit]);
	}
*/
	return 0;
}