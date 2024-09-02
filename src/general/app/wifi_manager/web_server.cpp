#include <unistd.h>
#include <esp_tls_crypto.h>
#include "global_defs.h"
#include "general_info.h"
#include "web_server_cfg.h"
#include "web_server.h"
#include <cJSON.h> // Include cJSON for JSON handling
#include "device_id/device_id.h"
#include "modem/quectel.h"
namespace web::server
{

/*
 * Local Variables
 */
static httpd_handle_t server = NULL;
static bool b_started = false;

static char recv_buf[1024];
static char send_buf[256];

#ifdef WEB_SERVER_BASIC_AUTH
static char *http_auth_basic(const char *username, const char *password)
{
    size_t out;
    char *user_info = NULL;
    char *digest = NULL;
    size_t n = 0;
    if (asprintf(&user_info, "%s:%s", username, password) < 0) {
        return NULL;
    }

    if (!user_info) {
        LOGE("No enough memory for user information");
        return NULL;
    }
    esp_crypto_base64_encode(NULL, 0, &n, (const unsigned char *)user_info, strlen(user_info));

    /* 6: The length of the "Basic " string
     * n: Number of bytes for a base64 encode format
     * 1: Number of bytes for a reserved which be used to fill zero
    */
    digest = (char *)calloc(1, 6 + n + 1);
    if (digest) {
        strcpy(digest, "Basic ");
        esp_crypto_base64_encode((unsigned char *)digest + 6, n, &out, (const unsigned char *)user_info, strlen(user_info));
    }
    free(user_info);
    return digest;
}

static bool authenticate(httpd_req_t *req)
{
    static const char *hdr = "Authorization";
    static const char *digest = NULL;
    char    val_buf[64] = {0, };
    size_t  val_len = 0;
    bool    b_status = false;

    if (NULL == digest)
    {
        digest = http_auth_basic(WEB_SERVER_AUTH_USERNAME, WEB_SERVER_AUTH_PASSWORD);
    }

    if (NULL == digest)
    {
        LOGE("unknown auth");
    }
    else if ((val_len = httpd_req_get_hdr_value_len(req, hdr)) < 1)
    {
        LOGW("not authenticated");
    }
    else if (val_len >= sizeof(val_buf))
    {
        LOGE("not enough buffer");
    }
    else if (ESP_OK != httpd_req_get_hdr_value_str(req, hdr, val_buf, val_len + 1))
    {
        LOGE("no auth value");
    }
    else if (0 != strncmp(digest, val_buf, val_len))
    {
        LOGE("wrong auth: %s", val_buf);
    }
    else
    {
        b_status = true;
    }

    return b_status;
}

static esp_err_t request_auth(httpd_req_t *req)
{
    httpd_resp_set_status(req, "401 UNAUTHORIZED"); // HTTP Response 401
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"" WEB_SERVER_AUTH_REALM "\"");
    httpd_resp_send(req, NULL, 0);

    return ESP_OK;
}
#endif

static esp_err_t resp_home_after(httpd_req_t *req, const char *msg1="ok", const char *msg2="", uint32_t ms=5000UL)
{
    int len = snprintf(send_buf, sizeof(send_buf) - 1,
                    "<html><h3>%s</h3>%s<script type=\"text/javascript\">"
                    "(function () { setTimeout(function () { window.location.replace(\"/\"); }, %lu); })();"
                    "</script></html>", msg1, msg2, ms);
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, send_buf, len);
}

/* Serve index.html */
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
esp_err_t get_indexhtml_handler(httpd_req_t *req)
{
#ifdef WEB_SERVER_BASIC_AUTH
    if (!authenticate(req)) {
        return request_auth(req);
    }
#endif
    return httpd_resp_send(req, (const char *) index_html_start, index_html_end - index_html_start);
}

/* Serve favicon */
extern const uint8_t favicon_svg_start[] asm("_binary_favicon_svg_start");
extern const uint8_t favicon_svg_end[] asm("_binary_favicon_svg_end");
esp_err_t get_favicon_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "image/svg+xml");
    return httpd_resp_send(req, (const char *) favicon_svg_start, favicon_svg_end - favicon_svg_start);
}

/* Serve styles.css */
extern const uint8_t style_css_start[] asm("_binary_style_css_start");
extern const uint8_t style_css_end[] asm("_binary_style_css_end");
esp_err_t get_stylecss_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/css");
    return httpd_resp_send(req, (const char *) style_css_start, style_css_end - style_css_start);
}

/* Serve script.js */
extern const uint8_t script_js_start[] asm("_binary_script_js_start");
extern const uint8_t script_js_end[] asm("_binary_script_js_end");
esp_err_t get_scriptjs_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/javascript");
    return httpd_resp_send(req, (const char *) script_js_start, script_js_end - script_js_start);
}

int check4GCommsStat(){
    /* */
     return 2; //  0 - Offline ,  1 - Online ,  2 - Idle
}

int checkLoRaCommsStat(){
    /* */
    return 0;
}

int checkWifiCommsStat(){
    /* */
    return 1;
}

int checkBlufiCommsStat(){
    /* */
    return 1;
}



WebAppData get_web_app_data() {
    WebAppData data; 
    char active_comms_stat_list[24] = "";
    // active_comms_stat_list[0]  = '\0';
    // char temp_lora_buffer[12]  = "";
    // char temp_4g_buffer[12]    = "";
    // char temp_wifi_buffer[12]  = "";
    // char temp_blufi_buffer[12] = "";

    data.enmtr_version       = "1.0.0"; 
    data.modem               = "a";
    data.active_comms        = "4G,LoRa,WiFi,BlueFi";

    // sprintf(temp_4g_buffer, "%d", check4GCommsStat());
    // strcat(active_comms_stat_list, temp_4g_buffer);
    // strcat(active_comms_stat_list, ",");
    // sprintf(temp_lora_buffer, "%d", checkLoRaCommsStat());
    // strcat(active_comms_stat_list, temp_lora_buffer);

    // strcat(active_comms_stat_list, ",");
    // sprintf(temp_wifi_buffer, "%d", checkWifiCommsStat());
    // strcat(active_comms_stat_list, temp_wifi_buffer);

    // strcat(active_comms_stat_list, ",");
    // sprintf(temp_blufi_buffer, "%d", checkBlufiCommsStat());
    // strcat(active_comms_stat_list, temp_blufi_buffer);

    sprintf(active_comms_stat_list,"%d,%d,%d,%d",_4g_state,lora_state,wifi_state,blufi_state);
    strcpy(data.active_comms_status, active_comms_stat_list);
    
    char signal_strength_bufffer[32] = "";
    char temp_signal_strength[24] = "";

    sprintf(signal_strength_bufffer, "%ddBm,%ddBm,%ddBm,--",_4g_rssi,lora_rssi, wifi_rssi);
    
    strcat(temp_signal_strength ,signal_strength_bufffer);
    strcat(temp_signal_strength ,",");
    strcpy(data.signal_strength,temp_signal_strength);

    char temp_comms_link_status[12] = "";
    sprintf(temp_comms_link_status,"%d,%d,%d,%d",g_b_sim_ok,g_b_udp_ok,g_b_dtls_ok,g_b_mcu_to_modem_ok);
    strcpy(data.comms_link_status,temp_comms_link_status);
    // LOGW("%s",data.signal_strength);

    return data;
}

esp_err_t get_dynamic_data_handler(httpd_req_t *req)
{
    WebAppData data = get_web_app_data(); // Retrieve the data

    // Create JSON object
    cJSON *json = cJSON_CreateObject();
    // Add new fields to the JSON object
    cJSON_AddStringToObject(json, "serial_number", ac_serialnumber);
    cJSON_AddStringToObject(json, "device_id", device_id);
    cJSON_AddStringToObject(json, "fw_version", fw_version);
    cJSON_AddStringToObject(json, "enmtr_version", data.enmtr_version);
    cJSON_AddStringToObject(json, "modem", data.modem);
    cJSON_AddStringToObject(json, "imei", ac_imei);
    cJSON_AddStringToObject(json, "imsi", ac_imsi);
    cJSON_AddStringToObject(json, "ccid", ac_iccid);
    cJSON_AddStringToObject(json, "active_comms", data.active_comms);
    cJSON_AddStringToObject(json, "active_comms_status", data.active_comms_status);
    cJSON_AddStringToObject(json, "comms_link_status", data.comms_link_status);
    cJSON_AddStringToObject(json, "signal_strength", data.signal_strength);

    // Print the JSON response
    const char *response = cJSON_Print(json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));

    // Clean up
    cJSON_Delete(json);
    free((void *)response); // Free the allocated response string

    return ESP_OK;
}


/* Handle file uploads */
esp_err_t post_upload_handler(httpd_req_t *req)
{
    char type[8];
    char fname[16];
    char *ext = NULL;
    FILE *fp = NULL;
    esp_err_t err = ESP_OK;

    if (ESP_OK != httpd_req_get_hdr_value_str(req, "x-type", type, sizeof(type)))
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "unknown file type");
        err = ESP_ERR_HTTPD_INVALID_REQ;
    }
    else if ((ESP_OK != httpd_req_get_hdr_value_str(req, "x-filename", recv_buf, sizeof(recv_buf))) ||
             (NULL == (ext = strchr(recv_buf, '.'))))
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "unknown filename");
        err = ESP_ERR_HTTPD_INVALID_REQ;
    }
    else if ((snprintf(fname, sizeof(fname), "/mnt/%s%s", type, ext) < 0) ||
             (NULL == (fp = fopen(fname, "wb"))))
    {
        LOGE("failed to open \"%s\" for writing", fname);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "File Error");
        err = ESP_FAIL;
    }
    else
    {
        int remaining = req->content_len;
        LOGD("store \"%s\" -> \"%s\" (%d)", recv_buf, fname, remaining);
        while (remaining > 0)
        {
            size_t to_read = remaining;
            if (to_read > sizeof(recv_buf)) {
                to_read = sizeof(recv_buf);
            }
            int recv_len = httpd_req_recv(req, recv_buf, to_read);

            // Timeout Error: Just retry
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT){
                LOGW("socket timeout");
                continue;
            }

            // Serious Error: Abort OTA
            else if (recv_len <= 0) {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Protocol Error");
                LOGE("http error");
                err = ESP_FAIL;
                break;
            }

            if (recv_len != fwrite(recv_buf, 1, recv_len, fp)) {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "File Error");
                LOGE("write error");
                fclose(fp);
                fp = NULL;
                err = ESP_FAIL;
                // Try to clear storage
                (void)esp_vfs_fat_spiflash_format_rw_wl("/mnt", "storage");
                break;
            }

            remaining -= recv_len;

            printf("\r recv %d%% (%d)\r", (100 * (req->content_len - remaining)) / req->content_len, req->content_len - remaining);
        }

        LOGI("stored \"%s\" %d/%d ", fname, req->content_len - remaining, req->content_len);
        if (NULL != fp) {
            fclose(fp);
        }
    }

    if (ESP_OK == err)
    {
        resp_home_after(req, "upload done", "", 5000UL);
    }

    return err;
}


bool start()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = K_WEB_SERVER_MAX_URI_HANDLERS;

    if (b_started) {
        return b_started;
    }

    if (ESP_OK == httpd_start(&server, &config))
    {

  #define REGISTER_HANDLER(_handler, _uri, _method, func)   \
                httpd_uri_t _handler = {                    \
                    .uri = _uri, .method = _method,         \
                    .handler = func, .user_ctx = NULL       \
                };                                          \
                httpd_register_uri_handler(server, &_handler)

  #define REGISTER_POST_HANDLER(uri, func)  REGISTER_HANDLER(post_ ## func, uri, HTTP_POST, post_ ## func ## _handler)
  #define REGISTER_GET_HANDLER(uri, func)   REGISTER_HANDLER(get_ ## func, uri, HTTP_GET, get_ ## func ## _handler)

        REGISTER_GET_HANDLER("/", indexhtml);
        REGISTER_GET_HANDLER("/favicon.ico", favicon);
        REGISTER_GET_HANDLER("/style.css", stylecss);
        REGISTER_GET_HANDLER("/script.js", scriptjs);
        REGISTER_GET_HANDLER("/data", dynamic_data); // New dynamic data endpoint

        REGISTER_POST_HANDLER("/upload", upload);

        b_started = true;
    }

    LOGD("httpd_start() %s", b_started ? "ok" : "failed");
    return b_started;
}

void stop()
{
    // Stop the server
    if (b_started) {
        httpd_stop(server);
        b_started = false;
    }
}

} // namespace web::server
