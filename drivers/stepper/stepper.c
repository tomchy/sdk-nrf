/*
 * Copyright (c) 2022 Tomasz Chyrowicz
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <drivers/stepper.h>
#include <zephyr/device.h>
#include <zephyr/zephyr.h>
#include <zephyr/drivers/gpio.h>
#include <math.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(stepper, CONFIG_STEPPER_LOG_LEVEL);

#include <zephyr/devicetree.h>
#include <drivers/counter.h>

const struct device *timer_device = DEVICE_DT_GET(DT_NODELABEL(timer2));

static void stepper_timer_handler(const struct device *dev);

/* Timer interrupt handler. */
static void timer_alarm_handler(const struct device *counter_dev,
				   uint8_t chan_id,
				   uint32_t ticks, void *user_data)
{
	struct k_work * workq = (struct k_work *)user_data;
	k_work_submit(workq);
}



#define INTERVAL_TO_S(x) ((double)x / 1000000.0)
#define S_TO_INTERVAL(x) ((uint32_t)(x * 1000000.0 + 0.5))

#define DISTANCE_TO_M(x) ((double)x / 1000.0)
#define M_TO_DISTANCE(x) ((int32_t)(x * 1000.0))

struct stepper_config {
	const struct gpio_dt_spec dir;
	const struct gpio_dt_spec step;
	const struct gpio_dt_spec m1;
	const struct gpio_dt_spec m2;
	const struct gpio_dt_spec nstby;
	const struct device *timer;

	/** Linear travel distance per single motor step [mm/step]. */
	int32_t distance_per_step;
	/** Maximum allowed linear acceleration [m/second^2]. */
	double max_acceleration;
	/** Maximum allowed linear velocity [m/second]. */
	double max_velocity;
	/** Channel ID to be used. */
	uint8_t chan_id;
};

enum move_phase {
	STOP = 0,
	ACCELERATE,
	MOVE,
	DECELERATE,
};

struct stepper_data {
	enum move_phase current_phase;
	uint32_t current_interval; /* [ns] */
	stepper_path_t * path; /* [mm] */
	stepper_point_t position; /* [mm] */
	bool forward;
	bool half_step;
	struct counter_alarm_cfg alarm_cfg;
	double current_velocity;
};

static int stepper_init(const struct device *dev)
{
	const struct stepper_config *config = dev->config;
	int err = 0;
	const struct gpio_dt_spec *gpios[] = {
		&config->dir,
		&config->step,
		&config->m1,
		&config->m2,
		&config->nstby,
	};

	LOG_DBG("Initialize stepper driver GPIOs");

	for (size_t i = 0; (i < 5) && !err; i++) {
		if (device_is_ready(gpios[i]->port)) {
			err = gpio_pin_configure_dt(gpios[i], GPIO_OUTPUT_ACTIVE);
			if (err) {
				LOG_ERR("Cannot configure GPIO pin (err %d)", err);
			}
		} else {
			LOG_ERR("%s: GPIO pin device not ready", dev->name);
			err = -ENODEV;
		}
		if (err) {
			LOG_ERR("%s: GPIO pin device not ready", dev->name);
			break;
		}
	}

	LOG_DBG("Configure stepper driver microstepping mode");

	// Configure the microstepping mode
	gpio_pin_set_dt(&config->m1, 0);
	gpio_pin_set_dt(&config->m2, 0);

	// Enter stanby state
	gpio_pin_set_dt(&config->nstby, 0);

	if (device_is_ready(timer_device), "Timer device not ready") {
		counter_start(timer_device);
	}

	LOG_DBG("Stepper driver initialized");

	return err;
}

static int set_position(const struct device *dev, stepper_point_t *position)
{
	struct stepper_data *data = dev->data;

	if (position == NULL) {
		return -1;
	}

	data->position.x = position->x;
	data->position.cb = NULL;

	return 0;
}

static int set_next_coord(const struct device *dev, stepper_path_t *coord)
{
	struct stepper_data *data = dev->data;
	stepper_path_t * last_step = data->path;

	if (data->path == coord) {
		return -1;
	}

	if (last_step == NULL) {
		coord->next = NULL;
		data->path = coord;

		if (data->current_interval == 0) {
			struct k_work * workq = (struct k_work *)data->alarm_cfg.user_data;
			k_work_submit(workq);
		}
	} else {
		while (last_step->next != NULL) {
			last_step = last_step->next;
			/* Avoid loops */
			if (last_step == coord) {
				return -1;
			}
		}

		coord->next = NULL;
		last_step->next = coord;
	}

	return 0;
}

/* [mm/ms] == [m/s] */
double get_speed(const struct stepper_config *config, uint32_t interval)
{
	if (interval == 0) {
		return 0.0;
	} else {
		return DISTANCE_TO_M(config->distance_per_step) / INTERVAL_TO_S(interval);
	}
}

/* [mm/ms] == [m/s] */
double get_acc_speed(const struct stepper_config *config, double interval, double v0)
{
	if (interval == 0.0) {
		return 0.0;
	} else {
		return (double)2.0 * (DISTANCE_TO_M(config->distance_per_step) / interval - v0) + v0;
	}
}

/* [ms] */
uint32_t get_next_interval_acc(const struct device *dev, double *next_velocity)
{
	const struct stepper_config *config = dev->config;
	struct stepper_data *data = dev->data;
	double current_speed = data->current_velocity;
	// a * t * t / 2 + v * t – d = 0
	double delta = sqrt(current_speed * current_speed + (double)2.0 * config->max_acceleration * DISTANCE_TO_M(config->distance_per_step));
	double t = (-current_speed + delta) / config->max_acceleration;
	// Round up instead of returning an integer part.
	uint32_t next_interval = S_TO_INTERVAL(t);
	// v’ = (2d - v * t) / (t * t)
	t = INTERVAL_TO_S(next_interval);
	*next_velocity = get_acc_speed(config, t, current_speed);
	LOG_DBG("\tACC: interval: %f", INTERVAL_TO_S(next_interval));

	// Check for maximum allowed velocity.
	if ((next_interval > 0) && (*next_velocity > config->max_velocity)) {
		next_interval = S_TO_INTERVAL(DISTANCE_TO_M(config->distance_per_step) / config->max_velocity);
		*next_velocity = get_speed(config, next_interval);
		LOG_DBG("MAX velocity reached: %d", next_interval);
	}

	return next_interval;
}

/* [ms] */
uint32_t get_next_interval_dec(const struct device *dev, double *next_velocity)
{
	const struct stepper_config *config = dev->config;
	struct stepper_data *data = dev->data;
	double current_speed = data->current_velocity;
	double t;
	uint32_t next_interval;
	// -a * t * t / 2 + v * t – d = 0
	double delta = current_speed * current_speed - (double)2.0 * config->max_acceleration * DISTANCE_TO_M(config->distance_per_step);
	if (delta < 0.0) {
		return 0;
	}
	t = (current_speed - sqrt(delta)) / config->max_acceleration;
	next_interval = S_TO_INTERVAL(t);
	t = INTERVAL_TO_S(next_interval);
	*next_velocity = get_acc_speed(config, t, current_speed);
	LOG_DBG("\tDCC: interval: %f", INTERVAL_TO_S(next_interval));

	// Always decelerate, even if it results in larger deceleration.
	if (next_interval == data->current_interval) {
		next_interval += 1;
		*next_velocity = get_speed(config, next_interval);
	}

	return next_interval;
}

/* [ms] */
uint32_t get_next_interval_lin(const struct device *dev, double *next_velocity)
{
	const struct stepper_config *config = dev->config;
	struct stepper_data *data = dev->data;
	uint32_t interval = S_TO_INTERVAL(DISTANCE_TO_M(config->distance_per_step) / data->current_velocity);
	*next_velocity = get_speed(config, interval);
	return interval;
}

/* [mm] */
int32_t get_brake_distance(const struct device *dev)
{
	const struct stepper_config *config = dev->config;
	struct stepper_data *data = dev->data;

	// s = (v * v) / (2 * a)
	return M_TO_DISTANCE(data->current_velocity * data->current_velocity / (2.0 * config->max_acceleration)) + config->distance_per_step;
}


enum move_phase get_next_phase(const struct device *dev, stepper_point_t next_pos)
{
	const struct stepper_config *config = dev->config;
	struct stepper_data *data = dev->data;

	double current_speed = data->current_velocity;
	int32_t brake_point;
	if (data->forward) {
		brake_point = (int32_t)(next_pos.x - config->distance_per_step - get_brake_distance(dev));
	} else {
		brake_point = (int32_t)(next_pos.x + config->distance_per_step + get_brake_distance(dev));
	}

	LOG_DBG("\tCurrent speed: %f", current_speed);
	LOG_DBG("\tCurrent distance: %d", next_pos.x - data->position.x);
	LOG_DBG("\tCurrent brake point: %d", brake_point);

	if ((data->forward) && (data->position.x < brake_point)) {
		if (data->position.x < brake_point - config->distance_per_step) {
				return ACCELERATE;
		} else {
			return MOVE;
		}
	} else if ((!data->forward) && (data->position.x > brake_point)) {
		if (data->position.x > brake_point + config->distance_per_step) {
				return ACCELERATE;
		} else {
			return MOVE;
		}
	} else if (data->current_interval > 0) {
		return DECELERATE;
	} else {
		return STOP;
	}
}

uint32_t get_next_interval(const struct device *dev, enum move_phase phase, double *next_velocity)
{
	switch (phase) {
		case ACCELERATE:
			return get_next_interval_acc(dev, next_velocity);
		case MOVE:
			return get_next_interval_lin(dev, next_velocity);
		case DECELERATE:
			return get_next_interval_dec(dev, next_velocity);
		case STOP:
		default:
			return 0;
	}
}

static void stepper_update_state(const struct device *dev)
{
	const struct stepper_config *config = dev->config;
	struct stepper_data *data = dev->data;
	double next_velocity;

	/* Update current coordinates. */
	if (data->current_interval != 0) {
		if (data->forward) {
			data->position.x += config->distance_per_step;
		} else {
			data->position.x -= config->distance_per_step;
		}
	}
	LOG_DBG("Current position: x = %d", data->position.x);

	/* Check if the path is not empty. */
	if (data->path == NULL) {
		if (data->current_phase != STOP) {
			LOG_ERR("Emergency stop due to empty path queue!");
			data->current_phase = STOP;
			data->current_interval = 0;
			data->current_velocity = 0.0;
		}

		return;
	}

	bool wpt = false;
	/* Check if waypoint has been reached. */
	if (data->path->p.x == data->position.x) {
		wpt = true;
		if (data->path->p.cb != NULL) {
			data->path->p.cb(data->path->p.x);
		}
		if (data->path->next != NULL) {
			LOG_DBG("Load new waypoint: x = %d", data->path->next->p.x);
			data->path = data->path->next;
		} else {
			LOG_DBG("Last waypoint reached: x = %d", data->path->p.x);
		}
	}

	/* Navigate to the next point. */
	if (data->current_interval == 0) {
		if (data->path->p.x > data->position.x) {
			data->forward = true;
		} else {
			data->forward = false;
		}
	}

	data->current_phase = get_next_phase(dev, data->path->p);
	if (data->path->next != NULL) {
		if (data->forward) {
			if (data->path->p.x < data->path->next->p.x) {
				data->current_phase = get_next_phase(dev, data->path->next->p);
			}
		} else {
			if (data->path->p.x > data->path->next->p.x) {
				data->current_phase = get_next_phase(dev, data->path->next->p);
			}
		}
	}

	if ((get_next_interval(dev, data->current_phase, &next_velocity) != 0) || (wpt)) {
		data->current_interval = get_next_interval(dev, data->current_phase, &data->current_velocity);
	} else if ((data->forward) && (data->path->p.x > data->position.x)) {
		data->current_interval++;
		data->current_velocity = get_speed(config, data->current_interval);
	} else if ((!data->forward) && (data->path->p.x < data->position.x)) {
		data->current_interval++;
		data->current_velocity = get_speed(config, data->current_interval);
	} else {
		data->current_interval = 0;
		data->current_velocity = get_speed(config, data->current_interval);
	}

	LOG_DBG("Current steering: forward = %d, interval = %d, phase = %d", data->forward, data->current_interval, data->current_phase);

	/* If calculations returned stop state - pop item from the path. */
	if (data->current_phase == STOP) {
		LOG_DBG("Enter STOP state");
		data->current_interval = 0;
		data->current_velocity = 0;
		if (data->path != NULL) {
			data->path = data->path->next;
		}
	}

	/* If stopped - run again, there may be another waypoint to be processed. */
	if (data->current_interval == 0) {
		stepper_update_state(dev);
	}
}

static void stepper_timer_start(const struct device *dev)
{
	const struct stepper_config *config = dev->config;
	struct stepper_data *data = dev->data;

	if (data->current_interval > 0) {
		LOG_DBG("next interval: %d", data->current_interval);
		gpio_pin_set_dt(&config->nstby, 1);
		data->alarm_cfg.ticks = counter_us_to_ticks(timer_device, data->current_interval);
		counter_set_channel_alarm(timer_device, config->chan_id, &data->alarm_cfg);
	} else {
		LOG_DBG("Stop timer loop");
		gpio_pin_set_dt(&config->nstby, 0);
	}
}

static void stepper_timer_handler(const struct device *dev)
{
	const struct stepper_config *config = dev->config;
	struct stepper_data *data = dev->data;

	LOG_DBG("Stepper timer handler (dev: %p)", dev);

	if (data->current_interval == 0) {
		stepper_update_state(dev);
	} else {
		if (data->half_step == false) {
			stepper_update_state(dev);
		}

		if (data->half_step) {
			gpio_pin_set_dt(&config->step, 0);
			data->half_step = false;
		} else {
			gpio_pin_set_dt(&config->step, 1);
			data->half_step = true;
		}

		if (data->forward) {
			gpio_pin_set_dt(&config->dir, 1);
		} else {
			gpio_pin_set_dt(&config->dir, 0);
		}
	}

	stepper_timer_start(dev);
}

static const struct stepper_driver_api stepper_api = {
	.set_next_coord = set_next_coord,
	.set_position = set_position,
};

#ifdef CONFIG_STEPPER
#define STEPPER_DEVICE(node_id)					\
static void process_##node_id##_timer_interval(struct k_work *item)	\
{	\
	const struct device * dev = DEVICE_DT_GET(node_id);	\
	stepper_timer_handler(dev);	\
}	\
K_WORK_DEFINE(work_##node_id, process_##node_id##_timer_interval);	\
const struct stepper_config stepper_config_##node_id = {	\
	.dir = GPIO_DT_SPEC_GET(node_id, dir_gpios),				\
	.step = GPIO_DT_SPEC_GET(node_id, step_gpios),				\
	.m1 = GPIO_DT_SPEC_GET(node_id, m1_gpios),				\
	.m2 = GPIO_DT_SPEC_GET(node_id, m2_gpios),				\
	.nstby = GPIO_DT_SPEC_GET(node_id, nstby_gpios),				\
	.timer = DEVICE_DT_GET(DT_PHANDLE(node_id, counter_timer)),				\
	.distance_per_step = DT_PROP(node_id, distance_per_step_mm), \
	.max_acceleration = (double)(DT_PROP(node_id, max_acceleration)) / 1000.0,	       \
	.max_velocity = (double)(DT_PROP(node_id, max_velocity)) / 1000.0,	       \
	.chan_id = timer_channel_##node_id,	\
};\
								\
static struct stepper_data stepper_data_##node_id = {	\
	.current_phase = STOP,	\
	.current_interval = 0,	\
	.current_velocity = 0.0,	\
	.path = NULL,	\
	.position = {	\
		.x = 0,	\
        .cb = NULL, \
	},	\
	.forward = true,	\
	.half_step = false,	\
	.alarm_cfg = {	\
		.flags = 0,	\
		.ticks = 0,	\
		.callback = timer_alarm_handler,	\
		.user_data = (void *)&work_##node_id,	\
	},	\
};	\
								\
DEVICE_DT_DEFINE(node_id, &stepper_init, NULL,			\
		      &stepper_data_##node_id, &stepper_config_##node_id,		\
		      POST_KERNEL, CONFIG_STEPPER_INIT_PRIORITY,	\
		      &stepper_api);

#define STEPPER_DEVICE_CHANNEL_ID(node_id) timer_channel_##node_id,
enum stepper_channel_ids {
	DT_FOREACH_STATUS_OKAY(zephyr_stepper, STEPPER_DEVICE_CHANNEL_ID)
};

DT_FOREACH_STATUS_OKAY(zephyr_stepper, STEPPER_DEVICE)

#endif
