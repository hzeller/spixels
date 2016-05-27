LIB_OBJECTS=ft-gpio.o multi-spi.o rpi-dma.o mailbox.o
CFLAGS=-Wall -O3 $(INCLUDES) $(DEFINES)
CXXFLAGS=$(CFLAGS)
INCLUDES=-I../include -I.

libspixels.a: $(LIB_OBJECTS)
	ar rcs $@ $^

%.o : %.cc
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

%.o : %.c
	$(CC) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f libspixels.a $(LIB_OBJECTS)
