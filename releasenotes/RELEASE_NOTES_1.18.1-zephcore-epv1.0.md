# ZephCore 1.18.1-zephcore-epv1.0

First release of the EPV feature set on ZephCore 1.18.1: solar charge controller and INA3221
readings in repeater telemetry. The node reports `1.18.1-zephcore-epv1.0` on `ver`, in the MQTT
status and over BLE; the companion app's device info shows `1.18.1-epv1.0`.

> [!NOTE]
> A normal upgrade keeps your identity, settings, contacts and phone pairing.

---

## Epever charge controller in telemetry

A repeater or room server built with `boards/common/epever.conf` reads an Epever Tracer AN / AN
G3 charge controller over RS-485 once a minute and adds two telemetry channels after the I2C
sensors: battery (voltage, charging current, temperature) and PV (voltage, current, power). With
no controller connected, or one that stops answering, the channels are left out and the node
keeps looking for it. Supported boards so far: Heltec T114, RAK4631, RAK3401 1W, Heltec V3 and
V4. On the RAK boards the controller takes the GPS slot's UART, so that build has no GPS. Wiring
is in [docs/EPEVER.md](../docs/EPEVER.md).

## INA3221 at 0x41

The I2C sensor list now includes an INA3221 at 0x41 (A0 to VS), three channels of voltage,
current and power, each on its own telemetry channel. 0x40 stays with the INA219.

## External I2C sensors on the Heltec V3 and V4

The OLED's I2C bus is not on the pin header of these boards. A build with
`boards/common/sensors_i2c1.conf` looks for the external sensors on a second bus instead: GPIO6
(SDA) and GPIO4 (SCL), the same pins on both boards. The standard builds are unchanged.
