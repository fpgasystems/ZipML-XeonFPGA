CPPFLAGS += -I./ -I/usr/local/include/
CPPFLAGS += -march=native -mavx -D__AAL_USER__=1 -g -std=c++11 -O2 -O3
LDFLAGS  += -L/usr/local/lib -Wl,-rpath-link -Wl,/usr/local/lib -Wl,-rpath -Wl,/usr/local/lib -L/usr/local/lib64 -Wl,-rpath-link -Wl,/usr/local/lib64 -Wl,-rpath -Wl,/usr/local/lib64
LDFLAGS  += -lpthread -lOSAL -lAAS -lxlrt

all:
	g++ $(CPPFLAGS) main.cpp iFPGA.cpp RuntimeClient.cpp -o zipmlfpga $(LDFLAGS)