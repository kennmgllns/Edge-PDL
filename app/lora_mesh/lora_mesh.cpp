
#include "global_defs.h"
#include "lora_mesh.h"
#include <esp_system.h>
#include <stdlib.h>
#include <stdio.h>
// #include "data_types.h"
#include "global_defs.h"
#include "crc/crc16.h"

// #include "core_gpio_cfg.h"
// #include "core_gpio.h"

// #include "core_spi_master_cfg.h"
// #include "core_spi_master.h"

#include "system_flags/system_flags.h"

// #include "dev_id_gen_cfg.h"
#include "device_id/device_id.h"

#include "sx126x/sx126x.h"
#include "sx126x/sx126x-board.h"

#include "lora_mesh.h"
#include "lora_mesh_cfg.h"


namespace lora::mesh
{

/** The mesh node ID, created from comms_id */
static uint32_t nodeID = 0;

/** Send buffer for queued messages */
static mesh_data_msg_st as_send_messages[MESH_SEND_QUEUE_SIZE];

/** Queue to handle send requests */
static QueueHandle_t queue_send;

/** Mux used to enter critical code part (access to node list) */
static SemaphoreHandle_t accessNodeList;

/** LoRa RX buffer */
static uint8_t au8_rx_buffer[sizeof(mesh_data_msg_st)];

/** Sync time */
static uint32_t syncTime = MESH_INIT_SYNCTIME;

/** Map message buffer */
static mesh_map_msg_st syncMsg;

/** Counter for CAD retry */
static uint8_t channelFreeRetryNum = 0;

/** LoRa state machine status */
static meshRadioState_t loraState = MESH_IDLE;

/** LoRa callback events */
static RadioEvents_t RadioEvents;


/** Flag if the nodes map has changed */
static bool nodesChanged = false;


// task cycle variables
static uint32_t ms_channel_detect;
static uint32_t notifyTimer;
static uint32_t checkSwitchSyncTime;
static uint32_t txTimeout;
static uint32_t msTxStart;


mesh_diagnostics_st s_mesh_diag;


/*
 * Private Function Prototypes
 */

static void radio_init(void);
static bool radio_check(void);
static void radio_listen(void);
static void radio_cad(void);
static void update_route(void);
static void process_send_queue(void);

static void OnTxDone(void);
static void OnTxTimeout(void);
static void OnRxDone(uint8_t *rxPayload, uint16_t rxSize, int16_t rxRssi, int8_t rxSnr);
static void OnRxTimeout(void);
static void OnRxError(void);
static void OnCadDone(bool cadResult);

/*
 * Public Functions
 */

bool init()
{
    static uint8_t  dev_id_buf[K_DEV_ID_LEN];

    LOGD("%s", __func__);
  //LOGI("sizeof(mesh_data_msg_st) = %ld", sizeof(mesh_data_msg_st));

    //memset(&RadioEvents, 0, sizeof(RadioEvents));
    RadioEvents.TxDone      = OnTxDone;
    RadioEvents.TxTimeout   = OnTxTimeout;
    RadioEvents.RxDone      = OnRxDone;
    RadioEvents.RxTimeout   = OnRxTimeout;
    RadioEvents.RxError     = OnRxError;
    RadioEvents.CadDone     = OnCadDone;
    SX126xSpiInit();
    // Create queue
    queue_send = xQueueCreate(MESH_SEND_QUEUE_SIZE, sizeof(mesh_data_msg_st *));
    if (NULL == queue_send)
    {
        // Error_Handler();
        LOGE("\r\n--Error Handler--\r\n");
        while(1){

        }
    }
    memset(as_send_messages, 0, sizeof(as_send_messages));
    LOGD("Send queue created");

    // Create blocking semaphore for nodes list access
    accessNodeList = xSemaphoreCreateBinary();
    xSemaphoreGive(accessNodeList);

    // Create node ID anf broadcast ID from comms_id
    get_device_id(dev_id_buf);
    nodeID = (dev_id_buf[0] << 24) | (dev_id_buf[1] << 16) | (dev_id_buf[2] << 8) | dev_id_buf[3];
    LOGI("node_id %08lx (broadcast %08lx)", nodeID, meshBroadcastID(nodeID));

    memset(&s_mesh_diag, 0, sizeof(s_mesh_diag));

    loraState           = MESH_IDLE;
    ms_channel_detect   = 0;
    notifyTimer         = millis() + syncTime;
    checkSwitchSyncTime = millis();
    txTimeout           = millis();

    meshRouteInit();
    meshCommsInit();
    
    return true;
}

void cycle()
{
   //LOGD("%s", __func__);
    static enum {
        STATE_INIT = 0,     // initialize and check presense of module
        STATE_PROCESS,      // actual loop/cycle
    } e_state = STATE_INIT;
    static const uint32_t   MS_CHECK_INTERVAL = 1 * 60 * 1000UL;
    static uint32_t         ms_last_check = 0;
    static uint16_t         u16_max_nodes = 0;

    switch (e_state)
    {
    case STATE_INIT:
        if ((0 == ms_last_check) || (millis() - ms_last_check > MS_CHECK_INTERVAL))
        {
            radio_init();
            ms_last_check = millis();
            u16_max_nodes = numOfNodes();
            // (void)setLoraNodeDetectedStatus(u16_max_nodes > 1);
            if (radio_check())
            {
                e_state   = STATE_PROCESS;
                // setMCU2LoRaStatus(TRUE);
                radio_listen();
            }
            else
            {
                // (void)setMCU2LoRaStatus(FALSE);
            }
        }
        break;

    case STATE_PROCESS:
        Radio.IrqProcess();

        update_route();
        process_send_queue();

        meshCommsCycle();

        // check module every minute
        if (millis() - ms_last_check > MS_CHECK_INTERVAL)
        {
            ms_last_check = millis();
            // reinit module if not responding or num nodes is reduced
            if ((false == radio_check()) || (u16_max_nodes > numOfNodes()))
            {
                LOGW("reinitialize module (nodes %u-%u)", u16_max_nodes, numOfNodes());
                e_state = STATE_INIT;
            }

            if (u16_max_nodes < numOfNodes())
            {
                u16_max_nodes = numOfNodes();
                // (void)setLoraNodeDetectedStatus(u16_max_nodes > 1);
            }
        }
        break;

    default:
        e_state = STATE_INIT;
        break;
    }

    delayms(50);
}

uint32_t meshNodeID(void)
{
    return nodeID;
}

/**
 * Add a data package to the queue
 * @param package
 *             dataPckg * to the package data
 * @param msgSize
 *             Size of the data package
 * @return result
 *             TRUE if task could be added to queue
 *             FALSE if queue is full or not initialized
 */
bool meshSendRequest(mesh_data_msg_st *package, uint16_t msgSize)
{
    //LOGD("%s(%u)", __func__, msgSize);
    //dumpbytes(package, msgSize);

    mesh_data_msg_st *ps_msg = NULL;
    uint8_t ui8_idx;

    for (ui8_idx = 0; ui8_idx < MESH_SEND_QUEUE_SIZE; ui8_idx++)
    {
        if ((MESH_MSG_INVALID == as_send_messages[ui8_idx].s_hdr.u8_type) &&
            (0 == as_send_messages[ui8_idx].u16_len))
        {
            // found an empty entry
            ps_msg = &as_send_messages[ui8_idx];
            break;
        }
    }

    if (NULL == ps_msg)
    {
        LOGW("msg queue full");
        s_mesh_diag.u32_tx_error++;
    }
    else
    {
        memcpy(ps_msg, package, msgSize);
        ps_msg->u16_len = msgSize;
        if (pdTRUE == xQueueSend(queue_send, &ps_msg, (TickType_t)1000))
        {
            //LOGD("msg queue %u bytes", msgSize);
            return true;
        }
        else
        {
            memset(ps_msg, 0, sizeof(mesh_data_msg_st));
            LOGW("msg queue failed");
            s_mesh_diag.u32_tx_error++;
        }
    }

    return false;
}


/*
 * Private Functions
 */

static void radio_init(void)
{
    Radio.Init(&RadioEvents);

    Radio.SetChannel(LORA_RF_FREQUENCY);
    Radio.SetTxConfig(MODEM_LORA, LORA_TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                      LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                      LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                      false, 0, 0, LORA_IQ_INVERSION_ON, 3000);
    Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                      LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                      LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                      0, false, 0, 0, LORA_IQ_INVERSION_ON, true);
}

/* check for correct sync word */
static bool radio_check(void)
{
    uint8_t buf[2];
    uint16_t u16_syncword;

    SX126xReadRegisters(REG_LR_SYNCWORD, buf, sizeof(buf));
    u16_syncword  = (uint16_t)buf[0] << 8;
    u16_syncword |= buf[1];

    if ((LORA_MAC_PRIVATE_SYNCWORD == u16_syncword) || (LORA_MAC_PUBLIC_SYNCWORD == u16_syncword))
    {
        LOGD("syncword = 0x%04x", u16_syncword);
        return true;
    }

    LOGE("invalid syncword = 0x%04x", u16_syncword);
    return false;
}

/* [re]start listening */
static void radio_listen(void)
{
    Radio.Standby();
#if 0 // normal listen + sleep cycle for power saving
    Radio.SetRxDutyCycle(LORA_RX_SLEEP_TIMES);
#elif 0 // continuous receive
    Radio.Rx(0);
#else // continuous receive with improved sensitivity
    Radio.RxBoosted(0);
#endif
}

/* [re]start free-channel detect */
static void radio_cad(void)
{
    Radio.Standby();
#if 0
    Radio.SetCadParams(LORA_CAD_08_SYMBOL, LORA_SPREADING_FACTOR + 13, 10, LORA_CAD_ONLY, 0);
#else // see app note AN1200.48
    Radio.SetCadParams(LORA_CAD_02_SYMBOL, 22, 10, LORA_CAD_ONLY, 0);
#endif
    Radio.StartCad();

    ms_channel_detect = 0;
}

static void update_route(void)
{
    if (nodesChanged)
    {
        nodesChanged = false;
        printNodes();
        if (NULL != s_mesh_events.NodesListChanged)
        {
            s_mesh_events.NodesListChanged();
        }

        // setLoraNodeDetectedStatus(numOfNodes() > 1);
    }

    // Time to sync the Mesh ???
    if ((millis() - notifyTimer) >= syncTime)
    {
        if (xSemaphoreTake(accessNodeList, (TickType_t)1000) == pdTRUE)
        {
            //LOGD("Checking mesh map");
            if (!cleanMap())
            {
                syncTime = MESH_INIT_SYNCTIME;
                checkSwitchSyncTime = millis();
                printNodes();
                if (NULL != s_mesh_events.NodesListChanged)
                {
                    s_mesh_events.NodesListChanged();
                }
            }
            LOGD("Sending mesh map");
            memset(&syncMsg, 0, sizeof(syncMsg));
            memcpy(syncMsg.s_hdr.mark, MESH_MSG_HEADER_MARK);
            syncMsg.s_hdr.u32_src = nodeID;
            syncMsg.s_hdr.u8_type = MESH_MSG_NODEMAP;

            uint16_t u16_count = 0; // counter for both nodes and message size
            uint16_t u16_offset = 0; // map offset

            while (0 != (u16_count = nodeMap(syncMsg.au8_nodes, u16_offset)))
            {
                u16_offset += u16_count; // next offset

                u16_count = sizeof(mesh_msg_header_st) + (u16_count * NODE_INFO_SIZE);
                if (!meshSendRequest((mesh_data_msg_st *)&syncMsg, u16_count))
                {
                    LOGE("Cannot send map because send queue is full");
                    s_mesh_diag.u32_route_fail++;
                    break;
                }
            }

            xSemaphoreGive(accessNodeList);
            uint32_t ms_adjust_delay = 0; // random adjust next sync
            ms_adjust_delay = random();
            notifyTimer = millis() - (ms_adjust_delay & 0xFFF) /*- ~4sec*/;
        }
        else
        {
            LOGE("Cannot access map for clean up and syncing");
            s_mesh_diag.u32_route_fail++;
        }

#ifdef MESH_ENABLE_PRINT_DIAGNOSTICS
        LOGW("diagnostics: phy=[%lu, %lu, %lu] mesh=[%lu, %lu, %lu] app=[%lu, %lu, %lu]",
            s_mesh_diag.u32_rx_error, s_mesh_diag.u32_tx_error, s_mesh_diag.u32_cad_busy,
            s_mesh_diag.u32_msg_invalid, s_mesh_diag.u32_msg_crc, s_mesh_diag.u32_route_fail,
            s_mesh_diag.u32_sync_error, s_mesh_diag.u32_ack_error, s_mesh_diag.u32_rsp_error);
#endif

    }

    // Time to relax the syncing ???
    if (((millis() - checkSwitchSyncTime) >= MESH_SWITCH_SYNCTIME) && (syncTime != MESH_DEFAULT_SYNCTIME))
    {
        LOGD("Switching sync time to %u", MESH_DEFAULT_SYNCTIME);
        syncTime = MESH_DEFAULT_SYNCTIME;
        checkSwitchSyncTime = millis();
    }
}

static void process_send_queue(void)
{
    mesh_data_msg_st    *ps_msg = NULL;
    uint8_t             *pu8_buf = NULL;

    // Check if loraState is stuck in MESH_TX
    if ((loraState == MESH_TX) && ((millis() - txTimeout) > 7500))
    {
        // Restart listening
        radio_listen();

        loraState = MESH_IDLE;
        LOGE("loraState stuck in TX for 2 seconds");
        s_mesh_diag.u32_tx_error++;
    }

    if (0 != ms_channel_detect)
    {
        if (millis() > ms_channel_detect)
        {
            LOGD("retry channel activity detect");
            radio_cad();
        }
    }
    else if (loraState == MESH_TX)
    {
        //LOGD("tx busy");
    }
    // Check if we have something in the queue
    else if ((pdTRUE == xQueuePeek(queue_send, &ps_msg, 0)) && (NULL != ps_msg))
    {
        pu8_buf = (uint8_t *)ps_msg;
        /* fill-up crc */
        ps_msg->u16_crc = crc16CalcBlock(pu8_buf, ps_msg->u16_len);
        memcpy(&pu8_buf[ps_msg->u16_len], &ps_msg->u16_crc, sizeof(ps_msg->u16_crc));
        ps_msg->u16_len += sizeof(uint16_t); // crc appended

        radio_cad();
        txTimeout = millis();
        loraState = MESH_TX;
    }
}


/** Callbacks */

static void OnTxDone(void)
{
    LOGD("LoRa send finished (%lums)", millis() - msTxStart);
    loraState = MESH_IDLE;

    // Restart listening
    radio_listen();
}

static void OnTxTimeout(void)
{
    LOGW("LoRa TX timeout");
    s_mesh_diag.u32_tx_error++;

    loraState = MESH_IDLE;

    // Restart listening
    radio_listen();
}

/**
 * Callback after a LoRa package was received
 * @param rxPayload
 *             Pointer to the received data
 * @param rxSize
 *             Length of the received package
 * @param rxRssi
 *             Signal strength while the package was received
 * @param rxSnr
 *             Signal to noise ratio while the package was received
 */
static void OnRxDone(uint8_t *rxPayload, uint16_t rxSize, int16_t rxRssi, int8_t rxSnr)
{
    //uint16_t tempSize = rxSize;

    mesh_msg_header_st  *thisHdr        = (mesh_msg_header_st *)au8_rx_buffer;
    mesh_map_msg_st     *thisMapMsg     = (mesh_map_msg_st *)au8_rx_buffer;
    mesh_data_msg_st    *thisDataMsg    = (mesh_data_msg_st *)au8_rx_buffer;
    uint16_t             ui16_calc_crc; // expected crc

    if (rxSize > sizeof(au8_rx_buffer))
    {
        LOGE("not enough rx buffer size");
        s_mesh_diag.u32_rx_error++;
        rxSize = sizeof(au8_rx_buffer);  // should not go here
    }
    memset(au8_rx_buffer, 0, sizeof(au8_rx_buffer));
    memcpy(au8_rx_buffer, rxPayload, rxSize);

    loraState = MESH_IDLE;

    LOGD("LoRa Packet received size:%d, rssi:%d, snr:%d", rxSize, rxRssi, rxSnr);

    // Restart listening
    radio_listen();

    // Check the received data
    if (rxSize < offsetof(mesh_data_msg_st, au8_data) + sizeof(uint16_t))
    {
        // must have at least header and crc
        LOGD("Incomplete package (%u)", rxSize);
        return;
    }
    else if (0 != memcmp(au8_rx_buffer, MESH_MSG_HEADER_MARK))
    {
        LOGD("Invalid package (%u)", rxSize);
        //dumpbytes(rxPayload, rxSize);
    }
    else
    {
        // mesh data received within expected size
        thisDataMsg->u16_len = rxSize - sizeof(uint16_t); // exclude crc16 position
        ui16_calc_crc  = crc16CalcBlock(au8_rx_buffer, thisDataMsg->u16_len);
        memcpy(&thisDataMsg->u16_crc, &au8_rx_buffer[thisDataMsg->u16_len], sizeof(thisDataMsg->u16_crc));

        if (thisDataMsg->u16_crc != ui16_calc_crc)
        {
            LOGW("msg %d-%08lx crc fail %04x!=%04x", thisHdr->u8_type, thisHdr->u32_src, thisDataMsg->u16_crc, ui16_calc_crc);
            s_mesh_diag.u32_msg_crc++;
        }
        else if (thisHdr->u8_type == MESH_MSG_NODEMAP)
        {
            // Mapping received
            uint8_t subsSize = thisDataMsg->u16_len - sizeof(mesh_msg_header_st);
            uint8_t numSubs = subsSize / NODE_INFO_SIZE;

            LOGD("%u-node map msg (%08lx)", numSubs, thisHdr->u32_src);

            if (xSemaphoreTake(accessNodeList, (TickType_t)1000) == pdTRUE)
            {
                // Remove nodes that use sending node as hop
                clearSubs(thisHdr->u32_src);

                //LOGD("From %08lX Dest %08lX", thisHdr->u32_src, thisHdr->u32_dst);

                if (subsSize != 0)
                {
                    LOGD("Msg size %d #subs %d", thisDataMsg->u16_len, numSubs);

                    for (int idx = 0; idx < numSubs; idx++)
                    {
                        uint32_t subId = (uint32_t)thisMapMsg->au8_nodes[idx][0];
                        subId |= (uint32_t)thisMapMsg->au8_nodes[idx][1] << 8;
                        subId |= (uint32_t)thisMapMsg->au8_nodes[idx][2] << 16;
                        subId |= (uint32_t)thisMapMsg->au8_nodes[idx][3] << 24;
                        if (subId != nodeID)
                        {
                            LOGD("Subs %08lX", subId);
                            uint8_t u5_num_hops = MESH_NODE_HOPS(thisMapMsg->au8_nodes[idx][4]);
                            uint8_t u3_flags    = MESH_NODE_FLAGS(thisMapMsg->au8_nodes[idx][4]);
                            if (subId == thisHdr->u32_src)
                            {
                                nodesChanged = addNode(subId, 0, 0, u3_flags, rxSnr, rxRssi);
                            }
                            else
                            {
                                nodesChanged = addNode(subId, thisHdr->u32_src, u5_num_hops + 1, u3_flags, INT8_MIN, INT16_MIN);
                            }
                            // for debug only: force log print of node mapping
                            nodesChanged = true;
                        }
                    }
                }

                xSemaphoreGive(accessNodeList);
            }
            else
            {
                LOGE("Could not access map to add node");
                s_mesh_diag.u32_route_fail++;
            }
        }
        else if (thisHdr->u8_type == MESH_MSG_DIRECT)
        {
            if (thisHdr->u32_dst == nodeID)
            {
                // Message is for us, call user callback to handle the data
                //LOGD("Got data message: %s", thisDataMsg->au8_data);
                if (NULL != s_mesh_events.DataAvailable)
                {
                    s_mesh_events.DataAvailable(thisDataMsg->u32_orig, thisDataMsg->au8_data,
                                                thisDataMsg->u16_len - offsetof(mesh_data_msg_st, au8_data), rxRssi, rxSnr);
                }
            }
            else
            {
                // Message is not for us
            }
        }
        else if (thisHdr->u8_type == MESH_MSG_FORWARD)
        {
            if (thisHdr->u32_dst == nodeID)
            {
                // Message is for sub node, forward the message
                mesh_node_st subnode;
                if (xSemaphoreTake(accessNodeList, (TickType_t)1000) == pdTRUE)
                {
                    if (getRoute(thisHdr->u32_src, &subnode))
                    {
                        // We found a route, send package to next hop
                        if (subnode.u32_first_hop == 0)
                        {
                            LOGD("Route for %lX is direct", subnode.u32_id);
                            // Destination is a direct
                            thisHdr->u32_dst  = thisHdr->u32_src;
                            thisHdr->u32_src  = thisDataMsg->u32_orig;
                            thisHdr->u8_type = MESH_MSG_DIRECT;
                        }
                        else
                        {
                            LOGD("Route for %lX is to %lX", subnode.u32_id, subnode.u32_first_hop);
                            // Destination is a sub
                            thisHdr->u32_dst  = subnode.u32_first_hop;
                            thisHdr->u8_type = MESH_MSG_FORWARD;
                        }

                        // Put message into send queue
                        if (!meshSendRequest(thisDataMsg, thisDataMsg->u16_len))
                        {
                            LOGE("Cannot forward message because send queue is full");
                            s_mesh_diag.u32_route_fail++;
                        }
                    }
                    else
                    {
                        LOGE("No route found for %lX", thisHdr->u32_src);
                        s_mesh_diag.u32_route_fail++;
                    }
                    xSemaphoreGive(accessNodeList);
                }
                else
                {
                    LOGE("Could not access map to forward package");
                    s_mesh_diag.u32_route_fail++;
                }
            }
            else
            {
                //LOGD("Message is not for us"); // ignore
            }
        }
        else if (thisHdr->u8_type == MESH_MSG_BROADCAST)
        {
            // This is a broadcast. Forward to all direct nodes, but not to the one who sent it
            LOGD("Handling broadcast with ID %08lX from %08lX", thisHdr->u32_dst, thisHdr->u32_src);
            // Check if this broadcast is coming from ourself
            if (meshBroadcastID(thisHdr->u32_dst) == meshBroadcastID(nodeID))
            {
                LOGD("We received our own broadcast, dismissing it");
            }
            // Check if we handled this broadcast already
            else if (isOldBroadcast(thisHdr->u32_dst))
            {
                LOGD("Got an old broadcast, dismissing it");
            }
            else
            {
                // Put broadcast into send queue
                if (!meshSendRequest(thisDataMsg, thisDataMsg->u16_len))
                {
                    LOGE("Cannot forward broadcast because send queue is full");
                    s_mesh_diag.u32_route_fail++;
                }

                // This is a broadcast, call user callback to handle the data
                LOGD("Got data broadcast %s", (char *)thisDataMsg->au8_data);
                if (NULL != s_mesh_events.DataAvailable)
                {
                    s_mesh_events.DataAvailable(thisHdr->u32_src, thisDataMsg->au8_data,
                                                thisDataMsg->u16_len - offsetof(mesh_data_msg_st, au8_data), rxRssi, rxSnr);
                }
            }
        }
        else
        {
            LOGW("not supported message type %d", thisHdr->u8_type);
            s_mesh_diag.u32_msg_invalid++;
        }
    }
}

static void OnRxTimeout(void)
{
    LOGW("LoRa RX timeout");
    s_mesh_diag.u32_rx_error++;

    if (MESH_TX != loraState)
    {
        loraState = MESH_IDLE;

        // Restart listening
        radio_listen();
    }
}

static void OnRxError(void)
{
    LOGW("LoRa RX error");
    s_mesh_diag.u32_rx_error++;

    if (MESH_TX != loraState)
    {
        loraState = MESH_IDLE;

        // Restart listening
        radio_listen();
    }
}

/**
 * Callback if the Channel Activity Detection has finished
 * Starts sending the ACK package if the channel is available
 * Retries 5 times if the channel was busy.
 * Returns to listen mode if the channel was occupied <CAD_RETRY> times
 *
 * @param cadResult
 *         True if channel activity was detected
 *         False if the channel is available
 */
static void OnCadDone(bool cadResult)
{
    //LOGD("CAD result %d", cadResult);
    mesh_data_msg_st   *ps_msg = NULL;
    uint32_t            ms_retry_delay = 0;

    if (cadResult)
    {
        LOGW("channel busy");
        s_mesh_diag.u32_cad_busy++;
        channelFreeRetryNum++;
        if (channelFreeRetryNum >= MESH_CAD_RETRY)
        {
            LOGE("channel busy %d times, giving up", MESH_CAD_RETRY);
            loraState           = MESH_IDLE;
            channelFreeRetryNum = 0;
            ms_retry_delay      = 0; // no more retry
            (void)xQueueReceive(queue_send, &ps_msg, 0); // discard tx packet
        }
        else
        {
            // Wait a little bit before retrying
            ms_retry_delay = random();
            ms_retry_delay &= 0x3FE;
            // 50ms to ~1.0s random delay
            ms_channel_detect = millis() + ms_retry_delay;
        }

        // Restart listening
        radio_listen();
    }
    //LOGD("channel free");
    else if ((pdTRUE == xQueueReceive(queue_send, &ps_msg, 0)) && (NULL != ps_msg))
    {
        LOGD("Sending %d bytes (%p)", ps_msg->u16_len, ps_msg);
        msTxStart = millis();
        channelFreeRetryNum = 0;

        // Send the data package
        Radio.Standby();
        Radio.Send((uint8_t *)ps_msg, ps_msg->u16_len);

        // mark as invalid to free up the queue entry
        memset(ps_msg, 0, sizeof(mesh_data_msg_st));
    }
}
}