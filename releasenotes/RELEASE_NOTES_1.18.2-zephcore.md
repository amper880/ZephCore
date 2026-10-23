# ZephCore 1.18.2-zephcore

Solar charge controller and INA3221 readings in repeater telemetry.

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
