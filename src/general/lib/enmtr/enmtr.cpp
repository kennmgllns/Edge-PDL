
#include "global_defs.h"
#include "enmtr.h"


bool EnergyMeter::init(int miso, int mosi, int sclk, int cs)
{
    spi_bus_config_t                buscfg;
    spi_device_interface_config_t   devcfg;

    memset(&buscfg, 0, sizeof(buscfg));
    buscfg.miso_io_num      = miso;
    buscfg.mosi_io_num      = mosi;
    buscfg.sclk_io_num      = sclk;
    buscfg.quadwp_io_num    = -1;
    buscfg.quadhd_io_num    = -1;
    buscfg.max_transfer_sz  = ENMTR_SPI_MAX_TRANSFER_SIZE;

#if 0 // https://docs.espressif.com/projects/esp-idf/en/v5.2.2/esp32/api-reference/peripherals/spi_slave.html#restrictions-and-known-issues
    ESP_ERROR_CHECK( spi_bus_initialize(ENMTR_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO) );
#else
    ESP_ERROR_CHECK( spi_bus_initialize(ENMTR_SPI_HOST, &buscfg, SPI_DMA_DISABLED) );
#endif

    memset(&devcfg, 0, sizeof(devcfg));
    //devcfg.duty_cycle_pos   = 128; // default = 128/256
    //devcfg.duty_cycle_pos   = 256-64;
    devcfg.clock_speed_hz   = ENMTR_SPI_CLOCK_SPEED_HZ;
    //devcfg.input_delay_ns   = 32;
    devcfg.mode             = ENMTR_SPI_MODE;
    devcfg.spics_io_num     = cs;
    devcfg.queue_size       = 1;

    ESP_ERROR_CHECK( spi_bus_add_device(ENMTR_SPI_HOST, &devcfg, &spi) );

    return true;
}

bool EnergyMeter::transfer(const uint8_t *pui8_tx_buf, uint8_t *pui8_rx_buf, uint16_t ui16_size)
{
    spi_transaction_t   t;
    esp_err_t           ret = ESP_FAIL;

    memset(&t, 0, sizeof(t));
    t.length    = ui16_size * 8; // number of bits
    t.tx_buffer = pui8_tx_buf;
    t.rx_buffer = pui8_rx_buf;

    // https://docs.espressif.com/projects/esp-idf/en/v5.2.2/esp32/api-reference/peripherals/spi_master.html#driver-usage
#if 0 // polling
    ret = spi_device_polling_transmit(spi, &t);
#elif 0 // interrupt
    ret = spi_device_transmit(spi, &t);
#else // interrupt with timeout
    if (ESP_OK == spi_device_queue_trans(spi, &t, ENMTR_SPI_TRANSFER_TIMEOUT))
    {
        spi_transaction_t *rt;
        ret = spi_device_get_trans_result(spi, &rt, ENMTR_SPI_TRANSFER_TIMEOUT);
    }
#endif

    return (ESP_OK == ret);
}
