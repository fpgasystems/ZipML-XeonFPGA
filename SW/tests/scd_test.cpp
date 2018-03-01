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

	char* pathToDataset;
	uint32_t numSamples;
	uint32_t numFeatures;
	if (argc != 4) {
		cout << "Usage: ./ZipML.exe <pathToDataset> <numSamples> <numFeatures>" << endl;
		return 0;
	}
	else {
		pathToDataset = argv[1];
		numSamples = atoi(argv[2]);
		numFeatures = atoi(argv[3]);
	}

	uint32_t stepSizeShifter = 3;
	uint32_t numEpochs = 2;
	uint32_t miniBatchSize = 512;

	// Do SCD
	scd scd_app(1);

	// scd_app.load_libsvm_data(pathToDataset, numSamples, numFeatures);
	// scd_app.a_normalize(0, 'c');
	// scd_app.b_normalize(0, 0, 0.0);

	scd_app.generate_synthetic_data(numSamples, numFeatures, 0);

	scd_app.print_samples(1);

	// scd_app.float_linreg_SGD(NULL, numEpochs, numSamples, 1.0/(1 << stepSizeShifter));
	// scd_app.float_linreg_SGD(NULL, numEpochs, miniBatchSize, 1.0/(1 << 12));

	// scd_app.float_linreg_SCD(NULL, numEpochs, numSamples, 1.0/(1 << stepSizeShifter));
	scd_app.float_linreg_SCD(NULL, numEpochs, miniBatchSize, 1.0/(1 << stepSizeShifter));

	// scd_app.AVX_float_linreg_SCD(NULL, numEpochs, numSamples, 1.0/(1 << stepSizeShifter));
	// scd_app.AVX_float_linreg_SCD(NULL, numEpochs, 1024, 1.0/(1 << stepSizeShifter));

	// scd_app.AVXmulti_float_linreg_SCD(NULL, numEpochs, 1024, 1.0/(1 << stepSizeShifter));

	float compressionRate = scd_app.compress_a(miniBatchSize, VALUE_TO_INT_SCALER);
	cout << "compressionRate: " << compressionRate << endl;
	scd_app.compressed_linreg_SCD(NULL, numEpochs, miniBatchSize, 1.0/(1 << stepSizeShifter), VALUE_TO_INT_SCALER);



	scd_app.linreg_FSCD(NULL, numEpochs, miniBatchSize, 1.0/(1 << stepSizeShifter), 0, 0, VALUE_TO_INT_SCALER);



	// uint32_t* compressedColumn = (uint32_t*)malloc(miniBatchSize*sizeof(uint32_t));
	// cout << "beforeCompressionSize: " << scd_app.numSamples << endl;
	// uint32_t afterCompressionSize = scd_app.compress_column(scd_app.a[0], miniBatchSize, compressedColumn, VALUE_TO_INT_SCALER);
	// cout << "afterCompressionSize: " << afterCompressionSize << endl;

	// float* decompressedColumn = (float*)malloc(miniBatchSize*sizeof(float));
	// uint32_t afterDecompressionSize = scd_app.decompress_column(compressedColumn, afterCompressionSize, decompressedColumn, VALUE_TO_INT_SCALER);
	// cout << "afterDecompressionSize: " << afterDecompressionSize << endl;

	// for (uint32_t i = 0; i < 20; i++) {
	// 	cout << "scd_app.a[0][" << i << "]: " << scd_app.a[0][i] << endl;
	// 	cout << "decompressedColumn[" << i << "]: " << decompressedColumn[i] << endl;
	// }

	// free(compressedColumn);
	// free(decompressedColumn);



	// for (uint32_t i = 1; i < 10; i++) {
	// 	cout << scd_app.a[1][i] << endl;
	// 	cout << (int)(scd_app.a[1][i]*(1 << VALUE_TO_INT_SCALER)) << endl;
	// 	cout << "delta: " << (int)(scd_app.a[1][i]*(1 << VALUE_TO_INT_SCALER)) - (int)(scd_app.a[1][i-1]*(1 << VALUE_TO_INT_SCALER)) << endl;
	// }



	// ofstream f1("features.dat");
	// f1 << scd_app.numSamples << " " << scd_app.numFeatures << endl;
	// for (uint32_t i = 0; i < scd_app.numSamples; i++) {
	// 	for (uint32_t j = 0; j < scd_app.numFeatures; j++) {
	// 		f1 << j << " " << scd_app.a[j][i] << " ";
	// 	}
	// 	f1 << endl;
	// }
	// f1.close();
	// ofstream f2("labels.dat");
	// for (uint32_t i = 0; i < scd_app.numSamples; i++) {
	// 	f2 << scd_app.b[i] << endl;
	// }
	// f2.close();
}
