#!/usr/bin/env bash
# scripts/benchmark.sh: Multi-sample performance & power efficiency benchmark
# Compares Chicago-Wayland (labwc + chicago-desktop + chicago-taskbar) vs sfwbar variant vs Hyprland baseline.
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUNTIME_DIR="$HOME/.local/share/chicago-wayland/runtime"
export PATH="$REPO_DIR/bin:$HOME/.local/bin:$RUNTIME_DIR/usr/bin:$PATH"
export LD_LIBRARY_PATH="$RUNTIME_DIR/usr/lib64:${LD_LIBRARY_PATH:-}"

OUT_JSON="${1:-$REPO_DIR/docs/benchmark_results.json}"

python3 - "$REPO_DIR" "$OUT_JSON" << 'EOF'
import json, os, signal, statistics, subprocess, sys, time
from pathlib import Path

repo_dir = Path(sys.argv[1])
out_json = Path(sys.argv[2])
hz = os.sysconf('SC_CLK_TCK')
page_kb = os.sysconf('SC_PAGE_SIZE') // 1024

def read_text(p):
    try:
        return Path(p).read_text().strip()
    except Exception:
        return None

def read_proc_tree(pids):
    snap = {}
    for pid in pids:
        p = Path(f'/proc/{pid}')
        try:
            comm = (p / 'comm').read_text().strip()
            st = (p / 'stat').read_text().rsplit(')', 1)[1].split()
            ticks = int(st[11]) + int(st[12])
            rss_kb = int(st[21]) * page_kb
            vol_cs, nonvol_cs = 0, 0
            for line in (p / 'status').read_text().splitlines():
                if line.startswith('voluntary_ctxt_switches:'):
                    vol_cs = int(line.split()[1])
                elif line.startswith('nonvoluntary_ctxt_switches:'):
                    nonvol_cs = int(line.split()[1])
            pss_kb = rss_kb
            try:
                for line in (p / 'smaps_rollup').read_text().splitlines():
                    if line.startswith('Pss:'):
                        pss_kb = int(line.split()[1])
                        break
            except Exception:
                pass
            snap[pid] = {
                'comm': comm,
                'ticks': ticks,
                'rss_kb': rss_kb,
                'pss_kb': pss_kb,
                'ctx': vol_cs + nonvol_cs
            }
        except Exception:
            continue
    return snap

def find_pids_by_names(names):
    found = []
    uid = os.getuid()
    for p in Path('/proc').glob('[0-9]*'):
        try:
            if p.stat().st_uid != uid:
                continue
            comm = (p / 'comm').read_text().strip()
            if comm in names:
                found.append(int(p.name))
        except Exception:
            continue
    return found

def sample_pids(pids, num_samples=5, interval=2.0):
    s0 = read_proc_tree(pids)
    t0 = time.monotonic()
    bat_samples = []
    for _ in range(num_samples):
        time.sleep(interval)
        b_stat = read_text('/sys/class/power_supply/BAT0/status')
        cur = read_text('/sys/class/power_supply/BAT0/current_now')
        volt = read_text('/sys/class/power_supply/BAT0/voltage_now')
        w = (int(cur) * int(volt) / 1e12) if (cur and volt) else None
        bat_samples.append({'status': b_stat, 'sensor_watts': w})
    s1 = read_proc_tree(pids)
    elapsed = time.monotonic() - t0

    by_comm = {}
    tot_cpu = 0.0
    tot_ctx = 0.0
    tot_rss = 0
    tot_pss = 0
    for pid, d1 in s1.items():
        d0 = s0.get(pid)
        if not d0:
            continue
        comm = d1['comm']
        cpu_pct = 100.0 * ((d1['ticks'] - d0['ticks']) / hz) / elapsed
        ctx_s = (d1['ctx'] - d0['ctx']) / elapsed
        by_comm[comm] = {
            'cpu_pct': round(cpu_pct, 3),
            'ctx_switches_per_sec': round(ctx_s, 2),
            'rss_kb': d1['rss_kb'],
            'pss_kb': d1['pss_kb'],
        }
        tot_cpu += cpu_pct
        tot_ctx += ctx_s
        tot_rss += d1['rss_kb']
        tot_pss += d1['pss_kb']

    return {
        'elapsed_sec': round(elapsed, 2),
        'total_cpu_pct': round(tot_cpu, 3),
        'total_ctx_switches_per_sec': round(tot_ctx, 2),
        'total_rss_kb': tot_rss,
        'total_rss_mib': round(tot_rss / 1024.0, 2),
        'total_pss_kb': tot_pss,
        'total_pss_mib': round(tot_pss / 1024.0, 2),
        'components': by_comm,
        'battery_samples': bat_samples,
    }

sys.path.insert(0, str(repo_dir / 'scripts'))
from isolated import isolated_session
try:
    with isolated_session():
        pass
except PermissionError as exc:
    print(f'[SKIP] Cannot create private compositor socket: {exc}', file=sys.stderr)
    sys.exit(77)

print("[1/3] Measuring running Hyprland + Quickshell (qs) + Xwayland baseline...")
hypr_pids = find_pids_by_names({'Hyprland', 'qs', 'Xwayland'})
hypr_metrics = sample_pids(hypr_pids, num_samples=5, interval=2.0)

# Limit all Chicago metrics and cleanup to the private child process group.
sys.path.insert(0, str(repo_dir / 'scripts'))
from isolated import isolated_session, stop_group

def variant(panel):
    try:
        with isolated_session() as env:
            env['CHICAGO_PANEL'] = panel
            proc = subprocess.Popen(
                ['dbus-run-session', '--', str(repo_dir / 'bin/start-chicago-wayland'), '--headless'],
                env=env, stdout=subprocess.DEVNULL, start_new_session=True)
            try:
                launched = time.monotonic()
                expected = {'labwc', 'chicago-desktop', panel}
                def owned_pids():
                    return [pid for pid in find_pids_by_names(expected)
                            if os.getpgid(pid) == proc.pid]
                for _ in range(100):
                    if proc.poll() is not None:
                        raise RuntimeError('Isolated compositor exited before startup')
                    if len(owned_pids()) >= 3:
                        break
                    time.sleep(0.05)
                else:
                    raise RuntimeError('Isolated shell components did not start')
                startup = (time.monotonic() - launched) * 1000
                time.sleep(1.5)
                metrics = sample_pids(owned_pids())
                metrics['startup_time_ms'] = round(startup, 1)
                return metrics
            finally:
                stop_group(proc)
    except PermissionError as exc:
        print(f'[SKIP] Cannot create private compositor socket: {exc}', file=sys.stderr)
        sys.exit(77)

print("[2/3] Measuring isolated Chicago-Wayland default stack...")
cw_metrics = variant('chicago-taskbar')
print("[3/3] Measuring isolated sfwbar variant...")
sfw_metrics = variant('sfwbar')

report = {
    'timestamp': time.strftime('%Y-%m-%dT%H:%M:%S%z'),
    'hardware': {
        'cpu': 'Intel(R) Core(TM) i7-10750H CPU @ 2.60GHz',
        'kernel': os.uname().release,
        'fedora_release': read_text('/etc/fedora-release'),
        'battery_state': read_text('/sys/class/power_supply/BAT0/status'),
        'battery_capacity_pct': read_text('/sys/class/power_supply/BAT0/capacity'),
        'brightness': read_text('/sys/class/backlight/intel_backlight/brightness'),
        'max_brightness': read_text('/sys/class/backlight/intel_backlight/max_brightness'),
        'cpu_governor': read_text('/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor'),
        'cpu_epp': read_text('/sys/devices/system/cpu/cpu0/cpufreq/energy_performance_preference'),
        'nvidia_dgpu_runtime_status': read_text('/sys/bus/pci/devices/0000:01:00.0/power/runtime_status'),
    },
    'historical_battery_discharge_baseline': {
        'source': '/home/sanellathiya/power-management-fix/desktop-settled.json',
        'samples_watts': [9.25, 8.19, 8.10, 8.29, 8.06, 9.63],
        'mean_watts': 8.59,
        'reported_idle_target_watts': 7.0
    },
    'hyprland_baseline': hypr_metrics,
    'chicago_wayland_default': cw_metrics,
    'chicago_wayland_sfwbar': sfw_metrics,
}

out_json.write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
EOF
