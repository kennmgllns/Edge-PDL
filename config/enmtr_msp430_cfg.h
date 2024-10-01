#pragma once


#define ENMTR_SPI_MISO_PIN              (GPIO_NUM_22)
#define ENMTR_SPI_MOSI_PIN              (GPIO_NUM_23)
#define ENMTR_SPI_CLK_PIN               (GPIO_NUM_21)

#define ENMTR_SPI_SSA_PIN               (GPIO_NUM_19)
#define ENMTR_SPI_SSB_PIN               (GPIO_NUM_25)
#define ENMTR_SPI_SSC_PIN               (GPIO_NUM_26)

#if 1
static inline void enmtr_rst_pin(bool b_high)
{
    gpio_set_level(GPIO_NUM_14, b_high);
}

static inline void enmtr_init_pins(void)
{
    gpio_set_direction(ENMTR_SPI_SSA_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(ENMTR_SPI_SSB_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(ENMTR_SPI_SSC_PIN, GPIO_MODE_OUTPUT);

    gpio_set_direction(GPIO_NUM_14, GPIO_MODE_OUTPUT);
    enmtr_rst_pin(false);
}
#else
    // or RESET pin via I/O expander
#endif


/*
 * Configurable Constants
 */

#define K_ENMTR_MSP430_RESET_DELAY              (100)

#define K_ENMTR_SPI_CS_CLK_DELAY_US             (10)    // delay between chip select vs clock and mosi signal in uS
#define K_ENMTR_SPI_CS_DELAY_US                 (10)    // delay between each chip select in uS


#define K_ENMTR_MSP430_BOOT_SIG_W1_VAL          (0x4F42)
#define K_ENMTR_MSP430_BOOT_SIG_W2_VAL          (0x544F)
