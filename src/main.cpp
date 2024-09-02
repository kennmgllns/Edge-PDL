#include "global_defs.h"

#include "heartbeat.h"
#include "cloud_comms.h"
#include "data_logging.h"
#include "enmtr_manager.h"
#include "input_monitoring.h"
#include "lora_mesh.h"
#include "modem_manager.h"
#include "wifi_manager.h"
#include <stdio.h>


#define DECLARE_TASK(task, _setup, _loop, _delay)               \
            static esp_task_wdt_user_handle_t task##_wdt_hdl;   \
            static void task##Task(void *arg) {                 \
                _setup(); for (;;) {                            \
                    _loop(); delayms(_delay);                   \
                    esp_task_wdt_reset_user(task##_wdt_hdl);    \
                }}

DECLARE_TASK(Heartbeat,       heartbeat::init,          heartbeat::cycle,          1000);
DECLARE_TASK(EnmtrManager,    enmtr::manager::init,     enmtr::manager::cycle,      100);
DECLARE_TASK(InputMonitoring, input::monitoring::init,  input::monitoring::cycle,   100);
DECLARE_TASK(WifiManager,     wifi::manager::init,      wifi::manager::cycle,       100);
DECLARE_TASK(ModemManager,    modem::manager::init,     modem::manager::cycle,       10);
DECLARE_TASK(LoraMesh,        lora::mesh::init,         lora::mesh::cycle,          100);
DECLARE_TASK(CloudComms,      cloud::comms::init,       cloud::comms::cycle,         10);
DECLARE_TASK(DataLogging,     data::logging::init,      data::logging::cycle,       100);

    
extern "C" void app_main(void)
{
    global_init();
  #define RUN_TASK(task, stack, priority, core)                                        \
                    ESP_ERROR_CHECK(esp_task_wdt_add_user(#task, &task##_wdt_hdl)); \
                    assert(pdTRUE == xTaskCreatePinnedToCore(task##Task, #task, (stack), NULL, (priority), NULL, (core)))

    // RUN_TASK(Heartbeat,         2*1024,  2, 1);
    // RUN_TASK(EnmtrManager,      4*1024,  7, 0);
    // RUN_TASK(InputMonitoring,   2*1024,  5, 1);
    RUN_TASK(WifiManager,       8*1024,  4, 0);
    // RUN_TASK(ModemManager,      8*1024,  4, 0);
    // RUN_TASK(LoraMesh,          2*1024,  4, 0);
    // RUN_TASK(CloudComms,        8*1024,  3, 1);
    // RUN_TASK(DataLogging,       2*1024,  3, 1);
}
