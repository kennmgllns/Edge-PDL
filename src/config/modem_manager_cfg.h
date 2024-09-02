#pragma once

#define MODEM_CLASS                     Quectel_EG91x
#define K_MODEM_STR_CMD_INIT_CONFIG     "E0;+IFC=0,0;&D0;Q0;V1;&C1"

// network rf bands
#define K_MODEM_BANDS_GSM               "0x0"                   // for both 2G & 3G (not supported by EG915Q-NA)
#define K_MODEM_BANDS_LTE               "0x2000000000000181a"   // 4G : EG915Q-NA all bands = B2/4/5/12/13/66/71
//#define K_MODEM_BANDS_LTE               "0x181a"   // limit to bands B2/4/5/12/13 only
#define K_MODEM_BANDS_EXTRA             nullptr     // no other RAT support

// +COPS <mode> <format>
#define K_MODEM_OPERATOR_MODE           (0)     // 0 = auto select operator
#define K_MODEM_OPERATOR_FORMAT         (2)     // numeric only; alphanumeric not supported on EG915 (?)



// to do: support pins via I/O expander

// temporary
#define MODEM_RESET_PIN         (GPIO_NUM_4)    // keep low
#define MODEM_DTR_PIN           (GPIO_NUM_19)   // not used
#define MODEM_PWR_ON_PIN        (GPIO_NUM_21)   // power key
#define MODEM_VCC_RST_PIN       (GPIO_NUM_13)   // VBAT control


static inline void modem_reset_pin(bool b_high)
{
#if 1 // non-inverting (e.g. direct connection)
    gpio_set_level(MODEM_RESET_PIN, b_high);
#else // inverted (e.g. via bjt)
    gpio_set_level(MODEM_RESET_PIN, !b_high);
#endif
}

static inline void modem_pwr_on_pin(bool b_high)
{
#if 1 // non-inverting (e.g. direct connection)
    gpio_set_level(MODEM_PWR_ON_PIN, b_high);
#else // inverted (e.g. via bjt)
    gpio_set_level(MODEM_PWR_ON_PIN, !b_high);
#endif
}

static inline void modem_dtr_pin(bool b_high)
{
    gpio_set_level(MODEM_DTR_PIN, b_high);
}

static inline void modem_vcc_rst_pin(bool b_high)
{
    gpio_set_level(MODEM_VCC_RST_PIN, b_high);
}

static inline void modem_init_pins(void)
{
    gpio_set_direction(MODEM_VCC_RST_PIN, GPIO_MODE_OUTPUT);

#if 1 // direct connection
    gpio_set_direction(MODEM_RESET_PIN, GPIO_MODE_OUTPUT_OD);
    gpio_set_direction(MODEM_PWR_ON_PIN, GPIO_MODE_OUTPUT_OD);
    gpio_set_direction(MODEM_DTR_PIN, GPIO_MODE_OUTPUT_OD);
#else // with buffer|inverter
    gpio_set_direction(MODEM_RESET_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(MODEM_PWR_ON_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(MODEM_DTR_PIN, GPIO_MODE_OUTPUT);
#endif

    modem_reset_pin(true);
    modem_pwr_on_pin(true);
    modem_dtr_pin(true);
    modem_vcc_rst_pin(true);
}
