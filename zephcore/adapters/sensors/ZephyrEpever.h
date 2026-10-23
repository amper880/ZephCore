/*
 * SPDX-License-Identifier: MIT
 * Epever Tracer AN / AN G3 solar charge controller over Modbus RTU (RS-485).
 *
 * Present when the devicetree has a zephyr,modbus-serial node labelled
 * epever_modbus (boards/common/epever.conf + the board's epever.overlay).
 * A low-priority thread reads the controller every
 * CONFIG_ZEPHCORE_EPEVER_POLL_SEC and keeps the last good reading;
 * telemetry reads that copy and never touches the bus.
 */

#pragma once

#include <stdbool.h>
#include <helpers/EpeverRegisters.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Start the poll thread (call once at boot). */
int epever_init(void);

/* The last reading, if the controller answered within the last three poll
 * periods. Any thread. */
bool epever_get(struct epever_reading *out);

#ifdef __cplusplus
}
#endif
