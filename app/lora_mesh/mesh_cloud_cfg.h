/*
 * mesh_cloud_cfg.h
 *
 */

#ifndef _MESH_CLOUD_CFG_H_
#define _MESH_CLOUD_CFG_H_

// temporary filename for download via mesh
#define K_MESH_DOWNLOAD_TMP_FILENAME                    "MESHTMP.BIN"

// required buffer size
#define K_MESH_CLOUD_REPORT_PAYLOAD_MAXIMUM_SIZE        (320)   // at least K_CLOUD_MONITOR_PAYLOAD_BUF_SIZE
#define K_MESH_CLOUD_REPORT_PAYLOAD_SEGMENT_SIZE        (224)   // at most MESH_COMMS_PAYLOAD_MAX_SIZE

//------------------------------------------------------------------------------------------------------------------------------
/* for child nodes */

#define K_MESH_CLOUD_REPORT_ACK_TIMEOUT                 (5 * 1000UL)    // 5sec ACK timeout
#define K_MESH_CLOUD_REPORT_RETRY_DELAY                 (5 * 1000UL)    // 5sec retry delay
#define K_MESH_CLOUD_REPORT_SYNC_RETRIES                (16)            // re-sync to another parent after N retries
//#define K_MESH_CLOUD_REPORT_MAX_RETRIES                 ((12 * 60 * 60) / (5 + 10 + 10))  // give up after ~12 hours
#define K_MESH_CLOUD_REPORT_MAX_RESP_ERRORS             (5)


//------------------------------------------------------------------------------------------------------------------------------
/* for parent nodes */

#define K_MESH_CLOUD_REQUEST_INCOMPLETE_TIMEOUT         (60 * 1000UL)   // discard outdated parial request after a minute
#define K_MESH_CLOUD_REQUEST_RESPONSE_TIMEOUT           (10 * 1000UL)   // cloud server response timeout
#define K_MESH_CLOUD_REQUEST_RETRY_DELAY                (5 * 1000UL)    // 5sec retry delay
#define K_MESH_CLOUD_REQUEST_MAX_RETRIES                ((3 * 60 * 60) / (10 + 5))  // give up after ~3 hours

#define K_MESH_CLOUD_RESPONSE_ACK_TIMEOUT               (3000UL)        // 3sec timeout for the child node to acknowkedge the server response
#define K_MESH_CLOUD_RESPONSE_FWD_MAX_RETRIES           (5)             // forward server resp 5x at most

#define K_MESH_CLOUD_MAX_NUM_CHILD_NODES                (16)            // limit number of serving child nodes

// to do: transfer queue storage to the emmc
#define K_MESH_CLOUD_REQUEST_QUEUE_SIZE                 (16)            // mesh to cloud requests queue

//------------------------------------------------------------------------------------------------------------------------------

#endif /* _MESH_CLOUD_CFG_H_ */
