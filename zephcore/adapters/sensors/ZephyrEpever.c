/*
 * SPDX-License-Identifier: MIT
 * See ZephyrEpever.h; register map in helpers/EpeverRegisters.h.
 */

#include "ZephyrEpever.h"

#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/modbus/modbus.h>
#include <zephyr/pm/device.h>

#include <helpers/pm_sleep_guard.h>

LOG_MODULE_REGISTER(zephcore_epever, CONFIG_ZEPHCORE_SENSORS_LOG_LEVEL);

#define EPEVER_NODE DT_NODELABEL(epever_modbus)
#define EPEVER_UART DT_BUS(EPEVER_NODE)

/* nRF UARTE keeps its receiver (and HFCLK) running while enabled: suspend it
 * between polls. */
#if defined(CONFIG_PM_DEVICE) && DT_NODE_HAS_COMPAT(EPEVER_UART, nordic_nrf_uarte)
#define EPEVER_UART_PM 1
#else
#define EPEVER_UART_PM 0
#endif

#define RX_TIMEOUT_US     500000
/* Silence before each request, as EpView: well above the 3.5-character gap. */
#define FRAME_GAP_MS      20
#define FIRST_POLL_DELAY  K_SECONDS(5)

#define POLL_STACK_SIZE   1024

static K_THREAD_STACK_DEFINE(s_stack, POLL_STACK_SIZE);
static struct k_thread s_thread;
static K_MUTEX_DEFINE(s_lock);
static struct epever_reading s_reading;
static int64_t s_stamp_ms;
static bool s_valid;
static int s_iface = -1;
static int s_init_rc = -EAGAIN;
static uint32_t s_polls;
static uint32_t s_answers;
static int s_last_rc;
static int64_t s_last_poll_ms;

/* Bus active: no light sleep (an ESP32 UART stops in it) and, on nRF, the
 * UARTE resumed. Idle: the reverse. */
static void bus_active(bool on)
{
	if (on) {
		zc_pm_block_sleep();
	}
#if EPEVER_UART_PM
	int rc = pm_device_action_run(DEVICE_DT_GET(EPEVER_UART),
				      on ? PM_DEVICE_ACTION_RESUME : PM_DEVICE_ACTION_SUSPEND);

	if (rc != 0 && rc != -EALREADY) {
		LOG_WRN("UART %s failed: %d", on ? "resume" : "suspend", rc);
	}
#endif
	if (!on) {
		zc_pm_unblock_sleep();
	}
}

static int read_input_regs(uint16_t addr, uint16_t *buf, uint16_t count)
{
	k_msleep(FRAME_GAP_MS);
	return modbus_read_input_regs(s_iface, CONFIG_ZEPHCORE_EPEVER_UNIT_ID, addr, buf, count);
}

static void poll_once(void)
{
	uint16_t pv_block[EPEVER_PV_BLOCK_LEN];
	uint16_t temp;

	bus_active(true);
	int rc = read_input_regs(EPEVER_REG_PV_BLOCK, pv_block, EPEVER_PV_BLOCK_LEN);

	if (rc == 0) {
		rc = read_input_regs(EPEVER_REG_BATT_TEMP, &temp, 1);
	}
	/* Let the driver's RX turnaround settle before the suspend's STOPRX. */
	k_msleep(FRAME_GAP_MS);
	bus_active(false);

	k_mutex_lock(&s_lock, K_FOREVER);
	s_polls++;
	s_last_rc = rc;
	s_last_poll_ms = k_uptime_get();
	if (rc == 0) {
		s_answers++;
		if (!s_valid) {
			LOG_INF("Epever charge controller found (unit %d)",
				CONFIG_ZEPHCORE_EPEVER_UNIT_ID);
		}
		epever_decode(pv_block, temp, &s_reading);
		s_stamp_ms = k_uptime_get();
		s_valid = true;
	} else if (s_valid) {
		LOG_INF("Epever charge controller not answering: %d", rc);
		s_valid = false;
	}
	k_mutex_unlock(&s_lock);
}

static void poll_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);

	for (;;) {
		poll_once();
		k_sleep(K_SECONDS(CONFIG_ZEPHCORE_EPEVER_POLL_SEC));
	}
}

int epever_init(void)
{
	static const char iface_name[] = {DEVICE_DT_NAME(EPEVER_NODE)};
	const struct modbus_iface_param param = {
		.mode = MODBUS_MODE_RTU,
		.rx_timeout = RX_TIMEOUT_US,
		.serial = {
			.baud = DT_PROP(EPEVER_UART, current_speed),
			.parity = UART_CFG_PARITY_NONE,
			.stop_bits = UART_CFG_STOP_BITS_1,
		},
	};

	s_iface = modbus_iface_get_by_name(iface_name);
	if (s_iface < 0) {
		LOG_ERR("Modbus interface %s not found", iface_name);
		s_init_rc = -ENODEV;
		return s_init_rc;
	}
	s_init_rc = modbus_init_client(s_iface, param);
	if (s_init_rc != 0) {
		LOG_ERR("Modbus client init failed: %d", s_init_rc);
		return s_init_rc;
	}
#if EPEVER_UART_PM
	(void)pm_device_action_run(DEVICE_DT_GET(EPEVER_UART), PM_DEVICE_ACTION_SUSPEND);
#endif

	k_thread_create(&s_thread, s_stack, K_THREAD_STACK_SIZEOF(s_stack), poll_thread,
			NULL, NULL, NULL, K_LOWEST_APPLICATION_THREAD_PRIO, 0, FIRST_POLL_DELAY);
	k_thread_name_set(&s_thread, "epever");
	return 0;
}

bool epever_get(struct epever_reading *out)
{
	const int64_t max_age_ms = 3LL * CONFIG_ZEPHCORE_EPEVER_POLL_SEC * MSEC_PER_SEC;
	bool ok;

	k_mutex_lock(&s_lock, K_FOREVER);
	ok = s_valid && (k_uptime_get() - s_stamp_ms) <= max_age_ms;
	if (ok) {
		*out = s_reading;
	}
	k_mutex_unlock(&s_lock);
	return ok;
}

int epever_format_status(char *buf, size_t len)
{
	const struct device *uart = DEVICE_DT_GET(EPEVER_UART);

	k_mutex_lock(&s_lock, K_FOREVER);
	int n;

	if (s_init_rc != 0) {
		n = snprintf(buf, len, "> init failed: %d (%s ready=%d)", s_init_rc, uart->name,
			     device_is_ready(uart) ? 1 : 0);
	} else if (s_polls == 0) {
		n = snprintf(buf, len, "> %s %u baud unit %d, first poll pending", uart->name,
			     (unsigned)DT_PROP(EPEVER_UART, current_speed),
			     CONFIG_ZEPHCORE_EPEVER_UNIT_ID);
	} else {
		n = snprintf(buf, len, "> %s unit %d polls %u answered %u last %d %llds ago",
			     uart->name, CONFIG_ZEPHCORE_EPEVER_UNIT_ID, (unsigned)s_polls,
			     (unsigned)s_answers, s_last_rc,
			     (long long)((k_uptime_get() - s_last_poll_ms) / MSEC_PER_SEC));
		if (s_valid && n > 0 && (size_t)n < len) {
			n += snprintf(buf + n, len - n,
				      " batt %.2fV %.2fA %.1fC pv %.2fV %.2fA %.0fW",
				      (double)s_reading.batt_voltage_v,
				      (double)s_reading.batt_charge_current_a,
				      (double)s_reading.batt_temperature_c,
				      (double)s_reading.pv_voltage_v,
				      (double)s_reading.pv_current_a,
				      (double)s_reading.pv_power_w);
		}
	}
	k_mutex_unlock(&s_lock);
	return n;
}
