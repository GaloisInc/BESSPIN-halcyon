CXX = g++
OBJECTS = structs.o analyze.o

VERIFIC_ROOT = ..

CXXFLAGS = -I$(VERIFIC_ROOT)/verilog -I$(VERIFIC_ROOT)/util \
    -I$(VERIFIC_ROOT)/containers -O3 -Iinclude -std=c++11 -g

LDFLAGS = $(VERIFIC_ROOT)/verilog/verilog-linux.a   \
    $(VERIFIC_ROOT)/util/util-linux.a               \
    $(VERIFIC_ROOT)/containers/containers-linux.a   \
    $(VERIFIC_ROOT)/database/database-linux.a -lz

all:    analyze

analyze:    $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@ -O3

clean:
	$(RM) $(OBJECTS) analyze

.PHONY: all clean
