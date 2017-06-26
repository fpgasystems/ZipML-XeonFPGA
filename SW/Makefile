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

CPPFLAGS += -I./ -I/usr/local/include/
CPPFLAGS += -march=native -mavx -D__AAL_USER__=1 -g -std=c++11 -O2 -O3
LDFLAGS  += -L/usr/local/lib -Wl,-rpath-link -Wl,/usr/local/lib -Wl,-rpath -Wl,/usr/local/lib -L/usr/local/lib64 -Wl,-rpath-link -Wl,/usr/local/lib64 -Wl,-rpath -Wl,/usr/local/lib64
LDFLAGS  += -lpthread -lOSAL -lAAS -lxlrt

all:
	g++ $(CPPFLAGS) main.cpp iFPGA.cpp RuntimeClient.cpp -o zipmlfpga $(LDFLAGS)