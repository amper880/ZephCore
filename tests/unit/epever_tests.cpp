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
