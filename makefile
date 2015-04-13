CC=gcc
CXX=g++-4.8
RM=rm -f
CPPFLAGS=-O3 -I./src/core -std=c++11
LDFLAGS=--static
LDLIBS=-lboost_filesystem -lboost_system -std=c++11 -pthread -lboost_thread -lboost_program_options

SRCS=./src/core/net.cc ./src/core/trans.cc ./src/core/tstate.cc ./src/core/antichain.cc ./src/core/antichain_comb.cc ./src/core/bstate.cc ./src/core/cmb_node.cc ./src/bfc/bfc.cc ./src/bfc/minbw.cc ./src/bfc/ticketabs.cc ./src/core/complement.cc ./src/core/ostate.cc ./src/core/vstate.cc ./src/core/types.cc
OBJS=$(subst .cc,.o,$(SRCS))

all: bfc

bfc: $(OBJS)
	$(CXX) $(LDFLAGS) -o ./bin/bfc/release/bfc $(OBJS) $(LDLIBS)
	tar -cvzf ./bin/bfc/release/bfc-2.0-linux-x86-64.tar.gz -C ./bin/bfc/release/ bfc

ttstrans:
	 g++-4.8 -I./src/core/ --static -o ./src/ttstrans/ttstrans ./src/core/net.cc ./src/core/trans.cc ./src/core/tstate.cc ./src/core/antichain.cc ./src/core/bstate.cc ./src/ttstrans/ttstrans.cc ./src/core/ostate.cc ./src/core/vstate.cc ./src/core/types.cc -lboost_filesystem -lboost_system -std=c++11 -pthread -lboost_thread -lboost_program_options

depend: .depend

.depend: $(SRCS)
	rm -f ./.depend
	$(CXX) $(CPPFLAGS) -MM $^>>./.depend;

clean:
	$(RM) $(OBJS)

dist-clean: clean
	$(RM) *~ .dependtool

include .depend
