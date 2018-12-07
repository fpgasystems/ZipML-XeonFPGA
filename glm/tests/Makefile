include ../../base/sw/base_include.mk

# Primary test name
TEST = main

# Build directory
OBJDIR = obj
CFLAGS += -I./$(OBJDIR)
CPPFLAGS += -I./$(OBJDIR) -march=native

# Files and folders
SRCS = $(TEST).cpp $(BASE_FILE_SRC) 
MY_SRCS = ../src/ColumnML.cpp ../src/ColumnStore.cpp
OBJS = $(addprefix $(OBJDIR)/,$(patsubst %.cpp,%.o,$(SRCS)))
MY_OBJS = $(addprefix $(OBJDIR)/,$(patsubst ../src/%.cpp,%.o,$(MY_SRCS)))

# Targets
all: $(TEST) $(TEST)_ase

ex:
	export ASE_WORKDIR=../build_sim/work/

# AFU info from JSON file, including AFU UUID
AFU_JSON_INFO = $(OBJDIR)/afu_json_info.h
$(AFU_JSON_INFO): ../hw/rtl/glm_cci.json | objdir
	afu_json_mgr json-info --afu-json=$^ --c-hdr=$@
$(OBJS): $(AFU_JSON_INFO)

$(TEST): $(OBJS) $(MY_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS) $(FPGA_LIBS)

$(TEST)_ase: $(OBJS) $(MY_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS) $(ASE_LIBS)

$(OBJDIR)/%.o: %.cpp ../src/FPGA_ColumnML.h | objdir
	$(CXX) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: ../src/%.cpp ../src/FPGA_ColumnML.h | objdir
	$(CXX) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(TEST) $(TEST)_ase $(OBJDIR)

objdir:
	@mkdir -p $(OBJDIR)

.PHONY: all clean
