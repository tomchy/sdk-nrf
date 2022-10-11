/*
 * Copyright (c) 2022 Tomasz Chyrowicz
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public stepper motor driver APIs
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_STEPPER_H_
#define ZEPHYR_INCLUDE_DRIVERS_STEPPER_H_

/**
 * @brief Stepper motor driver Interface
 * @defgroup stepper_interface Stepper motor driver Interface
 * @ingroup io_interfaces
 * @{
 */

#include <errno.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/toolchain.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Container for stepper motor driver information specified in devicetree.
 *
 * This type contains a pointer to a stepper driver device, travel distance of
 * a single step, maximum allowed acceleration and velocity.
 *
 * @see STEPPER_DT_SPEC_GET
 */
struct stepper_dt_spec {
	/** Stepper motor device instance. */
	const struct device *dev;
    /** Linear travel distance per single motor step [mm/step]. */
    uint32_t distance_per_step;
    /** Maximum allowed linear acceleration [m/second^2]. */
    double max_acceleration;
    /** Maximum allowed linear velocity [m/second]. */
    double max_velocity;
};

/**@brief Prototype of a function, that may be called once the waypoint is reached. */
typedef void (*point_cb_t)(int32_t x);

/**@brief Structure describing a waypoint on the steper driver path. */
typedef struct {
	int32_t x;
	point_cb_t cb;
} stepper_point_t;

/**@brief Structure describing the steper driver path. */
typedef struct path_s {
	struct path_s * next;
	stepper_point_t p;
} stepper_path_t;


/**
 * @brief Static initializer for a struct stepper_dt_spec
 *
 * This returns a static initializer for a struct stepper_dt_spec given a devicetree
 * node identifier.
 *
 * Example devicetree fragment:
 *
 * @code{.dts}
 *    stepper1: stepper_1 {
 *        compatible = "zephyr,stepper";
 *        status = "okay";
 *        dir-gpios = <&gpio1 10 GPIO_ACTIVE_HIGH>;
 *        step-gpios = <&gpio1 11 GPIO_ACTIVE_HIGH>;
 *        m1-gpios = <&gpio1 12 GPIO_ACTIVE_HIGH>;
 *        nstby-gpios = <&gpio1 13 GPIO_ACTIVE_HIGH>;
 *        distance-per-step-mm = <1>;
 *        max-acceleration = <50>;
 *        max-velocity = <200>;
 *    };
 * @endcode
 *
 * Example usage:
 *
 * @code{.c}
 *    const struct stepper_dt_spec spec = STEPPER_DT_SPEC_GET(DT_NODELABEL(n));
 *
 *    // Initializes 'spec' to:
 *    // {
 *    //         .dev = DEVICE_DT_GET(DT_NODELABEL(stepper1)),
 *    //         .distance_per_step = 1,
 *    //         .max_acceleration = 0.05,
 *    //         .max_velocity = 0.2,
 *    // }
 * @endcode
 *
 * The device (dev) must still be checked for readiness, e.g. using
 * device_is_ready(). It is an error to use this macro unless the node exists,
 * and it specifies a distance, a maximum acceleration and velocity properties.
 *
 * @param node_id Devicetree node identifier.
 * @param name Lowercase-and-underscores name of a steppers element as defined by
 *             the node's stepper-names property.
 *
 * @return Static initializer for a struct stepper_dt_spec for the property.
 *
 * @see STEPPER_DT_SPEC_INST_GET_BY_NAME
 */
#define STEPPER_DT_SPEC_GET(node_id)    \
	{								       \
		.dev = DEVICE_DT_GET(node_id),       \
		.distance_per_step = DT_PROP(node_id, distance_per_step_mm), \
		.max_acceleration = (double)(DT_PROP(node_id, max_acceleration)) / 1000.0,	       \
		.max_velocity = (double)(DT_PROP(node_id, max_velocity)) / 1000.0,	       \
	}

/** @cond INTERNAL_HIDDEN */
/**
 * @brief STEPPER driver API call to add the next waypoint to the current path.
 * @see stepper_set_next_coord() for argument description
 */
typedef int (*stepper_api_set_next_coord_t)(const struct device *dev, stepper_path_t *coord);

/**
 * @brief STEPPER driver API call to set the current driver position.
 * @see stepper_set_position() for argument description
 */
typedef int (*stepper_api_set_position_t)(const struct device *dev, stepper_point_t *position);

/** @brief Stepper motor driver API definition. */
__subsystem struct stepper_driver_api {
	stepper_api_set_next_coord_t set_next_coord;
	stepper_api_set_position_t set_position;
};
/** @endcond */

/**
 * @brief Add the next waypoint to the current stepper driver path.
 *
 * @param dev STEPPER device instance.
 * @param coord STEPPER pointer to the structure, describing the waypoint.
 *
 * @retval 0 If successful.
 * @retval -errno Negative errno code on failure.
 */

__syscall int stepper_set_next_coord(const struct device *dev, stepper_path_t *coord);

static inline int z_impl_stepper_set_next_coord(const struct device *dev, stepper_path_t *coord)
{
	const struct stepper_driver_api *api =
		(const struct stepper_driver_api *)dev->api;

	if (api->set_next_coord == NULL) {
		return -ENOSYS;
	}
	return api->set_next_coord(dev, coord);
}


/**
 * @brief Set the current position.
 *
 * @param dev stepper device instance.
 * @param coord pointer to the structure, describing the current position.
 *
 * @retval 0 If successful.
 * @retval -errno Negative errno code on failure.
 */

__syscall int stepper_set_position(const struct device *dev, stepper_point_t *position);

static inline int z_impl_stepper_set_position(const struct device *dev, stepper_point_t *position)
{
	const struct stepper_driver_api *api =
		(const struct stepper_driver_api *)dev->api;

	if (api->set_position == NULL) {
		return -ENOSYS;
	}
	return api->set_position(dev, position);
}

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#include <syscalls/stepper.h>

#endif /* ZEPHYR_INCLUDE_DRIVERS_STEPPER_H_ */
