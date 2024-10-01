/*
 * mesh_route.c
 *
 */

#include <stdlib.h>
#include "global_defs.h"

#include "cloud_net_cfg.h"
#include "cloud_net.h"

#include "lora_mesh.h"
#include "lora_mesh_cfg.h"


#ifndef ENABLE_LORA_MESH_DEBUG
  #undef LOGD
  #define LOGD(...)
#endif


/** The list with all known nodes */
static mesh_node_st as_nodes_map[MESH_MAX_NUM_NODES];

/** Index to the first free node entry */
static uint16_t u16_nodes_count = 0;

#define NUM_OF_LAST_BROADCASTS 10
static uint8_t broadcastIndex = 0;
static uint32_t broadcastList[NUM_OF_LAST_BROADCASTS] = {0UL};


void meshRouteInit(void)
{
    // Prepare empty nodes map
    memset(&as_nodes_map, 0, sizeof(as_nodes_map));
    //LOGD("sizes %lu %lu", sizeof(mesh_node_st), sizeof(as_nodes_map));

    // first entry is itself
    as_nodes_map[0].u32_id = meshNodeID();
    u16_nodes_count = 1;

#if 0 // unit testing only
    for (uint8_t i = 1; i < 5; i++)
    {
        addNode(rand(), 0, 0, NODE_CLOUD_NET_AVAILBLE, INT8_MIN, INT16_MIN);
    }
#endif
}

/**
 * Delete a node route by copying following routes on top of it.
 * @param index
 *         The node to be deleted
 */
void deleteRoute(uint8_t index)
{
    // Delete a route by copying following routes on top of it
  //uint32_t nodeToDelete = as_nodes_map[index].u32_id;
#if 0
    memcpy(&as_nodes_map[index], &as_nodes_map[index + 1], sizeof(mesh_node_st) * (MESH_MAX_NUM_NODES - index - 1));
#else
    memmove(&as_nodes_map[index], &as_nodes_map[index + 1], sizeof(mesh_node_st) * (MESH_MAX_NUM_NODES - index - 1));
#endif
    u16_nodes_count--;
    as_nodes_map[u16_nodes_count].u32_id = 0;
}

/**
 * Find a route to a node
 * @param id
 *         Node ID we need a route to
 * @param route
 *         nodesList struct that will be filled with the route
 * @return bool
 *         True if a route was found, false if not
 */
bool getRoute(uint32_t id, mesh_node_st *route)
{
    for (uint16_t idx = 0; idx < MESH_MAX_NUM_NODES; idx++)
    {
        if ((0 == id) || (0 == as_nodes_map[idx].u32_id))
        {
            break; // end of list
        }
        if (as_nodes_map[idx].u32_id == id)
        {
            // node found in map
            memcpy(route, &as_nodes_map[idx], sizeof(mesh_node_st));
            return true;
        }
    }
    // Node not in map
    return false;
}

/**
 * Find a node connected directly to the cloud
 */
bool getCloudNode(mesh_node_st *cloudnode, uint32_t id_ignore)
{
    int16_t     max_rssi    = INT16_MIN;
    uint8_t     min_hop     = UINT8_MAX;
    uint16_t    u16_idx     = 0;
    uint16_t    u16_offset  = 0; // search offet
    bool        b_status    = false;

    LOGD("%s(%08lx)", __func__, id_ignore);

    if (0 != id_ignore)
    {
        for (u16_idx = 1 /*skip itself*/; u16_idx < u16_nodes_count; u16_idx++)
        {
            if (id_ignore == as_nodes_map[u16_idx].u32_id)
            {
                LOGD("ignore %02u-%08lx", u16_idx, id_ignore);
                u16_offset = u16_idx;
                break; // found index of the node to be ignored
            }
        }
    }

    for (u16_idx = 0; u16_idx < u16_nodes_count; u16_idx++)
    {
        // circular search
        uint16_t idx = u16_idx + u16_offset;
        if (idx >= u16_nodes_count)
        {
            idx -= u16_nodes_count;
        }

        const mesh_node_st *ps_node = &as_nodes_map[idx];

        if ((ps_node->u32_id == 0) ||
            (ps_node->u32_id == meshNodeID()) ||
            (ps_node->u32_id == id_ignore))
        {
            // ignore nodes
        }
        else if (0 == (ps_node->u3_flags & NODE_CLOUD_NET_AVAILBLE))
        {
            // node is 4G offline
        }
#ifdef MESH_PARENT_NODE_ID
        else if (MESH_PARENT_NODE_ID != ps_node->u32_id)
        {
            //LOGW("ignored node %08lx != %08lx", ps_node->u32_id, MESH_PARENT_NODE_ID);
        }
#endif
        else if (ps_node->u5_num_hops < min_hop)
        {
            // found an online node with smaller hops
            min_hop = ps_node->u5_num_hops;
            memcpy(cloudnode, ps_node, sizeof(mesh_node_st));
            b_status = true;
        }
#if 0
        else if ((ps_node->u5_num_hops == min_hop) && (ps_node->i16_rssi > max_rssi))
        {
            // found an online node with higher signal level
            max_rssi = ps_node->i16_rssi;
            memcpy(cloudnode, ps_node, sizeof(mesh_node_st));
            b_status = true;
        }
#else
        // just do round-robin of possible parent nodes
        UNUSED(max_rssi);
#endif
    }

    if (!b_status && (0 != id_ignore))
    {
        // retry without the "ignore"
        LOGD("retry seach with out ignore");
        return getCloudNode(cloudnode, 0);
    }

    return b_status;
}

/**
 * Add a node into the list.
 * Checks if the node already exists and
 * removes node if the existing entry has more hops
 */
bool addNode(uint32_t u32_id, uint32_t u32_hop,
             uint8_t u5_num_hops, uint8_t u3_flags,
             int8_t i8_snr, int16_t i16_rssi)
{
    bool listChanged = false;

    LOGD("%s(%08lx, %08lx, %u, %u, %d, %d)", __func__, u32_id, u32_hop, u5_num_hops, u3_flags, i8_snr, i16_rssi);

    mesh_node_st _newNode;
    _newNode.u32_id         = u32_id;
    _newNode.u3_flags       = u3_flags;
    _newNode.u5_num_hops    = u5_num_hops;
    _newNode.i8_snr         = i8_snr;
    _newNode.i16_rssi       = i16_rssi;
    _newNode.u32_first_hop  = u32_hop;
    _newNode.ms_timestamp   = millis();

    for (uint16_t idx = 0; idx < MESH_MAX_NUM_NODES; idx++)
    {
        if (as_nodes_map[idx].u32_id == u32_id)
        {
            if (u3_flags != as_nodes_map[idx].u3_flags)
            {
                as_nodes_map[idx].ms_timestamp  = millis();
                as_nodes_map[idx].u3_flags      = u3_flags;
                // node status changed
                listChanged = true;
            }

            if (as_nodes_map[idx].u32_first_hop == 0)
            {
                if (u32_hop == 0)
                { // Node entry exist already as direct, update timestamp & signal level
                    as_nodes_map[idx].ms_timestamp  = millis();
                    as_nodes_map[idx].i8_snr        = i8_snr;
                    as_nodes_map[idx].i16_rssi      = i16_rssi;
                }
                LOGD("Node %08lx already exists as direct", u32_id);
                return listChanged;
            }
            else if (u32_hop == 0)
            {
                // Found the node, but not as direct neighbor
                LOGD("Node %08lx removed because it was a sub", u32_id);
                deleteRoute(idx);
                idx--;
            }
            // Node entry exists, check number of hops
            else if (as_nodes_map[idx].u5_num_hops < u5_num_hops)
            {
                // Node entry exist with smaller # of hops
                LOGD("Node %08lx exist with a lower number of hops", u32_id);
                return listChanged;
            }
            else
            {
                // Found the node, but with higher # of hops
                LOGD("Node %08lx exist with a higher number of hops", u32_id);
                deleteRoute(idx);
                idx--;
            }

            break; // found same id
        }
    }

    if (u16_nodes_count >= MESH_MAX_NUM_NODES)
    {
        // Map is full, remove the oldest entry
        deleteRoute(1 /*skip self*/);
        listChanged = true;
    }

    // New node entry
    memcpy(&as_nodes_map[u16_nodes_count], &_newNode, sizeof(mesh_node_st));
    u16_nodes_count++;

    listChanged = true;
    LOGD("Added node %08lx with hop %08lx (status %u-%u)", u32_id, u32_hop, u3_flags, u5_num_hops);
    return listChanged;
}

/**
 * Remove all nodes that are a non direct and have a given node as first hop.
 * This is to clean up the nodes list from left overs of an unresponsive node
 * @param id
 *         The node which is listed as first hop
 */
void clearSubs(uint32_t id)
{
    for (uint16_t idx = 1 /*skip itself*/; idx < MESH_MAX_NUM_NODES; idx++)
    {
        if (as_nodes_map[idx].u32_first_hop == id)
        {
            LOGD("Removed node %lX with hop %lX", as_nodes_map[idx].u32_id, as_nodes_map[idx].u32_first_hop);
            deleteRoute(idx);
            idx--;
        }
    }
}

/**
 * Check the list for nodes that did not be refreshed within a given timeout
 * Checks as well for nodes that have "impossible" number of hops (> number of max nodes)
 * @return bool
 *             True if no changes were done, false if any node was removed
 */
bool cleanMap(void)
{
    // Check active nodes list
    bool mapUpToDate = true;
    for (uint16_t idx = 1 /*skip itself*/; idx < MESH_MAX_NUM_NODES; idx++)
    {
        if (as_nodes_map[idx].u32_id == 0)
        {
            // Last entry found
            break;
        }
        if (((millis() > (as_nodes_map[idx].ms_timestamp + MESH_NODE_INACTIVE_TIMEOUT))) || (as_nodes_map[idx].u5_num_hops > MESH_MAX_HOP_COUNT))
        {
            // Node was not refreshed for some time
            LOGW("Node %lX with hop %lX timed out or has too many hops", as_nodes_map[idx].u32_id, as_nodes_map[idx].u32_first_hop);
            if (as_nodes_map[idx].u32_first_hop == 0)
            {
                clearSubs(as_nodes_map[idx].u32_id);
            }
            deleteRoute(idx);
            idx--;
            mapUpToDate = false;
        }
    }
    return mapUpToDate;
}

/**
 * Create a list of nodes and hops to be broadcasted as this nodes map
 * @param nodes[]
 *         Pointer to an two dimensional array to hold the node IDs and hops
 * @param u16_offset
 *         Offset to the first element of the nodes map
 * @return uint8_t
 *         Number of nodes in the list
 */
uint16_t nodeMap(uint8_t nodes[][NODE_INFO_SIZE], uint16_t u16_offset)
{
    uint16_t u16_count = 0;

    for (uint16_t idx = u16_offset; (u16_count < NUM_NODES_PER_MSG) && (idx < MESH_MAX_NUM_NODES); idx++)
    {
        mesh_node_st *node = &as_nodes_map[idx];

        if (0 == idx) // update own flags
        {
            node->u3_flags = cloudNetInterfaceAvailable() ? NODE_CLOUD_NET_AVAILBLE : 0;
        }
        else if (as_nodes_map[idx].u32_id == 0)
        {
            // Last node found
            break;
        }
        nodes[u16_count][0] = node->u32_id & 0x000000FF;
        nodes[u16_count][1] = (node->u32_id >> 8) & 0x000000FF;
        nodes[u16_count][2] = (node->u32_id >> 16) & 0x000000FF;
        nodes[u16_count][3] = (node->u32_id >> 24) & 0x000000FF;
        nodes[u16_count][4] = MESH_NODE_STATUS(node->u3_flags, node->u5_num_hops);
        //LOGD("node #%u %08lx (%u)", idx + 1, node->u32_id, nodes[u16_count][4]);

        u16_count++;
    }

    return u16_count;
}

/**
 * Get number of nodes in the map
 * @return uint8_t
 *         Number of nodes
 */
uint16_t numOfNodes(void)
{
    return u16_nodes_count;
}

/**
 * Get the information of a specific node
 * @param nodeNum
 *         Index of the node we want to query
 * @param nodeId
 *         Pointer to an uint32_t to save the node ID to
 * @param firstHop
 *        Pointer to an uint32_t to save the nodes first hop ID to
 * @param numHops
 *         Pointer to an uint8_t to save the number of hops to
 * @return bool
 *         True if the data could be found, false if the requested index is out of range
 */
bool getNode(uint8_t nodeNum, uint32_t *nodeId, uint32_t *firstHop, uint8_t *numHops)
{
    if (nodeNum >= numOfNodes())
    {
        return false;
    }

    *nodeId = as_nodes_map[nodeNum].u32_id;
    *firstHop = as_nodes_map[nodeNum].u32_first_hop;
    *numHops = as_nodes_map[nodeNum].u5_num_hops;
    return true;
}

void printNodes(void)
{
    LOGI("%d nodes in the map", u16_nodes_count);

    for (uint16_t idx = 0; idx < u16_nodes_count; idx++)
    {
        mesh_node_st *node = &as_nodes_map[idx];
        if (meshNodeID() == node->u32_id)
        {
            LOGI("node #%02d: %08X (%u) self",  idx + 1, node->u32_id, node->u3_flags);
        }
        else if (node->u32_first_hop == 0)
        {
            LOGI("node #%02d: %08X (%u) direct (rssi %ddBm, snr %ddB)", idx + 1, node->u32_id, node->u3_flags, node->i16_rssi, node->i8_snr);
        }
        else
        {
            LOGI("node #%02d: %08X (%u) first hop %08X (#hops %d)", idx + 1, node->u32_id, node->u3_flags, node->u32_first_hop, node->u5_num_hops);
        }
    }
}

/**
 * Handle broadcast message ID's
 * to avoid circulating same broadcast over and over
 *
 * @param broadcastID
 *             The broadcast ID to be checked
 * @return bool
 *             True if the broadcast is an old broadcast, else false
 */
bool isOldBroadcast(uint32_t broadcastID)
{
    for (int idx = 0; idx < NUM_OF_LAST_BROADCASTS; idx++)
    {
        if (broadcastID == broadcastList[idx])
        {
            // Broadcast ID is already in the list
            return true;
        }
    }
    // This is a new broadcast ID
    broadcastList[broadcastIndex] = broadcastID;
    broadcastIndex ++;
    if (broadcastIndex == NUM_OF_LAST_BROADCASTS)
    {
        // Index overflow, reset to 0
        broadcastIndex = 0;
    }
    return false;
}
