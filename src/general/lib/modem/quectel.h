
#pragma once

#include "modem.h"
#include "quectel_defs.h"


class QuectelModem : public CellularModem
{
public:
    typedef void (*pin_set_func)(bool b_high);

    QuectelModem(pin_set_func reset, pin_set_func pwr_on, pin_set_func dtr);
    void init();

    void reset(bool b_full);
    bool is_ready() const { return m_ready; }
    bool is_powerdown() const { return m_powerdown; }

    void parse_urc(char *pc_urc);
    void handle_events(uint32_t ui32_wait_ms);

    bool get_modem_info();
    bool get_sim_info();
    bool set_network_bands(const char *pc_gsm, const char *pc_lte, const char *pc_extra);
    // model/series specific functions
    virtual bool set_lterat_search(int n_cat) { return false; }

    bool get_signal_quality();
    bool get_network_registration();
    bool get_operator_info(int n_mode, int n_format);
    bool get_cell_info();

    bool get_connection_config(pdp_ctx_et id_ctx);
    bool set_connection_config(pdp_ctx_et id_ctx, const char *pc_apn);
    bool activate_pdp(pdp_ctx_et id_ctx);
    bool deactivate_pdp(pdp_ctx_et id_ctx);
    bool get_active_connection();
    bool config_pdp_options();
    uint16_t retrieve_pdp_data(int id_conn, uint8_t **ppui8_buff /*in=buff and out=data*/, uint16_t ui16_buff_size);
    int show_pdp_error();
    int last_pdp_error();
    void getInfo(char *ac_serialnumber, char *ac_imei, char *ac_imsi, char *ac_iccid);

private:
    pin_set_func    fp_reset_pin;
    pin_set_func    fp_pwr_on_pin;
    pin_set_func    fp_dtr_pin;

    bool            m_ready;
    bool            m_powerdown;

    // s_handlers callbacks
    void parse_urc_cereg(const char *pc_cereg);     // registration status
    void parse_urc_qiopen(const char *pc_qiopen);   // session status
    void parse_urc_qiurc(const char *pc_qiurc);     // for both pdp_status & data_incoming
};


// or, use class template instead ?
class Quectel_EGx: public QuectelModem { using QuectelModem::QuectelModem; };
class Quectel_EG2x: public Quectel_EGx { using Quectel_EGx::Quectel_EGx; }; // e.g. EG21
class Quectel_EG9x: public Quectel_EGx { using Quectel_EGx::Quectel_EGx; }; // e.g. EG95
class Quectel_EG91x: public Quectel_EGx { using Quectel_EGx::Quectel_EGx; }; // e.g. EG915

class Quectel_BGx: public QuectelModem { using QuectelModem::QuectelModem; };
class Quectel_BG9x: public Quectel_BGx { using Quectel_BGx::Quectel_BGx; }; // e.g. BG95
