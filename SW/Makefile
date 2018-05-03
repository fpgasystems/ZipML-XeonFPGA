# Copyright (C) 2017 Kaan Kara - Systems Group, ETH Zurich

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.

# You should have received a copy of the GNU Affero General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
#*************************************************************************

CXX			= g++
CPPFLAGS	= -O3 -std=c++11 -march=native -Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function -Wno-reorder -Wno-narrowing -DAVX2
LDFLAGS		= -lpthread -lOSAL -lAAS
COMMON		= ./driver/iFPGA.cpp ./driver/RuntimeClient.cpp 

.PHONY: all clean distclean

all: all_scd_tests

all_scd_tests: ./tests/period_analysis.cpp ./tests/decomprate_analysis.cpp ./tests/numsamples_analysis.cpp ./tests/numfeatures_analysis.cpp ./tests/scratch.cpp ./tests/cpu_test_synthetic.cpp ./tests/fscd_test_synthetic.cpp ./tests/fscd_test_raw.cpp ./tests/prediction_test.cpp ./tests/scd_vs_miniscd_test.cpp $(COMMON) ./src/scd.h
	# $(CXX) -D HARPv2 -I$(AALSDK)/include $(CPPFLAGS) ./tests/period_analysis.cpp $(COMMON) -o period_analysis -L$(AALSDK)/lib $(LDFLAGS) -laalrt
	# $(CXX) -D HARPv2 -I$(AALSDK)/include $(CPPFLAGS) ./tests/decomprate_analysis.cpp $(COMMON) -o decomprate_analysis -L$(AALSDK)/lib $(LDFLAGS) -laalrt
	# $(CXX) -D HARPv2 -I$(AALSDK)/include $(CPPFLAGS) ./tests/numsamples_analysis.cpp $(COMMON) -o numsamples_analysis -L$(AALSDK)/lib $(LDFLAGS) -laalrt
	$(CXX) -D HARPv2 -I$(AALSDK)/include $(CPPFLAGS) ./tests/numfeatures_analysis.cpp $(COMMON) -o numfeatures_analysis -L$(AALSDK)/lib $(LDFLAGS) -laalrt
	# $(CXX) -D HARPv2 -I$(AALSDK)/include $(CPPFLAGS) ./tests/scratch.cpp $(COMMON) -o scratch -L$(AALSDK)/lib $(LDFLAGS) -laalrt
	# $(CXX) -D HARPv2 -I$(AALSDK)/include $(CPPFLAGS) ./tests/cpu_test_synthetic.cpp $(COMMON) -o cpu_test_synthetic -L$(AALSDK)/lib $(LDFLAGS) -laalrt
	# $(CXX) -D HARPv2 -I$(AALSDK)/include $(CPPFLAGS) ./tests/fscd_test_synthetic.cpp $(COMMON) -o fscd_test_synthetic -L$(AALSDK)/lib $(LDFLAGS) -laalrt
	# $(CXX) -D HARPv2 -I$(AALSDK)/include $(CPPFLAGS) ./tests/fscd_test_raw.cpp $(COMMON) -o fscd_test_raw -L$(AALSDK)/lib $(LDFLAGS) -laalrt
	# $(CXX) -D HARPv2 -I$(AALSDK)/include $(CPPFLAGS) ./tests/prediction_test.cpp $(COMMON) -o prediction_test -L$(AALSDK)/lib $(LDFLAGS) -laalrt
	# $(CXX) -D HARPv2 -I$(AALSDK)/include $(CPPFLAGS) ./tests/scd_vs_miniscd_test.cpp $(COMMON) -o scd_vs_miniscd_test -L$(AALSDK)/lib $(LDFLAGS) -laalrt

sgd_test: ./tests/sgd_test.cpp ./driver/iFPGA.cpp ./driver/RuntimeClient.cpp ./src/zipml_sgd.h
	$(CXX) -D HARPv1 -I$(AALSDK)/include $(CPPFLAGS) ./tests/sgd_test.cpp ./driver/iFPGA.cpp ./driver/RuntimeClient.cpp -o sgd_test -L$(AALSDK)/lib $(LDFLAGS) -lxlrt

zipml: ./tests/main.cpp ./driver/iFPGA.cpp ./driver/RuntimeClient.cpp
	$(CXX) -D HARPv1 -I$(AALSDK)/include $(CPPFLAGS) ./tests/main.cpp ./driver/iFPGA.cpp ./driver/RuntimeClient.cpp -o zipmlfpga -L$(AALSDK)/lib $(LDFLAGS) -lxlrt

clean:
	rm period_analysis
	rm decomprate_analysis
	rm numsamples_analysis
	rm numfeatures_analysis
	rm scratch
	rm cpu_test_synthetic
	rm fscd_test_synthetic
	rm fscd_test_raw
	rm prediction_test
	rm scd_vs_miniscd_test