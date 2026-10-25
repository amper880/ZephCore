# ZephCore 1.18.1-zephcore-epv1.1

Diagnostics for the EPV features.

> [!NOTE]
> A normal upgrade keeps your identity, settings, contacts and phone pairing.

---

## `get epever` and `get sensors`

Two new CLI commands show why a reading is missing from telemetry. `get epever` reports whether
the Modbus client started, how many polls went out and were answered, the last result and its
age, and the last reading. `get sensors` names the bus the sensor list is on and each I2C part
found at boot, read again on the spot. See [docs/CLI_commands.md](../docs/CLI_commands.md).
