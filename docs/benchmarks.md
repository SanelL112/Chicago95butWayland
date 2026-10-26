# Performance & Power Efficiency Benchmarks

## 1. Measurement Methodology & Hardware Conditions

All measurements were collected on the target system using `./scripts/benchmark.sh` (raw JSON output saved in [`docs/benchmark_results.json`](file:///home/sanellathiya/chicago-wayland/docs/benchmark_results.json)):

- **System**: Dell Precision 5550, Fedora Linux 44 (Workstation Edition), Kernel `7.2.11-200.fc44.x86_64`
- **CPU**: Intel(R) Core(TM) i7-10750H @ 2.60 GHz (6 cores / 12 threads), Governor: `powersave`, EPP: `performance`
- **Displays**:
  - `eDP-1`: Sharp 1920×1200 @ 59.95 Hz (Brightness: `15600 / 120000` = 13%)
  - `DP-6`: HP E22 G5 1920×1080 @ 74.97 Hz
  - `DP-9`: DELL P2222H 1920×1080 @ 60.00 Hz
- **Power Sensor State**:
  - During live benchmarking, the laptop was connected to AC power (`BAT0/status = Not charging`, 93% capacity, charge thresholds 75%–80% active). As documented in `/home/sanellathiya/power-management-fix/power-status`, `/sys/class/power_supply/BAT0` only reports whole-system discharge wattage when actively discharging on battery (`status == Discharging`), and `/sys/class/powercap/intel-rapl*/energy_uj` is mode `0400` root-only on Kernel 7.2.
  - Historical on-battery discharge measurements recorded on this exact machine for the settled Hyprland desktop (`/home/sanellathiya/power-management-fix/desktop-settled.json`) across 6 samples (`[9.25W, 8.19W, 8.10W, 8.29W, 8.06W, 9.63W]`) averaged **8.59 W** (with a 7.0 W minimum single-display target).

---

## 2. Measured Desktop Stack Comparison (Idle, 5 Samples × 2.0s = 10.0s Window)

| Metric | **Existing Hyprland Stack** (`Hyprland` + `qs -c ii` + `Xwayland`) | **Chicago-Wayland Default** (`labwc` + `chicago-desktop` + `chicago-taskbar`) | **Chicago-Wayland (`sfwbar` Variant)** | **Improvement (Default vs. Hyprland)** |
| :--- | :--- | :--- | :--- | :--- |
| **Total Desktop CPU (%)** | **10.745%** (`Hyprland`: 4.18%, `qs`: 6.57%) | **0.000%** (`labwc`: 0.0%, `desktop`: 0.0%, `taskbar`: 0.0%) | **0.000%** | **-100% idle CPU overhead** |
| **Desktop Context Switches / Wakeups (ctx/s)** | **351.39 ctx/s** (`Hyprland`: 169.6/s, `qs`: 181.8/s) | **0.00 ctx/s** (0.017/s avg across minute boundaries) | **0.10 ctx/s** | **-99.99% CPU wakeups** |
| **Total Resident Memory (RSS)** | **1,008.99 MiB** (`1,033,208 KB`) | **211.63 MiB** (`216,712 KB`) | **171.32 MiB** (`175,436 KB`) | **-79.0% RSS (-797.4 MiB)** |
| **Total Proportional Memory (PSS)** | **731.35 MiB** (`748,907 KB`) | **84.61 MiB** (`86,639 KB`) | **71.72 MiB** (`73,440 KB`) | **-88.4% PSS (-646.7 MiB)** |
| **Cold Startup Time to Mapped Desktop** | ~1,400 ms | **279.0 ms** | ~320 ms | **~5× faster startup** |

---

## 3. Measured Hardware Power (Intel RAPL Energy Counters, 5 Samples × 1.2s)

Using root access to `/sys/class/powercap/intel-rapl*/energy_uj` (raw JSON in [`docs/rapl_power_comparison.json`](file:///home/sanellathiya/chicago-wayland/docs/rapl_power_comparison.json)), we measured hardware energy consumption across all 5 Intel RAPL power domains on the Core i7-10750H comparing the active Hyprland + Quickshell (`qs -c ii`) session against Chicago-Wayland (`labwc` + `chicago-desktop` + `chicago-taskbar`):

| Intel RAPL Power Domain | **Hyprland + Quickshell (`qs`)** (Mean ± SD) | **Chicago-Wayland (`labwc` + C11 Shell)** (Mean ± SD) | **Measured Power Savings** |
| :--- | :--- | :--- | :--- |
| **CPU Package (`intel-rapl:0`)** | **10.666 W** ± 2.870 W | **8.410 W** ± 0.673 W *(settled: 8.01–8.18 W)* | **-2.256 W (-21.2%)** |
| **CPU Cores (`intel-rapl:0:0`)** | **5.422 W** ± 1.730 W | **4.125 W** ± 0.690 W *(settled: 3.73–3.89 W)* | **-1.297 W (-23.9%)** |
| **Intel iGPU / Uncore (`intel-rapl:0:1`)** | **1.612 W** ± 1.051 W | **0.737 W** ± 0.051 W | **-0.875 W (-54.3%)** |
| **DRAM (`intel-rapl:0:2`)** | **1.094 W** ± 0.179 W | **0.953 W** ± 0.010 W | **-0.141 W (-12.9%)** |
| **Platform Psys (`intel-rapl:1`)** | **2.773 W** ± 0.332 W | **2.517 W** ± 0.106 W | **-0.256 W (-9.2%)** |

---

## 4. Per-Component Breakdown

### A. Existing Hyprland Desktop Baseline
| Process | Idle CPU (%) | Wakeups (ctx/s) | RSS (KB / MiB) | PSS (KB / MiB) |
| :--- | :--- | :--- | :--- | :--- |
| `Hyprland` | 4.179% | 169.63 / s | 191,248 KB (186.8 MiB) | 114,553 KB (111.9 MiB) |
| `qs` (Quickshell `ii`) | 6.566% | 181.77 / s | 733,856 KB (716.7 MiB) | 591,259 KB (577.4 MiB) |
| `Xwayland` (persistent) | 0.000% | 0.00 / s | 108,104 KB (105.6 MiB) | 43,095 KB (42.1 MiB) |
| **Total** | **10.745%** | **351.39 / s** | **1,033,208 KB (1,009.0 MiB)** | **748,907 KB (731.4 MiB)** |

### B. Chicago-Wayland Default Stack
| Process | Idle CPU (%) | Wakeups (ctx/s) | RSS (KB / MiB) | PSS (KB / MiB) |
| :--- | :--- | :--- | :--- | :--- |
| `labwc` (`0.9.6`) | 0.000% | 0.00 / s | 132,740 KB (129.6 MiB) | 55,126 KB (53.8 MiB) |
| `chicago-desktop` | 0.000% | 0.00 / s | 43,312 KB (42.3 MiB) | 16,333 KB (15.9 MiB) |
| `chicago-taskbar` | 0.000% | 0.00 / s | 40,660 KB (39.7 MiB) | 15,180 KB (14.8 MiB) |
| `Xwayland` (on-demand) | 0.000% | 0.00 / s | 0 KB (not running at idle) | 0 KB |
| **Total** | **0.000%** | **0.00 / s** | **216,712 KB (211.6 MiB)** | **86,639 KB (84.6 MiB)** |

---

## 4. Why Chicago-Wayland Reduces Idle Power Overhead

1. **Elimination of Continuous QML/JS Timer Wakeups**: In the baseline Hyprland setup, `qs -c ii` (Quickshell) + `Hyprland` generate **351.4 context switches/sec** and consume **6.5%–10.7% of a CPU core continuously at idle**, preventing deep package C-states (`PC8`/`PC10`). In `chicago-wayland`, idle context switches drop to **0.00 ctx/s** between minute boundaries.
2. **Minute-Aligned Taskbar Clock**: Instead of a 1 Hz (`1000 ms`) timer that wakes the CPU 60 times per minute, `chicago-taskbar` calculates `60 - localtime(&now)->tm_sec` and sleeps until the exact minute rollover (0.017 wakeups/sec).
3. **Damage-Tracked Static Rendering**: With zero animations, zero blur, zero drop shadows, and a non-blinking terminal cursor, `labwc` performs **zero DRM pageflips or GPU composites** when no input occurs.
4. **On-Demand XWayland**: Setting `<xwaylandPersistence>no</xwaylandPersistence>` eliminates the idle `Xwayland` server until an X11 client is actually launched.

---

## 5. Measuring On-Battery Wattage

When running on battery power (`Discharging` state), run:
```bash
~/chicago-wayland/scripts/benchmark.sh
```
or check instantaneous draw via:
```bash
~/power-management-fix/power-status
```
