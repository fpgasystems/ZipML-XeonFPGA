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
#include <fstream>

#include "../src/ColumnML.h"

using namespace std;

#define VALUE_TO_INT_SCALER 10

int main(int argc, char* argv[]) {

	char* pathToDataset;
	uint32_t numSamples = 0;
	uint32_t numFeatures = 0;
	uint32_t numEpochs = 10;
	uint32_t blockSize = 1;
	uint32_t numBlocksAtATime = 1;
	if (!(argc == 7)) {
		cout << "Usage: ./app <pathToDataset> <numSamples> <numFeatures> <numEpochs> <blockSize> <numBlocksAtATime>" << endl;
		return 0;
	}
	pathToDataset = argv[1];
	numSamples = atoi(argv[2]);
	numFeatures = atoi(argv[3]);
	numEpochs = atoi(argv[4]);
	blockSize = atoi(argv[5]);
	numBlocksAtATime = atoi(argv[6]);
	
	float stepSize = 0.01;
	float lambda = 0;

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


	// uint32_t count = 50;
	// tuple_t temp[count];
	// for (uint32_t i = 0; i < count; i++) {
	// 	temp[i].index = i;
	// 	temp[i].feature = rand()%count;
	// }
	// for (uint32_t i = 0; i < count; i++) {
	// 	cout << temp[i].index << ": " << temp[i].feature << endl;
	// }
	// quicksort(temp, 0, count-1);
	// cout << "**********************" << endl;
	// for (uint32_t i = 0; i < count; i++) {
	// 	cout << temp[i].index << ": " << temp[i].feature << endl;
	// }


	AdditionalArguments args;
	args.m_firstSample = 0;
	args.m_numSamples = columnML->m_cstore->m_numSamples;
	args.m_constantStepSize = true;
	char sortByFeatureOrLabel = 'l';
	bool shuffle = true;

	float lossHistory[8][numEpochs+1];
	float trainAccuracyHistory[8][numEpochs+1];
	float testAccuracyHistory[8][numEpochs+1];

	columnML->blockwise_SGD(
		type,
		nullptr,
		lossHistory[0],
		trainAccuracyHistory[0],
		testAccuracyHistory[0],
		numEpochs,
		1,
		blockSize,
		numBlocksAtATime,
		stepSize, 
		lambda, 
		sortByFeatureOrLabel,
		shuffle,
		&args);

	blockSize = 1024;
	numBlocksAtATime = 1;
	for (uint32_t i = 0; i < 7; i++) {
		args.m_firstSample = 0;
		args.m_numSamples = columnML->m_cstore->m_numSamples;
		cout << "blockSize: " << blockSize << endl;
		cout << "numBlocksAtATime: " << numBlocksAtATime << endl;
		columnML->blockwise_SGD(
			type,
			nullptr,
			lossHistory[i+1],
			trainAccuracyHistory[i+1],
			testAccuracyHistory[i+1],
			numEpochs,
			1,
			blockSize,
			numBlocksAtATime,
			stepSize, 
			lambda, 
			sortByFeatureOrLabel,
			shuffle,
			&args);
		numBlocksAtATime *= 2;
	}

	ofstream ofs ("temp.log", std::ofstream::out);

	for (uint32_t e = 0; e < numEpochs+1; e++) {
		for (uint32_t i = 0; i < 8; i++) {
			ofs << lossHistory[i][e] << " ";
		}
		ofs << " ";
		for (uint32_t i = 0; i < 8; i++) {
			ofs << trainAccuracyHistory[i][e] << " ";
		}
		ofs << " ";
		for (uint32_t i = 0; i < 8; i++) {
			ofs << testAccuracyHistory[i][e] << " ";
		}
		ofs << endl;
	}

	ofs.close();

	return 0;
}