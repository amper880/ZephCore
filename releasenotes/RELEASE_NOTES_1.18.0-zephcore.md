# ZephCore 1.18.0-zephcore

Safer off-grid mode, working buttons on four boards with a display, a GPS clock fix, and the
changes from upstream MeshCore 1.18.0.

> [!NOTE]
> A normal upgrade keeps your identity, settings, contacts and phone pairing.

---

## Off-grid mode stays off the live mesh

Off-grid mode makes a companion forward packets for nearby handhelds when no repeater is in reach. It
belongs on 433.000, 869.495 or 918.000 MHz, but the display's Offgrid toggle turned forwarding on
without changing the frequency, so the node repeated everything on your normal mesh.

- **Turning Offgrid on now moves the node** to the nearest off-grid frequency; turning it off returns
  to the frequency you came from. Bandwidth, SF, CR and TX power are kept.
- **A companion never forwards anywhere else.** If you enabled Offgrid from the display before, it
  shows as off after the update; turn it on again if you want it.
- **`set repeat on|off` works on a companion** and does the same as the toggle. The display also
  follows changes made from the app.

More in [OFFGRID.md](../docs/OFFGRID.md). Thanks to **N8HOP** for the report.

## Buttons

- **Station G2:** the user button now works like on other one-button boards: one press for the next
  page, two for the previous page, hold for a second to run the item.
- **Heltec Mesh Node T1, both LR2021 LoRa Plus EVK kits:** Power Off, DFU and Offgrid can be
  confirmed again.
- On all four boards the confirmation is a second **hold** within 3 seconds.

## GPS no longer sets the clock to an old time

Just after waking, a GPS receiver can report its last position and its own, possibly stale, clock
before any satellite confirms them. That was taken as a fix and could set a repeater's clock hours
back. Now only satellite-solved positions count, five in a row. A fix takes about two seconds longer.
This applies to every board with a GPS. Thanks to **Innocent Killer 101** for the report and the
measurements.

## From upstream MeshCore 1.18.0

- **Acknowledgements on direct routes are relayed after a short random delay**, like other direct
  packets, and with `multi.acks 1` the two copies go out 300 ms apart. This means fewer lost
  acknowledgements.
- **Floods with transport code 0 are treated as un-scoped.** Unlike upstream, ZephCore applies your
  `flood.max.unscoped` and `region denyf *` settings to them.
- **Room server:** `set repeat on` now answers `ERR - repeat now deprecated`. A room server never
  forwarded packets anyway.
