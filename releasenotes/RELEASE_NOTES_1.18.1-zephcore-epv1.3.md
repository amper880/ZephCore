# ZephCore 1.18.1-zephcore-epv1.3

The Epever controller's load output in telemetry.

> [!NOTE]
> A normal upgrade keeps your identity, settings, contacts and phone pairing.

---

## Load output channel

The node now also reads the controller's switched load output and reports its voltage and current
on a channel of its own, after the battery and PV channels. The existing channels keep their
numbers. A controller that does not answer for those registers still reports the battery and PV
channels. `get epever` shows the load values too.
