
#include <rom/ets_sys.h> // for ets_delay_us()

#include "global_defs.h"
#include "msp430.h"
#include "enmtr_msp430_cfg.h"


EnMtrMSP430::EnMtrMSP430()
{
    //
}

bool EnMtrMSP430::init()
{
    enmtr_init_pins();

    return EnergyMeter::init(ENMTR_SPI_MISO_PIN, ENMTR_SPI_MOSI_PIN, ENMTR_SPI_CLK_PIN, -1);
}

bool EnMtrMSP430::reset()
{
    enmtr_rst_pin(false);
    write_ss(DEVICE_PHASE_A, false);
    write_ss(DEVICE_PHASE_B, false);
    write_ss(DEVICE_PHASE_C, false);
    delayms(K_ENMTR_MSP430_RESET_DELAY);
    enmtr_rst_pin(true);
    delayms(K_ENMTR_MSP430_RESET_DELAY);

    memset(&s_fw_info, 0, sizeof(s_fw_info));

    if (!init_config(DEVICE_PHASE_A) || !init_config(DEVICE_PHASE_B) || !init_config(DEVICE_PHASE_C))
    {
        return false;
    }

    return true;
}

void EnMtrMSP430::write_ss(device_et e_device, bool b_active)
{
    switch (e_device)
    {
    // active low
    case DEVICE_PHASE_A: gpio_set_level(ENMTR_SPI_SSA_PIN, b_active ? 0 : 1); break;
    case DEVICE_PHASE_B: gpio_set_level(ENMTR_SPI_SSB_PIN, b_active ? 0 : 1); break;
    case DEVICE_PHASE_C: gpio_set_level(ENMTR_SPI_SSC_PIN, b_active ? 0 : 1); break;
    default: break; // ignore
    }
}

bool EnMtrMSP430::send_cmd(device_et e_device, command_et e_cmd, reg_addr_et e_reg_addr, uint16_t *pui16_reg_data, uint8_t ui8_nreg)
{
    uint8_t     aui8_cmd[2];
    uint8_t     aui8_data_buf[32];
    uint8_t     aui8_crc_buf[2];
    uint8_t     aui8_ack_stat[2];
    uint8_t     ui8_dummy;
    uint8_t     ui8_data_ctr;
    uint8_t     ui8_reg_ctr;
    uint8_t     ui8_ndata_bytes;
    uint16_t    ui16_crc_calc;
    uint16_t    ui16_crc_recv;
    bool        b_status = false;

    if ((NULL == pui16_reg_data) || (e_device > DEVICE_PHASE_C) ||
        (0 == ui8_nreg) || (ui8_nreg > 16))
    {
        // invalid args
    }
    else if (((CMD_SINGLE_REG_READ == e_cmd) && (ui8_nreg != 1)) ||
             ((CMD_SINGLE_REG_WRITE == e_cmd) && (ui8_nreg != 1)) )
    {
        // invalid length
    }
    else
    {
        aui8_cmd[0]     = ((((ui8_nreg - 1) & 0x0F) << 4) | (((uint8_t)e_reg_addr & 0xC0) >> 4) | (0x01));
        aui8_cmd[1]     = (((uint8_t)e_reg_addr & 0x3F) << 2);
        ui8_dummy       = 0;
        ui8_ndata_bytes = (ui8_nreg * 2);
        aui8_ack_stat[0] = 0;
        aui8_ack_stat[1] = 0;

        write_ss(e_device, true);
        ets_delay_us(K_ENMTR_SPI_CS_CLK_DELAY_US);

        switch (e_cmd)
        {
        case CMD_SINGLE_REG_READ:   // fall-through
        case CMD_MULTIPLE_REG_READ:
            if (false == transfer(&aui8_cmd[0], NULL, 2))
            {
                //
            }
            else if (false == transfer(&ui8_dummy, NULL, 1))
            {
                //
            }
            else if (false == transfer(NULL, &aui8_data_buf[0], (uint16_t)ui8_ndata_bytes))
            {
                //
            }
            else if (false == transfer(NULL, &aui8_crc_buf[0], 2))
            {
                //
            }
            else if (false == transfer(NULL, &aui8_ack_stat[0], 2))
            {
                //
            }
            else if ((0xA5 != aui8_ack_stat[0]) || (0x5A != aui8_ack_stat[1]))
            {
                LOGW("cmd %x-%02x (%d) ack error %02x-%02x", e_cmd, e_reg_addr, e_device, aui8_ack_stat[0], aui8_ack_stat[1]);
            }
            else
            {
                ui16_crc_calc = crc16CalcBlock(&aui8_data_buf[0], (uint16_t)ui8_ndata_bytes);
                ui16_crc_recv = (((uint16_t)aui8_crc_buf[1] << 8) | ((uint16_t)aui8_crc_buf[0]));

                if (ui16_crc_calc != ui16_crc_recv)
                {
                    LOGW("cmd %x (%d) crc failed (%04x != %04x)", e_cmd, e_device, ui16_crc_calc, ui16_crc_recv);
                }
                else
                {
                    ui8_data_ctr = 0;
                    for (ui8_reg_ctr = 0; ui8_reg_ctr < ui8_nreg; ui8_reg_ctr++)
                    {
                        pui16_reg_data[ui8_reg_ctr]  = (((uint16_t)aui8_data_buf[ui8_data_ctr++] & 0xFF) << 8);
                        pui16_reg_data[ui8_reg_ctr] |= (((uint16_t)aui8_data_buf[ui8_data_ctr++] & 0xFF) << 0);
                    }
                    b_status = true;
                }
            }
            break;

        case CMD_SINGLE_REG_WRITE:  // fall-through
        case CMD_MULTIPLE_REG_WRITE:
            aui8_cmd[1] |= 0x02;

            ui8_data_ctr = 0;
            for(ui8_reg_ctr = 0; ui8_reg_ctr < ui8_nreg; ui8_reg_ctr++)
            {
                aui8_data_buf[ui8_data_ctr++] = (uint8_t)((pui16_reg_data[ui8_reg_ctr] >> 8) & 0xFF);
                aui8_data_buf[ui8_data_ctr++] = (uint8_t)((pui16_reg_data[ui8_reg_ctr] >> 0) & 0xFF);
            }

            ui16_crc_calc = crc16CalcBlock(&aui8_data_buf[0], (uint16_t)ui8_ndata_bytes);
            aui8_crc_buf[1] = (uint8_t)((ui16_crc_calc >> 8) & 0xFF);
            aui8_crc_buf[0] = (uint8_t)((ui16_crc_calc >> 0) & 0xFF);

            if (false == transfer(&aui8_cmd[0], NULL, 2))
            {
                //
            }
            else if (false == transfer(&aui8_data_buf[0], NULL, (uint16_t)ui8_ndata_bytes))
            {
                //
            }
            else if (false == transfer(&aui8_crc_buf[0], NULL, 2))
            {
                //
            }
            else if (false == transfer(&ui8_dummy, NULL, 1))
            {
                //
            }
            else if (false == transfer(NULL, &aui8_ack_stat[0], 2))
            {
                //
            }
            else if ((0xA5 != aui8_ack_stat[0]) || (0x5A != aui8_ack_stat[1]))
            {
                LOGW("cmd %x (%d) ack error %02x-%02x", e_cmd, e_device, aui8_ack_stat[0], aui8_ack_stat[1]);
            }
            else
            {
                b_status = true;
            }

            break;

        default:
            // ignore
            break;
        }

        ets_delay_us(K_ENMTR_SPI_CS_DELAY_US);
        write_ss(e_device, false);
    }

    return b_status;
}

bool EnMtrMSP430::init_config(device_et e_device)
{
    uint16_t    aui16_boot_sig[2];
    bool        b_status = false;

    if (false == read_mult_regs(e_device, REG_ADDR_BTLDR_BOOTSIG_W1, aui16_boot_sig, 2))
    {
        //
    }
    else if ((K_ENMTR_MSP430_BOOT_SIG_W1_VAL == aui16_boot_sig[0]) && (K_ENMTR_MSP430_BOOT_SIG_W2_VAL == aui16_boot_sig[1]))
    {
        LOGW("invalid boot sig (%d) %04x-%04x", e_device, aui16_boot_sig[0], aui16_boot_sig[1]);
    }
    else
    {
        b_status = true;
    }

    return b_status;
}

bool EnMtrMSP430::read_reg(device_et e_device, reg_addr_et e_reg_addr, uint16_t *pui16_reg_val)
{
    return send_cmd(e_device, CMD_SINGLE_REG_READ, e_reg_addr, pui16_reg_val, 1);
}

bool EnMtrMSP430::write_reg(device_et e_device, reg_addr_et e_reg_addr, uint16_t ui16_reg_val)
{
    return send_cmd(e_device, CMD_SINGLE_REG_WRITE, e_reg_addr, &ui16_reg_val, 1);
}

bool EnMtrMSP430::read_mult_regs(device_et e_device, reg_addr_et e_reg_addr, uint16_t *pui16_reg_val, uint8_t ui8_n_reg)
{
    return send_cmd(e_device, CMD_MULTIPLE_REG_READ, e_reg_addr, pui16_reg_val, ui8_n_reg);
}

bool EnMtrMSP430::read_versions(device_et e_device)
{
    bool b_status = false;

    if (false == read_reg(e_device, REG_ADDR_FWVER, &s_fw_info[e_device].u16_ver))
    {
        LOGW("read version failed");
    }
    else if (false == read_reg(e_device, REG_ADDR_FWVER_TEST, &s_fw_info[e_device].u16_test))
    {
        LOGW("read test-version failed");
    }
    else
    {
        b_status = true;
    }

    return b_status;
}
