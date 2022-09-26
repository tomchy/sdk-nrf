/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef DOOR_LOCK_H
#define DOOR_LOCK_H

#include <zcl/zb_zcl_door_lock_addons.h>
#include <zcl/zb_zcl_basic_addons.h>
#include <zcl/zb_zcl_groups_addons.h>


/* Number chosen for the single endpoint provided by weather station */
#define DOOR_LOCK_ENDPOINT_NB 43

#define ZB_HA_DEVICE_VER_DOOR_LOCK 0  /*!< Door Lock device version */

/** @cond internal */

#define ZB_HA_DOOR_LOCK_IN_CLUSTER_NUM  3  /*!< @internal Door Lock IN clusters number */
#define ZB_HA_DOOR_LOCK_OUT_CLUSTER_NUM 1  /*!< @internal Door Lock OUT clusters number */

/** @internal @brief Number of clusters for DoorLock HA device. */
#define ZB_HA_DOOR_LOCK_CLUSTER_NUM (ZB_HA_DOOR_LOCK_IN_CLUSTER_NUM + ZB_HA_DOOR_LOCK_OUT_CLUSTER_NUM)


/** @internal @brief Number of attribute for reporting on Door Lock device */
#define ZB_HA_DOOR_LOCK_REPORT_ATTR_COUNT (ZB_ZCL_DOOR_LOCK_REPORT_ATTR_COUNT)

/** @endcond */

/** @brief Declare cluster list for Door Lock device.
  * @param cluster_list_name - cluster list variable name
  * @param door_lock_attr_list - attribute list for On/off switch configuration cluster
  * @param identify_client_attr_list - attribute list for Identify cluster (client role)
  * @param identify_server_attr_list - attribute list for Identify cluster (server role)
  * @param groups_attr_list - attribute list for Groups cluster
  */
#define ZB_HA_DECLARE_DOOR_LOCK_CLUSTER_LIST(                    \
	cluster_list_name,                                         \
	door_lock_attr_list,                                       \
		identify_client_attr_list,							\
		identify_server_attr_list,							\
	groups_attr_list)                                          \
	zb_zcl_cluster_desc_t cluster_list_name[] =                \
	{                                                          \
		ZB_ZCL_CLUSTER_DESC(								\
			ZB_ZCL_CLUSTER_ID_IDENTIFY,						\
			ZB_ZCL_ARRAY_SIZE(identify_server_attr_list, zb_zcl_attr_t),		\
			(identify_server_attr_list),						\
			ZB_ZCL_CLUSTER_SERVER_ROLE,						\
			ZB_ZCL_MANUF_CODE_INVALID						\
			),									\
		ZB_ZCL_CLUSTER_DESC(								\
			ZB_ZCL_CLUSTER_ID_DOOR_LOCK,						\
			ZB_ZCL_ARRAY_SIZE(door_lock_attr_list, zb_zcl_attr_t), \
			(door_lock_attr_list),                                 \
			ZB_ZCL_CLUSTER_SERVER_ROLE,                            \
			ZB_ZCL_MANUF_CODE_INVALID                              \
			),                                                       \
		ZB_ZCL_CLUSTER_DESC(								\
			ZB_ZCL_CLUSTER_ID_GROUPS,						\
			ZB_ZCL_ARRAY_SIZE(groups_attr_list, zb_zcl_attr_t),    \
			(groups_attr_list),                                    \
			ZB_ZCL_CLUSTER_SERVER_ROLE,                            \
			ZB_ZCL_MANUF_CODE_INVALID                              \
			),                                                       \
		ZB_ZCL_CLUSTER_DESC(								\
			ZB_ZCL_CLUSTER_ID_IDENTIFY,						\
			ZB_ZCL_ARRAY_SIZE(identify_client_attr_list, zb_zcl_attr_t),		\
			(identify_client_attr_list),						\
			ZB_ZCL_CLUSTER_CLIENT_ROLE,						\
			ZB_ZCL_MANUF_CODE_INVALID						\
			),									\
	}

/** @cond internals_doc */
/** @brief Declare simple descriptor for Door Lock device
    @param ep_name - endpoint variable name
    @param ep_id - endpoint ID
    @param in_clust_num   - number of supported input clusters
    @param out_clust_num  - number of supported output clusters
    @note in_clust_num, out_clust_num should be defined by numeric constants, not variables or any
    definitions, because these values are used to form simple descriptor type name
*/
#define ZB_ZCL_DECLARE_DOOR_LOCK_SIMPLE_DESC(ep_name, ep_id, in_clust_num, out_clust_num) \
	ZB_DECLARE_SIMPLE_DESC(in_clust_num, out_clust_num);                  \
	ZB_AF_SIMPLE_DESC_TYPE(in_clust_num, out_clust_num)  simple_desc_##ep_name = \
	{                                                                     \
		ep_id,                                                              \
		ZB_AF_HA_PROFILE_ID,                                                \
		ZB_HA_DOOR_LOCK_DEVICE_ID,                                          \
		ZB_HA_DEVICE_VER_DOOR_LOCK,                                         \
		0,                                                                  \
		in_clust_num,                                                       \
		out_clust_num,                                                      \
		{                                                                   \
			ZB_ZCL_CLUSTER_ID_IDENTIFY,                                       \
			ZB_ZCL_CLUSTER_ID_DOOR_LOCK,                                      \
			ZB_ZCL_CLUSTER_ID_GROUPS,                                          \
			ZB_ZCL_CLUSTER_ID_IDENTIFY,					\
		}                                                                   \
	}

/** @endcond */

/** @brief Declare endpoint for Door Lock device
    @param ep_name - endpoint variable name
    @param ep_id - endpoint ID
    @param cluster_list - endpoint cluster list
 */
#define ZB_HA_DECLARE_DOOR_LOCK_EP(ep_name, ep_id, cluster_list)                \
	ZB_ZCL_DECLARE_DOOR_LOCK_SIMPLE_DESC(	\
		ep_name,	\
		ep_id,	\
		ZB_HA_DOOR_LOCK_IN_CLUSTER_NUM,	\
		ZB_HA_DOOR_LOCK_OUT_CLUSTER_NUM);	\
	ZBOSS_DEVICE_DECLARE_REPORTING_CTX(	\
		reporting_info## ep_name,	\
		ZB_HA_DOOR_LOCK_REPORT_ATTR_COUNT);	\
	ZB_AF_DECLARE_ENDPOINT_DESC(	\
		ep_name,	\
		ep_id,	\
		ZB_AF_HA_PROFILE_ID,	\
		0,	\
		NULL,	\
		ZB_ZCL_ARRAY_SIZE(cluster_list, zb_zcl_cluster_desc_t),	\
		cluster_list,	\
		(zb_af_simple_desc_1_1_t*)&simple_desc_##ep_name,	\
		ZB_HA_DOOR_LOCK_REPORT_ATTR_COUNT,	\
		reporting_info## ep_name,	\
		0,	\
		NULL)


struct zb_door_lock_ctx {
	zb_zcl_identify_attrs_t identify_attr;
	zb_zcl_groups_attrs_t groups_attr;
	zb_zcl_door_lock_attrs_t door_lock_attr;
};


typedef void (*door_lock_locked_cb)(bool locked);

/**
 * @brief Initializes HW lock and read it's state from NVM.
 *
 * @return 0 if success, error code if failure.
 */
int door_lock_init(void);

/**
 * @brief Read the current status of the door lock.
 *
 * @retval true   if the door lock is locked
 * @retval false  if the door lock is unlocked
 */
bool door_lock_is_locked(void);

/**
 * @brief Lock the physical door lock.
 *
 * @note This API does not update Zigbee attribute value.
 *       It manipulates the HW lock and updates the state
 *       stored in NVM.
 */
void door_lock_lock(door_lock_locked_cb cb);

/**
 * @brief Unlock the physical door lock.
 *
 * @note This API does not update Zigbee attribute value.
 *       It manipulates the HW lock and updates the state
 *       stored in NVM.
 */
void door_lock_unlock(door_lock_locked_cb cb);

#endif /* DOOR_LOCK_H */
