# Epever charge controller telemetry

A repeater or room server can read an Epever Tracer AN / AN G3 MPPT charge controller (for
example the Tracer 1210AN G3) over RS-485 and add its battery and PV values to telemetry. Build
with `boards/common/epever.conf` next to the role:

```bash
# nRF52 repeater, packet lines on USB for meshcoretomqtt
west build -b heltec_t114 zephcore --pristine -- \
  -DEXTRA_CONF_FILE="boards/common/repeater.conf;boards/common/packet_logging.conf;boards/common/epever.conf"

# ESP32-S3 repeater with the WiFi + MQTT uplink
west build -b heltec_wifi_lora32_v3/esp32s3/procpu zephcore --pristine --sysbuild -- \
  -DEXTRA_CONF_FILE="boards/common/repeater.conf;boards/common/repeater_uplink.conf;boards/common/epever.conf"
```

`epever.conf` brings in the board's `epever.overlay`, which says which UART the transceiver is on.
A board without one stops the build with an error.

## What is read

Every `ZEPHCORE_EPEVER_POLL_SEC` (default 60 s) a low-priority thread sends two Modbus RTU
requests (function 0x04, unit `ZEPHCORE_EPEVER_UNIT_ID`, default 1, 115200 8N1):

| Register | Value | Telemetry |
|---|---|---|
| 0x3100 | PV voltage | PV channel, voltage |
| 0x3101 | PV current | PV channel, current |
| 0x3102/0x3103 | PV power (low/high) | PV channel, power (1 W steps) |
| 0x3104 | Battery voltage | battery channel, voltage |
| 0x3105 | Battery charging current | battery channel, current |
| 0x3110 | Battery temperature (signed) | battery channel, temperature |

The two channels follow the I2C sensors: with an SHT40 and an INA3221 fitted, channel 2 is the
SHT40, 3 to 5 the INA3221, 6 the battery and 7 the PV side. Telemetry carries the last reading
only while it is younger than three poll periods; a controller that is missing or not answering
is simply left out, and found again on a later poll. The request follows the existing
permissions: guests get battery and MCU temperature only, as before.

On nRF52 the UART is suspended between polls, so the receiver does not keep the high-frequency
clock running.

## Wiring

Any 3.3 V TTL-to-RS485 module with automatic direction control works (MAX3485 / SP3485 modules
marked "auto direction", or a RAK5802 on WisBlock). A module with separate DE/RE pins needs
`de-gpios` / `re-gpios` added to the `epever_modbus` node in the overlay.

Controller RJ45 (COM port), as in the Tracer AN manual. Check it against your unit's manual:

| RJ45 pin | Signal |
|---|---|
| 1, 2 | +5 V out. **Leave unconnected** |
| 3, 4 | RS-485 B (−) |
| 5, 6 | RS-485 A (+) |
| 7, 8 | GND, to the transceiver's GND |

If the controller does not answer, swap A and B first.

| Board | UART | TX → module DI | RX ← module RO | Notes |
|---|---|---|---|---|
| Heltec T114 | uart1 | P0.28 | P0.29 | Header pins; the GPS keeps uart0 |
| RAK4631 | uart1 | P0.16 (TX1) | P0.15 (RX1) | WisBlock TXD1/RXD1, RX1/TX1 on the RAK19007 header or a RAK5802 in the IO slot. Shared with the GPS slot, so the build has no GPS. Power the module from 3V3_S |
| RAK3401 1W | uart0 | P0.16 (TX1) | P0.15 (RX1) | As the RAK4631: no GPS in this build |
| Heltec V3 | uart2 | GPIO40 | GPIO41 | Header pins; the GPS keeps uart1 (47/48) |
| Heltec V4 | uart2 | GPIO40 | GPIO41 | Header pins; the GPS keeps uart1 (38/39) |

## External I2C sensors on the Heltec V3 and V4

On these boards the I2C bus of the OLED (GPIO17/18) is not on the pin header. Add
`boards/common/sensors_i2c1.conf` to the build and the sensor list moves to a second bus,
i2c1, at 100 kHz: **GPIO6 = SDA, GPIO4 = SCL**, sensors powered from 3V3. The bus needs
pull-ups to 3.3 V; most SHT40 and INA3221 breakouts carry them, otherwise fit 4.7 kΩ on each
line. GPIO7 is not usable on the V4 (it powers the PA/FEM).

```bash
west build -b heltec_wifi_lora32_v3/esp32s3/procpu zephcore --pristine --sysbuild -- \
  -DEXTRA_CONF_FILE="boards/common/repeater.conf;boards/common/repeater_uplink.conf;boards/common/epever.conf;boards/common/sensors_i2c1.conf"
```

## Troubleshooting

Over USB, `get epever` shows whether the Modbus client started and what the polls returned, and
`get sensors` which I2C parts were found at boot and whether they read now. A poll is two short
frames every `ZEPHCORE_EPEVER_POLL_SEC`, the first 5 s after boot.

## INA3221

`boards/common/sensors-i2c.dtsi` declares an INA3221 at 0x41 (A0 strapped to VS) with 0.1 Ω
shunts on all three channels. 0x40 belongs to the INA219 node, whose driver writes to the part
without identifying it first, so a breakout left at its 0x40 default must be re-strapped. Each
channel is its own telemetry channel, in channel order.
