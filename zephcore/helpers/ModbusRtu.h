/*
 * SPDX-License-Identifier: MIT
 * Modbus RTU read-register framing (Modbus over Serial Line V1.02): the
 * request, the CRC, and finding the answer in what the UART received.
 *
 * The answer is looked for at every offset of the received bytes, so a local
 * echo of the request (transceivers that keep their receiver on while
 * driving the bus) or a stray byte from the bus turning around in front of
 * it does not hide it.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MODBUS_RTU_REQ_LEN      8
#define MODBUS_RTU_NOT_FOUND    (-1)

static inline uint16_t modbus_rtu_crc(const uint8_t *p, size_t len)
{
	uint16_t crc = 0xFFFF;

	for (size_t i = 0; i < len; i++) {
		crc ^= p[i];
		for (int b = 0; b < 8; b++) {
			crc = (crc & 1U) ? (uint16_t)((crc >> 1) ^ 0xA001U) : (uint16_t)(crc >> 1);
		}
	}
	return crc;
}

/* Read request (FC 0x03 / 0x04): unit, fc, address, count, CRC low byte first. */
static inline void modbus_rtu_read_request(uint8_t out[MODBUS_RTU_REQ_LEN], uint8_t unit,
					   uint8_t fc, uint16_t addr, uint16_t count)
{
	out[0] = unit;
	out[1] = fc;
	out[2] = (uint8_t)(addr >> 8);
	out[3] = (uint8_t)addr;
	out[4] = (uint8_t)(count >> 8);
	out[5] = (uint8_t)count;
	uint16_t crc = modbus_rtu_crc(out, 6);

	out[6] = (uint8_t)crc;
	out[7] = (uint8_t)(crc >> 8);
}

static inline int modbus_rtu_crc_ok(const uint8_t *p, size_t len)
{
	uint16_t crc = modbus_rtu_crc(p, len - 2);

	return p[len - 2] == (uint8_t)crc && p[len - 1] == (uint8_t)(crc >> 8);
}

/* Looks for the answer to a read of `count` registers in buf[0..len).
 * Returns 0 with regs[] filled, the exception code (> 0) for an exception
 * answer, or MODBUS_RTU_NOT_FOUND while no complete, CRC-valid answer is
 * there. */
static inline int modbus_rtu_find_read_answer(const uint8_t *buf, size_t len, uint8_t unit,
					      uint8_t fc, uint16_t count, uint16_t *regs)
{
	const size_t data_len = (size_t)count * 2U;

	for (size_t off = 0; off + 5 <= len; off++) {
		const uint8_t *p = &buf[off];

		if (p[0] != unit) {
			continue;
		}
		if (p[1] == (uint8_t)(fc | 0x80U) && modbus_rtu_crc_ok(p, 5)) {
			return p[2];
		}
		if (p[1] != fc || p[2] != data_len || off + 5 + data_len > len) {
			continue;
		}
		if (!modbus_rtu_crc_ok(p, 5 + data_len)) {
			continue;
		}
		for (uint16_t i = 0; i < count; i++) {
			regs[i] = (uint16_t)((p[3 + 2 * i] << 8) | p[4 + 2 * i]);
		}
		return 0;
	}
	return MODBUS_RTU_NOT_FOUND;
}

#ifdef __cplusplus
}
#endif
