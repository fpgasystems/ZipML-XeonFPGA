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

CPPFLAGS	= -O3 -std=c++11 -march=native -Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function -Wno-reorder
CXX			= g++
LDFLAGS		= -lpthread -lOSAL -lAAS

all: scd_test # sgd_test zipml

scd_test: ./tests/scd_test.cpp ./src/scd.h
	$(CXX) -D HARPv2 -I$(AALSDK)/include $(CPPFLAGS) ./tests/scd_test.cpp -o scd_test -L$(AALSDK)/lib $(LDFLAGS) -laalrt

sgd_test: ./tests/sgd_test.cpp ./driver/iFPGA.cpp ./driver/RuntimeClient.cpp ./src/zipml_sgd.h
	$(CXX) -D HARPv1 -I$(AALSDK)/include $(CPPFLAGS) ./tests/sgd_test.cpp ./driver/iFPGA.cpp ./driver/RuntimeClient.cpp -o sgd_test -L$(AALSDK)/lib $(LDFLAGS) -lxlrt

zipml: ./tests/main.cpp ./driver/iFPGA.cpp ./driver/RuntimeClient.cpp
	$(CXX) -D HARPv1 -I$(AALSDK)/include $(CPPFLAGS) ./tests/main.cpp ./driver/iFPGA.cpp ./driver/RuntimeClient.cpp -o zipmlfpga -L$(AALSDK)/lib $(LDFLAGS) -lxlrt

clean:
	rm scd_test
	#rm sgd_test
	#rm zipmlfpga