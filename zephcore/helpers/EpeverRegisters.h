/*
 * SPDX-License-Identifier: MIT
 * Epever Tracer AN / AN G3 register map and decoding (EPSolar "Controller
 * Protocol V2.5"). Input registers, FC 0x04, values x100; a 32-bit value is
 * a low/high register pair.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 0x3100-0x3107: PV V, PV I, PV P (L, H), battery V, charging I, charging P (L, H) */
#define EPEVER_REG_PV_BLOCK      0x3100
#define EPEVER_PV_BLOCK_LEN      8
/* 0x310C-0x310D: load output V, I */
#define EPEVER_REG_LOAD          0x310C
#define EPEVER_LOAD_LEN          2
/* Battery temperature, signed */
#define EPEVER_REG_BATT_TEMP     0x3110

struct epever_reading {
	float pv_voltage_v;
	float pv_current_a;
	float pv_power_w;
	float batt_voltage_v;
	float batt_charge_current_a;
	float batt_temperature_c;
	float load_voltage_v;
	float load_current_a;
};

static inline void epever_decode(const uint16_t pv_block[EPEVER_PV_BLOCK_LEN],
				 uint16_t batt_temp, struct epever_reading *out)
{
	uint32_t pv_power = (uint32_t)pv_block[2] | ((uint32_t)pv_block[3] << 16);

	out->pv_voltage_v = pv_block[0] / 100.0f;
	out->pv_current_a = pv_block[1] / 100.0f;
	out->pv_power_w = (float)pv_power / 100.0f;
	out->batt_voltage_v = pv_block[4] / 100.0f;
	out->batt_charge_current_a = pv_block[5] / 100.0f;
	out->batt_temperature_c = (int16_t)batt_temp / 100.0f;
}

static inline void epever_decode_load(const uint16_t load[EPEVER_LOAD_LEN],
				      struct epever_reading *out)
{
	out->load_voltage_v = load[0] / 100.0f;
	out->load_current_a = load[1] / 100.0f;
}

#ifdef __cplusplus
}
#endif
