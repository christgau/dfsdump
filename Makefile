CXXFLAGS=-O0 -g -Wall
LDFLAGS=-ldfs -ldaos -ldaos_common -lgurt -luuid

ddump: ddump.cpp
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS)  -o $@

.PHONY: clean

clean:
	rm -f ddump
