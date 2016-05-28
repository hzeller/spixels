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

    for (int i = 0;;++i) {
        strip1->SetPixel(i % strip1->count(), 255, 0, 0);  // red pixel  
        strip2->SetPixel(i % strip2->count(), 0, 255, 0);  // green pixel.
    
        spi->SendBuffers();  // Send all pixels out at once.
        usleep(1000000 / FRAME_RATE);
    }

    delete strip1;
    delete strip2;
    delete spi;
}
