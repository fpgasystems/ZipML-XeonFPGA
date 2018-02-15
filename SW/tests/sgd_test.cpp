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

#include "../src/zipml_sgd.h"

using namespace std;

#define VALUE_TO_INT_SCALER 0x00800000
#define NUM_VALUES_PER_LINE 16

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

	uint32_t stepSizeShifter = 21;
	uint32_t numEpochs = 100;

	// Do SGD
	zipml_sgd sgd_app(0, VALUE_TO_INT_SCALER, NUM_VALUES_PER_LINE);

	sgd_app.load_libsvm_data(pathToDataset, numSamples, numFeatures);

	sgd_app.a_normalize(0, 'c');
	sgd_app.b_normalize(0, 0, 0.0);

	sgd_app.print_samples(1);

	sgd_app.float_linreg_SGD(NULL, numEpochs, 1, 1.0/(1 << stepSizeShifter));
}