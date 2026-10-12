# ZephCore 1.17.8-zephcore

Off-grid mode from the display no longer turns a companion into a repeater on the live mesh. Button
fixes on four boards with a display: the Station G2's button now drives the pages, and
Power Off, DFU and Offgrid can be confirmed on the Heltec Mesh Node T1 and both LR2021 LoRa Plus EVK
kits.

> [!NOTE]
> A normal upgrade keeps your identity, settings, contacts and phone pairing.

---

## Off-grid mode: only on the off-grid frequencies

Off-grid mode makes a companion forward packets for others, for a group of handhelds out of reach of
any repeater. It belongs on three frequencies set aside for it: 433.000, 869.495 and 918.000 MHz. The
app only allows it there, but the Offgrid toggle on the display switched forwarding on without
changing the frequency, so the node repeated everything it heard on the live mesh.

- **The display toggle now moves the node.** Turning Offgrid on goes to the off-grid frequency nearest
  to the one you are on and starts forwarding; turning it off returns to the frequency you came from.
  Bandwidth, spreading factor, coding rate and TX power stay as they are.
- **A companion never forwards on any other frequency.** If you had switched Offgrid on from the
  display on an earlier version, the node stops repeating after this update and shows Offgrid as off.
  Nothing else changes; turn it on again if you want off-grid mode.
- **`set repeat on` / `set repeat off` work on a companion** and do the same as the display toggle;
  `get repeat` shows the real state. Before, they answered but did nothing.
- **The display follows the app.** Offgrid switched from the app or the CLI now shows on the display
  page within a few seconds.

While off-grid mode is on, the node is off your normal mesh. How it works: [OFFGRID.md](../docs/OFFGRID.md).

Thanks to N8HOP for the report.

## Station G2: the user button drives the display

Until now a press of the user button only woke the display: the pages could not be turned, and
nothing on them could be run. The button now works as on the other one-button boards:

- **one press**: next page,
- **two presses**: previous page,
- **hold for one second**: run the item on the page (Bluetooth on/off, send an advert, GPS on/off,
  and so on).

Offgrid, DFU and Power Off ask for confirmation: hold the button again within 3 seconds.

Reports of how it behaves on your Station G2 are welcome.

## Heltec Mesh Node T1 and LR2021 LoRa Plus EVK: Power Off, DFU and Offgrid can be confirmed

On these boards Power Off, DFU and Offgrid could not be confirmed from the display. The confirmation
is a second long press of the user button, and it was accepted only within half a second of the
first, which a long press cannot meet: every long press asked for the confirmation again. It now has
3 seconds, as on the other one-button boards.

The confirmation is a second **hold**. A short press pages on instead, which on the Power Off page
goes back to the first page.
