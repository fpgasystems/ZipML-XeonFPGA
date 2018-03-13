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
	uint32_t useEncryption = 0;
	uint32_t useCompression = 0;
	if (argc != 6) {
		cout << "Usage: ./ZipML.exe <pathToDataset> <numSamples> <numFeatures> <encrypt> <compress>" << endl;
		return 0;
	}
	else {
		pathToDataset = argv[1];
		numSamples = atoi(argv[2]);
		numFeatures = atoi(argv[3]);
		useEncryption = atoi(argv[4]);
		useCompression = atoi(argv[5]);
	}

	uint32_t stepSizeShifter = 3;
	uint32_t numEpochs = 2;
	uint32_t miniBatchSize = 64;

	// Do SCD
	scd scd_app(1);

	// scd_app.load_libsvm_data(pathToDataset, numSamples, numFeatures);
	// scd_app.a_normalize(0, 'c');
	// scd_app.b_normalize(0, 0, 0.0);

	scd_app.generate_synthetic_data(numSamples, numFeatures, 0);

	scd_app.print_samples(1);


	if (useCompression == 1) {
		float compressionRate = scd_app.compress_a(miniBatchSize, VALUE_TO_INT_SCALER);
		cout << "compressionRate: " << compressionRate << endl;
	}
	if (useEncryption == 1) {
		scd_app.encrypt_a(miniBatchSize, useCompression);
	}
	
	scd_app.float_linreg_SCD(NULL, numEpochs, miniBatchSize, 1.0/(1 << stepSizeShifter), 0, 0, VALUE_TO_INT_SCALER);

	scd_app.float_linreg_SCD(NULL, numEpochs, miniBatchSize, 1.0/(1 << stepSizeShifter), useEncryption, useCompression, VALUE_TO_INT_SCALER);

	scd_app.float_linreg_FSCD(NULL, numEpochs, miniBatchSize, 1.0/(1 << stepSizeShifter), 0, useEncryption, useCompression, VALUE_TO_INT_SCALER, 4);




	// unsigned char initKey[32];
	// for (uint32_t i = 0; i < 32; i++) {
	// 	initKey[i] = (unsigned char)i;
	// }
	// unsigned char KEYS_enc[16*15];
	// unsigned char KEYS_dec[16*15];
	// AES_256_Key_Expansion(initKey, KEYS_enc);
	// AES_256_Decryption_Keys(KEYS_enc, KEYS_dec);
	// unsigned char ivec[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

	
	// unsigned char* encryptedColumn = (unsigned char*)malloc(numSamples*sizeof(float));
	// unsigned char* decryptedColumn = (unsigned char*)malloc(numSamples*sizeof(float));


	// AES_CBC_encrypt((unsigned char*)scd_app.a[0], encryptedColumn, ivec, numSamples*sizeof(float), KEYS_enc, 14);
	// for (uint32_t i = 0; i < 10; i++) {
	// 	cout << "scd_app.a[0][" << i << "]: " << scd_app.a[0][i] << endl;
	// 	cout << "encryptedColumn[" << i << "]: " << hex << ((int*)encryptedColumn)[i] << dec << endl;
	// }
	// cout << "--------------------------------" << endl;
	

	// AES_CBC_decrypt(encryptedColumn, decryptedColumn, ivec, numSamples*sizeof(float), KEYS_dec, 14);
	// for (uint32_t i = 0; i < 10; i++) {
	// 	cout << "scd_app.a[0][" << i << "]: " << scd_app.a[0][i] << endl;
	// 	cout << "decryptedColumn[" << i << "]: " << hex << ((float*)decryptedColumn)[i] << dec << endl;
	// }
	// free(encryptedColumn);
	// free(decryptedColumn);



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
