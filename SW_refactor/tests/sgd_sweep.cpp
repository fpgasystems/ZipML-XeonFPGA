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
#include <vector>
#include <thread>

#include "../src/ColumnML.h"

using namespace std;

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
	uint32_t numSamples = 0;
	uint32_t numFeatures = 0;
	uint32_t minibatchSize = 32;
	bool doLogreg = false;
	uint32_t numEpochs = 10;
	uint32_t numSweeps = 1;
	uint32_t numThreads = 1;
	if (argc != 9) {
		cout << "Usage: ./app <pathToDataset> <numSamples> <numFeatures> <minibatchSize> <doLogreg> <numEpochs> <numSweeps> <numThreads>" << endl;
		return 1;
	}
	pathToDataset = argv[1];
	numSamples = atoi(argv[2]);
	numFeatures = atoi(argv[3]);
	minibatchSize = atoi(argv[4]);
	doLogreg = atoi(argv[5]) == 1;
	numEpochs = atoi(argv[6]);
	numSweeps = atoi(argv[7]);
	numThreads = atoi(argv[8]);

	float stepSize = 0.001;
	float lambda = 0;

	ColumnML* columnML = new ColumnML();

	ModelType type = doLogreg ? logreg : linreg;
	if ( strcmp(pathToDataset, "syn") == 0) {
		columnML->m_cstore->GenerateSyntheticData(numSamples, numFeatures, doLogreg, ZeroToOne);
	}
	else {
		columnML->m_cstore->LoadRawData(pathToDataset, numSamples, numFeatures, true);
		columnML->m_cstore->NormalizeSamples(ZeroToOne, column);
		columnML->m_cstore->NormalizeLabels(ZeroToOne, true, 1);
	}

	AdditionalArguments args;
	args.m_firstSample = 0;
	args.m_numSamples = columnML->m_cstore->m_numSamples;
	args.m_constantStepSize = true;

	vector<vector<float> > x_history(numSweeps);
	for (unsigned i = 0; i < numSweeps; i++) {
		x_history[i].resize(numEpochs*columnML->m_cstore->m_numFeatures);
	}

	double start = get_time();
	unsigned index = 0;
	while (index < numSweeps) {
		vector<std::thread*> sgd_threads;
		while (sgd_threads.size() < numThreads && index < numSweeps) {
			std::thread* temp_thread = new std::thread(&ColumnML::AVXrowwise_SGD, columnML, type, x_history[index].data(), numEpochs, minibatchSize, stepSize, lambda, &args);
			sgd_threads.push_back(temp_thread);
			index++;
		}

		for (std::thread* t: sgd_threads) {
			t->join();
		}
	}
	double end = get_time();
	cout << "Total time: " << end-start << endl;

	for (unsigned e = 0; e < numEpochs; e++) {
		if (type == logreg) {
			cout << columnML->LogregLoss(x_history[0].data() + e*columnML->m_cstore->m_numFeatures, lambda, &args) << endl;
		}
		else {
			cout << columnML->LinregLoss(x_history[0].data() + e*columnML->m_cstore->m_numFeatures, lambda, &args) << endl;
		}
	}

}