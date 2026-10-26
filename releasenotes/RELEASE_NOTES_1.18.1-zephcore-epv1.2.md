# ZephCore 1.18.1-zephcore-epv1.2

The Epever controller is read through RS-485 transceivers that echo the request.

> [!NOTE]
> A normal upgrade keeps your identity, settings, contacts and phone pairing.

---

## Epever behind an echoing transceiver

Some automatic-direction RS-485 modules keep their receiver on while they transmit, so the
node hears its own request in front of the controller's answer. The answer was then thrown away
(`get epever` showed `last -5`). The node now finds the answer behind the echo, or behind a
stray byte from the bus turning around, and checks it by its CRC.

`get epever` also shows the bytes received for the last request, and `get sensors` the values
each sensor reads, so a wiring or address problem shows on the console.
