This project is a fork of the original [bfc][bfc] safety model-checker (a.k.a.
breach) hosted at: [original][original].  The fork happened at **revision 58**
and the two repositories have not been synced ever since.

The focus of this fork is to implement additional functionalities to
`ttstrans`. Therefore, code not related to `ttstrans` was left untouched.

The goal of `ttstrans` is to translate from and to various formalisms to
describe Petri-net like models. Our effort focused on the translation to the
[input language][spec] used by the [mist][mist] tool.

We implemented additional translations into the input language of mist. The
goal was to obtain problem encodings using less variables that the original
translation. 


## Compilation instructions for g++

1. Required:
	* g++ >= 4.8 only (available in Ubuntu 14.10)
	* boost libraries (tested with version 1.46--1.49; 1.49 is available in Ubuntu 14.10)
	* boost_filesystem
	* boost_system
	* boost_thread
	* boost_program_options

	To install these libraries:
```bash
$ sudo apt-get install libboost-dev
$ sudo apt-get install libboost-filesystem-dev
$ sudo apt-get install libboost-thread-dev
$ sudo apt-get install libboost-program-options-dev
```

2. Create the target directory:
```bash
$ mkdir ./bin/bfc/release
```

3. On Ubuntu 14.10, compile `ttstrans` with:

```bash
$ g++-4.8 -O3 -I./src/core ./src/core/trans.cc ./src/core/tstate.cc  ./src/core/antichain.cc ./src/core/bstate.cc ./src/core/cmb_node.cc ./src/bfc/bfc.cc ./src/core/complement.cc ./src/core/ostate.cc ./src/core/vstate.cc ./src/core/types.cc ./src/core/net.cc -o./bfc -lboost_filesystem -lboost_system -std=c++11 -pthread -lboost_thread -lboost_program_options
```

3. On OS X 10.9.5, compile with 

```bash
$ clang++ -I./src/core/ -o ./ttstrans/ttstrans ./src/core/net.cc ./src/core/trans.cc ./src/core/tstate.cc ./src/core/antichain.cc ./src/core/bstate.cc ./ttstrans/ttstrans.cc ./src/core/ostate.cc ./src/core/vstate.cc ./src/core/types.cc -lboost_filesystem -lboost_system -std=c++11 -stdlib=libc++  -pthread -lboost_thread-mt -lboost_program_options
```


## Compilation instructions for Visual Studio 2010

1. Required:
	* boost libraries (tested with version 1.46 and 1.47)
	* boost_filesystem
	* boost_system
	* boost_thread
	* boost_program_options
Pre-compiled 32-bit library binaries are also available here: http://www.boostpro.com/download/.

2. Open bfc.sln and adjust the project properties:
	* Path "C:\Program Files (x86)\boost\boost_1_47" in the "Additional Include Directories" field under "C/C++".
	* Path "C:\Program Files (x86)\boost\boost_1_47\lib" in the "Additional Library Directories" field under "Linker/General".

3. Build the solution.

### Usage

```bash
$ ttstrans --input-file <filename> -w MIST -a <target_state> -i <initial_state> -[z|s|l]
```

### Options
| Option | Effect of the option                                                                           |
|--------|------------------------------------------------------------------------------------------------|
| z      | Produces `N` counters where `N` is the total number of shared states (originally implemented). |
| s      | Produces `2` counters regardless of the number of shared states.                               |
| l      | Produces `2*log(N)` counters where `N` is the total number of shared states.                   | 

* `-z` implements the original translation. It produces one counter per shared state.  
* `-s` implements the most aggressive translation. It produces two counters,
  `c0` and `c1`, to represent all the shared states. Say we have `N` shared
  states.  To represent the state `N-5` we use the valuation `c0 = 5` and `c1 =
  N-5`.  Note that we use two counters because we want the output to
  corresponds to a Petri net (or vector addition system) and thus are limited
  to guard given by inequalities. In this case the conjunction of inequalities
  `c0 >= 5` and `c1 >= N-5` suffices to identify the shared state `5`.
* `-l` implements a less aggressive encoding that uses the binary encoding of
  `N` and then uses two counters to represent each bit of the encoding. Overall
  the produces `2*log(N)` counters where `N` is the total number of shared states.



[original]: http://www.cprover.org/svn/software/bfc/
[mist]: https://github.com/pierreganty/mist
[bfc]: http://www.cprover.org/bfc/
[spec]:https://github.com/pierreganty/mist/wiki#input-format-of-mist
