
#ifndef __EDGE_PAYLOAD_DEFS_H__
#define __EDGE_PAYLOAD_DEFS_H__

#include "edge_payload_cfg.h"


/*
 * Global Constants
 */
typedef enum
{
    EP_SERVER_RESP_SERVER_BUSY_ERROR    = 0xFA
    , EP_SERVER_RESP_AUTH_ERROR         = 0xFB
    , EP_SERVER_RESP_TIMEOUT_ERROR      = 0xFC
    , EP_SERVER_RESP_INVALID_DATA_ERROR = 0xFD
    , EP_SERVER_RESP_DATABASE_ERROR     = 0xFE
    , EP_SERVER_RESP_GENERAL_ERROR      = 0xFF
    , EP_SERVER_RESP_OK                 = 0x50
    , EP_SERVER_RESP_UPDATE_REQ         = 0x51
    , EP_SERVER_RESP_CONFIG_REQ         = 0x52
    , EP_SERVER_RESP_EXEC_SCRIPT_REQ    = 0x53
    , EP_SERVER_RESP_OPERATION_REQ      = 0x54
} ep_server_response_et;

typedef enum
{
    EP_PROTOCOL_VER_LEGACY              = 0x01
    , EP_PROTOCOL_VER_LINEAR11          = 0x02

    , EP_PROTOCOL_VER_MIN               = EP_PROTOCOL_VER_LEGACY
    , EP_PROTOCOL_VER_MAX               = EP_PROTOCOL_VER_LINEAR11
} ep_protocol_ver_et;

typedef enum
{
    EP_DEVICE_TYPE_PFC                  = 0x01  /* Powersave+/eController */
    , EP_DEVICE_TYPE_CVR                = 0x02  /* SolarIQ/eSaver+/EdgeIQ */
    , EP_DEVICE_TYPE_BMS                = 0x03  /* ? */
    , EP_DEVICE_TYPE_PFCMOD             = 0x04  /* dkVar Module */
    , EP_DEVICE_TYPE_SNR                = 0x05  /* eSensor */
    , EP_DEVICE_TYPE_SNR2               = 0x06  /* eSensor unified */
    , EP_DEVICE_TYPE_PFC052             = 0x07  /* kVar 052's */
    , EP_DEVICE_TYPE_PPEM               = 0x08  /* PowerPlay Energy Monitor */
    , EP_DEVICE_TYPE_GDL                = 0x09  /* Grid Data Logger */
    , EP_DEVICE_TYPE_NVRD               = 0x0A  /* Network Voltage Regulating Device / VRDT / EdgeIQ-based */
    , EP_DEVICE_TYPE_PDL                = 0x0B  /* Phase Data Logger (GDL-based)*/

    , EP_DEVICE_TYPE_MIN                = EP_DEVICE_TYPE_PFC
    , EP_DEVICE_TYPE_MAX                = EP_DEVICE_TYPE_PDL
} ep_dev_type_et;

typedef enum
{
    EP_PHASE_INDEX_SINGLE               = 0x00
    , EP_PHASE_INDEX_L1                 = 0x00
    , EP_PHASE_INDEX_L2                 = 0x01
    , EP_PHASE_INDEX_L3                 = 0x02
    , EP_PHASE_INDEX_GENERAL            = 0x03

    , EP_PHASE_INDEX_MAX                = EP_PHASE_INDEX_GENERAL
} ep_phase_index_et;

typedef enum
{
    /* CVR device monitor id */
    EP_MON_ID_CVR_LINE_FREQ             = 0x30
    , EP_MON_ID_CVR_IN_PF               = 0x31
    , EP_MON_ID_CVR_IN_VRMS             = 0x32
    , EP_MON_ID_CVR_IN_IRMS             = 0x33
    , EP_MON_ID_CVR_IN_WATT             = 0x34
    , EP_MON_ID_CVR_IN_VA               = 0x35
    , EP_MON_ID_CVR_IN_VAR              = 0x36

    , EP_MON_ID_CVR_OUT_PF              = 0x37
    , EP_MON_ID_CVR_OUT_VRMS            = 0x38
    , EP_MON_ID_CVR_OUT_IRMS            = 0x39
    , EP_MON_ID_CVR_OUT_WATT            = 0x3A
    , EP_MON_ID_CVR_OUT_VA              = 0x3B
    , EP_MON_ID_CVR_OUT_VAR             = 0x3C

    , EP_MON_ID_CVR_EFFICIENCY          = 0x3D
    , EP_MON_ID_CVR_IN_ITHD             = 0x3E
    , EP_MON_ID_CVR_IN_VTHD             = 0x3F
    , EP_MON_ID_CVR_OUT_ITHD            = 0x40
    , EP_MON_ID_CVR_OUT_VTHD            = 0x41

    , EP_MON_ID_CVR_THERM1              = 0X42
    , EP_MON_ID_CVR_THERM2              = 0x43
    , EP_MON_ID_CVR_THERM3              = 0x44
    , EP_MON_ID_CVR_OUT_ENERGY          = 0x45

    , EP_MON_ID_CVR_INV_PF              = 0x46
    , EP_MON_ID_CVR_INV_VRMS            = 0x47
    , EP_MON_ID_CVR_INV_IRMS            = 0x48
    , EP_MON_ID_CVR_INV_WATT            = 0x49

    , EP_MON_ID_CVR_MIN                 = EP_MON_ID_CVR_LINE_FREQ
    , EP_MON_ID_CVR_MAX                 = EP_MON_ID_CVR_INV_WATT

    /* PFC device monitor id */
    , EP_MON_ID_PFC_CORR_PF             = 0x30
    , EP_MON_ID_PFC_UNCORR_PF           = 0x31
    , EP_MON_ID_PFC_LINE_FREQ           = 0x32

    , EP_MON_ID_PFC_WATT                = 0x33
    , EP_MON_ID_PFC_WATT_T              = 0x34
    , EP_MON_ID_PFC_VA                  = 0x35
    , EP_MON_ID_PFC_VA_T                = 0x36
    , EP_MON_ID_PFC_VAR                 = 0x37
    , EP_MON_ID_PFC_VAR_T               = 0x38
    , EP_MON_ID_PFC_PF                  = 0x39
    , EP_MON_ID_PFC_PF_T                = 0x3A

    , EP_MON_ID_PFC_HARM_REAL           = 0x3B
    , EP_MON_ID_PFC_HARM_REACTIVE       = 0x3C

    , EP_MON_ID_PFC_VRMS                = 0x3D
    , EP_MON_ID_PFC_VHARM               = 0x3E
    , EP_MON_ID_PFC_VTHD                = 0x3F
    , EP_MON_ID_PFC_VIMBALANCE          = 0x40

    , EP_MON_ID_PFC_IRMS                = 0x41
    , EP_MON_ID_PFC_IHARM               = 0x42
    , EP_MON_ID_PFC_ITHD                = 0x43
    , EP_MON_ID_PFC_IIMBALANCE          = 0x44

    , EP_MON_ID_PFC_VPHASE              = 0x45
    , EP_MON_ID_PFC_AMBIENT_THERM       = 0x46

    , EP_MON_ID_PFC_MIN                 = EP_MON_ID_PFC_CORR_PF
    , EP_MON_ID_PFC_MAX                 = EP_MON_ID_PFC_AMBIENT_THERM

    /* PFCMOD device monitor id */
    , EP_MON_ID_PFCMOD_MODULE           = 0x30
    , EP_MON_ID_PFCMOD_KVAR             = 0x31

    , EP_MON_ID_PFCMOD_ACTIVE_CAPS      = 0x32
    , EP_MON_ID_PFCMOD_QUEUED_CAPS      = 0x33

    , EP_MON_ID_PFCMOD_CAP_IRMS         = 0x34
    , EP_MON_ID_PFCMOD_CAP_ITHD         = 0x35

    , EP_MON_ID_PFCMOD_THERM1           = 0x36
    , EP_MON_ID_PFCMOD_THERM2           = 0x37
    , EP_MON_ID_PFCMOD_THERM3           = 0x38
    , EP_MON_ID_PFCMOD_THERM4           = 0x39

    , EP_MON_ID_PFCMOD_SET_CAPS         = 0x3A
    , EP_MON_ID_PFCMOD_STATE            = 0x3B

    , EP_MON_ID_PFCMOD_MIN              = EP_MON_ID_PFCMOD_MODULE
    , EP_MON_ID_PFCMOD_MAX              = EP_MON_ID_PFCMOD_STATE

    /* SNR device monitor id */
    , EP_MON_ID_SNR_CORRECTED_PF        = 0x30
    , EP_MON_ID_SNR_UNCORRECTED_PF      = 0x31
    , EP_MON_ID_SNR_LINE_FREQ           = 0x32

    , EP_MON_ID_SNR_WATT                = 0x33
    , EP_MON_ID_SNR_WATT_T              = 0x34
    , EP_MON_ID_SNR_VA                  = 0x35
    , EP_MON_ID_SNR_VA_T                = 0x36
    , EP_MON_ID_SNR_VAR                 = 0x37
    , EP_MON_ID_SNR_VAR_T               = 0x38
    , EP_MON_ID_SNR_PF                  = 0x39
    , EP_MON_ID_SNR_PF_T                = 0x3A

    , EP_MON_ID_SNR_HARM_REAL           = 0x3B
    , EP_MON_ID_SNR_HARM_REACTIVE       = 0x3C

    , EP_MON_ID_SNR_VRMS                = 0x3D
    , EP_MON_ID_SNR_VHARM               = 0x3E
    , EP_MON_ID_SNR_VTHD                = 0x3F
    , EP_MON_ID_SNR_VIMBALANCE          = 0x40

    , EP_MON_ID_SNR_IRMS                = 0x41
    , EP_MON_ID_SNR_IHARM               = 0x42
    , EP_MON_ID_SNR_ITHD                = 0x43
    , EP_MON_ID_SNR_IIMBALANCE          = 0x44

    , EP_MON_ID_SNR_VPHASE              = 0x45
    , EP_MON_ID_SNR_THERM1              = 0x46
    , EP_MON_ID_SNR_THERM2              = 0x47

    , EP_MON_ID_SNR_EXPORT_KWH          = 0x48
    , EP_MON_ID_SNR_IMPORT_KWH          = 0x49

    , EP_MON_ID_SNR_DEMAND_WATT         = 0x4A
    , EP_MON_ID_SNR_DEMAND_VA           = 0x4B
    , EP_MON_ID_SNR_DEMAND_VAR          = 0x4C

    , EP_MON_ID_SNR_MIN                 = EP_MON_ID_SNR_CORRECTED_PF
    , EP_MON_ID_SNR_MAX                 = EP_MON_ID_SNR_DEMAND_VAR

    /* SNR2 device monitor id */
    , EP_MON_ID_SNR2_LINE_FREQ          = 0x30
    , EP_MON_ID_SNR2_WATT_T             = 0x31
    , EP_MON_ID_SNR2_VA_T               = 0x32
    , EP_MON_ID_SNR2_VAR_T              = 0x33
    , EP_MON_ID_SNR2_PF_T               = 0x34
    , EP_MON_ID_SNR2_DEMAND_IRMS_T      = 0x35
    , EP_MON_ID_SNR2_INEUTRAL           = 0x36
    , EP_MON_ID_SNR2_TRF_LOADING        = 0x37

    , EP_MON_ID_SNR2_RSSI_MIN           = 0x40
    , EP_MON_ID_SNR2_RSSI_AVE           = 0x41
    , EP_MON_ID_SNR2_RSRP               = 0x42
    , EP_MON_ID_SNR2_RSRQ               = 0x43

    , EP_MON_ID_SNR2_EXPORT_KWH_T       = 0x50
    , EP_MON_ID_SNR2_IMPORT_KWH_T       = 0x51
    , EP_MON_ID_SNR2_LEADING_KVARH_T    = 0x52
    , EP_MON_ID_SNR2_LAGGING_KVARH_T    = 0x53
    , EP_MON_ID_SNR2_KVAH_T             = 0x54

    , EP_MON_ID_SNR2_TEMPERATURE_1      = 0x70
    , EP_MON_ID_SNR2_TEMPERATURE_2      = 0x71
    , EP_MON_ID_SNR2_TEMPERATURE_3      = 0x72
    , EP_MON_ID_SNR2_TEMPERATURE_4      = 0x73

    , EP_MON_ID_SNR2_WATT               = 0x80
    , EP_MON_ID_SNR2_VA                 = 0x81
    , EP_MON_ID_SNR2_VAR                = 0x82
    , EP_MON_ID_SNR2_PF                 = 0x83
    , EP_MON_ID_SNR2_WATTAVE            = 0x84
    , EP_MON_ID_SNR2_WATTMAX            = 0x85
    , EP_MON_ID_SNR2_WATTMIN            = 0x86
    , EP_MON_ID_SNR2_VAAVE              = 0x87
    , EP_MON_ID_SNR2_VAMAX              = 0x88
    , EP_MON_ID_SNR2_VAMIN              = 0x89
    , EP_MON_ID_SNR2_VARAVE             = 0x8A
    , EP_MON_ID_SNR2_VARMAX             = 0x8B
    , EP_MON_ID_SNR2_VARMIN             = 0x8C

    , EP_MON_ID_SNR2_VRMS               = 0x90
    , EP_MON_ID_SNR2_VAVE               = 0x91
    , EP_MON_ID_SNR2_VMAX               = 0x92
    , EP_MON_ID_SNR2_VMIN               = 0x93
    , EP_MON_ID_SNR2_VTHD               = 0x94
    , EP_MON_ID_SNR2_VPHASE             = 0x95
    , EP_MON_ID_SNR2_VRATIO             = 0x96
    , EP_MON_ID_SNR2_VHARM              = 0x97
    , EP_MON_ID_SNR2_FLICKER            = 0x98

    , EP_MON_ID_SNR2_IRMS               = 0xA0
    , EP_MON_ID_SNR2_IAVE               = 0xA1
    , EP_MON_ID_SNR2_IMAX               = 0xA2
    , EP_MON_ID_SNR2_IMIN               = 0xA3
    , EP_MON_ID_SNR2_ITHD               = 0xA4
    , EP_MON_ID_SNR2_IHARM              = 0xA5

    , EP_MON_ID_SNR2_DEMAND_WATT        = 0xB0
    , EP_MON_ID_SNR2_DEMAND_VA          = 0xB1
    , EP_MON_ID_SNR2_DEMAND_VAR         = 0xB2
    , EP_MON_ID_SNR2_DEMAND_IAVE        = 0xB3
    , EP_MON_ID_SNR2_DEMAND_IMAX        = 0xB4
    , EP_MON_ID_SNR2_TDD                = 0xB5
    , EP_MON_ID_SNR2_DEMAND_WATTMAX     = 0xB6
    , EP_MON_ID_SNR2_DEMAND_VAMAX       = 0xB7
    , EP_MON_ID_SNR2_DEMAND_VARMAX      = 0xB8

    , EP_MON_ID_SNR2_EXPORT_KWH         = 0xC0
    , EP_MON_ID_SNR2_IMPORT_KWH         = 0xC1
    , EP_MON_ID_SNR2_LEADING_KVARH      = 0xC2
    , EP_MON_ID_SNR2_LAGGING_KVARH      = 0xC3
    , EP_MON_ID_SNR2_KVAH               = 0xC4

    , EP_MON_ID_SNR2_EXPORT_KWH_32      = 0xC8
    , EP_MON_ID_SNR2_IMPORT_KWH_32      = 0xC9
    , EP_MON_ID_SNR2_LEADING_KVARH_32   = 0xCA
    , EP_MON_ID_SNR2_LAGGING_KVARH_32   = 0xCB
    , EP_MON_ID_SNR2_KVAH_32            = 0xCC

    , EP_MON_ID_SNR2_V_FUND             = 0xD0
    , EP_MON_ID_SNR2_V_3RD_HARM         = 0xD1
    , EP_MON_ID_SNR2_V_5TH_HARM         = 0xD2
    , EP_MON_ID_SNR2_V_7TH_HARM         = 0xD3
    , EP_MON_ID_SNR2_V_9TH_HARM         = 0xD4
    , EP_MON_ID_SNR2_V_11TH_HARM        = 0xD5
    , EP_MON_ID_SNR2_V_13TH_HARM        = 0xD6
    , EP_MON_ID_SNR2_V_15TH_HARM        = 0xD7
    , EP_MON_ID_SNR2_V_17TH_HARM        = 0xD8
    , EP_MON_ID_SNR2_V_19TH_HARM        = 0xD9
    , EP_MON_ID_SNR2_V_21TH_HARM        = 0xDA

    , EP_MON_ID_SNR2_I_FUND             = 0xE0
    , EP_MON_ID_SNR2_I_3RD_HARM         = 0xE1
    , EP_MON_ID_SNR2_I_5TH_HARM         = 0xE2
    , EP_MON_ID_SNR2_I_7TH_HARM         = 0xE3
    , EP_MON_ID_SNR2_I_9TH_HARM         = 0xE4
    , EP_MON_ID_SNR2_I_11TH_HARM        = 0xE5
    , EP_MON_ID_SNR2_I_13TH_HARM        = 0xE6
    , EP_MON_ID_SNR2_I_15TH_HARM        = 0xE7
    , EP_MON_ID_SNR2_I_17TH_HARM        = 0xE8
    , EP_MON_ID_SNR2_I_19TH_HARM        = 0xE9
    , EP_MON_ID_SNR2_I_21TH_HARM        = 0xEA

    , EP_MON_ID_SNR2_MIN                = EP_MON_ID_SNR2_LINE_FREQ
    , EP_MON_ID_SNR2_MAX                = EP_MON_ID_SNR2_I_21TH_HARM

    /* PPEM device monitor id */
    , EP_MON_ID_PPEM_VRMS               = 0x30
    , EP_MON_ID_PPEM_VRMS_MAX           = 0x31
    , EP_MON_ID_PPEM_VRMS_MIN           = 0x32
    , EP_MON_ID_PPEM_IRMS_1             = 0x34
    , EP_MON_ID_PPEM_IRMS_2             = 0x35
    , EP_MON_ID_PPEM_PF_1               = 0x38
    , EP_MON_ID_PPEM_PF_2               = 0x39
    , EP_MON_ID_PPEM_IRMS_1_MAX         = 0x3A
    , EP_MON_ID_PPEM_IRMS_1_MIN         = 0x3B
    , EP_MON_ID_PPEM_IRMS_2_MAX         = 0x3C
    , EP_MON_ID_PPEM_IRMS_2_MIN         = 0x3D
    , EP_MON_ID_PPEM_WATT_1             = 0x40
    , EP_MON_ID_PPEM_WATT_2             = 0x41
    , EP_MON_ID_PPEM_VAR_1              = 0x44
    , EP_MON_ID_PPEM_VAR_2              = 0x45
    , EP_MON_ID_PPEM_VA_1               = 0x48
    , EP_MON_ID_PPEM_VA_2               = 0x49
    , EP_MON_ID_PPEM_VTHD               = 0x50
    , EP_MON_ID_PPEM_ITHD_1             = 0x54
    , EP_MON_ID_PPEM_ITHD_2             = 0x55
    // measurement/sampling interval
    , EP_MON_ID_PPEM_INTERVAL           = 0x60
    // import/export kWh (?)
    , EP_MON_ID_PPEM_WATTSECPOS_1       = 0x61
    , EP_MON_ID_PPEM_WATTSECPOS_2       = 0x62
    , EP_MON_ID_PPEM_WATTSECNEG_1       = 0x63
    , EP_MON_ID_PPEM_WATTSECNEG_2       = 0x64
    // leading/lagging kVARh (?)
    , EP_MON_ID_PPEM_VARSECPOS_1        = 0x65
    , EP_MON_ID_PPEM_VARSECPOS_2        = 0x66
    , EP_MON_ID_PPEM_VARSECNEG_1        = 0x67
    , EP_MON_ID_PPEM_VARSECNEG_2        = 0x68
    , EP_MON_ID_PPEM_LINEFREQ           = 0x80
    , EP_MON_ID_PPEM_VFUND              = 0x91
    , EP_MON_ID_PPEM_VHARM3             = 0x93
    , EP_MON_ID_PPEM_VHARM5             = 0x95
    , EP_MON_ID_PPEM_VHARM7             = 0x97
    , EP_MON_ID_PPEM_VHARM9             = 0x99
    , EP_MON_ID_PPEM_VHARM11            = 0x9B
    , EP_MON_ID_PPEM_VHARM13            = 0x9D
    , EP_MON_ID_PPEM_VHARM15            = 0x9F
    , EP_MON_ID_PPEM_IFUND_1            = 0xB1
    , EP_MON_ID_PPEM_IHARM3_1           = 0xB3
    , EP_MON_ID_PPEM_IHARM5_1           = 0xB5
    , EP_MON_ID_PPEM_IHARM7_1           = 0xB7
    , EP_MON_ID_PPEM_IHARM9_1           = 0xB9
    , EP_MON_ID_PPEM_IHARM11_1          = 0xBB
    , EP_MON_ID_PPEM_IHARM13_1          = 0xBD
    , EP_MON_ID_PPEM_IHARM15_1          = 0xBF
    , EP_MON_ID_PPEM_IFUND_2            = 0xD1
    , EP_MON_ID_PPEM_IHARM3_2           = 0xD3
    , EP_MON_ID_PPEM_IHARM5_2           = 0xD5
    , EP_MON_ID_PPEM_IHARM7_2           = 0xD7
    , EP_MON_ID_PPEM_IHARM9_2           = 0xD9
    , EP_MON_ID_PPEM_IHARM11_2          = 0xDB
    , EP_MON_ID_PPEM_IHARM13_2          = 0xDD
    , EP_MON_ID_PPEM_IHARM15_2          = 0xDF
    , EP_MON_ID_PPEM_RSSI_MIN           = 0xE1
    , EP_MON_ID_PPEM_RSSI_AVE           = 0xE2
    , EP_MON_ID_PPEM_RSRP               = 0xE3
    , EP_MON_ID_PPEM_RSRQ               = 0xE4

    , EP_MON_ID_PPEM_MIN                = EP_MON_ID_PPEM_VRMS
    , EP_MON_ID_PPEM_MAX                = EP_MON_ID_PPEM_IHARM15_2

    /* GDL device monitor id */
    , EP_MON_ID_GDL_LINE_FREQ          = 0x30
    , EP_MON_ID_GDL_WATT_T             = 0x31
    , EP_MON_ID_GDL_VA_T               = 0x32
    , EP_MON_ID_GDL_VAR_T              = 0x33
    , EP_MON_ID_GDL_PF_T               = 0x34
    , EP_MON_ID_GDL_DEMAND_IRMS_T      = 0x35
    , EP_MON_ID_GDL_INEUTRAL           = 0x36
    , EP_MON_ID_GDL_TRF                = 0x37

    , EP_MON_ID_GDL_RSSI_MIN           = 0x40
    , EP_MON_ID_GDL_RSSI_AVE           = 0x41
    , EP_MON_ID_GDL_RSRP               = 0x42
    , EP_MON_ID_GDL_RSRQ               = 0x43

    , EP_MON_ID_GDL_EXPORT_KWH_T       = 0x50
    , EP_MON_ID_GDL_IMPORT_KWH_T       = 0x51
    , EP_MON_ID_GDL_LEADING_KVARH_T    = 0x52
    , EP_MON_ID_GDL_LAGGING_KVARH_T    = 0x53
    , EP_MON_ID_GDL_KVAH_T             = 0x54

    , EP_MON_ID_GDL_TEMPERATURE_1      = 0x70
    , EP_MON_ID_GDL_TEMPERATURE_2      = 0x71
    , EP_MON_ID_GDL_TEMPERATURE_3      = 0x72
    , EP_MON_ID_GDL_TEMPERATURE_4      = 0x73

    , EP_MON_ID_GDL_WATT               = 0x80
    , EP_MON_ID_GDL_VA                 = 0x81
    , EP_MON_ID_GDL_VAR                = 0x82
    , EP_MON_ID_GDL_PF                 = 0x83
    , EP_MON_ID_GDL_WATTAVE            = 0x84
    , EP_MON_ID_GDL_WATTMAX            = 0x85
    , EP_MON_ID_GDL_WATTMIN            = 0x86
    , EP_MON_ID_GDL_VAAVE              = 0x87
    , EP_MON_ID_GDL_VAMAX              = 0x88
    , EP_MON_ID_GDL_VAMIN              = 0x89
    , EP_MON_ID_GDL_VARAVE             = 0x8A
    , EP_MON_ID_GDL_VARMAX             = 0x8B
    , EP_MON_ID_GDL_VARMIN             = 0x8C

    , EP_MON_ID_GDL_VRMS               = 0x90
    , EP_MON_ID_GDL_VAVE               = 0x91
    , EP_MON_ID_GDL_VMAX               = 0x92
    , EP_MON_ID_GDL_VMIN               = 0x93
    , EP_MON_ID_GDL_VTHD               = 0x94
    , EP_MON_ID_GDL_VPHASE             = 0x95
    , EP_MON_ID_GDL_VRATIO             = 0x96
    , EP_MON_ID_GDL_VHARM              = 0x97
    , EP_MON_ID_GDL_FLICKER            = 0x98

    , EP_MON_ID_GDL_IRMS               = 0xA0
    , EP_MON_ID_GDL_IAVE               = 0xA1
    , EP_MON_ID_GDL_IMAX               = 0xA2
    , EP_MON_ID_GDL_IMIN               = 0xA3
    , EP_MON_ID_GDL_ITHD               = 0xA4
    , EP_MON_ID_GDL_IHARM              = 0xA5

    , EP_MON_ID_GDL_DEMAND_WATT        = 0xB0
    , EP_MON_ID_GDL_DEMAND_VA          = 0xB1
    , EP_MON_ID_GDL_DEMAND_VAR         = 0xB2
    , EP_MON_ID_GDL_DEMAND_IAVE        = 0xB3
    , EP_MON_ID_GDL_DEMAND_IMAX        = 0xB4
    , EP_MON_ID_GDL_TDD                = 0xB5
    , EP_MON_ID_GDL_DEMAND_WATTMAX     = 0xB6
    , EP_MON_ID_GDL_DEMAND_VAMAX       = 0xB7
    , EP_MON_ID_GDL_DEMAND_VARMAX      = 0xB8

    , EP_MON_ID_GDL_EXPORT_KWH         = 0xC0
    , EP_MON_ID_GDL_IMPORT_KWH         = 0xC1
    , EP_MON_ID_GDL_LEADING_KVARH      = 0xC2
    , EP_MON_ID_GDL_LAGGING_KVARH      = 0xC3
    , EP_MON_ID_GDL_KVAH               = 0xC4

    , EP_MON_ID_GDL_EXPORT_KWH_32      = 0xC8
    , EP_MON_ID_GDL_IMPORT_KWH_32      = 0xC9
    , EP_MON_ID_GDL_LEADING_KVARH_32   = 0xCA
    , EP_MON_ID_GDL_LAGGING_KVARH_32   = 0xCB
    , EP_MON_ID_GDL_KVAH_32            = 0xCC

    , EP_MON_ID_GDL_V_FUND             = 0xD0
    , EP_MON_ID_GDL_V_3RD_HARM         = 0xD1
    , EP_MON_ID_GDL_V_5TH_HARM         = 0xD2
    , EP_MON_ID_GDL_V_7TH_HARM         = 0xD3
    , EP_MON_ID_GDL_V_9TH_HARM         = 0xD4
    , EP_MON_ID_GDL_V_11TH_HARM        = 0xD5
    , EP_MON_ID_GDL_V_13TH_HARM        = 0xD6
    , EP_MON_ID_GDL_V_15TH_HARM        = 0xD7
    , EP_MON_ID_GDL_V_17TH_HARM        = 0xD8
    , EP_MON_ID_GDL_V_19TH_HARM        = 0xD9
    , EP_MON_ID_GDL_V_21TH_HARM        = 0xDA

    , EP_MON_ID_GDL_I_FUND             = 0xE0
    , EP_MON_ID_GDL_I_3RD_HARM         = 0xE1
    , EP_MON_ID_GDL_I_5TH_HARM         = 0xE2
    , EP_MON_ID_GDL_I_7TH_HARM         = 0xE3
    , EP_MON_ID_GDL_I_9TH_HARM         = 0xE4
    , EP_MON_ID_GDL_I_11TH_HARM        = 0xE5
    , EP_MON_ID_GDL_I_13TH_HARM        = 0xE6
    , EP_MON_ID_GDL_I_15TH_HARM        = 0xE7
    , EP_MON_ID_GDL_I_17TH_HARM        = 0xE8
    , EP_MON_ID_GDL_I_19TH_HARM        = 0xE9
    , EP_MON_ID_GDL_I_21TH_HARM        = 0xEA

} ep_monitor_id_et;

typedef enum
{
    EP_STATUS_TAG_PWR_ON_TIME           = 0x70

    , EP_STATUS_TAG_GENERAL_STATUS      = 0x71
    , EP_STATUS_TAG_STATUS_BYTE         = 0x72

    , EP_STATUS_TAG_COMMS_SW_VER        = 0x73
    , EP_STATUS_TAG_ENMTR_FW_VER        = 0x74
    , EP_STATUS_TAG_PART_NUMBER_HW_VER  = 0x75

    , EP_STATUS_TAG_RESERVED_0X76       = 0x76

    , EP_STATUS_TAG_FAN_DUTY            = 0x77
    , EP_STATUS_TAG_FAN_TACHO           = 0x78
    , EP_STATUS_TAG_LED_STATE           = 0x79

    , EP_STATUS_TAG_LOCATION            = 0x7A

    , EP_STATUS_TAG_UPDATE_INFO         = 0x7B
    , EP_STATUS_TAG_CONF_FILE_PROCESSED = 0x7C
    , EP_STATUS_TAG_SCRIPT_RUN_RES      = 0x7D
    , EP_STATUS_TAG_OPER_CMD_PROCESSED  = 0x7E

    , EP_STATUS_TAG_RESERVED_0X7F       = 0x7F

    , EP_STATUS_TAG_MOBILE_BRDBND_STAT  = 0x80
    , EP_STATUS_TAG_SIM_STAT            = 0x81
    , EP_STATUS_TAG_TRANSFORMER_STAT    = 0x82
    , EP_STATUS_TAG_LORA_MESH_STAT      = 0x83
    , EP_STATUS_TAG_VRDT_SER_NUM_STAT   = 0x84
    , EP_STATUS_TAG_ENERGY_BUCKET       = 0x85

    , EP_STATUS_TAG_MIN                 = EP_STATUS_TAG_PWR_ON_TIME
    , EP_STATUS_TAG_MAX                 = EP_STATUS_TAG_ENERGY_BUCKET
} ep_tag_id_et;

typedef enum
{
    /* CVR event types */
    /* General events */
    EP_EVT_TYP_CVR_GEN_POWERED_ON       = 0x21
    /* Input events */
    , EP_EVT_TYP_CVR_IN_UV_WARN         = 0x61
    , EP_EVT_TYP_CVR_IN_UV_FAULT        = 0x62
    , EP_EVT_TYP_CVR_IN_OV_WARN         = 0x63
    , EP_EVT_TYP_CVR_IN_OV_FAULT        = 0x64
    /* Inverter events */
    , EP_EVT_TYP_CVR_INV_OC_WARN        = 0x81
    , EP_EVT_TYP_CVR_INV_OC_FAULT       = 0x82
    , EP_EVT_TYP_CVR_INV_OP_WARN        = 0x83
    , EP_EVT_TYP_CVR_INV_OP_FAULT       = 0x84
    /* Output events */
    , EP_EVT_TYP_CVR_OUT_UV_WARN        = 0xA1
    , EP_EVT_TYP_CVR_OUT_UV_FAULT       = 0xA2
    , EP_EVT_TYP_CVR_OUT_OV_WARN        = 0xA3
    , EP_EVT_TYP_CVR_OUT_OV_FAULT       = 0xA4
    , EP_EVT_TYP_CVR_OUT_SC_FAULT       = 0xA6
    , EP_EVT_TYP_CVR_OUT_OC_SLOW_WARN   = 0xA7
    , EP_EVT_TYP_CVR_OUT_OC_SLOW_FAULT  = 0xA8
    , EP_EVT_TYP_CVR_OUT_OC_FAST_WARN   = 0xA9
    , EP_EVT_TYP_CVR_OUT_OC_FAST_FAULT  = 0xAA
    , EP_EVT_TYP_CVR_OUT_OP_WARN        = 0xAB
    , EP_EVT_TYP_CVR_OUT_OP_FAULT       = 0xAC
    , EP_EVT_TYP_CVR_OUT_PKXPK_FAULT    = 0xAE
    , EP_EVT_TYP_CVR_OUT_NO_COMMS_FAULT = 0xB0
    /* Common events */
    , EP_EVT_TYP_CVR_COM_AUX_UV_WARN    = 0xC1
    , EP_EVT_TYP_CVR_COM_AUX_UV_FAULT   = 0xC2
    , EP_EVT_TYP_CVR_COM_AUX_OV_WARN    = 0xC3
    , EP_EVT_TYP_CVR_COM_AUX_OV_FAULT   = 0xC4
    , EP_EVT_TYP_CVR_COM_UT1_WARN       = 0xC5
    , EP_EVT_TYP_CVR_COM_UT1_FAULT      = 0xC6
    , EP_EVT_TYP_CVR_COM_OT1_WARN       = 0xC7
    , EP_EVT_TYP_CVR_COM_OT1_FAULT      = 0xC8
    , EP_EVT_TYP_CVR_COM_OT2_WARN       = 0xC9
    , EP_EVT_TYP_CVR_COM_OT2_FAULT      = 0xCA
    , EP_EVT_TYP_CVR_COM_OT3_WARN       = 0xCB
    , EP_EVT_TYP_CVR_COM_OT3_FAULT      = 0xCC
    , EP_EVT_TYP_CVR_COM_IN_MEAS_FAULT  = 0xCE
    , EP_EVT_TYP_CVR_COM_FAN_FAIL_FAULT = 0xD0
    , EP_EVT_TYP_CVR_COM_MEM_FAIL_FAULT = 0xD2

    /* SNR event types */
    /* General Events */
    , EP_EVT_TYP_SNR_POWERED_ON         = 0x21
    , EP_EVT_TYP_SNR_EMU_FAULT          = 0x22
    , EP_EVT_TYP_SNR_UV_WARN            = 0x61
    , EP_EVT_TYP_SNR_UV_FAULT           = 0x62
    , EP_EVT_TYP_SNR_OV_WARN            = 0x63
    , EP_EVT_TYP_SNR_OV_FAULT           = 0x64
    , EP_EVT_TYP_SNR_VSAG_FAULT         = 0x65
    , EP_EVT_TYP_SNR_VSWELL_FAULT       = 0x66
    , EP_EVT_TYP_SNR_VFLICKER_FAULT     = 0x67
    /* Current related events */
    , EP_EVT_TYP_SNR_NO_CURRENT_FAULT   = 0x81
    , EP_EVT_TYP_SNR_OVER_CURRENT_WARN  = 0x82
    , EP_EVT_TYP_SNR_OVER_CURRENT_FAULT = 0x83
    , EP_EVT_TYP_SNR_REVERSE_CURRENT    = 0x84
    , EP_EVT_TYP_SNR_DEVICE_TAMPER      = 0x85
    , EP_EVT_TYP_SNR_PWR_GOOD           = 0x86
    /* Power related events */
    , EP_EVT_TYP_SNR_UPF_WARN           = 0xA1
    , EP_EVT_TYP_SNR_UPF_FAULT          = 0xA2
    , EP_EVT_TYP_SNR_POVERLOAD_WARN     = 0xA3
    , EP_EVT_TYP_SNR_POVERLOAD_FAULT    = 0xA4
    , EP_EVT_TYP_SNR_POUTAGE_WARN       = 0xA5
    , EP_EVT_TYP_SNR_POUTAGE_FAULT      = 0xA6
    /* Common events */
    , EP_EVT_TYP_SNR_UFREQ_WARN         = 0xC1
    , EP_EVT_TYP_SNR_UFREQ_FAULT        = 0xC2
    , EP_EVT_TYP_SNR_OFREQ_WARN         = 0xC3
    , EP_EVT_TYP_SNR_OFREQ_FAULT        = 0xC4
    , EP_EVT_TYP_SNR_UTEMP1_WARN        = 0xC5
    , EP_EVT_TYP_SNR_UTEMP1_FAULT       = 0xC6
    , EP_EVT_TYP_SNR_OTEMP1_WARN        = 0xC7
    , EP_EVT_TYP_SNR_OTEMP1_FAULT       = 0xC8
    , EP_EVT_TYP_SNR_UTEMP2_WARN        = 0xC9
    , EP_EVT_TYP_SNR_UTEMP2_FAULT       = 0xCA
    , EP_EVT_TYP_SNR_OTEMP2_WARN        = 0xCB
    , EP_EVT_TYP_SNR_OTEMP2_FAULT       = 0xCC
    , EP_EVT_TYP_SNR_UTEMP3_WARN        = 0xCD
    , EP_EVT_TYP_SNR_UTEMP3_FAULT       = 0xCE
    , EP_EVT_TYP_SNR_OTEMP3_WARN        = 0xCF
    , EP_EVT_TYP_SNR_OTEMP3_FAULT       = 0xD0
    , EP_EVT_TYP_SNR_UTEMP4_WARN        = 0xD1
    , EP_EVT_TYP_SNR_UTEMP4_FAULT       = 0xD2
    , EP_EVT_TYP_SNR_OTEMP4_WARN        = 0xD3
    , EP_EVT_TYP_SNR_OTEMP4_FAULT       = 0xD4

    /* SNR2 event types */
    /* General Events */
    , EP_EVT_TYP_SNR2_POWERED_ON            = 0x21
    , EP_EVT_TYP_SNR2_EMU_FAULT             = 0x22
    , EP_EVT_TYP_SNR2_COMMS_LOST_ALERT      = 0x23
    , EP_EVT_TYP_SNR2_ERROR_PACKET          = 0x24
    , EP_EVT_TYP_SNR2_ERROR_EMMC            = 0x25
    , EP_EVT_TYP_SNR2_DAILY_RESET           = 0x26
    , EP_EVT_TYP_SNR2_UPDATE_EVENT          = 0x27
    /* Voltage related events */
    , EP_EVT_TYP_SNR2_UV_WARN               = 0x61
    , EP_EVT_TYP_SNR2_UV_FAULT              = 0x62
    , EP_EVT_TYP_SNR2_OV_WARN               = 0x63
    , EP_EVT_TYP_SNR2_OV_FAULT              = 0x64
    , EP_EVT_TYP_SNR2_VSAG_FAULT            = 0x65
    , EP_EVT_TYP_SNR2_VSWELL_FAULT          = 0x66
    , EP_EVT_TYP_SNR2_VFLICKER_FAULT        = 0x67
    , EP_EVT_TYP_SNR2_NO_VOLTAGE_FAULT      = 0x68
    , EP_EVT_TYP_SNR2_VOLTAGE_IMBALACE      = 0x69
    , EP_EVT_TYP_SNR2_MAX_VTHD              = 0x6A
    , EP_EVT_TYP_SNR2_DELTA_EARTH_FAULT     = 0x6B
    , EP_EVT_TYP_SNR2_UPPER_RVC_FAULT       = 0x6C
    , EP_EVT_TYP_SNR2_LOWER_RVC_FAULT       = 0x6D
    , EP_EVT_TYP_SNR2_VOLTAGE_RECOVERY      = 0x6E
    /* Current related events */
    , EP_EVT_TYP_SNR2_NO_CURRENT_FAULT      = 0x81
    , EP_EVT_TYP_SNR2_OVER_CURRENT_WARN     = 0x82
    , EP_EVT_TYP_SNR2_OVER_CURRENT_FAULT    = 0x83
    , EP_EVT_TYP_SNR2_REVERSE_CURRENT       = 0x84
    , EP_EVT_TYP_SNR2_DEVICE_TAMPER         = 0x85
    , EP_EVT_TYP_SNR2_PWR_GOOD              = 0x86
    , EP_EVT_TYP_SNR2_FAULT_CURRENT_RECORD  = 0x87
    , EP_EVT_TYP_SNR2_PEAK_DEMAND_ALERT     = 0x88
    , EP_EVT_TYP_SNR2_CURRENT_IMBALACE      = 0x89
    , EP_EVT_TYP_SNR2_MAX_ITHD              = 0x8A
    , EP_EVT_TYP_SNR2_NEUTRAL_OVER_CURRENT  = 0x8B


    /* Power related events */
    , EP_EVT_TYP_SNR2_UPF_WARN              = 0xA1
    , EP_EVT_TYP_SNR2_UPF_FAULT             = 0xA2
    , EP_EVT_TYP_SNR2_POVERLOAD_WARN        = 0xA3
    , EP_EVT_TYP_SNR2_POVERLOAD_FAULT       = 0xA4
    , EP_EVT_TYP_SNR2_POUTAGE_WARN          = 0xA5
    , EP_EVT_TYP_SNR2_POUTAGE_FAULT         = 0xA6
    , EP_EVT_TYP_SNR2_POUTAGE_ALERT         = 0xA7
    , EP_EVT_TYP_SNR2_TRUE_POUTAGE          = 0xA8

    /* Common events */
    , EP_EVT_TYP_SNR2_UFREQ_WARN            = 0xC1
    , EP_EVT_TYP_SNR2_UFREQ_FAULT           = 0xC2
    , EP_EVT_TYP_SNR2_OFREQ_WARN            = 0xC3
    , EP_EVT_TYP_SNR2_OFREQ_FAULT           = 0xC4
    , EP_EVT_TYP_SNR2_UTEMP1_WARN           = 0xC5
    , EP_EVT_TYP_SNR2_UTEMP1_FAULT          = 0xC6
    , EP_EVT_TYP_SNR2_OTEMP1_WARN           = 0xC7
    , EP_EVT_TYP_SNR2_OTEMP1_FAULT          = 0xC8
    , EP_EVT_TYP_SNR2_UTEMP2_WARN           = 0xC9
    , EP_EVT_TYP_SNR2_UTEMP2_FAULT          = 0xCA
    , EP_EVT_TYP_SNR2_OTEMP2_WARN           = 0xCB
    , EP_EVT_TYP_SNR2_OTEMP2_FAULT          = 0xCC
    , EP_EVT_TYP_SNR2_UTEMP3_WARN           = 0xCD
    , EP_EVT_TYP_SNR2_UTEMP3_FAULT          = 0xCE
    , EP_EVT_TYP_SNR2_OTEMP3_WARN           = 0xCF
    , EP_EVT_TYP_SNR2_OTEMP3_FAULT          = 0xD0
    , EP_EVT_TYP_SNR2_UTEMP4_WARN           = 0xD1
    , EP_EVT_TYP_SNR2_UTEMP4_FAULT          = 0xD2
    , EP_EVT_TYP_SNR2_OTEMP4_WARN           = 0xD3
    , EP_EVT_TYP_SNR2_OTEMP4_FAULT          = 0xD4

        /* PPEM event types */
    , EP_EVT_TYP_PPEM_DEV_STATUS_FLAGS      = 0x20
    , EP_EVT_TYP_PPEM_OFFLINE               = 0x23
    , EP_EVT_TYP_PPEM_ERROR_PACKET          = 0x24
    , EP_EVT_TYP_PPEM_UV_WARN               = 0x61
    , EP_EVT_TYP_PPEM_UV_FAULT              = 0x62
    , EP_EVT_TYP_PPEM_OV_WARN               = 0x63
    , EP_EVT_TYP_PPEM_OV_FAULT              = 0x64
    , EP_EVT_TYP_PPEM_OVTHD_WARN            = 0x65
    , EP_EVT_TYP_PPEM_OVTHD_FAULT           = 0x66
    /* Current related events */
    , EP_EVT_TYP_PPEM_OC_1_WARN             = 0x81
    , EP_EVT_TYP_PPEM_OC_2_WARN             = 0x82
    , EP_EVT_TYP_PPEM_OC_3_WARN             = 0x83
    , EP_EVT_TYP_PPEM_OC_4_WARN             = 0x84
    , EP_EVT_TYP_PPEM_OC_5_WARN             = 0x85
    , EP_EVT_TYP_PPEM_OC_6_WARN             = 0x86
    , EP_EVT_TYP_PPEM_UFREQ_WARN            = 0xC1
    , EP_EVT_TYP_PPEM_UFREQ_FAULT           = 0xC2
    , EP_EVT_TYP_PPEM_OFREQ_WARN            = 0xC3
    , EP_EVT_TYP_PPEM_OFREQ_FAULT           = 0xC4
    , EP_EVT_TYP_PPEM_ORWIRE_WARN           = 0xC5
    , EP_EVT_TYP_PPEM_ORWIRE_FAULT          = 0xC6

    /* GDL event types */
    /* General Events */
    , EP_EVT_TYP_GDL_POWERED_ON             = 0x21
    , EP_EVT_TYP_GDL_EMU_FAULT              = 0x22
    , EP_EVT_TYP_GDL_OFFLINE                = 0x23
    , EP_EVT_TYP_GDL_ERROR_PACKET           = 0x24
    , EP_EVT_TYP_GDL_EMMC_ERROR             = 0x25
    , EP_EVT_TYP_GDL_SYSTEM_RESET           = 0x26
    , EP_EVT_TYP_GDL_TASK_SKIP_ERR          = 0x28
    /* Voltage related events */
    , EP_EVT_TYP_GDL_UV_WARN                = 0x61
    , EP_EVT_TYP_GDL_OV_WARN                = 0x63
    , EP_EVT_TYP_GDL_VSAG_FAULT             = 0x65
    , EP_EVT_TYP_GDL_VSWELL_FAULT           = 0x66
    , EP_EVT_TYP_GDL_VFLICKER_FAULT         = 0x67
    , EP_EVT_TYP_GDL_NO_VOLTAGE_FAULT       = 0x68
    , EP_EVT_TYP_GDL_VOLTAGE_IMBALACE       = 0x69
    , EP_EVT_TYP_GDL_MAX_VTHD_WARN          = 0x6A
    , EP_EVT_TYP_GDL_DELTA_EARTH_FAULT      = 0x6B
    , EP_EVT_TYP_GDL_UPPER_RVC_FAULT        = 0x6C
    , EP_EVT_TYP_GDL_LOWER_RVC_FAULT        = 0x6D
    , EP_EVT_TYP_GDL_VOLTAGE_RECOVERY       = 0x6E
    /* Current related events */
    , EP_EVT_TYP_GDL_NO_CURRENT_FAULT       = 0x81
    , EP_EVT_TYP_GDL_OVER_CURRENT_WARN      = 0x82
    , EP_EVT_TYP_GDL_REVERSE_POWER          = 0x84
    , EP_EVT_TYP_GDL_DEVICE_TAMPER          = 0x85
    , EP_EVT_TYP_GDL_POWER_GOOD             = 0x86
    , EP_EVT_TYP_GDL_FAULT_CURRENT_RECORD   = 0x87
    , EP_EVT_TYP_GDL_PEAK_DEMAND_ALERT      = 0x88
    , EP_EVT_TYP_GDL_CURRENT_IMBALACE       = 0x89
    , EP_EVT_TYP_GDL_MAX_ITHD_WARN          = 0x8A
    , EP_EVT_TYP_GDL_NEUTRAL_OVER_CURRENT   = 0x8B
    /* Power related events */
    , EP_EVT_TYP_GDL_UPF_WARN               = 0xA1
    , EP_EVT_TYP_GDL_POVERLOAD_WARN         = 0xA3
    , EP_EVT_TYP_GDL_POUTAGE_FAULT          = 0xA6
    , EP_EVT_TYP_GDL_POUTAGE_ALERT          = 0xA7
    , EP_EVT_TYP_GDL_TRUE_POUTAGE           = 0xA8
    /* Common events */
    , EP_EVT_TYP_GDL_UFREQ_WARN             = 0xC1
    , EP_EVT_TYP_GDL_OFREQ_WARN             = 0xC3
} ep_event_type_et;


#endif // __EDGE_PAYLOAD_DEFS_H__
