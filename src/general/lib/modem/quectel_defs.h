
#pragma once


#define K_MODEM_STR_CMD_SET_FUNCTIONALITY           "+CFUN"
#define K_MODEM_STR_CMD_POWER_DOWN                  "+QPOWD"

#define K_MODEM_STR_CMD_SET_CMEE                    "+CMEE"

#define K_MODEM_STR_CMD_GET_MANUFACTURER            "+CGMI"
#define K_MODEM_STR_CMD_GET_MODEL                   "+CGMM"
#define K_MODEM_STR_CMD_GET_REVISION                "+CGMR"
#define K_MODEM_STR_CMD_GET_IMEI                    "+CGSN"
#define K_MODEM_STR_CMD_GET_IMSI                    "+CIMI"
#define K_MODEM_STR_CMD_GET_ICCID                   "+QCCID"


#define K_MODEM_STR_CMD_GET_SIGNAL_QUALITY          "+CSQ"
#define K_MODEM_STR_CMD_GET_EXT_SIGNAL_QUALITY      "+QCSQ"
#define K_MODEM_STR_CMD_NETWORK_REGISTRATION        "+CREG"
#define K_MODEM_STR_CMD_EPS_REGISTRATION            "+CEREG"    // LTE-EPS network
#define K_MODEM_STR_CMD_NETWORK_OPERATOR            "+COPS"
#define K_MODEM_STR_CMD_EXT_CONFIGURATION           "+QCFG"
#define K_MODEM_STR_CMD_ENGINEERING_MODE            "+QENG"     // ??


#define K_MODEM_STR_CMD_DEFINE_PDP_CONTEXT          "+CGDCONT"      // set APN only
#define K_MODEM_STR_CMD_CONFIG_PDP_CONTEXT          "+QICSGP"       // set APN and (optionally) Auth
#define K_MODEM_STR_CMD_ACTIVATE_PDP_CONTEXT        "+QIACT"
#define K_MODEM_STR_CMD_DEACTIVATE_PDP_CONTEXT      "+QIDEACT"
#define K_MODEM_STR_CMD_PDP_CONFIG_OPT_PARAMS       "+QICFG"
#define K_MODEM_STR_CMD_GET_PDP_ERROR_CODE          "+QIGETERROR"
#define K_MODEM_STR_CMD_PDP_OPEN_SOCKET             "+QIOPEN"
#define K_MODEM_STR_CMD_PDP_CLOSE_SOCKET            "+QICLOSE"
#define K_MODEM_STR_CMD_QUERY_SOCKET_STATUS         "+QISTATE"
#define K_MODEM_STR_CMD_PDP_SEND_DATA               "+QISEND"
#define K_MODEM_STR_CMD_PDP_RECEIVE_DATA            "+QIRD"

#define K_MODEM_STR_PDP_READY_TO_SEND               ">"
#define K_MODEM_STR_PDP_RESP_SEND_OK                "SEND OK"
#define K_MODEM_STR_PDP_RESP_SEND_FAIL              "SEND FAIL"
#define K_MODEM_STR_URC_PDP_NOTIF                   "+QIURC"

#define K_MODEM_STR_URC_READY                       "RDY"           // ME init ok (i.e. after power on)
#define K_MODEM_STR_URC_POWERDOWN                   "POWERED DOWN"  // module power down

