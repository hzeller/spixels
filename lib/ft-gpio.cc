// -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
// SPI Pixels - Control SPI LED strips (spixels)
// Copyright (C) 2013 Henner Zeller <h.zeller@acm.org>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "ft-gpio.h"

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

// Raspberry 1 and 2 have different base addresses for the periphery
#define BCM2708_PERI_BASE        0x20000000
#define BCM2709_PERI_BASE        0x3F000000
#define BCM2711_PERI_BASE        0xFE000000

#define GPIO_REGISTER_OFFSET         0x200000

#define REGISTER_BLOCK_SIZE (4*1024)

// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x).
#define INP_GPIO(g) *(gpio_port_+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio_port_+((g)/10)) |=  (1<<(((g)%10)*3))

#define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0

namespace ft {
/*static*/ const uint32_t GPIO::kValidBits
= ((1 <<  0) | (1 <<  1) | // RPi 1 - Revision 1 accessible
   (1 <<  2) | (1 <<  3) | // RPi 1 - Revision 2 accessible
   (1 <<  4) | (1 <<  7) | (1 << 8) | (1 <<  9) |
   (1 << 10) | (1 << 11) | (1 << 14) | (1 << 15)| (1 <<17) | (1 << 18) |
   (1 << 22) | (1 << 23) | (1 << 24) | (1 << 25)| (1 << 27) |
   // support for A+/B+ and RPi2 with additional GPIO pins.
   (1 <<  5) | (1 <<  6) | (1 << 12) | (1 << 13) | (1 << 16) |
   (1 << 19) | (1 << 20) | (1 << 21) | (1 << 26)
   );

GPIO::GPIO() : output_bits_(0), gpio_port_(NULL) {
}

bool GPIO::AddOutput(int bit) {
    if (gpio_port_ == NULL) {
        fprintf(stderr, "Attempt to init outputs but not yet Init()-ialized.\n");
        return 0;
    }

    const uint32_t gpio_mask = 1 << bit;
    if (bit < 0 || ((gpio_mask & kValidBits) == 0))
        return false;
    INP_GPIO(bit);   // for writing, we first need to set as input.
    OUT_GPIO(bit);

    output_bits_ |= gpio_mask;
    return true;
}

// We are not interested in the _exact_ model, just good enough to determine
// What to do.
enum RaspberryPiModel {
  PI_MODEL_1,
  PI_MODEL_2,
  PI_MODEL_3,
  PI_MODEL_4
};

static int ReadFileToBuffer(char *buffer, size_t size, const char *filename) {
  const int fd = open(filename, O_RDONLY);
  if (fd < 0) return -1;
  ssize_t r = read(fd, buffer, size - 1); // assume one read enough
  buffer[r >= 0 ? r : 0] = '\0';
  close(fd);
  return r;
}

static RaspberryPiModel DetermineRaspberryModel() {
  char buffer[4096];
  if (ReadFileToBuffer(buffer, sizeof(buffer), "/proc/cpuinfo") < 0) {
    fprintf(stderr, "Reading cpuinfo: Could not determine Pi model\n");
    return PI_MODEL_3;  // safe guess fallback.
  }
  static const char RevisionTag[] = "Revision";
  const char *revision_key;
  if ((revision_key = strstr(buffer, RevisionTag)) == NULL) {
    fprintf(stderr, "non-existent Revision: Could not determine Pi model\n");
    return PI_MODEL_3;
  }
  unsigned int pi_revision;
  if (sscanf(index(revision_key, ':') + 1, "%x", &pi_revision) != 1) {
    fprintf(stderr, "Unknown Revision: Could not determine Pi model\n");
    return PI_MODEL_3;
  }

  // https://www.raspberrypi.org/documentation/hardware/raspberrypi/revision-codes/README.md
  const unsigned pi_type = (pi_revision >> 4) & 0xff;
  switch (pi_type) {
  case 0x00: /* A */
  case 0x01: /* B, Compute Module 1 */
  case 0x02: /* A+ */
  case 0x03: /* B+ */
  case 0x05: /* Alpha ?*/
  case 0x06: /* Compute Module1 */
  case 0x09: /* Zero */
  case 0x0c: /* Zero W */
    return PI_MODEL_1;

  case 0x04:  /* Pi 2 */
    return PI_MODEL_2;

  case 0x11: /* Pi 4 */
    return PI_MODEL_4;

  default:  /* a bunch of versions represneting Pi 3 */
    return PI_MODEL_3;
  }
}

static RaspberryPiModel GetPiModel() {
  static RaspberryPiModel pi_model = DetermineRaspberryModel();
  return pi_model;
}

// Public interface
uint32_t *mmap_bcm_register(off_t register_offset) {
    off_t base = BCM2709_PERI_BASE;  // safe fallback guess.
    switch (GetPiModel()) {
    case PI_MODEL_1: base = BCM2708_PERI_BASE; break;
    case PI_MODEL_2: base = BCM2709_PERI_BASE; break;
    case PI_MODEL_3: base = BCM2709_PERI_BASE; break;
    case PI_MODEL_4: base = BCM2711_PERI_BASE; break;
    }

    int mem_fd;
    if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
        // Try to fall back to /dev/gpiomem. Unfortunately, that device
        // is implemented in a way that it _only_ supports GPIO, not the
        // other registers.
        // But, instead of failing, mmap() then silently succeeds with the
        // unsupported offset. So bail out here.
        if (register_offset != GPIO_REGISTER_OFFSET)
            return NULL;

        mem_fd = open("/dev/gpiomem", O_RDWR|O_SYNC);
        if (mem_fd < 0) return NULL;
    }

    uint32_t *result =
        (uint32_t*) mmap(NULL,                  // Any adddress will do
                         REGISTER_BLOCK_SIZE,   // Map length
                         PROT_READ|PROT_WRITE,  // Enable r/w on GPIO registers.
                         MAP_SHARED,
                         mem_fd,                // File to map
                         base + register_offset // Offset to bcm register
                         );
    close(mem_fd);

    if (result == MAP_FAILED) {
        fprintf(stderr, "mmap error %p\n", result);
        return NULL;
    }
    return result;
}

// Based on code example found in http://elinux.org/RPi_Low-level_peripherals
bool GPIO::Init() {
    gpio_port_ = mmap_bcm_register(GPIO_REGISTER_OFFSET);
    if (gpio_port_ == NULL) {
        return false;
    }
    gpio_set_bits_ = gpio_port_ + (0x1C / sizeof(uint32_t));
    gpio_clr_bits_ = gpio_port_ + (0x28 / sizeof(uint32_t));
    return true;
}

} // namespace ft
