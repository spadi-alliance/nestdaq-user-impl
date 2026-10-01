#
#
#

CXX = g++
CXXFLAGS = -Wall -g -O
INCLUDES = -I../../include
LIBS = -L../../lib -L../../lib64 -lhiredis -lredis++
LDFLAGS = -Wl,-rpath=../../lib:../../lib64

EXECS = ExprParser LogiCalc
all: $(EXECS)

ExprParser: ExprParser.cxx
	$(CXX) $(CXXFLAGS) -o $@ \
		-D EXPRPARSER_TEST_MAIN \
		$<

LogiCalc: LogiCalc.cxx
	$(CXX) $(CXXFLAGS) -o $@ \
		-D LOGICALC_TEST_MAIN \
		$(INCLUDES) \
		$<


clean: 
	rm -f $(EXECS)
