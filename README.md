## Description

This project is a fork of the original one hosted at: [original][original]. [Here][description] there is a description of the original tool and its usage.

The main functionalities of the original project are retained but there are different options added to the ttstrans in order to achieve a better compatibility between it and the tool [mist][mist]


## Compilation instructions for g++

1. Required:
	* g++ >= 4.8 only (available in Ubuntu 14.10; older version compile but are too buggy)
	* boost libraries (tested with version 1.46--1.49; 1.49 is available in Ubuntu 14.10)
	* boost_filesystem
	* boost_system
	* boost_thread
	* boost_program_options

	To install these libraries:

	 	 $ sudo apt-get install libboost-dev
	 	 $ sudo apt-get install libboost-filesystem-dev
	 	 $ sudo apt-get install libboost-thread-dev
	 	 $ sudo apt-get install libboost-program-options-dev

2. Create the target directory:
	 $ mkdir ./bin/bfc/release

3. Compile with:

	$ g++-4.8 -O3 -I./src/core ./src/core/trans.cc ./src/core/tstate.cc  ./src/core/antichain.cc ./src/core/bstate.cc ./src/core/cmb_node.cc ./src/bfc/bfc.cc ./src/core/complement.cc ./src/core/ostate.cc ./src/core/vstate.cc ./src/core/types.cc ./src/core/net.cc -o./bfc -lboost_filesystem -lboost_system -std=c++11 -pthread -lboost_thread -lboost_program_options --static

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


For furhter questions contact Alexander Kaiser (alexander.kaiser@cs.ox.ac.uk or alex@akaiser.net).


## ttstrans

This tool has been updated by adding several options to obtain an optimize translation. This options only aplies to the translation from bfc format to MIST.

### Usage

	 $ ttstrans --input-file <filename> -w MIST -a <target_state> -i <initial_state> -[z|s|l]

### Options
| Option      | Effect of the option                                                                             |
|-------------|-----------------------------------------------------------------------------------------------------------|
| z | Initializes sX variables to 0.|
| s | Uses just two s-variables: s0, s1. It here are 0 to N states, the state i will be represented as s0≥i s1≥N-i |
| l | Uses just 2xlog(N) variables, being N the number of states. Each state will be represented in binary form using this variables. For each number of the binary form we need two variables: si≥0 s(i+1)≥1 represents '0'; si≥1 s(i+1)≥0, '1'


## Relation with mist

The added options optimize the way [mist][mist] works with the generated .spec file.

* The *-z* option gives us an intuitive output. If a variable is not initialized, [mist][mist] will interpret it as unbounded so won't now anything about its value. This property was modified at [pull request][PR1].
* The *-s* option avoids the use of a big number of sX variables to represent a number N of states. When N is big enough it forces [mist][mist] to work with a great number of variables which make it really slow. By reducing the number of variables to two we avoid this problem.
* The *-l* option solves the overflow problem that can appear when using the -s option with big N. This option let us work with 2xlog(N) variables which implies a great reduction of the execution time of mist and avoid the possible overflow because of there won't be any s variable with a lower bound greater than 1



[description]:http://www.cprover.org/bfc/
[original]: http://www.cprover.org/svn/software/bfc/
[mist]:https://github.com/pierreganty/mist
[PR1]:https://github.com/pierreganty/mist/pull/1