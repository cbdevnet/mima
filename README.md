﻿# MIMA Toolchain

A set of tools for developing/testing MIMA assembler code, written in plain C.  

## mimasm

mimasm transforms [assembly instructions](mimasm/MIMA-ASSEMBLER.txt) into an intermediate representation (memory map). This memory map can then be viewed and experimented with, or it can be fed to mimasim for execution.

### Features

*   Cross-platform, builds on Windows, Linux and BSD
*   Supports complete instruction set
*   Keeps labels in memory map for runtime resolving
*   Outputs intermediate format accepted by multiple interpreters

### Download & Resources

*   [github.com/cbdevnet/mima](https://github.com/cbdevnet/mima/) - mimasm and mimasim on github
*   [mimasm/](mimasm/) - The mimasm project
*   [mimasm/mimasm.c](mimasm/mimasm.c) - main source
*   [mimasm/MIMA-ASSEMBLER.txt](mimasm/MIMA-ASSEMBLER.txt) - Assembly language documentation
*   [mimasm/demo-sort.txt](mimasm/demo-sort.txt) - Demonstration source sorting an array


## mimasim

mimasim allows you to execute a memory map generated by mimasm (or written by yourself), either in a single run or interactively. It supports the full MIMA instruction set, as well as adding support for breakpoints, runtime label resolution and interactive memory inspection.

### Features

*   Cross-platform, builds on Windows, Linux and BSD
*   Supports complete instruction set
*   Easy-to-understand and useful [input file format (memory map)](mimasim/demo-sort.mima)
*   Useful (optional) [output file format](mimasim/SAMPLE-OUTPUT.txt)
*   Interactive execution, including memory inspection
*   Breakpoint functionality for debugging complex programs
*   Run-time label resolution
*   Arbitrary program entry point
*   Arbitrary run-time step limit

### Download & Resources

*   [github.com/cbdevnet/mima](https://github.com/cbdevnet/mima/) - mimasim and mimasm on github
*   [mimasim/](mimasim/) - The mimasim project
*   [mimasim/mimasim.c](mimasim/mimasim.c) - main source
*   [mimasim/WORKFLOW.txt](mimasim/WORKFLOW.txt) - Basic workflow information
*   [mimasim/SAMPLE-OUTPUT.txt](mimasim/SAMPLE-OUTPUT.txt) - Example output of running an array sorting program


## About the MIMA

The MIMA is a greatly simplified example of a microcomputer (hence the name, MInimal MAchine), used for demonstration and educational purposes in lectures at the [KIT](http://kit.edu/). The original work in developing the MIMA and its instruction set has been done by Prof. T. Asfour ([http://ti.ira.uka.de/](http://ti.ira.uka.de/)).

## Bug Bounty

The first 10 people to report a critical (as in, crashes on execution or missing/erroneous/broken functionality), not yet submitted/known bug or fix a listed known bug in the latest releases of either mimasim or mimasm are eligible, upon disclosure of said bug/fix to cb@cbcdn.com, to receive one Club Mate each (in Person, in Karlsruhe, at the KIT, in the FSMI) from me ;)

### People yet eligible:

*   Sinan (claimed)
*   drone| (claimed)
*   Indidev (claimed)


## License & Copying

mimasm and mimasim are distributed under the terms of the BSD 2-Clause license, see [LICENSE.txt](LICENSE.txt).
