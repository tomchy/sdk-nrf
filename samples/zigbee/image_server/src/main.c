/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 * @brief Zigbee Image Server.
 */

#include <zephyr.h>
#include <device.h>
#include <logging/log.h>
#include <dk_buttons_and_leds.h>
#include <ram_pwrdn.h>

#include <zboss_api.h>
#include <zboss_api_addons.h>
#include <zigbee/zigbee_app_utils.h>
#include <zigbee/zigbee_error_handler.h>
#include <zb_nrf_platform.h>
#include "zb_mem_config_custom.h"
#include "ota_upgrade_server.h"

#if CONFIG_ZIGBEE_FOTA
#include <zigbee/zigbee_fota.h>
#include <power/reboot.h>
#include <dfu/mcuboot.h>

/* LED indicating OTA Client Activity. */
#define OTA_ACTIVITY_LED          DK_LED2
#endif /* CONFIG_ZIGBEE_FOTA */

/* Source endpoint used to share image data. */
#define IMAGE_SERVER_ENDPOINT      2

/* Do not erase NVRAM to save the network parameters after device reboot or
 * power-off. NOTE: If this option is set to ZB_TRUE then do full device erase
 * for all network devices before running other samples.
 */
#define ERASE_PERSISTENT_CONFIG    ZB_FALSE
/* LED indicating that image server successfully joind Zigbee network. */
#define ZIGBEE_NETWORK_STATE_LED   DK_LED3
/* LED indicating that image server shares an image. */
#define IMAGE_PRESENT_LED          DK_LED4
/* Button ID used to insert image. */
#define BUTTON_INSERT              DK_BTN1_MSK
/* Button ID used to remove image. */
#define BUTTON_REMOVE              DK_BTN2_MSK
/* Dim step size - increases/decreses current level (range 0x000 - 0xfe). */
#define DIMM_STEP                  15
/* Button ID used to enable sleepy behavior. */
#define BUTTON_SLEEPY              DK_BTN3_MSK

/* Transition time for a single step operation in 0.1 sec units.
 * 0xFFFF - immediate change.
 */
#define DIMM_TRANSACTION_TIME      2

/* Time after which the button state is checked again to detect button hold,
 * the dimm command is sent again.
 */
#define BUTTON_LONG_POLL_TMO       K_MSEC(500)

#define NUMBER_OF_IMAGES           1
#define IMAGE_SEND_CURRENT_TIME    0x00000000  /**< Server does not support Time cluster, use IMAGE_SEND_ADVERTISE_TIME as delay value. */
#define IMAGE_SEND_ADVERTISE_TIME  0x00000000  /**< If IMAGE_SEND_CURRENT_TIME set to zero, use this value as image transfer delay in seconds. */

#if !defined ZB_ED_ROLE
#error Define ZB_ED_ROLE to compile image server (End Device) source code.
#endif

LOG_MODULE_REGISTER(app);

typedef struct
{
	zb_uint8_t zcl_version;
	zb_uint8_t power_source;
} image_server_basic_attr_t;

typedef struct
{
	zb_uint8_t  query_jitter;
	zb_uint32_t current_time;
} image_server_ota_upgrade_attr_t;

typedef struct
{
	image_server_basic_attr_t       basic_attr;
	image_server_ota_upgrade_attr_t ota_attr;
} image_server_ctx_t;

/* Define sample image data */
typedef ZB_PACKED_PRE struct ota_upgrade_test_file_s
{
	zb_zcl_ota_upgrade_file_header_t head;
	zb_uint8_t image[16];
} ZB_PACKED_STRUCT image_file_t;

static image_file_t * image_file = NULL;
static bool image_file_inserted = false;


static image_file_t sample_image =
{
	{
		ZB_ZCL_OTA_UPGRADE_FILE_HEADER_FILE_ID,      // OTA upgrade file identifier
		ZB_ZCL_OTA_UPGRADE_FILE_HEADER_FILE_VERSION, // OTA Header version
		sizeof(zb_zcl_ota_upgrade_file_header_t),    // OTA Header length (includeing optional fields)
		0x00,                                        // OTA Header Field control (no optional fields)
		CONFIG_ZIGBEE_FOTA_MANUFACTURER_ID,          // Manufacturer code
		0xFFC3,                                      // Image type - picture
		1,                                           // File version
		ZB_ZCL_OTA_UPGRADE_FILE_HEADER_STACK_PRO,    // Zigbee Stack version
		// OTA Header string. human readable, 32-bytes long, null-terminated
		{
			'P', 'i', 'c', 't', 'u', 'r', 'e', ' ',   'd', 'a', 't', 'a', '\0', 0x0, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,   0x0, 0x0, 0x0, 0x0, 0x00, 0x0, 0x0, 0x0
		},
		sizeof(image_file_t),                        // Total Image size (including header)
	},
	{
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
	}
};

/********************* Declare attributes **************************/

static image_server_ctx_t dev_ctx;

ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST(basic_attr_list,
				 &dev_ctx.basic_attr.zcl_version,
				 &dev_ctx.basic_attr.power_source);

ZB_ZCL_DECLARE_OTA_UPGRADE_ATTRIB_LIST_SERVER(image_ota_upgrade_attr_list,
					      &dev_ctx.ota_attr.query_jitter,
					      &dev_ctx.ota_attr.current_time,
					      NUMBER_OF_IMAGES);

/********************* Declare device **************************/

ZB_HA_DECLARE_OTA_UPGRADE_SERVER_CLUSTER_LIST(image_server_clusters,
					      basic_attr_list,
					      image_ota_upgrade_attr_list);

ZB_HA_DECLARE_OTA_UPGRADE_SERVER_EP(image_server_ep, IMAGE_SERVER_ENDPOINT, image_server_clusters);


/* Declare application's device context (list of registered endpoints)
 * for Image Server device.
 */
#ifndef CONFIG_ZIGBEE_FOTA
ZBOSS_DECLARE_DEVICE_CTX_1_EP(image_server_ctx, image_server_ep);
#else

#if IMAGE_SERVER_ENDPOINT == CONFIG_ZIGBEE_FOTA_ENDPOINT
	#error "Image server and Zigbee OTA endpoints should be different."
#endif

extern zb_af_endpoint_desc_t zigbee_fota_client_ep;
ZBOSS_DECLARE_DEVICE_CTX_2_EP(image_server_ctx,
			      zigbee_fota_client_ep,
			      image_server_ep);
#endif /* CONFIG_ZIGBEE_FOTA */


/**@brief Function for initializing all clusters attributes.
 */
static void image_server_attr_init(void)
{
	/* Basic cluster attributes data */
	dev_ctx.basic_attr.zcl_version  = ZB_ZCL_VERSION;
	dev_ctx.basic_attr.power_source = ZB_ZCL_BASIC_POWER_SOURCE_UNKNOWN;

	/* OTA cluster attributes data */
	dev_ctx.ota_attr.query_jitter = ZB_ZCL_OTA_UPGRADE_QUERY_JITTER_MAX_VALUE;
	dev_ctx.ota_attr.current_time = IMAGE_SEND_CURRENT_TIME;
}


/**@brief This callback is called on next image block request
 *
 * @param index  file index
 * @param offset current offset of the file
 * @param size   block size
 */
static zb_ret_t next_data_ind_cb(zb_uint8_t index,
				 zb_zcl_parsed_hdr_t* zcl_hdr,
				 zb_uint32_t offset,
				 zb_uint8_t size,
				 zb_uint8_t** data)
{
	ZVUNUSED(index);
	ZVUNUSED(zcl_hdr);
	ZVUNUSED(size);
	*data = ((zb_uint8_t *)&image_file + offset);
	return RET_OK;
}

/**@brief Function for checking if a new image is present at given address.
 *
 * @params[in] ota_file  Pointer to the memory, where image starts.
 *
 * @returns true if a valid image is found, false otherwise.
 */
static bool image_file_sanity_check(image_file_t * ota_file)
{
	if (ota_file->head.file_id != ZB_ZCL_OTA_UPGRADE_FILE_HEADER_FILE_ID)
	{
		return false;
	}
	else
	{
		return true;
	}
}

static void insert_image_file(zb_bufid_t bufid)
{
	zb_ret_t zb_err_code;

	if (!image_file_sanity_check(image_file)) {
		return;
	}

	/* The function assumes that at the UPGRADE_IMAGE_OFFSET address the correct image file can be found */
	ZB_ZCL_OTA_UPGRADE_INSERT_FILE(bufid, IMAGE_SERVER_ENDPOINT, 0, (zb_uint8_t *)(image_file), IMAGE_SEND_ADVERTISE_TIME, ZB_TRUE, zb_err_code);
	ZB_ERROR_CHECK(zb_err_code);
	dk_set_led(IMAGE_PRESENT_LED, 0);
	image_file_inserted = true;
}

static void remove_image_file(zb_bufid_t bufid)
{
	zb_ret_t zb_err_code;

	if (!image_file_inserted)
	{
		return;
	}

	ZB_ZCL_OTA_UPGRADE_REMOVE_FILE(bufid, IMAGE_SERVER_ENDPOINT, 0, zb_err_code);
	ZB_ERROR_CHECK(zb_err_code);
	dk_set_led(IMAGE_PRESENT_LED, 0);
	image_file_inserted = false;
}

/**@brief Callback for button events.
 *
 * @param[in]   button_state  Bitmask containing buttons state.
 * @param[in]   has_changed   Bitmask containing buttons that has
 *                            changed their state.
 */
static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	zb_ret_t zb_err_code;

	/* Inform default signal handler about user input at the device. */
	user_input_indicate();

	switch (has_changed) {
	case BUTTON_INSERT:
		LOG_DBG("Insert - button changed");
		/* Allocate output buffer and advertise new image data. */
		image_file = &sample_image;
		zb_err_code = zb_buf_get_out_delayed(insert_image_file);
		ZB_ERROR_CHECK(zb_err_code);
		break;
	case BUTTON_REMOVE:
		LOG_DBG("Remove - button changed");
		zb_err_code = zb_buf_get_out_delayed(remove_image_file);
		ZB_ERROR_CHECK(zb_err_code);
		break;
	default:
		LOG_DBG("Unhandled button");
		return;
	}
}

/**@brief Function for initializing LEDs and Buttons. */
static void configure_gpio(void)
{
	int err;

	err = dk_buttons_init(button_handler);
	if (err) {
		LOG_ERR("Cannot init buttons (err: %d)", err);
	}

	err = dk_leds_init();
	if (err) {
		LOG_ERR("Cannot init LEDs (err: %d)", err);
	}
}

#ifdef CONFIG_ZIGBEE_FOTA
static void confirm_image(void)
{
	if (!boot_is_img_confirmed()) {
		int ret = boot_write_img_confirmed();

		if (ret) {
			LOG_ERR("Couldn't confirm image: %d", ret);
		} else {
			LOG_INF("Marked image as OK");
		}
	}
}

static void ota_evt_handler(const struct zigbee_fota_evt *evt)
{
	switch (evt->id) {
	case ZIGBEE_FOTA_EVT_PROGRESS:
		dk_set_led(OTA_ACTIVITY_LED, evt->dl.progress % 2);
		break;

	case ZIGBEE_FOTA_EVT_FINISHED:
		LOG_INF("Reboot application.");
		sys_reboot(SYS_REBOOT_COLD);
		break;

	case ZIGBEE_FOTA_EVT_ERROR:
		LOG_ERR("OTA image transfer failed.");
		break;

	default:
		break;
	}
}
#endif /* CONFIG_ZIGBEE_FOTA */

/**@brief Zigbee stack event handler.
 *
 * @param[in]   bufid   Reference to the Zigbee stack buffer
 *                      used to pass signal.
 */
void zboss_signal_handler(zb_bufid_t bufid)
{
	/* Update network status LED. */
	zigbee_led_status_update(bufid, ZIGBEE_NETWORK_STATE_LED);

#ifdef CONFIG_ZIGBEE_FOTA
	/* Pass signal to the OTA client implementation. */
	zigbee_fota_signal_handler(bufid);
#endif /* CONFIG_ZIGBEE_FOTA */

	/* Call default signal handler. */
	ZB_ERROR_CHECK(zigbee_default_signal_handler(bufid));

	if (bufid) {
		zb_buf_free(bufid);
	}
}


void main(void)
{
	LOG_INF("Starting Zigbee Image Server example");

	/* Initialize. */
	configure_gpio();

	zigbee_erase_persistent_storage(ERASE_PERSISTENT_CONFIG);
	zb_set_ed_timeout(ED_AGING_TIMEOUT_64MIN);
	zb_set_keepalive_timeout(ZB_MILLISECONDS_TO_BEACON_INTERVAL(3000));

	/* If "sleepy button" is defined, check its state during Zigbee
	 * initialization and enable sleepy behavior at device if defined button
	 * is pressed. Additionally, power off unused sections of RAM to lower
	 * device power consumption.
	 */
#if defined BUTTON_SLEEPY
	if (dk_get_buttons() & BUTTON_SLEEPY) {
		zigbee_configure_sleepy_behavior(true);

		if (IS_ENABLED(CONFIG_RAM_POWER_DOWN_LIBRARY)) {
			power_down_unused_ram();
		}
	}
#endif

#ifdef CONFIG_ZIGBEE_FOTA
	/* Initialize Zigbee FOTA download service. */
	zigbee_fota_init(ota_evt_handler);

	/* Mark the current firmware as valid. */
	confirm_image();

	/* Register callback for handling ZCL commands. */
	ZB_ZCL_REGISTER_DEVICE_CB(zigbee_fota_zcl_cb);
#endif /* CONFIG_ZIGBEE_FOTA */

	/* Register image server device context (endpoints). */
	ZB_AF_REGISTER_DEVICE_CTX(&image_server_ctx);

	image_server_attr_init();
	zb_zcl_ota_upgrade_init_server(IMAGE_SERVER_ENDPOINT, next_data_ind_cb);

	/* Start Zigbee default thread. */
	zigbee_enable();

	LOG_INF("Zigbee Image Server example started");

	while (1) {
		k_sleep(K_FOREVER);
	}
}
