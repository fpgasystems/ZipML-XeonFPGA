# Copyright (C) 2018 Kaan Kara - Systems Group, ETH Zurich

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
COMMON		= ./driver/iFPGA.cpp ./driver/RuntimeClient.cpp ./src/ColumnML.cpp ./src/ColumnStore.cpp

.PHONY: all clean distclean

all: all_scd_tests

all_scd_tests: ./tests/all_tests.cpp $(COMMON)
	$(CXX) -D HARPv2 -I$(AALSDK)/include $(CPPFLAGS) ./tests/all_tests.cpp $(COMMON) -o all_tests -L$(AALSDK)/lib $(LDFLAGS) -laalrt

clean:
	rm all_tests