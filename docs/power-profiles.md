# Power profiles in Chicago-Wayland

The taskbar power icon cycles Saver → Balanced → Performance. The Power Management Properties selector uses the same Power Profiles daemon state. Both update from D-Bus signals rather than guessing the selected profile from Dell firmware thermal labels. Failed changes show an error dialog. Systems without Performance support skip it when cycling.

The `chicago-power` helper provides `get`, `set PROFILE`, and `cycle` commands. The labwc startup also runs `chicago-power watch`: it listens for power-profile and battery-source changes and applies the internal panel refresh policy through wlr-output-management. Saver or battery operation selects the lowest refresh rate at the current resolution; AC Balanced/Performance restore the preferred rate. It preserves external monitor modes, positions and scaling, and does nothing if the panel already has the requested mode. Kanshi profile hooks reapply this policy after docking/layout changes. Nested sessions skip the watcher and display changes.

On the tested Dell Precision 5550, the existing system hardware bridge supplies the CPU/GPU policy. It was corrected to respect deliberate Balanced selections and to keep the Performance thermal mode consistent with the power daemon. This machine-specific hardware bridge is separate from the portable Chicago desktop source.

Live verification on AC passed:

| Profile | CPU PL1 / PL2 | EPP | Turbo | Internal panel |
| --- | --- | --- | --- | --- |
| Saver | 10 / 15 W | power | disabled | 47.96 Hz |
| Balanced | 18 / 25 W | balance_power | enabled | 59.95 Hz |
| Performance | 55 / 80 W | performance | enabled | 59.95 Hz |

These are CPU power limits, not measured idle consumption. External monitor modes and positions stayed unchanged during the test. Performance was restored afterward. Eight private-D-Bus/display-policy regression tests passed; both C components compiled without warnings. A live hardware unplug/replug test and battery-discharge comparison have not been performed as part of this change.

Run the focused tests with `python3 tests/power_profiles.py`. Check the watcher log at `$XDG_RUNTIME_DIR/chicago-power.log`, and the machine hardware bridge with `power-status` and `journalctl -u precision-power-manager.service`.

When the host's separate PSR update helper is installed, the standalone startup invokes its existing login check too. This preserves the local screen-confirmation workflow for prepared update trials; ordinary logins remain quiet.
