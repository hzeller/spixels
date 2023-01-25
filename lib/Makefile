LIB_OBJECTS=ft-gpio.o dma-multi-spi.o rpi-dma.o mailbox.o direct-multi-spi.o led-strip.o
CFLAGS=-Wall -O3 $(INCLUDES) $(DEFINES)
CXXFLAGS=$(CFLAGS)
INCLUDES=-I../include -I. -I/opt/vc/include

libspixels.a: $(LIB_OBJECTS)
	ar rcs $@ $^

%.o : %.cc
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

%.o : %.c
	$(CC) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f libspixels.a $(LIB_OBJECTS)
