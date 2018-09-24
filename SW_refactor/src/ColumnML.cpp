// Copyright (C) 2018 Kaan Kara - Systems Group, ETH Zurich

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received m_cstore->m_samples copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//*************************************************************************

#include "ColumnML.h"

using namespace std;

void ColumnML::printTimeout() {
	uint32_t numRequestedReads = m_interfaceFPGA->readFromMemory32('i', 0);
	uint32_t numReceivedReads = m_interfaceFPGA->readFromMemory32('i', 1);
	uint32_t residualNumReceivedReads = m_interfaceFPGA->readFromMemory32('i', 2);
	uint32_t labelsNumReceivedReads = m_interfaceFPGA->readFromMemory32('i', 3);
	uint32_t samplesNumReceivedReads = m_interfaceFPGA->readFromMemory32('i', 4);
	uint32_t residualNumWriteRequests = m_interfaceFPGA->readFromMemory32('i', 5);
	uint32_t stepNumWriteRequests = m_interfaceFPGA->readFromMemory32('i', 6);
	uint32_t numWriteResponses = m_interfaceFPGA->readFromMemory32('i', 7);
	uint32_t completedEpochs = m_interfaceFPGA->readFromMemory32('i', 8);
	uint32_t reorderFreeCount = m_interfaceFPGA->readFromMemory32('i', 9);
	uint32_t featureUpdateIndex = m_interfaceFPGA->readFromMemory32('i', 10);
	uint32_t writeBatchIndex = m_interfaceFPGA->readFromMemory32('i', 11);
	uint32_t decompressorOutFifoFreeCount = m_interfaceFPGA->readFromMemory32('i', 12);

	cout << "numRequestedReads: " << numRequestedReads << endl;
	cout << "numReceivedReads: " << numReceivedReads << endl;
	cout << "residualNumReceivedReads: " << residualNumReceivedReads << endl;
	cout << "labelsNumReceivedReads: " << labelsNumReceivedReads << endl;
	cout << "samplesNumReceivedReads: " << samplesNumReceivedReads << endl;
	cout << "residualNumWriteRequests: " << residualNumWriteRequests << endl;
	cout << "stepNumWriteRequests: " << stepNumWriteRequests << endl;
	cout << "numWriteResponses: " << numWriteResponses << endl;
	cout << "completedEpochs: " << completedEpochs << endl;
	cout << "reorderFreeCount: " << reorderFreeCount << endl;
	cout << "featureUpdateIndex: " << featureUpdateIndex << endl;
	cout << "writeBatchIndex: " << writeBatchIndex << endl;
	cout << "decompressorOutFifoFreeCount: " << decompressorOutFifoFreeCount << endl;
}

void ColumnML::WriteLogregPredictions(float* x) {
	ofstream ofs ("predictions.txt", ofstream::out);
	for(uint32_t i = 0; i < m_cstore->m_numSamples; i++) {
		float dot = 0.0;
		for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
			if (m_cstore->m_samplesRange[j] > 0) {
				dot += x[j]*((m_cstore->m_samples[j][i] - m_cstore->m_samplesMin[j])/m_cstore->m_samplesRange[j]);
			}
			else {
				dot += x[j]*m_cstore->m_samples[j][i];
			}
		}
		float prediction = 1/(1+exp(-dot));
		ofs << prediction << endl;
	}
	ofs.close();
}

float ColumnML::L2regularization(float* x, float lambda) {
	float regularizer = 0;
	for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
		regularizer += x[j]*x[j];
	}
	regularizer *= (lambda*0.5)/m_cstore->m_numSamples;

	return regularizer;
}

float ColumnML::L1regularization(float* x, float lambda) {
	float regularizer = 0;
	for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
		regularizer += (x[j] < 0) ? -x[j] : x[j];
	}
	regularizer *= lambda;

	return regularizer;
}

float ColumnML::L2svmLoss(float* x, float costPos, float costNeg, float lambda) {
	float loss = 0;
	for(uint32_t i = 0; i < m_cstore->m_numSamples; i++) {
		float dot = getDot(x, i);
		float temp = 1 - m_cstore->m_labels[i]*dot;
		if (temp > 0) {
			if (m_cstore->m_labels[i] > 0) {
				loss += costPos*temp*temp;
			}
			else {
				loss += costNeg*temp*temp;
			}
		}
	}
	loss /= (float)(2*m_cstore->m_numSamples);
	loss += L1regularization(x, lambda);

	return loss;
}

float ColumnML::LogregLoss(float* x, float lambda) {
	float loss = 0;
	for(uint32_t i = 0; i < m_cstore->m_numSamples; i++) {
		float dot = getDot(x, i);
		float prediction = 1/(1+exp(-dot));
		loss += m_cstore->m_labels[i]*log(prediction) + (1-m_cstore->m_labels[i])*log(1 - prediction);			
	}
	loss /= (float)m_cstore->m_numSamples;
	loss = -loss;
	loss += L1regularization(x, lambda);

	return loss;
}

float ColumnML::LinregLoss(float* x, float lambda) {
	float loss = 0;
	for(uint32_t i = 0; i < m_cstore->m_numSamples; i++) {
		float dot = getDot(x, i);
		loss += (dot - m_cstore->m_labels[i])*(dot - m_cstore->m_labels[i]);
	}
	loss /= (float)(2*m_cstore->m_numSamples);
	loss += L1regularization(x, lambda);

	return loss;
}

uint32_t ColumnML::LogregAccuracy(float* x, uint32_t startIndex, uint32_t length) {
	uint32_t corrects = 0;
	for(uint32_t i = startIndex; i < startIndex+length; i++) {
		float dot = getDot(x, i);
		float prediction = 1/(1+exp(-dot));
		if ( (prediction > 0.5 && m_cstore->m_labels[i] == 1.0) || (prediction < 0.5 && m_cstore->m_labels[i] == 0) ) {
			corrects++;
		}
	}

	return corrects;
}

uint32_t ColumnML::LinregAccuracy(float* x, float decisionBoundary, int trueLabel, int falseLabel) {
	uint32_t corrects = 0;
	for(uint32_t i = 0; i < m_cstore->m_numSamples; i++) {
		float dot = getDot(x, i);
		if ( (dot > decisionBoundary && m_cstore->m_labels[i] == trueLabel) || (dot < decisionBoundary && m_cstore->m_labels[i] == falseLabel) ) {
			corrects++;
		}
	}
	return corrects;
}

void ColumnML::SGD(
	ModelType type, 
	float* xHistory, 
	uint32_t numEpochs, 
	uint32_t minibatchSize, 
	float stepSize, 
	float lambda, 
	AdditionalArguments* args) 
{
	float* x = (float*)aligned_alloc(64, m_cstore->m_numFeatures*sizeof(float));
	memset(x, 0, m_cstore->m_numFeatures*sizeof(float));
	float* gradient = (float*)aligned_alloc(64, m_cstore->m_numFeatures*sizeof(float));
	memset(gradient, 0, m_cstore->m_numFeatures*sizeof(float));

	cout << "SGD ---------------------------------------" << endl;
	uint32_t numMinibatches = m_cstore->m_numSamples/minibatchSize;
	cout << "numMinibatches: " << numMinibatches << endl;
	uint32_t rest = m_cstore->m_numSamples - numMinibatches*minibatchSize;
	cout << "rest: " << rest << endl;

	cout << "Initial loss: " << Loss(type, x, lambda, args) << endl;
	cout << "Initial accuracy: " << Accuracy(type, x, args) << " corrects out of " << m_cstore->m_numSamples << endl;

	float scaledStepSize = stepSize/minibatchSize;
	float scaledLambda = stepSize*lambda;
	cout << "scaledLambda: " << scaledLambda << endl;
	for(uint32_t epoch = 0; epoch < numEpochs; epoch++) {

		double start = get_time();

		for (uint32_t k = 0; k < numMinibatches; k++) {
			// uint32_t rand = 0;
			// _rdseed32_step(&rand);
			// uint32_t m = numMinibatches*((float)(rand-1)/(float)UINT_MAX);
			uint32_t m = k;
			for (uint32_t i = 0; i < minibatchSize; i++) {
				updateGradient(type, gradient, x, m*minibatchSize + i, args);
			}
			for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
				float regularizer = (x[j] < 0) ? -scaledLambda : scaledLambda;
				x[j] -= scaledStepSize*gradient[j] + regularizer;
				gradient[j] = 0.0;
			}
		}

		double end = get_time();
#ifdef PRINT_TIMING
		cout << "time for one epoch: " << end-start << endl;
#endif
		if (xHistory != nullptr) {
			for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
				xHistory[epoch*m_cstore->m_numFeatures + j] = x[j];
			}
		}
		else {
#ifdef PRINT_LOSS
			cout << "Loss " << epoch << ": " << Loss(type, x, lambda, args) << endl;
#endif
#ifdef PRINT_ACCURACY
			cout << Accuracy(type, x, args) << " corrects out of " << m_cstore->m_numSamples << endl;
#endif
		}
	}

	free(x);
	free(gradient);
}

void ColumnML::AVX_SGD(
	ModelType type, 
	float* xHistory, 
	uint32_t numEpochs, 
	uint32_t minibatchSize, 
	float stepSize, 
	float lambda, 
	AdditionalArguments* args) 
{
	float* x = (float*)aligned_alloc(64, m_cstore->m_numFeatures*sizeof(float));
	memset(x, 0, m_cstore->m_numFeatures*sizeof(float));
	float* gradient = (float*)aligned_alloc(64, m_cstore->m_numFeatures*sizeof(float));
	memset(gradient, 0, m_cstore->m_numFeatures*sizeof(float));

	cout << "AVX_SGD ---------------------------------------" << endl;
	uint32_t numMinibatches = m_cstore->m_numSamples/minibatchSize;
	cout << "numMinibatches: " << numMinibatches << endl;
	uint32_t rest = m_cstore->m_numSamples - numMinibatches*minibatchSize;
	cout << "rest: " << rest << endl;

	cout << "Initial loss: " << Loss(type, x, lambda, args) << endl;
	cout << "Initial accuracy: " << Accuracy(type, x, args) << " corrects out of " << m_cstore->m_numSamples << endl;

	__m256 AVX_ones = _mm256_set1_ps(1.0);
	__m256 AVX_minusOnes = _mm256_set1_ps(-1.0);

	float scaledStepSize = stepSize/minibatchSize;
	float scaledLambda = stepSize*lambda;
	cout << "scaledLambda: " << scaledLambda << endl;
	for(uint32_t epoch = 0; epoch < numEpochs; epoch++) {

		double start = get_time();

		for (uint32_t k = 0; k < numMinibatches; k++) {
			// uint32_t rand = 0;
			// _rdseed32_step(&rand);
			// uint32_t m = numMinibatches*((float)(rand-1)/(float)UINT_MAX)*minibatchSize;
			uint32_t minibatchOffset = k*minibatchSize;
			if (minibatchSize%8 == 0) {
				for (uint32_t i = 0; i < minibatchSize-(minibatchSize%8); i+=8) {
					__m256 AVX_dot = AVX_verticalGetDot(x, minibatchOffset + i);
					if (type == logreg) {
						AVX_dot = _mm256_mul_ps(AVX_minusOnes, AVX_dot);
						AVX_dot = exp256_ps(AVX_dot);
						AVX_dot = _mm256_add_ps(AVX_ones, AVX_dot);
						AVX_dot = _mm256_div_ps(AVX_ones, AVX_dot);
					}
					__m256 AVX_labels = _mm256_load_ps(m_cstore->m_labels + minibatchOffset + i);
					AVX_dot = _mm256_sub_ps(AVX_dot, AVX_labels);
					for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
						__m256 AVX_samples = _mm256_load_ps(m_cstore->m_samples[j] + minibatchOffset + i);
						__m256 AVX_gradient = _mm256_mul_ps(AVX_dot, AVX_samples);
						float delta[8];
						_mm256_store_ps(delta, AVX_gradient);
						gradient[j] += delta[0] + delta[1] + delta[2] + delta[3] + delta[4] + delta[5] + delta[6] + delta[7];
					}
				}
			}
			for (uint32_t i = minibatchSize-(minibatchSize%8); i < minibatchSize; i++) {
				float dot = AVX_horizontalGetDot(x, minibatchOffset + i);
				if (type == logreg) {
					dot = 1/(1+exp(-dot));
				}
				dot = (dot-m_cstore->m_labels[minibatchOffset + i]);
				for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
					gradient[j] += dot*m_cstore->m_samples[j][minibatchOffset + i];
				}
			}
			for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
				float regularizer = (x[j] < 0) ? -scaledLambda : scaledLambda;
				x[j] -= scaledStepSize*gradient[j] + regularizer;
				gradient[j] = 0.0;
			}
		}

		double end = get_time();
#ifdef PRINT_TIMING
		cout << "time for one epoch: " << end-start << endl;
#endif
		if (xHistory != nullptr) {
			for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
				xHistory[epoch*m_cstore->m_numFeatures + j] = x[j];
			}
		}
		else {
#ifdef PRINT_LOSS
			cout << "Loss " << epoch << ": " << Loss(type, x, lambda, args) << endl;
#endif
#ifdef PRINT_ACCURACY
			cout << Accuracy(type, x, args) << " corrects out of " << m_cstore->m_numSamples << endl;
#endif
		}
	}

	free(x);
	free(gradient);
}

void ColumnML::SCD(
	ModelType type, 
	float* xHistory, 
	uint32_t numEpochs, 
	uint32_t minibatchSize, 
	float stepSize, 
	float lambda, 
	uint32_t numMinibatchesAtATime, 
	uint32_t residualUpdatePeriod, 
	bool useEncrypted, 
	bool useCompressed, 
	uint32_t toIntegerScaler, 
	AdditionalArguments* args)
{
	cout << "SCD ---------------------------------------" << endl;
	uint32_t numMinibatches = m_cstore->m_numSamples/minibatchSize;
	cout << "numMinibatches: " << numMinibatches << endl;
	uint32_t rest = m_cstore->m_numSamples - numMinibatches*minibatchSize;
	cout << "rest: " << rest << endl;

	float* residual = (float*)aligned_alloc(64, m_cstore->m_numSamples*sizeof(float));
	memset(residual, 0, m_cstore->m_numSamples*sizeof(float));
	float* x = (float*)aligned_alloc(64, (numMinibatches + m_cstore->m_numSamples%minibatchSize)*m_cstore->m_numFeatures*sizeof(float));
	memset(x, 0, (numMinibatches + m_cstore->m_numSamples%minibatchSize)*m_cstore->m_numFeatures*sizeof(float));
	float* xFinal = (float*)aligned_alloc(64, m_cstore->m_numFeatures*sizeof(float));
	memset(xFinal, 0, m_cstore->m_numFeatures*sizeof(float));
	
	cout << "Initial loss: " << Loss(type, xFinal, lambda, args) << endl;
	cout << "Initial accuracy: " << Accuracy(type, xFinal, args) << " corrects out of " << m_cstore->m_numSamples << endl;

	float* transformedColumn1 = nullptr;
	float* transformedColumn2 = nullptr;
	if (useEncrypted && useCompressed) {
		transformedColumn1 = (float*)aligned_alloc(64, numMinibatchesAtATime*minibatchSize*sizeof(float));
		transformedColumn2 = (float*)aligned_alloc(64, numMinibatchesAtATime*minibatchSize*sizeof(float));
	}
	else {
		transformedColumn2 = (float*)aligned_alloc(64, numMinibatchesAtATime*minibatchSize*sizeof(float));
	}

	float scaledStepSize = stepSize/minibatchSize;
	float scaledLambda = stepSize*lambda;
	uint32_t epoch_index = 0;
	for(uint32_t epoch = 0; epoch < numEpochs + (numEpochs/residualUpdatePeriod); epoch++) {

		double decryption_time = 0;
		double decompression_time = 0;
		double dot_product_time = 0;
		double residual_update_time = 0;
		double temp_time1, temp_time2, temp_time3;
		double start = get_time();

		for (uint32_t k = 0; k < numMinibatches/numMinibatchesAtATime; k++) {

			if (numMinibatchesAtATime > 1) {
				uint32_t m[numMinibatchesAtATime];
				for (uint32_t l = 0; l < numMinibatchesAtATime; l++) {
					m[l] = l*(numMinibatches/numMinibatchesAtATime)+k;
				}
				for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {

					for (uint32_t l = 0; l < numMinibatchesAtATime; l++) {
						if (useEncrypted && useCompressed) {
							int32_t compressedSamplesOffset = 0;
							if (m[l] > 0) {
								compressedSamplesOffset = m_cstore->m_compressedSamplesSizes[j][m[l]-1];
							}
							temp_time1 = get_time();
							m_cstore->decryptColumn(m_cstore->m_encryptedSamples[j] + compressedSamplesOffset, m_cstore->m_compressedSamplesSizes[j][m[l]] - compressedSamplesOffset, transformedColumn1 + l*minibatchSize);
							temp_time2 = get_time();
							decryption_time += (temp_time2-temp_time1);
							ColumnStore::decompressColumn((uint32_t*)transformedColumn1 + l*minibatchSize, m_cstore->m_compressedSamplesSizes[j][m[l]] - compressedSamplesOffset, transformedColumn2 + l*minibatchSize, toIntegerScaler);
							temp_time3 = get_time();
							decompression_time += (temp_time3-temp_time2);
						}
						else if (useEncrypted) {
							temp_time1 = get_time();
							m_cstore->decryptColumn(m_cstore->m_encryptedSamples[j] + m[l]*minibatchSize, minibatchSize, transformedColumn2 + l*minibatchSize);
							temp_time2 = get_time();
							decryption_time += (temp_time2-temp_time1);
						}
						else if (useCompressed) {
							temp_time1 = get_time();
							int32_t compressedSamplesOffset = 0;
							if (m[l] > 0) {
								compressedSamplesOffset = m_cstore->m_compressedSamplesSizes[j][m[l]-1];
							}
							ColumnStore::decompressColumn(m_cstore->m_compressedSamples[j] + compressedSamplesOffset, m_cstore->m_compressedSamplesSizes[j][m[l]] - compressedSamplesOffset, transformedColumn2 + l*minibatchSize, toIntegerScaler);
							temp_time2 = get_time();
							decompression_time += (temp_time2-temp_time1);
						}
						else {
							for (uint32_t i = 0; i < minibatchSize; i++) {
								transformedColumn2[l*minibatchSize + i] = m_cstore->m_samples[j][m[l]*minibatchSize + i];
							}
						}
					}

					if ( (epoch+1)%(residualUpdatePeriod+1) == 0 ) {
						for (uint32_t l = 0; l < numMinibatchesAtATime; l++) {
							for (uint32_t i = 0; i < minibatchSize; i++) {
								if (j == 0) {
									residual[m[l]*minibatchSize + i] = xFinal[j]*transformedColumn2[l*minibatchSize + i];
								}
								else {
									residual[m[l]*minibatchSize + i] += xFinal[j]*transformedColumn2[l*minibatchSize + i];
								}
							}
						}
					}
					else {
						temp_time1 = get_time();
						float gradient = 0;
						for (uint32_t l = 0; l < numMinibatchesAtATime; l++) {
							for (uint32_t i = 0; i < minibatchSize; i++) {
								switch(type) {
									case linreg:
										gradient += (residual[m[l]*minibatchSize + i] - m_cstore->m_labels[m[l]*minibatchSize + i])*transformedColumn2[l*minibatchSize + i];
										break;
									case logreg:
										gradient += (1/(1+exp(-residual[m[l]*minibatchSize + i])) - m_cstore->m_labels[m[l]*minibatchSize + i])*transformedColumn2[l*minibatchSize + i];
										break;
									default:
										break;
								}								
							}
						}
						temp_time2 = get_time();
						dot_product_time += (temp_time2-temp_time1);

						float regularizer = (x[k*m_cstore->m_numFeatures + j] < 0) ? -scaledLambda : scaledLambda;
						float step = scaledStepSize*gradient + regularizer;
						x[k*m_cstore->m_numFeatures + j] -= step;

						for (uint32_t l = 0; l < numMinibatchesAtATime; l++) {
							for (uint32_t i = 0; i < minibatchSize; i++) {
								residual[m[l]*minibatchSize + i] -= step*transformedColumn2[l*minibatchSize + i];
							}
						}
						temp_time3 = get_time();
						residual_update_time += (temp_time3-temp_time2);
					}
				}
			}
			else {
				uint32_t m = k;

				for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {

					if (useEncrypted == 1 && useCompressed == 1) {
						int32_t compressedSamplesOffset = 0;
						if (m > 0) {
							compressedSamplesOffset = m_cstore->m_compressedSamplesSizes[j][m-1];
						}
						temp_time1 = get_time();
						m_cstore->decryptColumn(m_cstore->m_encryptedSamples[j] + compressedSamplesOffset, m_cstore->m_compressedSamplesSizes[j][m] - compressedSamplesOffset, transformedColumn1);
						temp_time2 = get_time();
						decryption_time += (temp_time2-temp_time1);
						ColumnStore::decompressColumn((uint32_t*)transformedColumn1, m_cstore->m_compressedSamplesSizes[j][m] - compressedSamplesOffset, transformedColumn2, toIntegerScaler);
						temp_time3 = get_time();
						decompression_time += (temp_time3-temp_time2);
					}
					else if (useEncrypted == 1) {
						temp_time1 = get_time();
						m_cstore->decryptColumn(m_cstore->m_encryptedSamples[j] + m*minibatchSize, minibatchSize, transformedColumn2);
						temp_time2 = get_time();
						decryption_time += (temp_time2-temp_time1);
					}
					else if (useCompressed == 1) {
						temp_time1 = get_time();
						int32_t compressedSamplesOffset = 0;
						if (m > 0) {
							compressedSamplesOffset = m_cstore->m_compressedSamplesSizes[j][m-1];
						}
						ColumnStore::decompressColumn(m_cstore->m_compressedSamples[j] + compressedSamplesOffset, m_cstore->m_compressedSamplesSizes[j][m] - compressedSamplesOffset, transformedColumn2, toIntegerScaler);
						temp_time2 = get_time();
						decompression_time += (temp_time2-temp_time1);
					}
					else {
						transformedColumn2 = m_cstore->m_samples[j] + m*minibatchSize;
					}

					if ( (epoch+1)%(residualUpdatePeriod+1) == 0 ) {
						for (uint32_t i = 0; i < minibatchSize; i++) {
							if (j == 0) {
								residual[m*minibatchSize + i] = xFinal[j]*transformedColumn2[i];
							}
							else {
								residual[m*minibatchSize + i] += xFinal[j]*transformedColumn2[i];
							}
						}
					}
					else {
						temp_time1 = get_time();
						float gradient = 0;
						switch(type) {
							case linreg:
								for (uint32_t i = 0; i < minibatchSize; i++) {
									gradient += (residual[m*minibatchSize + i] - m_cstore->m_labels[m*minibatchSize + i])*transformedColumn2[i];
								}
								break;
							case logreg:
								for (uint32_t i = 0; i < minibatchSize; i++) {
									gradient += (1/(1+exp(-residual[m*minibatchSize + i])) - m_cstore->m_labels[m*minibatchSize + i])*transformedColumn2[i];
								}
								break;
							default:
								break;
						}
						temp_time2 = get_time();
						dot_product_time += (temp_time2-temp_time1);

						float regularizer = (x[m*m_cstore->m_numFeatures + j] < 0) ? -scaledLambda : scaledLambda;
						float step = scaledStepSize*gradient + regularizer;
						x[m*m_cstore->m_numFeatures + j] -= step;

						for (uint32_t i = 0; i < minibatchSize; i++) {
							residual[m*minibatchSize + i] -= step*transformedColumn2[i];
						}
						temp_time3 = get_time();
						residual_update_time += (temp_time3-temp_time2);
					}
				}
			}
		}

		if ( (epoch+1)%(residualUpdatePeriod+1) == 0 ) {
			cout << "--> PERFORMED RESIDUAL UPDATE !!!" << endl;
		}
		else {
			temp_time1 = get_time();
			for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
				xFinal[j] = 0;
			}
			for (uint32_t k = 0; k < numMinibatches/numMinibatchesAtATime; k++) {
				for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
					xFinal[j] += x[k*m_cstore->m_numFeatures + j];
				}
			}
			for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
				xFinal[j] = xFinal[j]/(numMinibatches/numMinibatchesAtATime);
			}

			double end = get_time();
#ifdef PRINT_TIMING
			cout << "decryption_time: " << decryption_time << endl;
			cout << "decompression_time: " << decompression_time << endl;
			cout << "dot_product_time: " << dot_product_time << endl;
			cout << "residual_update_time: " << residual_update_time << endl;
			cout << "x_average_time: " << end-temp_time1 << endl;
			cout << "Time for one epoch: " << end-start << endl;
#endif
			if (xHistory != NULL) {
				for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
					xHistory[epoch_index*m_cstore->m_numFeatures + j] = xFinal[j];
				}
				epoch_index++;
			}
			else {
#ifdef PRINT_LOSS
				cout << "Loss " << epoch << ": " << Loss(type, xFinal, lambda, args) << endl;
#endif
#ifdef PRINT_ACCURACY
				cout << Accuracy(type, xFinal, args) << " corrects out of " << m_cstore->m_numSamples << endl;
#endif
			}
		}
	}

	if (useEncrypted == 1 && useCompressed == 1) {
		free(transformedColumn1);
		free(transformedColumn2);
	}
	else if (useEncrypted == 1 || useCompressed == 1) {
		free(transformedColumn2);
	}
	free(x);
	free(xFinal);
	free(residual);
}

void ColumnML::AVX_SCD(
		ModelType type,
		float* xHistory, 
		uint32_t numEpochs, 
		uint32_t minibatchSize, 
		float stepSize, 
		float lambda, 
		uint32_t residualUpdatePeriod, 
		bool useEncrypted, 
		bool useCompressed, 
		uint32_t toIntegerScaler,
		AdditionalArguments* args)
{
	if (minibatchSize%8 > 0) {
		cout << "For AVX minibatchSize%8 must be 0!" << endl;
		exit(1);
	}

	cout << "AVX_SCD ---------------------------------------" << endl;
	uint32_t numMinibatches = m_cstore->m_numSamples/minibatchSize;
	cout << "numMinibatches: " << numMinibatches << endl;
	uint32_t rest = m_cstore->m_numSamples - numMinibatches*minibatchSize;
	cout << "rest: " << rest << endl;

	float scaledStepSize = -stepSize/(float)minibatchSize;
	float scaledLambda = -stepSize*lambda;
	__m256 AVX_ones = _mm256_set1_ps(1.0);
	__m256 AVX_minusOnes = _mm256_set1_ps(-1.0);

	float* residual = (float*)aligned_alloc(64, m_cstore->m_numSamples*sizeof(float));
	memset(residual, 0, m_cstore->m_numSamples*sizeof(float));
	float* x = (float*)aligned_alloc(64, (numMinibatches + m_cstore->m_numSamples%minibatchSize)*m_cstore->m_numFeatures*sizeof(float));
	memset(x, 0, (numMinibatches + m_cstore->m_numSamples%minibatchSize)*m_cstore->m_numFeatures*sizeof(float));
	float* xFinal = (float*)aligned_alloc(64, m_cstore->m_numFeatures*sizeof(float));
	memset(xFinal, 0, m_cstore->m_numFeatures*sizeof(float));

	cout << "Initial loss: " << Loss(type, xFinal, lambda, args) << endl;
	cout << "Initial accuracy: " << Accuracy(type, xFinal, args) << " corrects out of " << m_cstore->m_numSamples << endl;

	float* transformedColumn1 = NULL;
	float* transformedColumn2 = NULL;
	if (useEncrypted && useCompressed) {
		transformedColumn1 = (float*)aligned_alloc(64, minibatchSize*sizeof(float));
		transformedColumn2 = (float*)aligned_alloc(64, minibatchSize*sizeof(float));
	}
	else if (useEncrypted || useCompressed) {
		transformedColumn2 = (float*)aligned_alloc(64, minibatchSize*sizeof(float));
	}

	uint32_t epoch_index = 0;
	for(uint32_t epoch = 0; epoch < numEpochs + (numEpochs/residualUpdatePeriod); epoch++) {
		double decryption_time = 0;
		double decompression_time = 0;
		double dot_product_time = 0;
		double residual_update_time = 0;
		double temp_time1, temp_time2, temp_time3;
		double start = get_time();

		__m256 AVX_residual;
		__m256 AVX_samples;
		__m256 AVX_labels;

		for (uint32_t m = 0; m < numMinibatches; m++) {
			for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {

				if (useEncrypted && useCompressed) {
					int32_t compressedSamplesOffset = 0;
					if (m > 0) {
						compressedSamplesOffset = m_cstore->m_compressedSamplesSizes[j][m-1];
					}

					temp_time1 = get_time();
					m_cstore->decryptColumn(m_cstore->m_encryptedSamples[j] + compressedSamplesOffset, m_cstore->m_compressedSamplesSizes[j][m] - compressedSamplesOffset, transformedColumn1);
					temp_time2 = get_time();
					decryption_time += (temp_time2-temp_time1);
					m_cstore->decompressColumn((uint32_t*)transformedColumn1, m_cstore->m_compressedSamplesSizes[j][m] - compressedSamplesOffset, transformedColumn2, toIntegerScaler);
					temp_time3 = get_time();
					decompression_time += (temp_time3-temp_time2);
				}
				else if (useEncrypted) {
					temp_time1 = get_time();
					m_cstore->decryptColumn(m_cstore->m_encryptedSamples[j] + m*minibatchSize, minibatchSize, transformedColumn2);
					temp_time2 = get_time();
					decryption_time += (temp_time2-temp_time1);
				}
				else if (useCompressed) {
					temp_time1 = get_time();
					int32_t compressedSamplesOffset = 0;
					if (m > 0) {
						compressedSamplesOffset = m_cstore->m_compressedSamplesSizes[j][m-1];
					}
					m_cstore->decompressColumn(m_cstore->m_compressedSamples[j] + compressedSamplesOffset, m_cstore->m_compressedSamplesSizes[j][m] - compressedSamplesOffset, transformedColumn2, toIntegerScaler);
					temp_time2 = get_time();
					decompression_time += (temp_time2-temp_time1);
				}
				else {
					transformedColumn2 = m_cstore->m_samples[j] + m*minibatchSize;
				}

				if ( (epoch+1)%(residualUpdatePeriod+1) == 0 ) {
					for (uint32_t i = 0; i < minibatchSize; i++) {
						if (j == 0) {
							residual[m*minibatchSize + i] = xFinal[j]*transformedColumn2[i];
						}
						else {
							residual[m*minibatchSize + i] += xFinal[j]*transformedColumn2[i];
						}
					}
				}
				else {
					temp_time1 = get_time();
					__m256 gradient = _mm256_setzero_ps();
					__m256 AVX_error;
					for (uint32_t i = 0; i < minibatchSize; i+=8) {
						switch(type) {
							case linreg:
								AVX_samples = _mm256_load_ps(transformedColumn2 + i);
								AVX_labels = _mm256_load_ps(m_cstore->m_labels + m*minibatchSize + i);
								AVX_residual = _mm256_load_ps(residual + m*minibatchSize + i);

								AVX_error = _mm256_sub_ps(AVX_residual, AVX_labels);
								gradient = _mm256_fmadd_ps(AVX_samples, AVX_error, gradient);
								break;
							case logreg:
								AVX_samples = _mm256_load_ps(transformedColumn2 + i);
								AVX_labels = _mm256_load_ps(m_cstore->m_labels + m*minibatchSize + i);
								AVX_residual = _mm256_load_ps(residual + m*minibatchSize + i);

								AVX_residual = _mm256_mul_ps(AVX_minusOnes, AVX_residual);
								AVX_residual = exp256_ps(AVX_residual);
								AVX_residual = _mm256_add_ps(AVX_ones, AVX_residual);
								AVX_residual = _mm256_div_ps(AVX_ones, AVX_residual);

								AVX_error = _mm256_sub_ps(AVX_residual, AVX_labels);
								gradient = _mm256_fmadd_ps(AVX_samples, AVX_error, gradient);
								break;
							default:
								break;
						}
					}

					float gradientReduce[8];
					_mm256_store_ps(gradientReduce, gradient);
					gradientReduce[0] = (gradientReduce[0] + 
										gradientReduce[1] + 
										gradientReduce[2] + 
										gradientReduce[3] + 
										gradientReduce[4] + 
										gradientReduce[5] + 
										gradientReduce[6] + 
										gradientReduce[7]);

					float regularizer = (x[m*m_cstore->m_numFeatures + j] < 0) ? -scaledLambda : scaledLambda;
					float step = scaledStepSize*gradientReduce[0] + regularizer;
					__m256 AVX_step = _mm256_set1_ps(step);

					x[m*m_cstore->m_numFeatures + j] += step;

					temp_time2 = get_time();
					dot_product_time += (temp_time2-temp_time1);

					for (uint32_t i = 0; i < minibatchSize; i+=8) {
						AVX_samples = _mm256_load_ps(transformedColumn2 + i);
						AVX_residual = _mm256_load_ps(residual + m*minibatchSize + i);
						AVX_residual = _mm256_fmadd_ps(AVX_samples, AVX_step, AVX_residual);

						_mm256_store_ps(residual + m*minibatchSize + i, AVX_residual);
					}

					temp_time3 = get_time();
					residual_update_time += (temp_time3-temp_time2);
				}
			}
		}

		if ( (epoch+1)%(residualUpdatePeriod+1) == 0 ) {
			cout << "--> PERFORMED RESIDUAL UPDATE !!!" << endl;
		}
		else {
			temp_time1 = get_time();
			for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
				xFinal[j] = 0;
			}
			for (uint32_t m = 0; m < numMinibatches; m++) {
				for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
					xFinal[j] += x[m*m_cstore->m_numFeatures + j];
				}
			}
			for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
				xFinal[j] = xFinal[j]/numMinibatches;
			}

			double end = get_time();
#ifdef PRINT_TIMING
			cout << "decryption_time: " << decryption_time << endl;
			cout << "decompression_time: " << decompression_time << endl;
			cout << "dot_product_time: " << dot_product_time << endl;
			cout << "residual_update_time: " << residual_update_time << endl;
			cout << "x_average_time: " << end-temp_time1 << endl;
			cout << "Time for one epoch: " << end-start << endl;
#endif
			if (xHistory != NULL) {
				for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
					xHistory[epoch_index*m_cstore->m_numFeatures + j] = xFinal[j];
				}
				epoch_index++;
			}
			else {
#ifdef PRINT_LOSS
				cout << "Loss " << epoch << ": " << Loss(type, xFinal, lambda, args) << endl;
#endif
#ifdef PRINT_ACCURACY
				cout << Accuracy(type, xFinal, args) << " corrects out of " << m_cstore->m_numSamples << endl;
#endif
			}
		}
	}

	if (useEncrypted == 1 && useCompressed == 1) {
		free(transformedColumn1);
		free(transformedColumn2);
	}
	else if (useEncrypted == 1 || useCompressed == 1) {
		free(transformedColumn2);
	}
	free(x);
	free(xFinal);
	free(residual);
}