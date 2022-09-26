/*
 * Copyright (c) 2022 Tomasz Chyrowicz
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SFINDER_H
#define SFINDER_H

#include <zboss_api.h>
#include <zcl/zb_zcl_level_control_addons.h>


/* Number chosen for the single endpoint provided by sensor finder */
#define SFINDER_ENDPOINT_NB 45

#define ZB_HA_DEVICE_VER_SFINDER 0  /*!< Sensor finder device version */
#define ZB_HA_CUSTOM_SFINDER_DEVICE_ID 0x0101 /*!< Sensor finder device ID - use dimmable light for smeeze-unaware systems */

/** @cond internal */

#define ZB_HA_SFINDER_IN_CLUSTER_NUM  3  /*!< @internal Sensor finder IN clusters number */
#define ZB_HA_SFINDER_OUT_CLUSTER_NUM 2  /*!< @internal Sensor finder OUT clusters number */

/** @internal @brief Number of clusters for DoorLock HA device. */
#define ZB_HA_SFINDER_CLUSTER_NUM (ZB_HA_SFINDER_IN_CLUSTER_NUM + ZB_HA_SFINDER_OUT_CLUSTER_NUM)


/**
 *  @brief Declare attribute list for Temperature Measurement cluster (client).
 *  @param attr_list - attribute list name.
 */
#define ZB_ZCL_DECLARE_TEMP_MEASUREMENT_CLIENT_ATTRIB_LIST(attr_list)                   \
  ZB_ZCL_START_DECLARE_ATTRIB_LIST_CLUSTER_REVISION(attr_list, ZB_ZCL_TEMP_MEASUREMENT) \
  ZB_ZCL_FINISH_DECLARE_ATTRIB_LIST

/** @internal @brief Number of attribute for reporting on Door Lock device */
#define ZB_HA_SFINDER_REPORT_ATTR_COUNT \
	(ZB_ZCL_ON_OFF_REPORT_ATTR_COUNT + ZB_ZCL_LEVEL_CONTROL_REPORT_ATTR_COUNT)

/** @endcond */

/** @brief Declare cluster list for Door Lock device.
  * @param cluster_list_name - cluster list variable name
  * @param identify_client_attr_list - attribute list for Identify cluster (client role)
  * @param identify_server_attr_list - attribute list for Identify cluster (server role)
  * @param on_off_attr_list - attribute list for On/Off cluster (server role)
  * @param level_control_attr_list - attribute list for Level Control cluster (server role)
  * @param temp_measurement_client_attr_list - attribute list for Identify cluster (client role)
  */
#define ZB_HA_DECLARE_SFINDER_CLUSTER_LIST(                    \
	cluster_list_name,                                         \
	identify_client_attr_list,							\
	identify_server_attr_list,							\
	on_off_attr_list,							   \
	level_control_attr_list,						   \
	temp_measurement_client_attr_list)						\
	zb_zcl_cluster_desc_t cluster_list_name[] =                \
	{                                                          \
		ZB_ZCL_CLUSTER_DESC(								\
			ZB_ZCL_CLUSTER_ID_IDENTIFY,						\
			ZB_ZCL_ARRAY_SIZE(identify_server_attr_list, zb_zcl_attr_t),		\
			(identify_server_attr_list),						\
			ZB_ZCL_CLUSTER_SERVER_ROLE,						\
			ZB_ZCL_MANUF_CODE_INVALID						\
			),									\
		ZB_ZCL_CLUSTER_DESC(						   \
			ZB_ZCL_CLUSTER_ID_ON_OFF,				   \
			ZB_ZCL_ARRAY_SIZE(on_off_attr_list, zb_zcl_attr_t),	   \
			(on_off_attr_list),					   \
			ZB_ZCL_CLUSTER_SERVER_ROLE,				   \
			ZB_ZCL_MANUF_CODE_INVALID				   \
			),								   \
		ZB_ZCL_CLUSTER_DESC(						   \
			ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,			   \
			ZB_ZCL_ARRAY_SIZE(level_control_attr_list, zb_zcl_attr_t), \
			(level_control_attr_list),				   \
			ZB_ZCL_CLUSTER_SERVER_ROLE,				   \
			ZB_ZCL_MANUF_CODE_INVALID				   \
			),								   \
		ZB_ZCL_CLUSTER_DESC(								\
			ZB_ZCL_CLUSTER_ID_IDENTIFY,						\
			ZB_ZCL_ARRAY_SIZE(identify_client_attr_list, zb_zcl_attr_t),		\
			(identify_client_attr_list),						\
			ZB_ZCL_CLUSTER_CLIENT_ROLE,						\
			ZB_ZCL_MANUF_CODE_INVALID						\
			),									\
		ZB_ZCL_CLUSTER_DESC(								\
			ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,					\
			ZB_ZCL_ARRAY_SIZE(temp_measurement_client_attr_list, zb_zcl_attr_t),	\
			(temp_measurement_client_attr_list),					\
			ZB_ZCL_CLUSTER_CLIENT_ROLE,						\
			ZB_ZCL_MANUF_CODE_INVALID						\
			),									\
	}

/** @cond internals_doc */
/** @brief Declare simple descriptor for sensor finder device
    @param ep_name - endpoint variable name
    @param ep_id - endpoint ID
    @param in_clust_num   - number of supported input clusters
    @param out_clust_num  - number of supported output clusters
    @note in_clust_num, out_clust_num should be defined by numeric constants, not variables or any
    definitions, because these values are used to form simple descriptor type name
*/
#define ZB_ZCL_DECLARE_SFINDER_SIMPLE_DESC(ep_name, ep_id, in_clust_num, out_clust_num) \
	ZB_DECLARE_SIMPLE_DESC(in_clust_num, out_clust_num);                  \
	ZB_AF_SIMPLE_DESC_TYPE(in_clust_num, out_clust_num)  simple_desc_##ep_name = \
	{                                                                     \
		ep_id,                                                              \
		ZB_AF_HA_PROFILE_ID,                                                \
		ZB_HA_CUSTOM_SFINDER_DEVICE_ID,                                          \
		ZB_HA_DEVICE_VER_SFINDER,                                         \
		0,                                                                  \
		in_clust_num,                                                       \
		out_clust_num,                                                      \
		{                                                                   \
			ZB_ZCL_CLUSTER_ID_IDENTIFY,                                       \
			ZB_ZCL_CLUSTER_ID_ON_OFF,                                      \
			ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,                                          \
			ZB_ZCL_CLUSTER_ID_IDENTIFY,					\
			ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,					\
		}                                                                   \
	}

/** @endcond */

/** @brief Declare endpoint for sensor finder device
    @param ep_name - endpoint variable name
    @param ep_id - endpoint ID
    @param cluster_list - endpoint cluster list
 */
#define ZB_HA_DECLARE_SFINDER_EP(ep_name, ep_id, cluster_list)                \
	ZB_ZCL_DECLARE_SFINDER_SIMPLE_DESC(	\
		ep_name,	\
		ep_id,	\
		ZB_HA_SFINDER_IN_CLUSTER_NUM,	\
		ZB_HA_SFINDER_OUT_CLUSTER_NUM);	\
	ZBOSS_DEVICE_DECLARE_REPORTING_CTX(	\
		reporting_info## ep_name,	\
		ZB_HA_SFINDER_REPORT_ATTR_COUNT);	\
	ZB_AF_DECLARE_ENDPOINT_DESC(	\
		ep_name,	\
		ep_id,	\
		ZB_AF_HA_PROFILE_ID,	\
		0,	\
		NULL,	\
		ZB_ZCL_ARRAY_SIZE(cluster_list, zb_zcl_cluster_desc_t),	\
		cluster_list,	\
		(zb_af_simple_desc_1_1_t*)&simple_desc_##ep_name,	\
		ZB_HA_SFINDER_REPORT_ATTR_COUNT,	\
		reporting_info## ep_name,	\
		0,	\
		NULL)


struct zb_sfinder_ctx {
	zb_zcl_identify_attrs_t identify_attr;
	zb_zcl_on_off_attrs_t on_off_attr;
	zb_zcl_level_control_attrs_t level_control_attr;
};


/**
 * @brief Initializes sensor finder and read it's state from NVM.
 *
 * @return 0 if success, error code if failure.
 */
int sfinder_init(void);

/**
 * @brief Read the current status of the sensor finder.
 *
 * @retval true   if the temperature sensor is found
 * @retval false  if the temperature sensor is not found
 */
bool sfinder_is_found(void);

/**
 * @brief Mark sensor as found in NVM.
 */
void sfinder_found(zb_ieee_addr_t ieee_addr, zb_uint8_t ep);

/**
 * @brief Remove data about sensor in NVM.
 */
void sfinder_forget(void);

/**
 * @brief Read the stored long address of the sensor.
 */
int sfinder_get_ieee(zb_ieee_addr_t ieee_addr);

/**
 * @brief Read the stored endpoint of the sensor.
 */
int sfinder_get_ep(uint8_t *ep);

#endif /* SFINDER_H */
