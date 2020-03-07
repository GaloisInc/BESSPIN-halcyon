# Halcyon #

A tool for analyzing information leakage in Verilog code.


## Overview ##

This repository contains code for statically analyzing processor definitions
written in Verilog, to find whether a value of a given signal may be leaked
through timing or non-timing leakage.  For instance, when this analysis run on
the BOOM RISC-V processor definition, the code shows that the `io_resp_valid`
signal in the `MulDiv` module leaks the clock signal (i.e. the timing) and
also, among other signals, the kill and reset signals to the module.

	$ ./halcyon processors/boom/\*.v

	>> MulDiv.io_resp_valid

	found timing leak:
		MulDiv.clock

	found non-timing leak:
		MulDiv.io_kill MulDiv.io_req_bits_dw MulDiv.io_req_bits_fn
		MulDiv.io_req_bits_in1 MulDiv.io_req_bits_in2 MulDiv.io_req_ready
		MulDiv.io_req_valid MulDiv.io_resp_ready MulDiv.reset


The goal of writing this analysis tool is to reduce the number of candidate
instructions that need to be tested in a black-box manner using the
[timing-tests](), but it may also be possible to prune the set of false
positives, enough to rely on the static analysis alone.

Halcyon implements a context-insensitive inter-procedural analysis. Since the
analysis understands module invocations, it can also accept top-level
definitions such as the `Rocket` module's `alu_io_out` or `div_io_resp_valid`
signals, and the analysis tracks the data flow from the `Rocket` module to the
relevant (`ALU` or `MulDiv`) modules.


## How to Use Halcyon ##

Halcyon requires that the [Verific parser (on
gitlab-int)](https://gitlab-int.galois.com/arane/verific) is located in the
parent directory.  If not, please change the variable `VERIFIC_ROOT` in
`Makefile` or set it from the command line when running make (e.g.
`VERIFIC_ROOT=/opt/verific-dir make`).

Halcyon accepts input as module-name.signal-name (e.g. `MulDiv.io_resp_valid`).
Halcyon also supports tab-completion on module names and ports.

### JSON I/O

For non-interactive usage, Halcyon accepts a JSON file like the following:

```
{
  "signals" : [ "modules" : "MulDiv", "field" : "io_resp_valid"],
  "sources" : [ "/path/to/foo.v", "/path/to/bar.v"  ]
}
```

Which directs Halcyon to analyze the sources `foo.v` and `bar.v` and check
`MulDiv.io_resp_valid`.


## Implementation Details of Halcyon ##

### Data Flow Analysis ###
Halcyon uses standard data flow analysis techniques, with a few extensions that
are specifically targeted for Verilog programs.  Specifically, Halcyon tracks
the flow of information through not just explicit and implicit flows, but also
through triggers (i.e. through `always` blocks and triggered events in
Verilog).

### Differences from Analysis in Procedural Programs ###
Since Verilog does not strictly conform to standard rules of
composition of instructions and basic blocks (since instructions -- or really,
statements -- may contain nested statements), the Halcyon has to relax the
definition of its internal data structures accordingly.  Hence, an instruction
(`instr_t`) can contain pseudo-instructions (see `pinstr_t`), modules can
contain multiple entry points (see `module_t::top_level_blocks`), and some
basic blocks are not included in the list of basic blocks in the containing
module (see `BB_HIDDEN`).

### Flow Tracking ###
For tracing explicit flows, Halcyon uses def-use chains constructed from
lvalues and rvalues of statements.  For tracing implicit flows, Halcyon uses
the domiantor and postdominator tree to discover guard conditions.  Finally,
for tracing timing-related flows, Halcyon traces the type of basic block (i.e.
whether the basic block represents an `always` block).


## Comparison with [SecVerilog](http://www.cs.cornell.edu/projects/secverilog/) ##

Although [SecVerilog](http://www.cs.cornell.edu/projects/secverilog/) exists
and is similar, in my opinion, the implementations and use of SecVerilog and
Halcyon differ significantly. SecVerilog seems to introduce more complexity
than is needed for flow tracking, requiring Z3, a lattice and structural
specification in SMTLIB, and also labels in the Verilog code. Instead, Halcyon
is designed to accept the module name and the name of the signal and to tell
whether there exists a possibility of timing (and non-timing) leakage while
also pointing out the signals whose information is leaked.


## Known Issues ##

Halcyon does not support a few isolated constructs that do not appear to be
generated by either [Chisel compiler](https://chisel.eecs.berkeley.edu/) or the
[BlueSpec SystemVerilog compiler](http://wiki.bluespec.com/).  If Halcyon
discovers an unspported construct (e.g. a `wait_order` statement), it aborts
the execution instead of silently ignoring the error.

Halcyon uses an iterative (i.e. naive) algorithm for constructing dominator and
post-dominator set (see `module_t::build_dominator_sets()`).  The performance
of this code will be dramatically better if the current implementation is
replaced with an implementation of the Lengauer-Tarjan Dominator Tree
Algorithm.
