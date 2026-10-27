#!/usr/bin/env bash
# Shared transaction used by the launcher and the manual recovery command.
STATE_DIR="${XDG_STATE_HOME:-$HOME/.local/state}/chicago-wayland"
BACKUP_DIR="$STATE_DIR/gtk-backup"
GTK_CONFIG_ROOT="${XDG_CONFIG_HOME:-$HOME/.config}"

restore_gtk_configs() {
    [[ -f "$STATE_DIR/gtk-swapped.lock" ]] || return 0
    local ver f saved schema key
    for ver in gtk-3.0 gtk-4.0; do
        mkdir -p "$GTK_CONFIG_ROOT/$ver"
        for f in settings.ini gtk.css; do
            saved="$BACKUP_DIR/$ver/$f"
            if [[ -e "$saved" || -L "$saved" ]]; then
                rm -f "$GTK_CONFIG_ROOT/$ver/$f"
                cp -a "$saved" "$GTK_CONFIG_ROOT/$ver/$f" || return 1
            elif [[ -f "$saved.absent" ]]; then
                rm -f "$GTK_CONFIG_ROOT/$ver/$f" || return 1
            fi
        done
    done
    if command -v gsettings >/dev/null 2>&1; then
        for saved in "$BACKUP_DIR/gsettings/"*; do
            [[ -f "$saved" ]] || continue
            schema="$(basename "$saved")"
            key="${schema##*__}"
            schema="${schema%__*}"
            gsettings set "$schema" "$key" "$(cat "$saved")" || return 1
        done
    elif [[ -d "$BACKUP_DIR/gsettings" ]] && compgen -G "$BACKUP_DIR/gsettings/*" >/dev/null; then
        echo 'gsettings is required to finish restoring the session settings.' >&2
        return 1
    fi
    # Recover snapshots made by older launchers without executing shell code.
    if [[ ! -d "$BACKUP_DIR/gsettings" && -f "$STATE_DIR/saved_gsettings.sh" ]]; then
        python3 - "$STATE_DIR/saved_gsettings.sh" <<'PY_LEGACY'
import shlex, subprocess, sys
from pathlib import Path
allowed = {
    'org.gnome.desktop.interface': {'gtk-theme', 'icon-theme', 'cursor-theme', 'color-scheme', 'font-name'},
    'org.gnome.desktop.wm.preferences': {'button-layout'},
}
for line in Path(sys.argv[1]).read_text().splitlines():
    words = shlex.split(line, comments=True)
    if not words:
        continue
    if (len(words) != 5 or words[:2] != ['gsettings', 'set'] or
            words[3] not in allowed.get(words[2], set())):
        raise SystemExit('Unrecognized legacy settings snapshot; recovery left pending.')
    subprocess.run(words, check=True)
PY_LEGACY
        [[ $? -eq 0 ]] || return 1
    fi
    rm -f "$STATE_DIR/saved_gsettings.sh"
    rm -f "$STATE_DIR/gtk-swapped.lock"
    rm -rf "$BACKUP_DIR"
}

apply_session_gtk_configs() {
    local ver f schema key value target
    # The caller holds session.lock; recover an interrupted earlier transaction.
    restore_gtk_configs || return 1
    rm -rf "$BACKUP_DIR"
    mkdir -p "$BACKUP_DIR" "$STATE_DIR"
    for ver in gtk-3.0 gtk-4.0; do
        mkdir -p "$BACKUP_DIR/$ver" "$GTK_CONFIG_ROOT/$ver"
        for f in settings.ini gtk.css; do
            if [[ -e "$GTK_CONFIG_ROOT/$ver/$f" || -L "$GTK_CONFIG_ROOT/$ver/$f" ]]; then
                cp -a "$GTK_CONFIG_ROOT/$ver/$f" "$BACKUP_DIR/$ver/$f" || return 1
            else
                touch "$BACKUP_DIR/$ver/$f.absent" || return 1
            fi
        done
    done
    # Snapshot every setting before making any change. Store values as data.
    if command -v gsettings >/dev/null 2>&1; then
        mkdir -p "$BACKUP_DIR/gsettings"
        while read -r schema key target; do
            if value="$(gsettings get "$schema" "$key" 2>/dev/null)"; then
                printf '%s\n' "$value" > "$BACKUP_DIR/gsettings/${schema}__${key}"
            fi
        done < "$REPO_DIR/config/gtk/gsettings.tsv"
    fi
    touch "$STATE_DIR/gtk-swapped.lock"
    for ver in gtk-3.0 gtk-4.0; do
        # Remove symlinks before writing, so targets outside the config stay intact.
        rm -f "$GTK_CONFIG_ROOT/$ver/settings.ini" "$GTK_CONFIG_ROOT/$ver/gtk.css"
        cp "$REPO_DIR/config/gtk/settings.ini" "$GTK_CONFIG_ROOT/$ver/settings.ini" || return 1
        cp "$REPO_DIR/config/gtk/gtk-win95-override.css" "$GTK_CONFIG_ROOT/$ver/gtk.css" || return 1
    done
    while read -r schema key target; do
        if [[ -f "$BACKUP_DIR/gsettings/${schema}__${key}" ]]; then
            gsettings set "$schema" "$key" "$target" 2>/dev/null || true
        fi
    done < "$REPO_DIR/config/gtk/gsettings.tsv"
}
