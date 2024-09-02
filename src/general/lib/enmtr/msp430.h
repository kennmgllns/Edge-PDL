
#pragma once

#include "enmtr.h"


class EnMtrMSP430 : public EnergyMeter
{
public:
    typedef enum
    {
        DEVICE_PHASE_A,
        DEVICE_PHASE_B,
        DEVICE_PHASE_C,
        NUM_DEVICES
    } device_et;

    typedef enum
    {
        CMD_SINGLE_REG_READ,
        CMD_MULTIPLE_REG_READ,
        CMD_SINGLE_REG_WRITE,
        CMD_MULTIPLE_REG_WRITE,

        CMD_SOFT_RESET                  = 0x41A5,
        CMD_ENTR_BOOTLDR                = 0x41AB,
        CMD_SAVE_USR_CFG                = 0x41AC,

        /* Commands on Bootloader Mode */
        CMD_BTLDR_WRITE_FLASH           = 0x421A,
        CMD_BTLDR_ERASE_APPCODE         = 0x42FB
    } command_et;

    typedef enum
    {
        REG_ADDR_COMMAND                = 0x00,
        REG_ADDR_STATUS                 = 0x01,

        /* Register Addresses on Application */
        REG_ADDR_PCOMP_A                = 0x08,
        REG_ADDR_PCOMP_B                = 0x09,
        REG_ADDR_HPF_COEFF_V            = 0x0A,
        REG_ADDR_HPF_COEFF_I            = 0x0B,
        REG_ADDR_HARM                   = 0x0C,

        REG_ADDR_FWVER                  = 0x18,
        REG_ADDR_FWVER_TEST             = 0x19,
        REG_ADDR_BOOTVER                = 0x1A,
        REG_ADDR_BOOTVER_TEST           = 0x1B,

        REG_ADDR_LINE_FREQ              = 0x20,

        REG_ADDR_VRMS_A_H               = 0x21,
        REG_ADDR_VRMS_A_L               = 0x22,
        REG_ADDR_VRMS_B_H               = 0x23,
        REG_ADDR_VRMS_B_L               = 0x24,

        REG_ADDR_IRMS_A_H               = 0x25,
        REG_ADDR_IRMS_A_L               = 0x26,
        REG_ADDR_IRMS_B_H               = 0x27,
        REG_ADDR_IRMS_B_L               = 0x28,

        REG_ADDR_PF_A_H                 = 0x29,
        REG_ADDR_PF_A_L                 = 0x2A,
        REG_ADDR_PF_B_H                 = 0x2B,
        REG_ADDR_PF_B_L                 = 0x2C,

        REG_ADDR_WATT_A_W2              = 0x2D,
        REG_ADDR_WATT_A_W1              = 0x2E,
        REG_ADDR_WATT_A_W0              = 0x2F,
        REG_ADDR_WATT_B_W2              = 0x30,
        REG_ADDR_WATT_B_W1              = 0x31,
        REG_ADDR_WATT_B_W0              = 0x32,

        REG_ADDR_VAR_A_W2               = 0x33,
        REG_ADDR_VAR_A_W1               = 0x34,
        REG_ADDR_VAR_A_W0               = 0x35,
        REG_ADDR_VAR_B_W2               = 0x36,
        REG_ADDR_VAR_B_W1               = 0x37,
        REG_ADDR_VAR_B_W0               = 0x38,

        REG_ADDR_VA_A_W2                = 0x39,
        REG_ADDR_VA_A_W1                = 0x3A,
        REG_ADDR_VA_A_W0                = 0x3B,
        REG_ADDR_VA_B_W2                = 0x3C,
        REG_ADDR_VA_B_W1                = 0x3D,
        REG_ADDR_VA_B_W0                = 0x3E,

        REG_ADDR_VFUND_A_H              = 0x40,
        REG_ADDR_VFUND_A_L              = 0x41,
        REG_ADDR_VHARM_A_H              = 0x42,
        REG_ADDR_VHARM_A_L              = 0x43,

        REG_ADDR_VFUND_B_H              = 0x44,
        REG_ADDR_VFUND_B_L              = 0x45,
        REG_ADDR_VHARM_B_H              = 0x46,
        REG_ADDR_VHARM_B_L              = 0x47,

        REG_ADDR_IFUND_A_H              = 0x48,
        REG_ADDR_IFUND_A_L              = 0x49,
        REG_ADDR_IHARM_A_H              = 0x4A,
        REG_ADDR_IHARM_A_L              = 0x4B,

        REG_ADDR_IFUND_B_H              = 0x4C,
        REG_ADDR_IFUND_B_L              = 0x4D,
        REG_ADDR_IHARM_B_H              = 0x4E,
        REG_ADDR_IHARM_B_L              = 0x4F,

        /* Register Addresses on Bootloader Mode */
        REG_ADDR_BTLDR_FLASH_ADDR       = 0x04,
        REG_ADDR_BTLDR_FLASH_DATA_0     = 0x05,
        REG_ADDR_BTLDR_FLASH_DATA_1     = 0x06,
        REG_ADDR_BTLDR_FLASH_DATA_2     = 0x07,
        REG_ADDR_BTLDR_FLASH_DATA_3     = 0x08,
        REG_ADDR_BTLDR_FLASH_DATA_4     = 0x09,
        REG_ADDR_BTLDR_FLASH_DATA_5     = 0x0A,
        REG_ADDR_BTLDR_FLASH_DATA_6     = 0x0B,
        REG_ADDR_BTLDR_FLASH_DATA_7     = 0x0C,
        REG_ADDR_BTLDR_FLASH_DATA_8     = 0x0D,
        REG_ADDR_BTLDR_FLASH_DATA_9     = 0x0E,
        REG_ADDR_BTLDR_FLASH_DATA_10    = 0x0F,
        REG_ADDR_BTLDR_FLASH_DATA_11    = 0x10,
        REG_ADDR_BTLDR_FLASH_DATA_12    = 0x11,
        REG_ADDR_BTLDR_FLASH_DATA_13    = 0x12,
        REG_ADDR_BTLDR_FLASH_DATA_14    = 0x13,
        REG_ADDR_BTLDR_FLASH_DATA_15    = 0x14,

        REG_ADDR_BTLDR_BOOTSIG_W1       = 0x18,
        REG_ADDR_BTLDR_BOOTSIG_W2       = 0x19,
        REG_ADDR_BTLDR_BOOTVER          = 0x1A,
        REG_ADDR_BTLDR_BOOTVER_TEST     = 0x1B
    } reg_addr_et;

    typedef struct
    {
        uint16_t    u16_ver;    // MM.mm version
        uint16_t    u16_test;   // test version
    } fw_info_st;


    fw_info_st      s_fw_info[NUM_DEVICES];

    EnMtrMSP430();
    bool init();
    bool reset();

    bool read_reg(device_et e_device, reg_addr_et e_reg_addr, uint16_t *pui16_reg_val);
    bool write_reg(device_et e_device, reg_addr_et e_reg_addr, uint16_t ui16_reg_val);
    bool read_mult_regs(device_et e_device, reg_addr_et e_reg_addr, uint16_t *pui16_reg_val, uint8_t ui8_n_reg);

    bool read_versions(device_et e_device);

private:
    void write_ss(device_et e_device, bool b_active);
    bool send_cmd(device_et e_device, command_et e_cmd, reg_addr_et e_reg_addr, uint16_t *pui16_reg_data, uint8_t ui8_nreg);
    bool init_config(device_et e_device);

};
