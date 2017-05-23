#include <stdio.h>
#include <stdint.h>
#include <pthread.h>

#include "zipml_sgd.h"

using namespace std;

int main(int argc, char* argv[]) {
	double start, end;

	// Set parameters
	int stepSizeShifter = 12;
	int numberOfIterations = 10;
	int quantizationBits = 1;
	uint32_t value_to_integer_scaler = 0x00800000;
	int numberOfIndices = numberOfIterations;

	// Instantiate
	zipml_sgd app(1, value_to_integer_scaler);

	// Load data
	//app.load_tsv_data((char*)"./Datasets/synthetic/synth[m=10000][d=100][sigma=1][sparsity_fraction=0.1][mu_a=0][sigma_a=1]_sparse.tsv", 10000, 100);
	//app.load_tsv_data((char*)"./Datasets/synthetic/synth[m=10000][d=1000][sigma=1][sparsity_fraction=0.1][mu_a=0][sigma_a=1]_sparse.tsv", 10000, 1000);
	//app.load_libsvm_data((char*)"./Datasets/cadata", 20640, 8);
	//app.load_libsvm_data((char*)"./Datasets/YearPredictionMSD", 463715, 90);
	//app.load_libsvm_data((char*)"./Datasets/gisette_scale", 6000, 5000);
	//app.load_libsvm_data((char*)"./Datasets/epsilon_normalized", 10000, 2000);
	//app.load_libsvm_data((char*)"./Datasets/mnist", 60000, 780);
	//app.load_libsvm_data((char*)"./Datasets/mnist", 100, 780);
/*
	// Do normalization
	app.a_normalize(0, 'r');
	app.b_normalize(0, 1, 7.0);
*/
/*
	// Print first 10 tuples
	cout << "a: " << endl;
	for (int i = 0; i < 10; i++) {
		for (int j = 0; j < app.numFeatures; j++) {
			cout << app.a[app.numFeatures*i + j] << " ";
		}
		cout << endl << endl;
	}
	cout << "b: " << endl;
	for (int i = 0; i < 10; i++) {
		cout << app.b[i] << endl;
	}
*/
/*
	// Full precision linear regression in SW
	float x_history1[numberOfIterations*app.numFeatures];
	start = get_time();
	app.float_linreg_SGD( x_history1, numberOfIterations, 1.0/(1 << stepSizeShifter) );
	end = get_time();
	app.log_history('s', 0, quantizationBits, 1.0/(1 << stepSizeShifter), numberOfIterations, end-start, x_history1);
*/
/*
	// Quantized linear regression in SW
	float x_history2[numberOfIterations*app.numFeatures];
	start = get_time();
	app.Qfixed_linreg_SGD( x_history2, numberOfIterations, stepSizeShifter, quantizationBits );
	end = get_time();
	app.log_history('s', 0, quantizationBits, 1.0/(1 << stepSizeShifter), numberOfIterations, end-start, x_history2);
*/
/*
	// Full precision linear regression on FPGA
	float x1[numberOfIterations*app.numFeatures];
	app.numCacheLines = app.copy_data_into_FPGA_memory();
	start = get_time();
	app.floatFSGD( x1, numberOfIterations, 1.0/(1 << stepSizeShifter), 0, 0.0 );
	end = get_time();
	app.log_history('h', 0, quantizationBits, 1.0/(1 << stepSizeShifter), numberOfIterations, end-start, NULL);
*/
/*
	// Quantized linear regression on FPGA
	float x2[numberOfIterations*app.numFeatures];
	app.numCacheLines = app.copy_data_into_FPGA_memory_after_quantization(quantizationBits, numberOfIndices, 0);
	start = get_time();
	app.qFSGD( x2, numberOfIterations, stepSizeShifter, quantizationBits, 0, 0.0);
	end = get_time();
	app.log_history('h', 0, quantizationBits, 1.0/(1 << stepSizeShifter), numberOfIterations, end-start, NULL);
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
		app.qFSGD( xs[digit], numberOfIterations, stepSizeShifter, quantizationBits, 1, digit*value_to_integer_scaler);
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