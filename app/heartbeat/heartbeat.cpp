
#include "global_defs.h"
#include "heartbeat.h"
namespace heartbeat
{

#if 0
    /*
 * Local Constants
 */
typedef enum
{
    HB_LED_BLINKING_RED
    , HB_LED_SOLID_RED
    , HB_LED_2S_1S_RED
    , EM_LED_ALT_GREEN_RED
    , HB_LED_2S_1S_GREEN
    , HB_LED_SOLID_GREEN
	, HB_LED_OFF
} heartbeat_led_mode_et;

typedef enum
{
	HB_LED_DBG_OFF
	, HB_LED_DBG_ON
    , HB_LED_DBG_ONCE_IN_5S
    , HB_LED_DBG_ONCE_IN_10S
    , HB_LED_DBG_BLINKING
    , HB_LED_STATUS_MAX
} heartbeat_dbg_led_mode_et;

typedef enum
{
    HEARTBEAT_BLINK_STATE_OFF
    , HEARTBEAT_BLINK_STATE_ON
} heartbeat_blink_state_et;

typedef enum
{
    HEARTBEAT_LED_GREEN
    , HEARTBEAT_LED_RED
    , NUM_OF_HEARTBEAT_LED
} heartbeat_led_et;

static const uint16_t				UI16_SLOW_LOOP_DELAY_VAL							= (uint16_t)K_SLOW_LOOP_DELAY_VAL;
static const uint16_t				UI16_BLINK_ON_TIME									= (uint16_t) K_BLINK_ON_TIME_VALUE;
static const uint16_t				UI16_BLINK_OFF_TIME									= (uint16_t) K_BLINK_OFF_TIME_VALUE;

static const uint16_t               UI16_HEARTBEAT_FAST_BLINK_ON_TIME_CTR_VAL           = (uint16_t)K_HEARTBEAT_FAST_BLINK_ON_TIME_CTR_VAL;
static const uint16_t               UI16_HEARTBEAT_FAST_BLINK_OFF_TIME_CTR_VAL          = (uint16_t)K_HEARTBEAT_FAST_BLINK_OFF_TIME_CTR_VAL;

static const uint16_t               UI16_HEARTBEAT_PULSE_2S_BLINK_TIME_CTR_VAL          = (uint16_t)K_HEARTBEAT_PULSE_2S_BLINK_TIME_CTR_VAL;
static const uint16_t               UI16_HEARTBEAT_PULSE_1S_BLINK_TIME_CTR_VAL          = (uint16_t)K_HEARTBEAT_PULSE_1S_BLINK_TIME_CTR_VAL;

static const uint8_t                UI8_HEARTBEAT_BLINK_10S_TIME_CTR_VAL				= (uint8_t)K_HEARTBEAT_BLINK_10S_TIME_CTR;
static const uint8_t                UI8_HEARTBEAT_BLINK_5S_TIME_CTR_VAL					= (uint8_t)K_HEARTBEAT_BLINK_5S_TIME_CTR;

static const uint8_t                UI8_LED_STARTUP_VALID_DELAY                         = (uint8_t)K_LED_STARTUP_DELAY_VAL;
static const uint16_t				UI16_COMMS_LOST_VALIDATION_VAL						= (uint16_t)K_COMMS_LOST_VALIDATION;

/*
 * Local Variables
 */
#define LED_IDX      23
#define LED_IDX_MASK (1 << LED_IDX)

typedef struct
{
    heartbeat_blink_state_et    e_state;
    uint16_t                    ui16_on_delay;
    uint16_t                    ui16_off_delay;
    uint16_t                    ui16_state_ctr;
    bool                      b_led_state;
} heartbeat_blink_ctx_st;

typedef enum
{
	BLINK_STATE_OFF
	, BLINK_STATE_ON
} blink_state_et;

typedef enum
{
	HB_LED_STATE_OFF
	, HB_LED_STATE_ON
	, HB_LED_STATE_BLINK_OFF
	, HB_LED_STATE_BLINK_ON
} heartbeat_led_blink_state_et;

static rtc_time_st                  s_proc_time;
static uint32_t                     ui32_uptime_secs;
static uint32_t                     ui32_last_sync;
static int32_t                      si32_ref_epoch;

static heartbeat_led_mode_et        e_led_mode;
static heartbeat_blink_ctx_st       s_fast_blink_ctx;
static heartbeat_blink_ctx_st       s_pulse_2s_1s_blink_ctx;
static heartbeat_blink_ctx_st       s_pulse_1s_blink_ctx;
static heartbeat_blink_ctx_st       s_pulse_inv_1s_blink_ctx;
static heartbeat_dbg_led_mode_et	e_dbg_led_mode;
static heartbeat_led_blink_state_et e_led_4g_debug_blink_state;

static blink_state_et				e_blink_state;
static uint16_t						ui16_blink_counter;
static uint16_t						ui16_slow_loop_ctr;
static uint16_t						ui16_comms_lost_cntr;
static uint8_t                      ui8_dbg_blink_ctr;
static uint8_t                      ui8_dbg_blink_delay;
static uint8_t                      ui8_start_led_delay_ctr;

static bool                       b_mcu_reset;
static bool                       b_comms_outage_reset;
static int32_t                      si32_connected_epoch;
static int32_t                      si32_curr_epoch;

/*
 * Private Function Prototypes
 */
static void blinkCycle(void);

static void setLedState(heartbeat_led_et e_led, bool b_state);

static void uptimeProcess(void);
static void blinkProcess(heartbeat_blink_ctx_st *ps_blink_ctx);
static void updateLedModes(void);
static void updateLedStates(void);
static void updateDbgLedStates(void);

static void handleMcuResetRequest(void);
static void handleCommsLostEvent(void);

#endif 0

/*
 * Public Functions
 */
bool init()
{
   // LOGD("core %u: %s", xPortGetCoreID(), __PRETTY_FUNCTION__);
    LOGD("task_hb_init\n");
    return true;
}

void cycle()
{
    LOGD("hb cyc: ");
   // LOGD("%s %u", __PRETTY_FUNCTION__, millis());
}

} // namespace heartbeat
