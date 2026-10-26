#include "ZephyrEpever.h"

#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>

#include <helpers/ModbusRtu.h>
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

#define FC_READ_INPUT     0x04
#define RX_TIMEOUT_MS     500
/* Silence before each request, as EpView: well above the 3.5-character gap. */
#define FRAME_GAP_MS      20
#define FIRST_POLL_DELAY  K_SECONDS(5)
#define RX_BUF_LEN        64
#define RAW_KEEP          24

#define POLL_STACK_SIZE   1024

static const struct device *const s_uart = DEVICE_DT_GET(EPEVER_UART);
static K_THREAD_STACK_DEFINE(s_stack, POLL_STACK_SIZE);
static struct k_thread s_thread;
static K_MUTEX_DEFINE(s_lock);
static K_SEM_DEFINE(s_rx_sem, 0, 1);
static uint8_t s_rx[RX_BUF_LEN];
static volatile size_t s_rx_len;

static struct epever_reading s_reading;
static int64_t s_stamp_ms;
static bool s_valid;
static int s_init_rc = -EAGAIN;
static uint32_t s_polls;
static uint32_t s_answers;
static int s_last_rc;
static int64_t s_last_poll_ms;
static uint8_t s_last_raw[RAW_KEEP];
static size_t s_last_raw_len;

static void uart_isr(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	uart_irq_update(dev);
	while (uart_irq_rx_ready(dev) > 0) {
		uint8_t tmp[16];
		int n = uart_fifo_read(dev, tmp, sizeof(tmp));

		if (n <= 0) {
			break;
		}
		for (int i = 0; i < n && s_rx_len < RX_BUF_LEN; i++) {
			s_rx[s_rx_len++] = tmp[i];
		}
	}
	k_sem_give(&s_rx_sem);
}

/* Bus active: no light sleep (an ESP32 UART stops in it), on nRF the UARTE
 * resumed, and the receiver on. Idle: the reverse. */
static void bus_active(bool on)
{
	if (on) {
		zc_pm_block_sleep();
	} else {
		uart_irq_rx_disable(s_uart);
	}
#if EPEVER_UART_PM
	int rc = pm_device_action_run(s_uart, on ? PM_DEVICE_ACTION_RESUME
						 : PM_DEVICE_ACTION_SUSPEND);

	if (rc != 0 && rc != -EALREADY) {
		LOG_WRN("UART %s failed: %d", on ? "resume" : "suspend", rc);
	}
#endif
	if (on) {
		uart_irq_rx_enable(s_uart);
	} else {
		zc_pm_unblock_sleep();
	}
}

/* One read of input registers. 0, the exception code (> 0), -ETIMEDOUT when
 * nothing came back, -EBADMSG when bytes came back but no valid answer. The
 * received bytes are kept for `get epever`. */
static int read_input_regs(uint16_t addr, uint16_t *regs, uint16_t count)
{
	uint8_t req[MODBUS_RTU_REQ_LEN];
	int rc = MODBUS_RTU_NOT_FOUND;

	modbus_rtu_read_request(req, CONFIG_ZEPHCORE_EPEVER_UNIT_ID, FC_READ_INPUT, addr, count);
	k_msleep(FRAME_GAP_MS);
	s_rx_len = 0;
	k_sem_reset(&s_rx_sem);
	for (size_t i = 0; i < sizeof(req); i++) {
		uart_poll_out(s_uart, req[i]);
	}

	const int64_t deadline = k_uptime_get() + RX_TIMEOUT_MS;

	while (rc == MODBUS_RTU_NOT_FOUND) {
		rc = modbus_rtu_find_read_answer(s_rx, s_rx_len, CONFIG_ZEPHCORE_EPEVER_UNIT_ID,
						 FC_READ_INPUT, count, regs);
		if (rc != MODBUS_RTU_NOT_FOUND) {
			break;
		}
		int64_t left = deadline - k_uptime_get();

		if (left <= 0) {
			rc = s_rx_len > 0 ? -EBADMSG : -ETIMEDOUT;
			break;
		}
		(void)k_sem_take(&s_rx_sem, K_MSEC(left));
	}

	size_t n = s_rx_len < RAW_KEEP ? s_rx_len : RAW_KEEP;

	k_mutex_lock(&s_lock, K_FOREVER);
	memcpy(s_last_raw, s_rx, n);
	s_last_raw_len = s_rx_len;
	k_mutex_unlock(&s_lock);
	return rc;
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
	/* Let a trailing byte land before the receiver stops. */
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
	if (!device_is_ready(s_uart)) {
		s_init_rc = -ENODEV;
		return s_init_rc;
	}
	s_init_rc = uart_irq_callback_user_data_set(s_uart, uart_isr, NULL);
	if (s_init_rc != 0) {
		LOG_ERR("UART callback: %d", s_init_rc);
		return s_init_rc;
	}
	uart_irq_rx_disable(s_uart);
#if EPEVER_UART_PM
	(void)pm_device_action_run(s_uart, PM_DEVICE_ACTION_SUSPEND);
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
	k_mutex_lock(&s_lock, K_FOREVER);
	int n;

	if (s_init_rc != 0) {
		n = snprintf(buf, len, "> init failed: %d (%s)", s_init_rc, s_uart->name);
	} else if (s_polls == 0) {
		n = snprintf(buf, len, "> %s %u baud unit %d, first poll pending", s_uart->name,
			     (unsigned)DT_PROP(EPEVER_UART, current_speed),
			     CONFIG_ZEPHCORE_EPEVER_UNIT_ID);
	} else {
		n = snprintf(buf, len, "> %s unit %d polls %u answered %u last %d %llds ago",
			     s_uart->name, CONFIG_ZEPHCORE_EPEVER_UNIT_ID, (unsigned)s_polls,
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
		if (n > 0 && (size_t)n < len) {
			n += snprintf(buf + n, len - n, " rx %u:", (unsigned)s_last_raw_len);
		}
		for (size_t i = 0; i < s_last_raw_len && i < RAW_KEEP && n > 0 && (size_t)n < len;
		     i++) {
			n += snprintf(buf + n, len - n, " %02X", s_last_raw[i]);
		}
	}
	k_mutex_unlock(&s_lock);
	return n;
}
