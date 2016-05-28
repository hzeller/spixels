LIBRARY=../lib/libspixels.a

LDFLAGS=-L../lib -lspixels
INCLUDE_FLAGS=-I../include

CXXFLAGS=-Wall -O3 $(INCLUDE_FLAGS)

simple : simple.cc $(LIBRARY)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(LIBRARY):
	$(MAKE)  -C ../lib

clean:
	rm -f simple
