// -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
// SPI Pixels - Control SPI LED strips (spixels)
// Copyright (C) 2016 Henner Zeller <h.zeller@acm.org>
//
// This library is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "multi-spi.h"

#include "ft-gpio.h"
#include "rpi-dma.h"

#include <assert.h>
#include <strings.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// mmap-bcm-register
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define BCM2708_PI1_PERI_BASE  0x20000000
#define BCM2709_PI2_PERI_BASE  0x3F000000

#define PERI_BASE BCM2709_PI2_PERI_BASE

#define PAGE_SIZE 4096

// ---- GPIO specific defines
#define GPIO_REGISTER_BASE 0x200000
#define GPIO_SET_OFFSET 0x1C
#define GPIO_CLR_OFFSET 0x28
#define PHYSICAL_GPIO_BUS (0x7E000000 + GPIO_REGISTER_BASE)

// ---- DMA specific defines
#define DMA_CHANNEL       5   // That usually is free.
#define DMA_BASE          0x007000

// Return a pointer to a periphery subsystem register.
// TODO: consolidate with gpio file.
static void *mmap_bcm_register(off_t register_offset) {
  const off_t base = PERI_BASE;

  int mem_fd;
  if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
    perror("can't open /dev/mem: ");
    fprintf(stderr, "You need to run this as root!\n");
    return NULL;
  }

  uint32_t *result =
    (uint32_t*) mmap(NULL,                  // Any adddress in our space will do
                     PAGE_SIZE,
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

namespace spixels {

namespace {

class DMAMultiSPI : public MultiSPI {
public:
    explicit DMAMultiSPI(MultiSPI::Pin clockPin);
    virtual ~DMAMultiSPI();

    virtual bool RegisterDataGPIO(int gpio, size_t serial_byte_size);
    virtual void SetBufferedByte(MultiSPI::Pin pin, size_t pos, uint8_t data);
    virtual void SendBuffers();

private:
    void FinishRegistration();

    struct GPIOData;
    ft::GPIO gpio_;
    const MultiSPI::Pin clockPin_;
    size_t serial_byte_size_;   // Number of serial bytes to send.

    struct UncachedMemBlock alloced_;
    GPIOData *gpio_dma_;
    struct dma_cb* start_block_;
    struct dma_channel_header* dma_channel_;

    GPIOData *gpio_shadow_;
    size_t gpio_buffer_size_;  // Buffer-size for GPIO operations needed.
};
}  // end anonymous namespace

struct DMAMultiSPI::GPIOData {
    uint32_t set;
    uint32_t ignored_upper_set_bits; // bits 33..54 of GPIO. Not needed.
    uint32_t reserved_area;          // gap between GPIO registers.
    uint32_t clr;
};

DMAMultiSPI::DMAMultiSPI(MultiSPI::Pin clockPin)
    : clockPin_(clockPin), serial_byte_size_(0),
      gpio_dma_(NULL), gpio_shadow_(NULL) {
    alloced_.mem = NULL;
    bool success = gpio_.Init();
    assert(success);  // gpio couldn't be initialized
    success = gpio_.AddOutput(clockPin);
    assert(success);  // clock pin not valid
}

DMAMultiSPI::~DMAMultiSPI() {
    UncachedMemBlock_free(&alloced_);
    free(gpio_shadow_);
}

static int bytes_to_gpio_ops(size_t bytes) {
    // We need two GPIO-operations to bit-bang transfer one bit: one to set the
    // data and one to create a positive clock-edge.
    // For each byte, we have 8 bits.
    // Also, we need a single operation at the end of everything to set clk low.
    return (int)(bytes * 8 * 2 + 1);
}

bool DMAMultiSPI::RegisterDataGPIO(int gpio, size_t requested_bytes) {
    if (gpio_dma_ != NULL) {
        fprintf(stderr, "Can not register DataGPIO after SendBuffers() has been"
                "called\n");
        assert(0);
    }
    if (requested_bytes > serial_byte_size_) {
        const int prev_gpio_end = bytes_to_gpio_ops(serial_byte_size_) - 1;
        serial_byte_size_ = requested_bytes;
        const int gpio_operations = bytes_to_gpio_ops(serial_byte_size_);
        gpio_buffer_size_ = gpio_operations * sizeof(GPIOData);
        // We keep an in-memory buffer that we directly manipulate in
        // SetBufferedByte() operations and then copy to the DMA managed buffer
        // when actually sending. Reason is, that the DMA buffer is uncached
        // memory and very slow to access in particular for the operations
        // needed in SetBufferedByte().
        // RegisterDataGPIO() can be called multiple times with different sizes,
        // so we need to be prepared to adjust size.
        gpio_shadow_ = (GPIOData*)realloc(gpio_shadow_, gpio_buffer_size_);
        bzero(gpio_shadow_ + prev_gpio_end,
              (gpio_operations - prev_gpio_end)*sizeof(GPIOData));
        // Prepare every other element to set the CLK pin so that later, we
        // only have to set the data.
        // Even: data, clock low; Uneven: clock pos edge
        for (int i = prev_gpio_end; i < gpio_operations; ++i) {
            if (i % 2 == 0)
                gpio_shadow_[i].clr = (1<<clockPin_);
            else
                gpio_shadow_[i].set = (1<<clockPin_);
        }
    }

    return gpio_.AddOutput(gpio);
}

void DMAMultiSPI::FinishRegistration() {
    assert(alloced_.mem == NULL);  // Registered twice ?
    // One DMA operation can only span a limited amount of range.
    const int kMaxOpsPerBlock = (2<<15) / sizeof(GPIOData);
    const int gpio_operations = bytes_to_gpio_ops(serial_byte_size_);
    const int control_blocks
        = (gpio_operations + kMaxOpsPerBlock - 1) / kMaxOpsPerBlock;
    const int alloc_size = (control_blocks * sizeof(struct dma_cb)
                            + gpio_operations * sizeof(GPIOData));
    alloced_ = UncachedMemBlock_alloc(alloc_size);
    gpio_dma_ = (struct GPIOData*) ((uint8_t*)alloced_.mem
                                    + control_blocks * sizeof(struct dma_cb));

    struct dma_cb* previous = NULL;
    struct dma_cb* cb = NULL;
    struct GPIOData *start_gpio = gpio_dma_;
    int remaining = gpio_operations;
    for (int i = 0; i < control_blocks; ++i) {
        cb = (struct dma_cb*) ((uint8_t*)alloced_.mem + i * sizeof(dma_cb));
        if (previous) {
            previous->next = (uint32_t)UncachedMemBlock_to_physical(&alloced_, cb);
        }
        const int n = remaining > kMaxOpsPerBlock ? kMaxOpsPerBlock : remaining;
        cb->info   = (DMA_CB_TI_SRC_INC | DMA_CB_TI_DEST_INC |
                      DMA_CB_TI_NO_WIDE_BURSTS | DMA_CB_TI_TDMODE);
        cb->src    = UncachedMemBlock_to_physical(&alloced_, start_gpio);
        cb->dst    = PHYSICAL_GPIO_BUS + GPIO_SET_OFFSET;
        cb->length = DMA_CB_TXFR_LEN_YLENGTH(n)
            | DMA_CB_TXFR_LEN_XLENGTH(sizeof(GPIOData));
        cb->stride = DMA_CB_STRIDE_D_STRIDE(-16) | DMA_CB_STRIDE_S_STRIDE(0);
        previous = cb;
        start_gpio += n;
        remaining -= n;
    }
    cb->next = 0;

    // First block in our chain.
    start_block_ = (struct dma_cb*) alloced_.mem;

    // 4.2.1.2
    char *dmaBase = (char*)mmap_bcm_register(DMA_BASE);
    dma_channel_ = (struct dma_channel_header*)(dmaBase + 0x100 * DMA_CHANNEL);
}

void DMAMultiSPI::SetBufferedByte(MultiSPI::Pin pin, size_t pos, uint8_t data) {
    assert(pos < serial_byte_size_);

    uint32_t        const pinBit = 1 << pin;
    uint32_t        const pinNotBit = ~pinBit;
    GPIOData*       buffer_pos = gpio_shadow_ + 2 * 8 * pos;

    for (uint8_t bit = 0x80; bit != 0; bit >>= 1)
    {
        if (data & bit)
        {   // set
            buffer_pos->set |= pinBit;
            buffer_pos->clr &= pinNotBit;
        }
        else
        {            // reset
            buffer_pos->set &= pinNotBit;
            buffer_pos->clr |= pinBit;
        }
        buffer_pos += 2;
    }
}

void DMAMultiSPI::SendBuffers() {
    if (!gpio_dma_) FinishRegistration();
    memcpy(gpio_dma_, gpio_shadow_, gpio_buffer_size_);

    dma_channel_->cs |= DMA_CS_END;
    dma_channel_->cblock = (uint32_t)UncachedMemBlock_to_physical(&alloced_, start_block_);
    dma_channel_->cs = DMA_CS_PRIORITY(7) | DMA_CS_PANIC_PRIORITY(7) | DMA_CS_DISDEBUG;
    dma_channel_->cs |= DMA_CS_ACTIVE;
    while ((dma_channel_->cs & DMA_CS_ACTIVE)
           && !(dma_channel_->cs & DMA_CS_ERROR)) {
        usleep(10);
    }

    dma_channel_->cs |= DMA_CS_ABORT;
    usleep(100);
    dma_channel_->cs &= ~DMA_CS_ACTIVE;
    dma_channel_->cs |= DMA_CS_RESET;
}


// Public interface
MultiSPI *CreateDMAMultiSPI(MultiSPI::Pin clockPin) {
    return new DMAMultiSPI(clockPin);
}

}  // namespace spixels
