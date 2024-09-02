
#include <time.h>
#include <sys/time.h>

#include "global_defs.h"
#include "cloud_comms.h"


namespace cloud::sync
{

/*
 * Local Variables
 */
static net_context_et   s_net;          // cloud sync stats
static uint32_t         ms_last_sync;   // millisecond timestamp of last sync run
static bool             b_synced;       // true = time synchronized with the server


/*
 * Public Functions
 */

void init(void)
{
    memset(&s_net, 0, sizeof(s_net));
    ms_last_sync    = 0;
    b_synced        = false;
    s_net.e_state   = NET_STATE_IDLE;
}

void cycle(void)
{
    if (true == getCommsFlag(CLOUD_COMMS_FAULT))
    {
        ms_last_sync = 0; // reset last sync during comms lost to time sync again
    }

    switch (s_net.e_state)
    {
    case NET_STATE_IDLE:
        if ((0 == ms_last_sync) ||
            (millis() - ms_last_sync > K_CLOUD_SYNC_INTERVAL))
        {
            s_net.e_state = NET_STATE_SEND_REQ;
        }
        break;

    case NET_STATE_SEND_REQ:
        s_net.ui16_message_id = net::newMessageId();
        s_net.b_resp_status   = false;
        s_net.b_resp_received = false;

        if (true == net::sendGetRequest(s_net.ui16_message_id, K_CLOUD_TIME_PATH, NULL, 0))
        {
            LOGD("sync request (msg %d)", s_net.ui16_message_id);
            s_net.ms_resp_timeout = millis();
            s_net.e_state         = NET_STATE_WAIT_RESP;
        }
        else
        {
            LOGW("request error");
            s_net.ms_retry_delay  = millis();
            s_net.e_state         = NET_STATE_RETRY_DELAY;
            net::timeoutOccured(); // considered as timeout
        }
        break;

    case NET_STATE_WAIT_RESP:
        if (true == s_net.b_resp_received)
        {
            if (true == s_net.b_resp_status)
            {
                s_net.e_state = NET_STATE_IDLE;
            }
            else
            {
                s_net.ms_retry_delay = millis();
                s_net.e_state        = NET_STATE_RETRY_DELAY;
            }
        }
        else if ((millis() - s_net.ms_resp_timeout) > K_CLOUD_SYNC_RESPONSE_TIMEOUT)
        {
            LOGW("response timeout");
            net::timeoutOccured();
            s_net.ms_retry_delay  = millis();
            s_net.e_state         = NET_STATE_RETRY_DELAY;
        }
        break;

    case NET_STATE_RETRY_DELAY:
        if (millis() - s_net.ms_retry_delay > K_CLOUD_SYNC_RETRY_DELAY)
        {
            s_net.e_state = NET_STATE_IDLE; // retry
        }
        break;

    default:
        init();
        break;
    }
}

static bool parseTimePayload(const char *pc_time, size_t sz_len)
{
    time_t          recv_ts;  // client epoch
    time_t          host_ts;  // server epoch
    struct tm       host_tm;  // server time
    struct timeval  sync_tv;
    char           *res;
    uint32_t        rtt; // round-trip time

    //LOGD("%s(%.*s)", __func__, sz_len, pc_time);

    memset(&host_tm, 0, sizeof(host_tm));
    setSystemFlag(TIME_SYNC, false);
    b_synced = false;

    recv_ts = time(NULL);
    res = strptime(pc_time, "%Y-%m-%d %H:%M:%S", &host_tm);

    if ((NULL == res) || ((res - pc_time) < 18))
    {
        LOGW("failed to parse \"%.*s\"", sz_len, pc_time);
    }
    else if ((-1) == (host_ts = mktime(&host_tm)))
    {
        LOGW("invalid time \"%.*s\"", sz_len, pc_time);
    }
    else
    {
        /* approximate time sync compensation from round-trip time */
        rtt = (millis() - s_net.ms_resp_timeout) / 1000;
        /* valid delay response */
        if (rtt <= K_CLOUD_SYNC_VALID_RESPONSE_DELAY)
        {
            sync_tv.tv_sec = host_ts + (rtt / 2);;
            sync_tv.tv_usec = 0;

            if (0 != settimeofday(&sync_tv, NULL))
            {
                LOGW("unable to set time");
            }
            else
            {
                LOGI("time sync (%lld -> %lld)", recv_ts, sync_tv.tv_sec);
                setSystemFlag(TIME_SYNC, true);
                b_synced = true;
            }
        }
        else
        {
            LOGW("response timeout (rtt = %u)", rtt);
        }
    }

    return b_synced;
}

void handleResponse(uint16_t ui16_message_id, const uint8_t *pui8_payload_buf, size_t sz_payload_len)
{
    //LOGD("%s", __func__);
    if (ui16_message_id == s_net.ui16_message_id)
    {
        // e.g. 2022-01-21 08:53:03
        //LOGD("recv [%lu] \"%.*s\"", sz_payload_len, sz_payload_len, pui8_payload_buf);
        s_net.b_resp_received = true;
        if (true == parseTimePayload((const char *)pui8_payload_buf, sz_payload_len))
        {
            s_net.b_resp_status = true;
            ms_last_sync        = millis();
            s_net.ui16_message_id = 0;    // ignore duplicate server response
        }

    }
}

bool getStatus(void)
{
#if 1
    return b_synced;
#else
    return getSystemFlag(TIME_SYNC);
#endif
}

} // namespace cloud::sync
