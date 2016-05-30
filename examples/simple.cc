/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * Simple example how to use the spixels library
 */

#include "led-strip.h"

#include <unistd.h>  // for usleep()

#define FRAME_RATE 60

using namespace spixels;

int main() {
    // If you are using WS2801, then you need CreateDMAMultiSPI() instead,
    // as these strips are finicky with timings.
    //
    // See include/multi-spi.h
    MultiSPI *spi = CreateDirectMultiSPI();

    // Connect LED strips with 144 LEDs to connector P1 and P2
    // Choose the type of LEDs from the factory name.
    //
    // See include/led-strip.h
    LEDStrip *strip1 = CreateAPA102Strip(spi, MultiSPI::SPI_P1, 144);
    LEDStrip *strip2 = CreateAPA102Strip(spi, MultiSPI::SPI_P2, 144);
    // ... register more strips here. They can be of different types

    for (unsigned int i = 0; /**/; ++i) {
	const int pos = i % strip1->count();
        strip1->SetPixel(pos, 0x000000);   // clear previous pixel.
        strip2->SetPixel(pos, 0x000000);

        // Various ways to set pixels

        // Red Pixel, given as RGB hex value.
        strip1->SetPixel(pos+1, 0xFF0000);

        // Alternative way: A Green pixel, given as RGB-color struct.
        strip1->SetPixel(pos+2, RGBc(0, 255, 0));

        // Alternative way: give values as separate red/green/blue value.
        strip1->SetPixel(pos+3, 0, 0, 255);

        // A Blue pixel on the second strip.
        strip2->SetPixel(pos+1, 0, 0, 255);
    
        spi->SendBuffers();  // Send all pixels out at once.
        usleep(1000000 / FRAME_RATE);
    }

    delete strip1;
    delete strip2;
    delete spi;
}
