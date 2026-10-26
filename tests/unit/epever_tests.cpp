// helpers/EpeverRegisters.h: Epever Tracer AN register decoding.
#include "test.h"
#include "EpeverRegisters.h"
#include "CayenneLPP.h"

#include <cmath>
#include <vector>

namespace {
bool near(float a, float b) { return std::fabs(a - b) < 0.001f; }
}

TEST(epever_decode_scaling, "UNIT-EPEVER-001", "Registers decode x100, PV power from its low/high pair") {
    // 0x3100..0x3107: 18.52 V, 3.10 A, 70000 (L 0x1170, H 0x0001) = 700.00 W, 13.28 V, 5.25 A, P
    const uint16_t block[EPEVER_PV_BLOCK_LEN] = {1852, 310, 0x1170, 0x0001, 1328, 525, 6972, 0};
    struct epever_reading r;
    epever_decode(block, 2150, &r);
    CHECK(near(r.pv_voltage_v, 18.52f));
    CHECK(near(r.pv_current_a, 3.10f));
    CHECK(near(r.pv_power_w, 700.0f));
    CHECK(near(r.batt_voltage_v, 13.28f));
    CHECK(near(r.batt_charge_current_a, 5.25f));
    CHECK(near(r.batt_temperature_c, 21.5f));
}

TEST(epever_decode_temp_sign, "UNIT-EPEVER-002", "Battery temperature is signed") {
    const uint16_t block[EPEVER_PV_BLOCK_LEN] = {0, 0, 0, 0, 1200, 0, 0, 0};
    struct epever_reading r;
    epever_decode(block, (uint16_t)-725, &r);
    CHECK(near(r.batt_temperature_c, -7.25f));
}

TEST(epever_lpp_fields, "UNIT-EPEVER-003", "Battery and PV fit their LPP fields (0.01 V, 0.001 A, 1 W)") {
    const uint16_t block[EPEVER_PV_BLOCK_LEN] = {3712, 152, 5640, 0, 1305, 431, 0, 0};
    struct epever_reading r;
    epever_decode(block, 1890, &r);
    CayenneLPP l(64);
    CHECK(l.addVoltage(4, r.batt_voltage_v) == 4);
    CHECK(l.addCurrent(4, r.batt_charge_current_a) == 8);
    CHECK(l.addPower(5, r.pv_power_w) == 12);
    // 1305, 4310 mA, 56 W
    CHECK(std::vector<uint8_t>(l.getBuffer(), l.getBuffer() + l.getSize()) ==
          std::vector<uint8_t>({4, 116, 0x05, 0x19, 4, 117, 0x10, 0xD6, 5, 128, 0x00, 0x38}));
}

// helpers/ModbusRtu.h
#include "ModbusRtu.h"

TEST(modbus_request, "UNIT-EPEVER-004", "Read request matches the Modbus spec example CRC") {
    uint8_t req[MODBUS_RTU_REQ_LEN];
    modbus_rtu_read_request(req, 1, 0x03, 0x0000, 10);
    // Read 10 holding registers from unit 1: CRC 0xCDC5, low byte first
    CHECK(std::vector<uint8_t>(req, req + 8) ==
          std::vector<uint8_t>({0x01, 0x03, 0x00, 0x00, 0x00, 0x0A, 0xC5, 0xCD}));
    modbus_rtu_read_request(req, 1, 0x04, 0x3100, 8);
    CHECK(modbus_rtu_crc(req, 8) == 0);
}

namespace {
std::vector<uint8_t> answer(std::vector<uint8_t> body) {
    uint16_t crc = modbus_rtu_crc(body.data(), body.size());
    body.push_back((uint8_t)crc);
    body.push_back((uint8_t)(crc >> 8));
    return body;
}
}

TEST(modbus_find, "UNIT-EPEVER-005", "The answer is found behind an echo of the request and a stray byte") {
    uint8_t req[MODBUS_RTU_REQ_LEN];
    modbus_rtu_read_request(req, 1, 0x04, 0x3110, 1);
    std::vector<uint8_t> rx(req, req + 8);
    rx.push_back(0x00);
    std::vector<uint8_t> a = answer({0x01, 0x04, 0x02, 0x08, 0x66});
    rx.insert(rx.end(), a.begin(), a.end());
    uint16_t reg = 0;
    CHECK(modbus_rtu_find_read_answer(rx.data(), rx.size(), 1, 0x04, 1, &reg) == 0);
    CHECK(reg == 0x0866);
    // Truncated: not yet found
    CHECK(modbus_rtu_find_read_answer(rx.data(), rx.size() - 1, 1, 0x04, 1, &reg) == MODBUS_RTU_NOT_FOUND);
    // Echo alone is not an answer
    CHECK(modbus_rtu_find_read_answer(req, 8, 1, 0x04, 1, &reg) == MODBUS_RTU_NOT_FOUND);
}

TEST(modbus_exception, "UNIT-EPEVER-006", "An exception answer returns its code; a bad CRC is not an answer") {
    std::vector<uint8_t> ex = answer({0x01, 0x84, 0x02});
    uint16_t reg = 0;
    CHECK(modbus_rtu_find_read_answer(ex.data(), ex.size(), 1, 0x04, 1, &reg) == 2);
    std::vector<uint8_t> bad = answer({0x01, 0x04, 0x02, 0x08, 0x66});
    bad.back() ^= 0xFF;
    CHECK(modbus_rtu_find_read_answer(bad.data(), bad.size(), 1, 0x04, 1, &reg) == MODBUS_RTU_NOT_FOUND);
}

TEST(epever_decode_load, "UNIT-EPEVER-007", "Load output registers decode x100") {
    const uint16_t load[EPEVER_LOAD_LEN] = {1328, 47};
    struct epever_reading r = {};
    epever_decode_load(load, &r);
    CHECK(near(r.load_voltage_v, 13.28f));
    CHECK(near(r.load_current_a, 0.47f));
}
