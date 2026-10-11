# ZephCore 1.17.8-zephcore

Button fixes on four boards with a display: the Station G2's button now drives the pages, and
Power Off, DFU and Offgrid can be confirmed on the Heltec Mesh Node T1 and both LR2021 LoRa Plus EVK
kits.

> [!NOTE]
> A normal upgrade keeps your identity, settings, contacts and phone pairing.

---

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
