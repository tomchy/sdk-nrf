/*
 * Copyright (c) 2022 Tomasz Chyrowicz
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/zephyr.h>
#include <drivers/stepper.h>
#include <zephyr/ztest.h>
#include "stepper.c"

struct stepper_config dev_config = {
    /** Linear travel distance per single motor step [mm/step]. */
    .distance_per_step = 1,
    /** Maximum allowed linear acceleration [m/second^2]. */
    .max_acceleration = 0.001,
    /** Maximum allowed linear velocity [m/second]. */
    .max_velocity = 0.01,
};

struct stepper_data dev_data = {
	.current_phase = STOP,
	.current_interval = 0,
	.path = NULL,
	.position = {
		.x = 0,
		.cb = NULL,
	},
	.forward = false,
	.half_step = false,
	.current_velocity = 0.0,
};

struct device dev = {
	.config = &dev_config,
	.data = &dev_data,
};

static double absd(double a, double b)
{
	if (a > b) {
		return a - b;
	} else {
		return b - a;
	}
}

static void test_stepper_acc(void)
{
	const struct stepper_config *config = dev.config;
	struct stepper_data *data = dev.data;

	double speed_before = 0.0;
	double speed_after = 0.0;
	while (speed_after < config->max_velocity) {
		speed_before = data->current_velocity;
		data->current_interval = get_next_interval_acc(&dev, &data->current_velocity);
		speed_after = data->current_velocity;

		zassert_true(speed_before <= config->max_velocity, "Calculated velocity is too high");
		zassert_true(speed_after <= config->max_velocity, "Calculated velocity is too high");
		if (speed_after == speed_before) {
			break;
		}

		double acc = (speed_after - speed_before) / INTERVAL_TO_S(data->current_interval);
		if (acc > config->max_acceleration) {
			double bak = data->current_interval;
			data->current_interval++;
			double t = INTERVAL_TO_S(data->current_interval);
			speed_after = get_acc_speed(config, t, speed_before);
			double acc2 = (speed_after - speed_before)/ INTERVAL_TO_S(data->current_interval);

			data->current_interval = bak;
			if (acc2 != 0.0) {
				zassert_true(absd(acc, config->max_acceleration) < absd(config->max_acceleration, acc2), "The approximation may be better");
			}
		} else if (speed_after != config->max_velocity) {
			double bak = data->current_interval;
			data->current_interval--;
			double t = INTERVAL_TO_S(data->current_interval);
			speed_after = get_acc_speed(config, t, speed_before);
			double acc2 = (speed_after - speed_before) / INTERVAL_TO_S(data->current_interval);

			data->current_interval = bak;
			if (acc2 != 0.0) {
				zassert_true(absd(acc, config->max_acceleration) < absd(config->max_acceleration, acc2), "The approximation may be better");
			}
		}
	}
}

static void test_stepper_dcc(void)
{
	const struct stepper_config *config = dev.config;
	struct stepper_data *data = dev.data;

	double speed_before = 0.0;
	double speed_after = 0.0;
	data->current_interval = S_TO_INTERVAL(0.100564);
	data->current_velocity = get_speed(config, data->current_interval);
	while (speed_after < config->max_velocity) {
		speed_before = data->current_velocity;
		data->current_interval = get_next_interval_dec(&dev, &data->current_velocity);
		speed_after = data->current_velocity;

		zassert_true(speed_before < config->max_velocity, "Calculated velocity is too high");
		if (data->current_interval == 0) {
			break;
		}
		zassert_true(speed_after < config->max_velocity, "Calculated velocity is too high");

		double dcc = -(speed_after - speed_before) / INTERVAL_TO_S(data->current_interval);
		if (dcc > config->max_acceleration) {
			double bak = data->current_interval;
			data->current_interval--;
			double t = INTERVAL_TO_S(data->current_interval);
			speed_after = get_acc_speed(config, t, speed_before);
			double dcc2 = -(speed_after - speed_before) / INTERVAL_TO_S(data->current_interval);

			data->current_interval = bak;
			if (dcc2 != 0.0) {
				zassert_true(absd(dcc, config->max_acceleration) < absd(config->max_acceleration, dcc2), "The approximation may be better");
			}
		} else {
			double bak = data->current_interval;
			data->current_interval++;
			double t = INTERVAL_TO_S(data->current_interval);
			speed_after = get_acc_speed(config, t, speed_before);
			double dcc2 = -(speed_after - speed_before) / INTERVAL_TO_S(data->current_interval);

			data->current_interval = bak;
			if (dcc2 != 0.0) {
				zassert_true(absd(dcc, config->max_acceleration) < absd(config->max_acceleration, dcc2), "The approximation may be better");
			}
		}
	}
}

static void test_stepper_lin(void)
{
	const struct stepper_config *config = dev.config;
	struct stepper_data *data = dev.data;

	double current_speed = 0.0;
	data->current_interval = S_TO_INTERVAL(0.11);
	data->current_velocity = get_speed(config, data->current_interval);
	data->current_interval = S_TO_INTERVAL(DISTANCE_TO_M(config->distance_per_step) / data->current_velocity);
	while ((current_speed < config->max_velocity) && (data->current_interval > 0)) {
		zassert_equal(data->current_interval, get_next_interval_lin(&dev, &data->current_velocity), "Interval has changed in linear movement");
		current_speed = data->current_velocity;

		data->current_interval--;
		data->current_velocity = get_speed(config, data->current_interval);
		data->current_interval = S_TO_INTERVAL(DISTANCE_TO_M(config->distance_per_step) / data->current_velocity);
	}
}

static void test_stepper_brakes(void)
{
	const struct stepper_config *config = dev.config;
	struct stepper_data *data = dev.data;
	static stepper_path_t path = {
		.p = {
			.x = 1000
		},
		.next = NULL
	};
	data->path= &path;
	data->position.x = 0;
	data->current_interval = 0;
	data->current_velocity = 0.0;

	int32_t brake_point = 1000 - get_brake_distance(&dev);	 

	stepper_update_state(&dev);
	while ((data->current_phase != STOP) && (data->position.x < 1000)) {
		if (data->current_phase != DECELERATE) {
			brake_point = 1000 - config->distance_per_step - get_brake_distance(&dev);
		}
		if (brake_point > data->position.x) {
			zassert_true(data->current_phase != DECELERATE, "Stepper is decelerating too early (%d > %d)", brake_point, data->position.x);
		} else {
			zassert_true(data->current_phase != ACCELERATE, "Stepper did not start to decelerate after passing brake point");
		}
		stepper_update_state(&dev);
	}
	zassert_true(data->current_interval == 0, "Stepper did not stop after passing waypoint");
	zassert_true(data->position.x == 1000, "Stepper did not reach waypoint");
	zassert_true(data->path == NULL, "Stepper did not pop waypoint");
}

static void test_stepper_brakes_2points(void)
{
	const struct stepper_config *config = dev.config;
	struct stepper_data *data = dev.data;
	static stepper_path_t path_1 = {
		.p = {
			.x = 200
		},
		.next = NULL
	};
	static stepper_path_t path_0 = {
		.p = {
			.x = 100
		},
		.next = &path_1
	};
	data->path= &path_0;
	data->position.x = 0;
	data->current_interval = 0;
	data->current_velocity = 0.0;

	int32_t brake_point = 200 - get_brake_distance(&dev);

	stepper_update_state(&dev);
	while ((data->current_phase != STOP) && (data->position.x < 200)) {
		if (data->current_phase != DECELERATE) {
			brake_point = 200 - config->distance_per_step - get_brake_distance(&dev);
		}
		if (brake_point > data->position.x) {
			zassert_true(data->current_phase != DECELERATE, "Stepper is decelerating too early (%d > %d)", brake_point, data->position.x);
		} else {
			zassert_true(data->current_phase != ACCELERATE, "Stepper did not start to decelerate after passing brake point");
		}
		stepper_update_state(&dev);
	}
	zassert_true(data->current_interval == 0, "Stepper did not stop after passing waypoint");
	zassert_true(data->position.x == 200, "Stepper did not reach waypoint");
	zassert_true(data->path == NULL, "Stepper did not pop waypoint correctly");
}

static void test_stepper_brakes_revert(void)
{
	const struct stepper_config *config = dev.config;
	struct stepper_data *data = dev.data;
	static stepper_path_t path_1 = {
		.p = {
			.x = 50
		},
		.next = NULL
	};
	static stepper_path_t path_0 = {
		.p = {
			.x = 100
		},
		.next = &path_1
	};
	data->path= &path_0;
	data->position.x = 0;
	data->current_interval = 0;
	data->current_velocity = 0.0;

	int32_t brake_point = 200 - get_brake_distance(&dev);	 

	stepper_update_state(&dev);
	while ((data->current_phase != STOP) && (data->position.x < 100)) {
		if (data->current_phase != DECELERATE) {
			brake_point = 100 - config->distance_per_step - get_brake_distance(&dev);
		}
		if (brake_point > data->position.x) {
			zassert_true(data->current_phase != DECELERATE, "Stepper is decelerating too early (%d > %d)", brake_point, data->position.x);
		} else if (100 != data->position.x) {
			zassert_true(data->current_phase != ACCELERATE, "Stepper did not start to decelerate after passing brake point");
		}
		stepper_update_state(&dev);
	}
	zassert_true(data->position.x == 100, "Stepper did not reach waypoint");
	zassert_true(data->path == &path_1, "Stepper did not pop waypoint correctly");

	while ((data->current_phase != STOP) && (data->position.x > 50)) {
		if (data->current_phase != DECELERATE) {
			brake_point = 50 + config->distance_per_step + get_brake_distance(&dev);
		}
		if (brake_point < data->position.x) {
			zassert_true(data->current_phase != DECELERATE, "Stepper is decelerating too early (%d < %d)", brake_point, data->position.x);
		} else {
			zassert_true(data->current_phase != ACCELERATE, "Stepper did not start to decelerate after passing brake point");
		}
		stepper_update_state(&dev);
	}
	zassert_true(data->current_interval == 0, "Stepper did not stop after passing waypoint");
	zassert_true(data->position.x == 50, "Stepper did not reach waypoint");
	zassert_true(data->path == NULL, "Stepper did not pop waypoint correctly");
}

static void test_negative_stepper_brakes(void)
{
	const struct stepper_config *config = dev.config;
	struct stepper_data *data = dev.data;
	static stepper_path_t path = {
		.p = {
			.x = -1000
		},
		.next = NULL
	};
	data->path= &path;
	data->position.x = 0;
	data->current_interval = 0;
	data->current_velocity = 0.0;

	int32_t brake_point = -1000 + get_brake_distance(&dev);	 

	stepper_update_state(&dev);
	while ((data->current_phase != STOP) && (data->position.x > -1000)) {
		if (data->current_phase != DECELERATE) {
			brake_point = -1000 + config->distance_per_step + get_brake_distance(&dev);
		}
		if (brake_point < data->position.x) {
			zassert_true(data->current_phase != DECELERATE, "Stepper is decelerating too early (%d > %d)", brake_point, data->position.x);
		} else {
			zassert_true(data->current_phase != ACCELERATE, "Stepper did not start to decelerate after passing brake point");
		}
		stepper_update_state(&dev);
	}
	zassert_true(data->current_interval == 0, "Stepper did not stop after passing waypoint");
	zassert_true(data->position.x == -1000, "Stepper did not reach waypoint");
	zassert_true(data->path == NULL, "Stepper did not pop waypoint");
}

void test_main(void)
{
	ztest_test_suite(stepper_test,
			 ztest_unit_test(test_stepper_acc),
			 ztest_unit_test(test_stepper_dcc),
			 ztest_unit_test(test_stepper_lin),
			 ztest_unit_test(test_stepper_brakes),
			 ztest_unit_test(test_stepper_brakes_2points),
			 ztest_unit_test(test_stepper_brakes_revert),
			 ztest_unit_test(test_negative_stepper_brakes)
			);
	ztest_run_test_suite(stepper_test);
}
