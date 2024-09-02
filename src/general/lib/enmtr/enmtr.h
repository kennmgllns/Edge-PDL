
#pragma once

#include <driver/gpio.h>
#include <driver/spi_master.h>

#include "enmtr_cfg.h"


/*
SPI Energy Meter class
*/
class EnergyMeter
{
public:
    EnergyMeter() { }
    bool init(int miso, int mosi, int clk, int cs);
    bool transfer(const uint8_t *pui8_tx_buf, uint8_t *pui8_rx_buf, uint16_t ui16_size);

protected:
    spi_device_handle_t spi;
};
