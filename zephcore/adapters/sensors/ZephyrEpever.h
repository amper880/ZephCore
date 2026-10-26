/*
 * SPDX-License-Identifier: MIT
 * Epever Tracer AN / AN G3 solar charge controller over Modbus RTU (RS-485).
 *
 * Present when the devicetree has a zephyr,modbus-serial node labelled
 * epever_modbus (boards/common/epever.conf + the board's epever.overlay).
 * A low-priority thread reads the controller every
 * CONFIG_ZEPHCORE_EPEVER_POLL_SEC and keeps the last good reading;
 * telemetry reads that copy and never touches the bus. The requests are
 * framed by helpers/ModbusRtu.h on the UART's interrupt API, which finds the
 * answer behind a local echo of the request.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <helpers/EpeverRegisters.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Start the poll thread (call once at boot). */
int epever_init(void);

/* The last reading, if the controller answered within the last three poll
 * periods; *has_load says whether its load output values are in it (a
 * controller that refuses those registers still reports the rest). Any
 * thread. */
bool epever_get(struct epever_reading *out, bool *has_load);

/* One `get epever` line: init result, poll and answer counts, the last
 * result and its age, and the last reading while it is valid. */
int epever_format_status(char *buf, size_t len);

#ifdef __cplusplus
}
#endif
