/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * Simple example how to use the spixels library
 */

#include "led-strip.h"

#include <unistd.h>

#define FRAME_RATE 200

using namespace spixels;

int main() {
    MultiSPI *spi = CreateDMAMultiSPI();

    // Connect LED strips with 144 LEDs to connector P1 and P2
    LEDStrip *strip1 = CreateWS2801Strip(spi, MultiSPI::SPI_P1, 144);
    LEDStrip *strip2 = CreateWS2801Strip(spi, MultiSPI::SPI_P2, 144);
    // ... register more strips here.

    spi->FinishRegistration();  // Done registering all the the strips.

    for (unsigned int i = 0; /**/; ++i) {
        // Red Pixel, given as RGB hex value.
        strip1->SetPixel(i % strip1->count(), 0xFF0000);

        // Alternative: A Green pixel, given as RGB-color struct.
        strip1->SetPixel(i % strip1->count()+1, RGBc(0, 255, 0));

        // Alternative: give values as separate red/green/blue value.
        strip1->SetPixel(i % strip1->count()+2, 0, 0, 255);

        // A Blue pixel on the second strip.
        strip2->SetPixel(i % strip2->count(), 0, 0, 255);
    
        spi->SendBuffers();  // Send all pixels out at once.
        usleep(1000000 / FRAME_RATE);
    }

    delete strip1;
    delete strip2;
    delete spi;
}
