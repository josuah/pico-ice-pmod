/*
 * MIT License
 * 
 * Copyright (c) 2023 tinyVision.ai
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

// TODO: For now, this is a bit-banged library which is sub-optimal.
// We could use the hardware SPI for when in forward pin order, and
// a PIO state machine for reverse pin order. But since we are going
// to need the reverse pin order anyway, we might as well make use
// of the PIO state machine right away. This way only one PIO state
// machine is used and no extra SPI peripheral.
//
// So the next evolution of this driver is to use PIO, and after that
// DMA channels along with PIO.

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include "hardware/sync.h"
#include "pico/stdlib.h"
#include "boards/pico_ice.h"
#include "pmod_spi.h"

void pmod_spi_init(struct pmod_spi *spi) {
    // This driver is only focused on one particular SPI bus
    // Everything in high impedance (input) before a transaction occurs
    gpio_init(spi->clk_pin);
    gpio_init(spi->clk_cipo);
    gpio_init(spi->clk_copi);
}

static uint8_t transfer_byte(struct pmod_spi *spi, uint8_t tx) {
    uint8_t rx;

    for (uint8_t i = 0; i < 8; i++) {
        // Update TX and immediately set negative edge.
        gpio_put(ICE_PMOD2A_SPI_SCK_PIN, false);
        gpio_put(ICE_PMOD2A_SPI_MOSI_PIN, tx >> 7);
        tx <<= 1;

        // stable for a while with clock low
        sleep_us(1);

        // Sample RX as we set positive edge.
        rx <<= 1;
        rx |= gpio_get(ICE_PMOD2A_SPI_MISO_PIN);
        gpio_put(ICE_PMOD2A_SPI_SCK_PIN, true);

        // stable for a while with clock high
        sleep_us(1);
    }
    return rx;
}

void pmod_spi_chip_select(struct pmod_spi *spi, uint8_t cs_n_pin) {
    // Drive the bus, going out of high-impedance mode
    gpio_put(ICE_PMOD2A_SPI_SCK_PIN, false);
    gpio_put(ICE_PMOD2A_SPI_MOSI_PIN, true);
    gpio_set_dir(ICE_PMOD2A_SPI_SCK_PIN, GPIO_OUT);
    gpio_set_dir(ICE_PMOD2A_SPI_MOSI_PIN, GPIO_OUT);

    // Start an SPI transaction
    gpio_put(cs_n_pin, false);
    gpio_set_dir(cs_n_pin, GPIO_OUT);
    sleep_us(5);
}

void pmod_spi_chip_deselect(struct pmod_spi *spi, uint8_t cs_n_pin) {
    // Terminate the transaction
    gpio_put(cs_n_pin, true);

    // Release the bus by putting it high-impedance mode
    gpio_set_dir(cs_n_pin, GPIO_IN);
    gpio_set_dir(ICE_PMOD2A_SPI_SCK_PIN, GPIO_IN);
    gpio_set_dir(ICE_PMOD2A_SPI_MOSI_PIN, GPIO_IN);
}

void pmod_spi_write(struct pmod_spi *spi, uint8_t const *buf, size_t len) {
    for (; len > 0; len--, buf++) {
        transfer_byte(*buf);
    }
}

void pmod_spi_read(struct pmod_spi *spi, uint8_t tx, uint8_t *buf, size_t len) {
    for (; len > 0; len--, buf++) {
        *buf = transfer_byte(tx);
    }
}
