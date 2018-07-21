OBJECTS = analyze.o structs.o

VERIFIC_ROOT = ..

CXXFLAGS = -I$(VERIFIC_ROOT)/verilog -I$(VERIFIC_ROOT)/util \
    -I$(VERIFIC_ROOT)/containers -O3 -Iinclude

LDFLAGS = $(VERIFIC_ROOT)/verilog/verilog-linux.a   \
    $(VERIFIC_ROOT)/util/util-linux.a               \
    $(VERIFIC_ROOT)/containers/containers-linux.a   \
    $(VERIFIC_ROOT)/database/database-linux.a -lz

all:    analyze

analyze:    $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@

clean:
	$(RM) $(OBJECTS) analyze

.PHONY: all clean
