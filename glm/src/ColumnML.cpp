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

void ColumnML::WriteLogregPredictions(char* fileName, float* x) {
	ofstream ofs (fileName, ofstream::out);
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

float ColumnML::L2regularization(float* x, float lambda, AdditionalArguments* args) {
	float regularizer = 0;
	for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
		regularizer += x[j]*x[j];
	}
	regularizer *= (lambda*0.5)/args->m_numSamples;

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

float ColumnML::L2svmLoss(float* x, float lambda, AdditionalArguments* args) {
	float loss = 0;
	for(uint32_t i = args->m_firstSample; i < args->m_firstSample + args->m_numSamples; i++) {
		float dot = getDot(x, i);
		float temp = 1 - m_cstore->m_labels[i]*dot;
		if (temp > 0) {
			if (m_cstore->m_labels[i] > 0) {
				loss += args->m_costPos*temp*temp;
			}
			else {
				loss += args->m_costNeg*temp*temp;
			}
		}
	}
	loss /= (float)(2*args->m_numSamples);
	loss += L1regularization(x, lambda);

	return loss;
}

float ColumnML::LogregLoss(float* x, float lambda, AdditionalArguments* args) {
	float loss = 0;
	for(uint32_t i = args->m_firstSample; i < args->m_firstSample + args->m_numSamples; i++) {
		float dot = getDot(x, i);
		float prediction = 1.0/(1.0+exp(-dot));

		float positiveLoss = log(prediction);
		float negativeLoss = log(1 - prediction);

		// This check is to eliminate numerical unstableness when doing log, leading to inf.
		if (isinf(positiveLoss)) {
			positiveLoss = -std::numeric_limits<float>::max();
		}
		if (isinf(negativeLoss)) {
			negativeLoss = -std::numeric_limits<float>::max();
		}

		float temp = m_cstore->m_labels[i]*positiveLoss + (1-m_cstore->m_labels[i])*negativeLoss;
		loss += temp;

		if(isnan(loss) || isnan(dot) || isnan(prediction) || isnan(log(prediction)) || isnan(log(1 - prediction)) ) {
			cout << "--------------------------------------------------------------" << endl;
			cout << "isnan for sample " << i << endl;
			cout << "loss: " << loss << endl;
			cout << "dot: " << dot << endl;
			cout << "prediction: " << prediction << endl;
			cout << "label: " << m_cstore->m_labels[i] << endl;
			cout << "log(prediction): " << log(prediction) << endl;
			cout << "log(1 - prediction): " << log(1 - prediction) << endl;
			return 0;
		}
	}

	loss /= (float)args->m_numSamples;
	loss = -loss;

	// cout << "LogregLoss without L1 penalty: " << loss << endl;

	loss += L1regularization(x, lambda);

	return loss;
}

float ColumnML::LinregLoss(float* x, float lambda, AdditionalArguments* args) {
	float loss = 0;
	for(uint32_t i = args->m_firstSample; i < args->m_firstSample + args->m_numSamples; i++) {
		float dot = getDot(x, i);
		loss += (dot - m_cstore->m_labels[i])*(dot - m_cstore->m_labels[i]);
	}
	loss /= (float)(2*args->m_numSamples);
	loss += L1regularization(x, lambda);

	return loss;
}

uint32_t ColumnML::LogregAccuracy(float* x, AdditionalArguments* args) {
	uint32_t corrects = 0;
	for(uint32_t i = args->m_firstSample; i < args->m_firstSample + args->m_numSamples; i++) {
		float dot = getDot(x, i);
		float prediction = 1/(1+exp(-dot));
		if ( (prediction > 0.5 && m_cstore->m_labels[i] == 1.0) || (prediction < 0.5 && m_cstore->m_labels[i] == 0) ) {
			corrects++;
		}
	}

	return corrects;
}

uint32_t ColumnML::LinregAccuracy(float* x, AdditionalArguments* args) {
	uint32_t corrects = 0;
	for(uint32_t i = args->m_firstSample; i < args->m_firstSample + args->m_numSamples; i++) {
		float dot = getDot(x, i);
		if ( (dot > args->m_decisionBoundary && m_cstore->m_labels[i] == args->m_trueLabel) || (dot < args->m_decisionBoundary && m_cstore->m_labels[i] == args->m_falseLabel) ) {
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
	uint32_t numMinibatches = args->m_numSamples/minibatchSize;
	cout << "numMinibatches: " << numMinibatches << endl;
	uint32_t rest = args->m_numSamples - numMinibatches*minibatchSize;
	cout << "rest: " << rest << endl;

#ifdef PRINT_LOSS
	cout << "Initial loss: " << Loss(type, x, lambda, args) << endl;
#endif
#ifdef PRINT_ACCURACY
	cout << "Initial accuracy: " << Accuracy(type, x, args) << " corrects out of " << args->m_numSamples << endl;
#endif

	float scaledStepSize = stepSize/minibatchSize;
	float scaledLambda = stepSize*lambda;
	for(uint32_t epoch = 0; epoch < numEpochs; epoch++) {

		double start = get_time();

		for (uint32_t k = 0; k < numMinibatches; k++) {
#ifdef SGD_SHUFFLE
			uint32_t rand = 0;
			_rdrand32_step(&rand);
			uint32_t m = numMinibatches*((float)(rand-1)/(float)UINT_MAX);
#else
			uint32_t m = k;
#endif
			for (uint32_t i = 0; i < minibatchSize; i++) {
				updateGradient(type, gradient, x, m*minibatchSize + i, args);
			}
			for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
				float regularizer = (x[j] < 0) ? -scaledLambda : scaledLambda;
				if (args->m_constantStepSize) {
					x[j] -= scaledStepSize*gradient[j] + regularizer;
				}
				else {
					regularizer /= (float)(epoch+1);
					x[j] -= scaledStepSize/(float)(epoch+1)*gradient[j] + regularizer;
				}
				
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
			cout << Loss(type, x, lambda, args) << endl;
#endif
#ifdef PRINT_ACCURACY
			cout << Accuracy(type, x, args) << " corrects out of " << args->m_numSamples << endl;
#endif
		}
	}

	free(x);
	free(gradient);
}

#ifdef AVX2
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
	uint32_t numMinibatches = args->m_numSamples/minibatchSize;
	cout << "numMinibatches: " << numMinibatches << endl;
	uint32_t rest = args->m_numSamples - numMinibatches*minibatchSize;
	cout << "rest: " << rest << endl;

#ifdef PRINT_LOSS
	cout << "Initial loss: " << Loss(type, x, lambda, args) << endl;
#endif
#ifdef PRINT_ACCURACY
	cout << "Initial accuracy: " << Accuracy(type, x, args) << " corrects out of " << args->m_numSamples << endl;
#endif

	__m256 AVX_ones = _mm256_set1_ps(1.0);
	__m256 AVX_minusOnes = _mm256_set1_ps(-1.0);

	float scaledStepSize = stepSize/minibatchSize;
	float scaledLambda = stepSize*lambda;
	for(uint32_t epoch = 0; epoch < numEpochs; epoch++) {

		double start = get_time();

		for (uint32_t k = 0; k < numMinibatches; k++) {
#ifdef SGD_SHUFFLE
			uint32_t rand = 0;
			_rdrand32_step(&rand);
			uint32_t m = numMinibatches*((float)(rand-1)/(float)UINT_MAX);
#else
			uint32_t m = k;
#endif
			uint32_t minibatchOffset = m*minibatchSize;

			if (minibatchSize == 1) {
				float dot = getDot(x, minibatchOffset);
				if (type == logreg) {
					dot = 1/(1+exp(-dot));
				}
				dot = (dot-m_cstore->m_labels[minibatchOffset]);
				for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
					float regularizer = (x[j] < 0) ? -scaledLambda : scaledLambda;
					if (args->m_constantStepSize) {
						x[j] -= scaledStepSize*dot*m_cstore->m_samples[j][minibatchOffset] + regularizer;
					}
					else {
						regularizer /= ((float)(epoch+1));
						x[j] -= scaledStepSize/(float)(epoch+1)*dot*m_cstore->m_samples[j][minibatchOffset] + regularizer;
					}
				}
			}
			else {
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
					if (args->m_constantStepSize) {
						x[j] -= scaledStepSize*gradient[j] + regularizer;
					}
					else {
						regularizer /= ((float)(epoch+1));
						x[j] -= scaledStepSize/(float)(epoch+1)*gradient[j] + regularizer;
					}
					gradient[j] = 0.0;
				}
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
			cout << Loss(type, x, lambda, args) << endl;
#endif
#ifdef PRINT_ACCURACY
			cout << Accuracy(type, x, args) << " corrects out of " << args->m_numSamples << endl;
#endif
		}
	}

	free(x);
	free(gradient);
}

void ColumnML::AVXrowwise_SGD(
	ModelType type, 
	float* xHistory, 
	uint32_t numEpochs, 
	uint32_t minibatchSize, 
	float stepSize, 
	float lambda, 
	AdditionalArguments* args) 
{
	if (minibatchSize != 1) {
		cout << "For AVXrowwise_SGD minibatchSize must be 1!" << endl;
		exit(1);
	}

	cout << "AVXrowwise_SGD ---------------------------------------" << endl;

	float* x = (float*)aligned_alloc(64, m_cstore->m_numFeatures*sizeof(float));
	memset(x, 0, m_cstore->m_numFeatures*sizeof(float));
	float* gradient = (float*)aligned_alloc(64, m_cstore->m_numFeatures*sizeof(float));
	memset(gradient, 0, m_cstore->m_numFeatures*sizeof(float));

	float* samples = (float*)aligned_alloc(64, args->m_numSamples*m_cstore->m_numFeatures*sizeof(float));
	for (uint32_t i = 0; i < args->m_numSamples; i++) {
		for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
			samples[i*m_cstore->m_numFeatures + j] = m_cstore->m_samples[j][i];
		}
	}

#ifdef PRINT_LOSS
	cout << "Initial loss: " << Loss(type, x, lambda, args) << endl;
#endif
#ifdef PRINT_ACCURACY
	cout << "Initial accuracy: " << Accuracy(type, x, args) << " corrects out of " << args->m_numSamples << endl;
#endif

	__m256 AVX_ones = _mm256_set1_ps(1.0);
	__m256 AVX_zeros = _mm256_setzero_ps();
	__m256 AVX_minusOnes = _mm256_set1_ps(-1.0);

	float scaledStepSize = stepSize/minibatchSize;
	float scaledLambda = stepSize*lambda;
	__m256 AVX_scaledLambda = _mm256_set1_ps(scaledLambda);
	__m256 AVX_minusScaledLambda = _mm256_set1_ps(-scaledLambda);
	for(uint32_t epoch = 0; epoch < numEpochs; epoch++) {

		double start = get_time();

		for (uint32_t k = 0; k < args->m_numSamples; k++) {
#ifdef SGD_SHUFFLE
			uint32_t rand = 0;
			_rdrand32_step(&rand);
			uint32_t m = args->m_numSamples*((float)(rand-1)/(float)UINT_MAX);
#else
			uint32_t m = k;
#endif

			float dot = 0.0;
			if (m_cstore->m_numFeatures >= 8) {
				__m256 AVX_dot = _mm256_set1_ps(0.0);
				for (uint32_t j = 0; j < m_cstore->m_numFeatures-(m_cstore->m_numFeatures%8); j+=8) {
					__m256 AVX_x = _mm256_load_ps(x + j);
					__m256 AVX_samples = _mm256_load_ps(samples + m*m_cstore->m_numFeatures + j);
					AVX_dot = _mm256_fmadd_ps(AVX_x, AVX_samples, AVX_dot);
				}
				float gather[8];
				_mm256_store_ps(gather, AVX_dot);
				dot = gather[0] + gather[1] + gather[2] + gather[3] + gather[4] + gather[5] + gather[6] + gather[7];
			}
			for (uint32_t j = m_cstore->m_numFeatures-(m_cstore->m_numFeatures%8); j < m_cstore->m_numFeatures; j++) {
				dot += x[j]*samples[m*m_cstore->m_numFeatures + j];
			}
			if (type == logreg) {
				dot = 1/(1+exp(-dot));
			}
			if (args->m_constantStepSize) {
				dot = scaledStepSize*(dot-m_cstore->m_labels[m]);
			}
			else {
				dot = scaledStepSize/(float)(epoch+1)*(dot-m_cstore->m_labels[m]);
			}

			if (m_cstore->m_numFeatures >= 8) {
				__m256 AVX_dot = _mm256_set1_ps(dot);
				for (uint32_t j = 0; j < m_cstore->m_numFeatures-(m_cstore->m_numFeatures%8); j+=8) {
					__m256 AVX_samples = _mm256_load_ps(samples + m*m_cstore->m_numFeatures + j);

					__m256 AVX_x = _mm256_load_ps(x + j);
					__m256 AVX_regularizer = _mm256_and_ps(_mm256_cmp_ps(AVX_x, AVX_zeros, 1), AVX_minusScaledLambda);
					AVX_regularizer = _mm256_or_ps(AVX_regularizer, _mm256_and_ps(_mm256_cmp_ps(AVX_x, AVX_zeros, 13), AVX_scaledLambda) );

					if (!args->m_constantStepSize) {
						__m256 AVX_temp = _mm256_set1_ps((float)(epoch+1));
						AVX_regularizer = _mm256_div_ps(AVX_regularizer, AVX_temp);
					}

					AVX_x = _mm256_sub_ps(AVX_x, _mm256_add_ps(_mm256_mul_ps(AVX_dot, AVX_samples), AVX_regularizer));
					_mm256_store_ps(x+j, AVX_x);
				}
			}
			for (uint32_t j = m_cstore->m_numFeatures-(m_cstore->m_numFeatures%8); j < m_cstore->m_numFeatures; j++) {
				float regularizer = (x[j] < 0) ? -scaledLambda : scaledLambda;
				if (!args->m_constantStepSize) {
					regularizer /= (float)(epoch+1);
				}
				x[j] -= dot*m_cstore->m_samples[j][m] + regularizer;
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
			cout << Loss(type, x, lambda, args) << endl;
#endif
#ifdef PRINT_ACCURACY
			cout << Accuracy(type, x, args) << " corrects out of " << args->m_numSamples << endl;
#endif
		}
	}

	free(x);
	free(gradient);
	free(samples);
}
#endif

static inline void UpdateResidual(
	float* residual,
	uint32_t coordinate,
	uint32_t* minibatchIndex,
	uint32_t numMinibatchesAtATime,
	uint32_t minibatchSize,
	float* transformedColumn,
	float* xFinal)
{
	for (uint32_t l = 0; l < numMinibatchesAtATime; l++) {
		for (uint32_t i = 0; i < minibatchSize; i++) {
			if (coordinate == 0) {
				residual[minibatchIndex[l]*minibatchSize + i] = xFinal[coordinate]*transformedColumn[l*minibatchSize + i];
			}
			else {
				residual[minibatchIndex[l]*minibatchSize + i] += xFinal[coordinate]*transformedColumn[l*minibatchSize + i];
			}
		}
	}
}

#ifdef AVX2
static inline void AVX_UpdateResidual(
	float* residual,
	uint32_t coordinate,
	uint32_t minibatchIndex,
	uint32_t minibatchSize,
	float* transformedColumn,
	float* xFinal)
{
	for (uint32_t i = 0; i < minibatchSize; i+=8) {
		__m256 AVX_residual = _mm256_load_ps(residual + minibatchIndex*minibatchSize + i);
		__m256 AVX_xFinal = _mm256_set1_ps(xFinal[coordinate]);
		__m256 AVX_samples = _mm256_load_ps(transformedColumn + i);

		if (coordinate == 0) {
			AVX_residual = _mm256_mul_ps(AVX_xFinal, AVX_samples);
		}
		else {
			AVX_residual = _mm256_fmadd_ps(AVX_xFinal, AVX_samples, AVX_residual);
		}

		_mm256_store_ps(residual + minibatchIndex*minibatchSize + i, AVX_residual);
	}
}
#endif

static inline void DoStep(
	ModelType type,
	float* residual,
	uint32_t coordinate,
	uint32_t* minibatchIndex,
	uint32_t numMinibatchesAtATime,
	uint32_t minibatchSize,
	ColumnStore* cstore,
	float* transformedColumn,
	float* x,
	float scaledStepSize,
	float scaledLambda,
	double &dotTime,
	double &residualUpdateTime)
{
	double timeStamp1, timeStamp2, timeStamp3;

	timeStamp1 = get_time();
	float gradient = 0;
	for (uint32_t l = 0; l < numMinibatchesAtATime; l++) {
		for (uint32_t i = 0; i < minibatchSize; i++) {
			float dot = residual[minibatchIndex[l]*minibatchSize + i];
			if (type == logreg) {
				dot = 1/(1+exp(-dot));
			}
			gradient += (dot - cstore->m_labels[minibatchIndex[l]*minibatchSize + i])*transformedColumn[l*minibatchSize + i];
		}
	}
	timeStamp2 = get_time();
	dotTime += (timeStamp2-timeStamp1);

	std::cout << "----------------------" << std::endl;
	std::cout << "gradient: " << gradient << std::endl;

	float step = scaledStepSize*gradient;

	if (x[minibatchIndex[0]*cstore->m_numFeatures + coordinate] - step > scaledLambda) {
		step += scaledLambda;
	}
	else if (x[minibatchIndex[0]*cstore->m_numFeatures + coordinate] - step < -scaledLambda) {
		step -= scaledLambda;
	}
	else {
		step = x[minibatchIndex[0]*cstore->m_numFeatures + coordinate];
	}

	std::cout << "scaledStepSize: " << scaledStepSize << std::endl; 
	std::cout << "coordinate=" << coordinate << ", minibatchIndex=" << minibatchIndex[0] << ", step=" << step << std::endl;

	x[minibatchIndex[0]*cstore->m_numFeatures + coordinate] -= step;

	std::cout << "coordinate=" << coordinate << ", minibatchIndex=" << minibatchIndex[0] << ", x=" << x[minibatchIndex[0]*cstore->m_numFeatures + coordinate] << std::endl;

	for (uint32_t l = 0; l < numMinibatchesAtATime; l++) {
		for (uint32_t i = 0; i < minibatchSize; i++) {
			residual[minibatchIndex[l]*minibatchSize + i] -= step*transformedColumn[l*minibatchSize + i];
		}
	}
	timeStamp3 = get_time();
	residualUpdateTime += (timeStamp3-timeStamp2);
}

#ifdef AVX2
static inline float AVX_GetStep(
	ModelType type,
	float* residual,
	uint32_t coordinate,
	uint32_t minibatchIndex,
	uint32_t minibatchSize,
	ColumnStore* cstore,
	float* transformedColumn,
	float scaledStepSize,
	double &dotTime)
{
	double timeStamp1, timeStamp2;
	__m256 AVX_ones = _mm256_set1_ps(1.0);
	__m256 AVX_minusOnes = _mm256_set1_ps(-1.0);

	timeStamp1 = get_time();
	__m256 AVX_gradient = _mm256_setzero_ps();
	__m256 AVX_error;
	for (uint32_t i = 0; i < minibatchSize; i+=8) {

		__m256 AVX_samples = _mm256_load_ps(transformedColumn + i);
		__m256 AVX_labels = _mm256_load_ps(cstore->m_labels + minibatchIndex*minibatchSize + i);
		__m256 AVX_residual = _mm256_load_ps(residual + minibatchIndex*minibatchSize + i);

		if (type == logreg) {
			AVX_residual = _mm256_mul_ps(AVX_minusOnes, AVX_residual);
			AVX_residual = exp256_ps(AVX_residual);
			AVX_residual = _mm256_add_ps(AVX_ones, AVX_residual);
			AVX_residual = _mm256_div_ps(AVX_ones, AVX_residual);
		}

		AVX_error = _mm256_sub_ps(AVX_residual, AVX_labels);
		AVX_gradient = _mm256_fmadd_ps(AVX_samples, AVX_error, AVX_gradient);
	}

	float gradientReduce[8];
	_mm256_store_ps(gradientReduce, AVX_gradient);
	gradientReduce[0] = (gradientReduce[0] + 
						gradientReduce[1] + 
						gradientReduce[2] + 
						gradientReduce[3] + 
						gradientReduce[4] + 
						gradientReduce[5] + 
						gradientReduce[6] + 
						gradientReduce[7]);

	float step = scaledStepSize*gradientReduce[0];
	
	timeStamp2 = get_time();
	dotTime += (timeStamp2-timeStamp1);

	return step;
}

static inline void AVX_ApplyStep(
	float step,
	float* residual,
	uint32_t minibatchIndex,
	uint32_t minibatchSize,
	float* transformedColumn,
	double &residualUpdateTime)
{
	__m256 AVX_step = _mm256_set1_ps(step);

	double timeStamp1, timeStamp2;
	timeStamp1 = get_time();

	for (uint32_t i = 0; i < minibatchSize; i+=8) {
		__m256 AVX_samples = _mm256_load_ps(transformedColumn + i);
		__m256 AVX_residual = _mm256_load_ps(residual + minibatchIndex*minibatchSize + i);
		AVX_residual = _mm256_fmadd_ps(AVX_samples, AVX_step, AVX_residual);

		_mm256_store_ps(residual + minibatchIndex*minibatchSize + i, AVX_residual);
	}

	timeStamp2 = get_time();
	residualUpdateTime += (timeStamp2-timeStamp1);
}
#endif

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
	uint32_t numMinibatches = args->m_numSamples/minibatchSize;
	cout << "numMinibatches: " << numMinibatches << endl;
	uint32_t rest = args->m_numSamples - numMinibatches*minibatchSize;
	cout << "rest: " << rest << endl;

	float* residual = (float*)aligned_alloc(64, args->m_numSamples*sizeof(float));
	memset(residual, 0, args->m_numSamples*sizeof(float));
	float* x = (float*)aligned_alloc(64, numMinibatches*m_cstore->m_numFeatures*sizeof(float));
	memset(x, 0, numMinibatches*m_cstore->m_numFeatures*sizeof(float));
	float* xFinal = (float*)aligned_alloc(64, m_cstore->m_numFeatures*sizeof(float));
	memset(xFinal, 0, m_cstore->m_numFeatures*sizeof(float));
	
#ifdef PRINT_LOSS
	cout << "Initial loss: " << Loss(type, xFinal, lambda, args) << endl;
#endif
#ifdef PRINT_ACCURACY
	cout << "Initial accuracy: " << Accuracy(type, xFinal, args) << " corrects out of " << args->m_numSamples << endl;
#endif

	float* transformedColumn1 = nullptr;
	float* transformedColumn2 = nullptr;
	if (useEncrypted && useCompressed) {
		transformedColumn1 = (float*)aligned_alloc(64, numMinibatchesAtATime*minibatchSize*sizeof(float));
		transformedColumn2 = (float*)aligned_alloc(64, numMinibatchesAtATime*minibatchSize*sizeof(float));
	}
	else if ( numMinibatchesAtATime > 1 ) {
		transformedColumn2 = (float*)aligned_alloc(64, numMinibatchesAtATime*minibatchSize*sizeof(float));
	}

	float scaledStepSize = stepSize/minibatchSize;
	float scaledLambda = stepSize*lambda;
	uint32_t epoch_index = 0;
	for(uint32_t epoch = 0; epoch < numEpochs + (numEpochs/residualUpdatePeriod); epoch++) {

		double decryptionTime = 0;
		double decompressionTime = 0;
		double dotTime = 0;
		double residualUpdateTime = 0;
		double start = get_time();

		for (uint32_t k = 0; k < numMinibatches/numMinibatchesAtATime; k++) {

			if (numMinibatchesAtATime > 1) {
				uint32_t m[numMinibatchesAtATime];
				for (uint32_t l = 0; l < numMinibatchesAtATime; l++) {
					m[l] = l*(numMinibatches/numMinibatchesAtATime) + k;
				}
				for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {

					m_cstore->ReturnDecompressedAndDecrypted(transformedColumn1, transformedColumn2, j, m, numMinibatchesAtATime, minibatchSize, useEncrypted, useCompressed, toIntegerScaler, decryptionTime, decompressionTime);

					if ( (epoch+1)%(residualUpdatePeriod+1) == 0 ) {
						UpdateResidual(residual, j, m, numMinibatchesAtATime, minibatchSize, transformedColumn2, xFinal);
					}
					else {
						DoStep(type, residual, j, m, numMinibatchesAtATime, minibatchSize, m_cstore, transformedColumn2, x, scaledStepSize, scaledLambda, dotTime, residualUpdateTime);
					}
				}
			}
			else {
				uint32_t m = k;

				for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {

					m_cstore->ReturnDecompressedAndDecrypted(transformedColumn1, transformedColumn2, j, &m, 1, minibatchSize, useEncrypted, useCompressed, toIntegerScaler, decryptionTime, decompressionTime);

					if ( (epoch+1)%(residualUpdatePeriod+1) == 0 ) {
						UpdateResidual(residual, j, &m, 1, minibatchSize, transformedColumn2, xFinal);
					}
					else {
						DoStep(type, residual, j, &m, 1, minibatchSize, m_cstore, transformedColumn2, x, scaledStepSize, scaledLambda, dotTime, residualUpdateTime);
					}
				}
			}
		}

		if ( (epoch+1)%(residualUpdatePeriod+1) == 0 ) {
			cout << "--> PERFORMED RESIDUAL UPDATE !!!" << endl;
		}
		else {
			double timeStamp1 = get_time();
			GetAveragedX(numMinibatches, numMinibatchesAtATime, m_cstore, xFinal, x);
			double end = get_time();
#ifdef PRINT_TIMING
			cout << "decryptionTime: " << decryptionTime << endl;
			cout << "decompressionTime: " << decompressionTime << endl;
			cout << "dotTime: " << dotTime << endl;
			cout << "residualUpdateTime: " << residualUpdateTime << endl;
			cout << "x_average_time: " << end-timeStamp1 << endl;
			cout << "Time for one epoch: " << end-start << endl;
#endif
			if (xHistory != nullptr) {
				for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
					xHistory[epoch_index*m_cstore->m_numFeatures + j] = xFinal[j];
				}
				epoch_index++;
			}
			else {
#ifdef PRINT_LOSS
				cout << Loss(type, xFinal, lambda, args) << endl;
#endif
#ifdef PRINT_ACCURACY
				cout << Accuracy(type, xFinal, args) << " corrects out of " << args->m_numSamples << endl;
#endif
			}
		}
	}

	if (useEncrypted && useCompressed) {
		free(transformedColumn1);
		free(transformedColumn2);
	}
	else if (numMinibatchesAtATime > 1) {
		free(transformedColumn2);
	}
	free(x);
	free(xFinal);
	free(residual);
}

#ifdef AVX2
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
	uint32_t numMinibatches = args->m_numSamples/minibatchSize;
	cout << "numMinibatches: " << numMinibatches << endl;
	uint32_t rest = args->m_numSamples - numMinibatches*minibatchSize;
	cout << "rest: " << rest << endl;

	float* residual = (float*)aligned_alloc(64, args->m_numSamples*sizeof(float));
	memset(residual, 0, args->m_numSamples*sizeof(float));
	float* x = (float*)aligned_alloc(64, numMinibatches*m_cstore->m_numFeatures*sizeof(float));
	memset(x, 0, numMinibatches*m_cstore->m_numFeatures*sizeof(float));
	float* xFinal = (float*)aligned_alloc(64, m_cstore->m_numFeatures*sizeof(float));
	memset(xFinal, 0, m_cstore->m_numFeatures*sizeof(float));

#ifdef PRINT_LOSS
	cout << "Initial loss: " << Loss(type, xFinal, lambda, args) << endl;
#endif
#ifdef PRINT_ACCURACY
	cout << "Initial accuracy: " << Accuracy(type, xFinal, args) << " corrects out of " << args->m_numSamples << endl;
#endif

	float* transformedColumn1 = nullptr;
	float* transformedColumn2 = nullptr;
	if (useEncrypted && useCompressed) {
		transformedColumn1 = (float*)aligned_alloc(64, minibatchSize*sizeof(float));
		transformedColumn2 = (float*)aligned_alloc(64, minibatchSize*sizeof(float));
	}
	else if (useEncrypted || useCompressed) {
		transformedColumn2 = (float*)aligned_alloc(64, minibatchSize*sizeof(float));
	}

	float scaledStepSize = -stepSize/(float)minibatchSize;
	float scaledLambda = -stepSize*lambda;
	__m256 AVX_ones = _mm256_set1_ps(1.0);
	__m256 AVX_minusOnes = _mm256_set1_ps(-1.0);
	uint32_t epoch_index = 0;
	for(uint32_t epoch = 0; epoch < numEpochs + (numEpochs/residualUpdatePeriod); epoch++) {
		double decryptionTime = 0;
		double decompressionTime = 0;
		double dotTime = 0;
		double residualUpdateTime = 0;
		double start = get_time();

		__m256 AVX_residual;
		__m256 AVX_samples;
		__m256 AVX_labels;

		for (uint32_t m = 0; m < numMinibatches; m++) {
			for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {

#ifdef SCD_SHUFFLE
				uint32_t rand = 0;
				_rdrand32_step(&rand);
				uint32_t coordinate = (m_cstore->m_numFeatures-1)*((float)(rand-1)/(float)UINT_MAX);
#else
				uint32_t coordinate = j;
#endif

				m_cstore->ReturnDecompressedAndDecrypted(transformedColumn1, transformedColumn2, coordinate, &m, 1, minibatchSize, useEncrypted, useCompressed, toIntegerScaler, decryptionTime, decompressionTime);

				if ( (epoch+1)%(residualUpdatePeriod+1) == 0 ) {
					AVX_UpdateResidual(residual, coordinate, m, minibatchSize, transformedColumn2, xFinal);
				}
				else {
					float step = AVX_GetStep(type, residual, j, m, minibatchSize, m_cstore, transformedColumn2, scaledStepSize, dotTime);

					if (x[m*m_cstore->m_numFeatures + coordinate] + step > -scaledLambda) {
						step += scaledLambda;
					}
					else if (x[m*m_cstore->m_numFeatures + coordinate] + step < scaledLambda) {
						step -= scaledLambda;
					}
					else {
						step = -x[m*m_cstore->m_numFeatures + coordinate];
					}
					x[m*m_cstore->m_numFeatures + coordinate] += step;

					AVX_ApplyStep(step, residual, m, minibatchSize, transformedColumn2, residualUpdateTime);
				}
			}
		}

		if ( (epoch+1)%(residualUpdatePeriod+1) == 0 ) {
#ifdef PRINT_TIMING
			double end = get_time();
			cout << "--> PERFORMED RESIDUAL UPDATE !!!" << endl;
			cout << "Time for one epoch: " << end-start << endl;
#endif
		}
		else {
			double timeStamp1 = get_time();
			GetAveragedX(numMinibatches, 1, m_cstore, xFinal, x);
			double end = get_time();
#ifdef PRINT_TIMING
			cout << "decryptionTime: " << decryptionTime << endl;
			cout << "decompressionTime: " << decompressionTime << endl;
			cout << "dotTime: " << dotTime << endl;
			cout << "residualUpdateTime: " << residualUpdateTime << endl;
			cout << "x_average_time: " << end-timeStamp1 << endl;
			cout << "Time for one epoch: " << end-start << endl;
#endif
			if (xHistory != nullptr) {
				for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
					xHistory[epoch_index*m_cstore->m_numFeatures + j] = xFinal[j];
				}
				epoch_index++;
			}
			else {
#ifdef PRINT_LOSS
				cout << Loss(type, xFinal, lambda, args) << endl;
#endif
#ifdef PRINT_ACCURACY
				cout << Accuracy(type, xFinal, args) << " corrects out of " << args->m_numSamples << endl;
#endif
			}
		}
	}

	if (useEncrypted && useCompressed) {
		free(transformedColumn1);
		free(transformedColumn2);
	}
	else if (useEncrypted || useCompressed) {
		free(transformedColumn2);
	}
	free(x);
	free(xFinal);
	free(residual);
}

typedef struct {
	pthread_barrier_t* m_barrier;
	uint32_t m_tid;
	ColumnML* m_obj;

	ModelType m_type;
	bool m_doRealSCD;
	float* m_xHistory;
	uint32_t m_numEpochs;
	uint32_t m_minibatchSize;
	float m_stepSize;
	float m_lambda;
	uint32_t m_residualUpdatePeriod;
	bool m_useCompressed;
	bool m_useEncrypted;
	uint32_t m_toIntegerScaler;
	AdditionalArguments* m_args;
	uint32_t m_numThreads;

	float* m_x;
	float* m_xFinal;
	float* m_residual;
	float* m_stepsFromThreads;
	uint32_t m_startingBatch;
	uint32_t m_numBatchesToProcess;
	uint32_t m_numMinibatches;
	
	double m_decryptionTime;
	double m_decompressionTime;
	double m_dotTime;
	double m_residualUpdateTime;
	double m_averageEpochTime;
} batch_thread_data;

void* batchThread(void* args) {
	batch_thread_data* r = (batch_thread_data*)args;

	ColumnStore* cstore = r->m_obj->m_cstore;

	float* transformedColumn1 = nullptr;
	float* transformedColumn2 = nullptr;
	if (r->m_useEncrypted && r->m_useCompressed) {
		transformedColumn1 = (float*)aligned_alloc(64, r->m_minibatchSize*sizeof(float));
		transformedColumn2 = (float*)aligned_alloc(64, r->m_minibatchSize*sizeof(float));
	}
	else if (r->m_useEncrypted || r->m_useCompressed) {
		transformedColumn2 = (float*)aligned_alloc(64, r->m_minibatchSize*sizeof(float));
	}

	double start, end, epochTimes;
	epochTimes = 0;

	float scaledStepSize;
	if (r->m_doRealSCD) {
		scaledStepSize = -r->m_stepSize/((float)r->m_minibatchSize*(float)r->m_numMinibatches);
	}
	else {
		scaledStepSize = -r->m_stepSize/(float)r->m_minibatchSize;
	}
	float scaledLambda = -r->m_stepSize*r->m_lambda;
	__m256 AVX_ones = _mm256_set1_ps(1.0);
	__m256 AVX_minusOnes = _mm256_set1_ps(-1.0);

	uint32_t epoch_index = 0;
	for(uint32_t epoch = 0; epoch < r->m_numEpochs + (r->m_numEpochs/r->m_residualUpdatePeriod); epoch++) {

		r->m_decryptionTime = 0;
		r->m_decompressionTime = 0;
		r->m_dotTime = 0;
		r->m_residualUpdateTime = 0;
		
		double timeStamp1, timeStamp2, timeStamp3;
		pthread_barrier_wait(r->m_barrier);
		double start = get_time();

		__m256 AVX_residual;
		__m256 AVX_samples;
		__m256 AVX_labels;

		if (r->m_doRealSCD) {
			for (uint32_t j = 0; j < cstore->m_numFeatures; j++) {
				r->m_stepsFromThreads[r->m_tid] = 0;
				pthread_barrier_wait(r->m_barrier);
				for (uint32_t m = r->m_startingBatch; m < r->m_startingBatch + r->m_numBatchesToProcess; m++) {
					cstore->ReturnDecompressedAndDecrypted(transformedColumn1, transformedColumn2, j, &m, 1, r->m_minibatchSize, r->m_useEncrypted, r->m_useCompressed, r->m_toIntegerScaler, r->m_decryptionTime, r->m_decompressionTime);

					float step = AVX_GetStep(r->m_type, r->m_residual, j, m, r->m_minibatchSize, cstore, transformedColumn2, scaledStepSize, r->m_dotTime);
					r->m_stepsFromThreads[r->m_tid] += step;
				}
				
				pthread_barrier_wait(r->m_barrier);
				if (r->m_tid == 0) {
					for (uint32_t t = 1; t < r->m_numThreads; t++) {
						r->m_stepsFromThreads[0] += r->m_stepsFromThreads[t];
					}

					if (r->m_xFinal[j] + r->m_stepsFromThreads[0] > -scaledLambda) {
						r->m_stepsFromThreads[0] += scaledLambda;
					}
					else if (r->m_xFinal[j] + r->m_stepsFromThreads[0] < scaledLambda) {
						r->m_stepsFromThreads[0] -= scaledLambda;
					}
					else {
						r->m_stepsFromThreads[0] = -r->m_xFinal[j];
					}

					for (uint32_t t = 0; t < r->m_numThreads; t++) {
						r->m_stepsFromThreads[t] = r->m_stepsFromThreads[0];
					}
					r->m_xFinal[j] += r->m_stepsFromThreads[0];
				}
				pthread_barrier_wait(r->m_barrier);

				for (uint32_t m = r->m_startingBatch; m < r->m_startingBatch + r->m_numBatchesToProcess; m++) {

					cstore->ReturnDecompressedAndDecrypted(transformedColumn1, transformedColumn2, j, &m, 1, r->m_minibatchSize, r->m_useEncrypted, r->m_useCompressed, r->m_toIntegerScaler, r->m_decryptionTime, r->m_decompressionTime);

					AVX_ApplyStep(r->m_stepsFromThreads[r->m_tid], r->m_residual, m, r->m_minibatchSize, transformedColumn2, r->m_residualUpdateTime);
				}
			}
			if (r->m_tid == 0) {
				end = get_time();
				epochTimes += (end-start);
#ifdef PRINT_TIMING
				cout << "Time for one epoch: " << end-start << endl;
#endif
				if (r->m_xHistory != nullptr) {
					for (uint32_t j = 0; j < cstore->m_numFeatures; j++) {
						r->m_xHistory[epoch*cstore->m_numFeatures + j] = r->m_xFinal[j];
					}
				}
				else {
#ifdef PRINT_LOSS
					cout << r->m_obj->Loss(r->m_type, r->m_xFinal, r->m_lambda, r->m_args) << endl;
#endif
#ifdef PRINT_ACCURACY
					cout << r->m_obj->Accuracy(r->m_type, r->m_xFinal, r->m_args) << " corrects out of " << cstore->m_numSamples << endl;
#endif
				}
			}
		}
		else {
			if ( (epoch+1)%(r->m_residualUpdatePeriod+1) == 0 ) {
				for (uint32_t m = r->m_startingBatch; m < r->m_startingBatch + r->m_numBatchesToProcess; m++) {
					for (uint32_t j = 0; j < cstore->m_numFeatures; j++) {
						cstore->ReturnDecompressedAndDecrypted(transformedColumn1, transformedColumn2, j, &m, 1, r->m_minibatchSize, r->m_useEncrypted, r->m_useCompressed, r->m_toIntegerScaler, r->m_decryptionTime, r->m_decompressionTime);
						AVX_UpdateResidual(r->m_residual, j, m, r->m_minibatchSize, transformedColumn2, r->m_xFinal);
					}
				}
			}
			else {
				for (uint32_t m = r->m_startingBatch; m < r->m_startingBatch + r->m_numBatchesToProcess; m++) {
					for (uint32_t j = 0; j < cstore->m_numFeatures; j++) {
#ifdef SCD_SHUFFLE
						uint32_t rand = 0;
						_rdrand32_step(&rand);
						uint32_t coordinate = (cstore->m_numFeatures-1)*((float)(rand-1)/(float)UINT_MAX);
#else
						uint32_t coordinate = j;
#endif
						cstore->ReturnDecompressedAndDecrypted(transformedColumn1, transformedColumn2, coordinate, &m, 1, r->m_minibatchSize, r->m_useEncrypted, r->m_useCompressed, r->m_toIntegerScaler, r->m_decryptionTime, r->m_decompressionTime);
						
						float step = AVX_GetStep(r->m_type, r->m_residual, coordinate, m, r->m_minibatchSize, cstore, transformedColumn2, scaledStepSize, r->m_dotTime);
						
						if (r->m_x[m*cstore->m_numFeatures + coordinate] + step > -scaledLambda) {
							step += scaledLambda;
						}
						else if (r->m_x[m*cstore->m_numFeatures + coordinate] + step < scaledLambda) {
							step -= scaledLambda;
						}
						else {
							step = -r->m_x[m*cstore->m_numFeatures + coordinate];
						}

						r->m_x[m*cstore->m_numFeatures + coordinate] += step;
						AVX_ApplyStep(step, r->m_residual, m, r->m_minibatchSize, transformedColumn2, r->m_residualUpdateTime);	
					}
				}
			}

			pthread_barrier_wait(r->m_barrier);
			timeStamp1 = get_time();

			if (r->m_tid == 0) {
				if ( (epoch+1)%(r->m_residualUpdatePeriod+1) == 0 ) {
					end = get_time();
					epochTimes += (end-start);
#ifdef PRINT_TIMING
					cout << "--> PERFORMED RESIDUAL UPDATE !!!" << endl;
					cout << "Time for one global residual update: " << end-start << endl;
#endif
				}
				else {
					ColumnML::GetAveragedX(r->m_numMinibatches, 1, cstore, r->m_xFinal, r->m_x);
					end = get_time();
					epochTimes += (end-start);
#ifdef PRINT_TIMING
					cout << "Time for one epoch: " << end-start << endl;
#endif
					if (r->m_xHistory != nullptr) {
						for (uint32_t j = 0; j < cstore->m_numFeatures; j++) {
							r->m_xHistory[epoch_index*cstore->m_numFeatures + j] = r->m_xFinal[j];
						}
						epoch_index++;
					}
					else {
#ifdef PRINT_LOSS
						cout << r->m_obj->Loss(r->m_type, r->m_xFinal, r->m_lambda, r->m_args) << endl;
#endif
#ifdef PRINT_ACCURACY
						cout << r->m_obj->Accuracy(r->m_type, r->m_xFinal, r->m_args) << " corrects out of " << cstore->m_numSamples << endl;
#endif
					}
				}
			}
		}
	}
	if (r->m_tid == 0){
		r->m_averageEpochTime = epochTimes/r->m_numEpochs;
		cout << "avg epoch time: " << r->m_averageEpochTime << endl;
	}

	if (r->m_useEncrypted && r->m_useCompressed) {
		free(transformedColumn1);
		free(transformedColumn2);
	}
	else if (r->m_useEncrypted || r->m_useCompressed) {
		free(transformedColumn2);
	}

	return nullptr;
}

double ColumnML::AVXmulti_SCD (
	ModelType type,
	bool doRealSCD,
	float* xHistory,
	uint32_t numEpochs,
	uint32_t minibatchSize,
	float stepSize,
	float lambda,
	uint32_t residualUpdatePeriod,
	bool useEncrypted,
	bool useCompressed,
	uint32_t toIntegerScaler,
	AdditionalArguments* args,
	uint32_t numThreads)
{
	if (minibatchSize%8 == 8) {
		cout << "For AVX minibatchSize%8 must be 0!" << endl;
		exit(1);
	}
	if (numThreads > MAX_NUM_THREADS) {
		cout << "numThreads: " << numThreads << " is not possible" << endl;
		exit(1);
	}
	cout << "AVXmulti_SCD with " << numThreads << " threads running..." << endl;
	cout << "useEncrypted: " << ((useEncrypted) ? 1 : 0) << endl;
	cout << "useCompressed: " << ((useCompressed) ? 1 : 0) << endl;

	pthread_barrier_t barrier;
	pthread_attr_t attr;
	pthread_t threads[MAX_NUM_THREADS];
	batch_thread_data thread_args[MAX_NUM_THREADS];
	cpu_set_t set;

	float* residual = (float*)aligned_alloc(64, args->m_numSamples*sizeof(float));
	memset(residual, 0, args->m_numSamples*sizeof(float));

	uint32_t numMinibatches = args->m_numSamples/minibatchSize;
	cout << "numMinibatches: " << numMinibatches << endl;
	uint32_t rest = args->m_numSamples - numMinibatches*minibatchSize;
	cout << "rest: " << rest << endl;

	float* x = (float*)aligned_alloc(64, numMinibatches*m_cstore->m_numFeatures*sizeof(float));
	memset(x, 0, numMinibatches*m_cstore->m_numFeatures*sizeof(float));
	float stepsFromThreads[MAX_NUM_THREADS];

	float* xFinal= (float*)aligned_alloc(64, m_cstore->m_numFeatures*sizeof(float));
	memset(xFinal, 0, m_cstore->m_numFeatures*sizeof(float));

#ifdef PRINT_LOSS
	cout << "Initial loss: " << Loss(type, x, lambda, args) << endl;
#endif
#ifdef PRINT_ACCURACY
	cout << "Initial accuracy: " << Accuracy(type, x, args) << " corrects out of " << args->m_numSamples << endl;
#endif

	uint32_t startingBatch = 0;
	pthread_barrier_init(&barrier, NULL, numThreads);
	pthread_attr_init(&attr);
	CPU_ZERO(&set);
	for (uint32_t n = 0; n < numThreads; n++) {
		CPU_SET(n, &set);
		pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &set);

		thread_args[n].m_barrier = &barrier;
		thread_args[n].m_tid = n;
		thread_args[n].m_obj = this;

		thread_args[n].m_type = type;
		thread_args[n].m_doRealSCD = doRealSCD;
		thread_args[n].m_xHistory = xHistory;
		thread_args[n].m_numEpochs = numEpochs;
		thread_args[n].m_minibatchSize = minibatchSize;
		thread_args[n].m_stepSize = stepSize;
		thread_args[n].m_lambda = lambda;
		thread_args[n].m_residualUpdatePeriod = residualUpdatePeriod;
		thread_args[n].m_useCompressed = useCompressed;
		thread_args[n].m_useEncrypted = useEncrypted;
		thread_args[n].m_toIntegerScaler = toIntegerScaler;
		thread_args[n].m_args = args;
		thread_args[n].m_numThreads = numThreads;

		thread_args[n].m_x = x;
		thread_args[n].m_xFinal = xFinal;
		thread_args[n].m_residual = residual;
		thread_args[n].m_stepsFromThreads = stepsFromThreads;
		thread_args[n].m_startingBatch = startingBatch;

		uint32_t temp = numMinibatches/numThreads + (numMinibatches%numThreads > 0);
		if (temp > numMinibatches - startingBatch) {
			thread_args[n].m_numBatchesToProcess = numMinibatches - startingBatch;
		}
		else {
			thread_args[n].m_numBatchesToProcess = temp;
		}
		thread_args[n].m_numMinibatches = numMinibatches;

		startingBatch += thread_args[n].m_numBatchesToProcess;
		cout << "Thread " << n << ", numBatchesToProcess: " << thread_args[n].m_numBatchesToProcess << endl;
	}
	for (uint32_t n = 0; n < numThreads; n++) {
		pthread_create(&threads[n], &attr, batchThread, (void*)&thread_args[n]);
	}
	for (uint32_t n = 0; n < numThreads; n++) {
		pthread_join(threads[n], NULL);
	}

	double decryptionTime = 0;
	double decompressionTime = 0;
	double dotTime = 0;
	double residualUpdateTime = 0;
	for (uint32_t n = 0; n < numThreads; n++) {
		decryptionTime += thread_args[n].m_decryptionTime;
		decompressionTime += thread_args[n].m_decompressionTime;
		dotTime += thread_args[n].m_dotTime;
		residualUpdateTime += thread_args[n].m_residualUpdateTime;
	}
	cout << "decryptionTime: " << decryptionTime/numThreads << endl;
	cout << "decompressionTime: " << decompressionTime/numThreads << endl;
	cout << "dotTime: " << dotTime/numThreads << endl;
	cout << "residualUpdateTime: " << residualUpdateTime/numThreads << endl;

	free(x);
	free(xFinal);
	free(residual);

	return thread_args[0].m_averageEpochTime;
}
#endif