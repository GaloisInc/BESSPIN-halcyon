CXX = g++
OBJECTS = src/structs.o  src/analyze.o  src/dependence.o

VERIFIC_ROOT ?= ../verific

CXXFLAGS = -I$(VERIFIC_ROOT)/verilog -I$(VERIFIC_ROOT)/util \
    -I$(VERIFIC_ROOT)/containers -O3 -Isrc/include -std=c++11 -g

LDFLAGS = $(VERIFIC_ROOT)/verilog/verilog-linux.a   \
    $(VERIFIC_ROOT)/util/util-linux.a               \
    $(VERIFIC_ROOT)/containers/containers-linux.a   \
    $(VERIFIC_ROOT)/database/database-linux.a -lz

all:    halcyon

halcyon:    $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@ -O3 -lreadline

clean:
	$(RM) $(OBJECTS) halcyon

.PHONY: all clean
