CC=gcc
CXX=g++-4.7
RM=rm -f
CPPFLAGS=-g -I./src/core -std=gnu++0x
LDFLAGS=-g --static
LDLIBS=-lboost_filesystem -lboost_system -std=gnu++0x -pthread -lboost_thread -lboost_program_options

SRCS=./src/core/trans.cc ./src/core/tstate.cc  ./src/core/antichain.cc ./src/core/bstate.cc ./src/core/cmb_node.cc ./src/bfc/bfc.cc ./src/core/complement.cc ./src/core/ostate.cc ./src/core/vstate.cc ./src/core/types.cc
OBJS=$(subst .cc,.o,$(SRCS))

all: bfc

bfc: $(OBJS)
	$(CXX) $(LDFLAGS) -o bfc $(OBJS) $(LDLIBS) 

depend: .depend

.depend: $(SRCS)
	rm -f ./.depend
	$(CXX) $(CPPFLAGS) -MM $^>>./.depend;

clean:
	$(RM) $(OBJS)

dist-clean: clean
	$(RM) *~ .dependtool

include .depend
