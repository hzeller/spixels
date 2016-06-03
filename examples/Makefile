# The toplevel directory of the spixels project. In this case
# this is one directory up, so ../
# If you are using this as a template for your own project and
# have checked out the spixels/ project as a submodule under
# the spixels/ directory, then use SPIXELS_DIR=spixels
SPIXELS_DIR=..

SPIXELS_LIBRARY=$(SPIXELS_DIR)/lib/libspixels.a

LDFLAGS=-L$(SPIXELS_DIR)/lib -lspixels
INCLUDE_FLAGS=-I$(SPIXELS_DIR)/include

CXXFLAGS=-Wall -O3 $(INCLUDE_FLAGS)

simple : simple.cc $(SPIXELS_LIBRARY)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(SPIXELS_LIBRARY):
	$(MAKE)  -C ../lib

clean:
	rm -f simple
