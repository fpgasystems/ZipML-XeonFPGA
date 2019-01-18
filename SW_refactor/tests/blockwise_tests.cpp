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

	AdditionalArguments args;
	args.m_firstSample = 0;
	args.m_numSamples = columnML->m_cstore->m_numSamples;
	args.m_constantStepSize = true;

	// columnML->SGD(
	// 	type,
	// 	nullptr,
	// 	numEpochs,
	// 	1,
	// 	stepSize,
	// 	lambda,
	// 	&args);

	columnML->blockwise_SGD(
		type,
		nullptr,
		numEpochs,
		1,
		blockSize,
		numBlocksAtATime,
		stepSize, 
		lambda, 
		&args);

	return 0;
}