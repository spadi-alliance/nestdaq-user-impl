#
#
#
CXX = g++
CXXFLAGS = -Wall -g -O

INCLUDES = -I../../../include
LIBS = -L../../../lib64 -lredis++ -L../../../lib -lhiredis -Wl,-R,../../../lib:../../../lib64

EXECS = GetTriggerInfo
all: $(EXECS)

GetTriggerInfo : GetTriggerInfo.cxx
	$(CXX) -o $@ $(INCLUDES) \
		-DTEST_MAIN_GETTRIGGERINFO \
		$< \
		$(LIBS)

clean:
	rm $(EXECS)
