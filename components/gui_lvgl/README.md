# LVGL GUI Overview

This component renders the LVGL tabview UI. The dashboard tab uses meters for SOC/SOH and temperature, a power/current chart, and a 16-cell bar graph with a color legend for normal/min/max/balancing and UV/OV alerts. Event bus handlers in `gui_init.c` forward battery, system, and pack statistics updates to the screen helpers with `lv_async_call()` so LVGL is only touched from the UI thread.

Key entry points:
- `gui_init(event_bus_t *bus)`: sets up the tabview and subscribes to events.
- `screen_dashboard_*` helpers: build and refresh the dashboard gauges, chart, and cell bars.
- Other screens (`screen_home`, `screen_battery`, etc.) keep their existing updates alongside the dashboard.
