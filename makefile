#
#
#

CXX = g++
CXXFLAGS = -Wall -g -O
INCLUDES = -I../../include
LIBS = -L../../lib -L../../lib64 -lhiredis -lredis++
LDFLAGS = -Wl,-rpath=../../lib:../../lib64

EXECS = GetTriggerInfo ExprParser
all: $(EXECS)

GetTriggerInfo: GetTriggerInfo.cxx
	$(CXX) $(CXXFLAGS) -o $@ \
		-D TEST_MAIN_GETTRIGGERINFO \
		$(INCLUDES) \
		$< \
		$(LIBS) $(LDFLAGS)

ExprParser: ExprParser.cxx
	$(CXX) $(CXXFLAGS) -o $@ \
		-D EXPRPARSER_TEST_MAIN \
		$<


clean: 
	rm -f $(EXECS)
