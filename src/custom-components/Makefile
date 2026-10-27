CC ?= gcc
WAYLAND_SCANNER ?= wayland-scanner
PKG_CONFIG ?= pkg-config

PKGS = gtk+-3.0 gtk-layer-shell-0 wayland-client gio-unix-2.0 cairo pango
CFLAGS ?= -O2 -Wall -Wextra
ALL_CFLAGS = $(CFLAGS) -Iinclude -I$(BUILDDIR) $(shell $(PKG_CONFIG) --cflags $(PKGS))
LIBS = $(shell $(PKG_CONFIG) --libs $(PKGS)) -lm

BINDIR = ../../bin
BUILDDIR = build
LOCK_PREFIX ?= $(if $(CHICAGO_LOCK_PREFIX),$(CHICAGO_LOCK_PREFIX),$(HOME)/.local/share/chicago-wayland/deps/gtk-session-lock)
LOCK_PC = env PKG_CONFIG_PATH="$(LOCK_PREFIX)/lib/pkgconfig:$(PKG_CONFIG_PATH)" $(PKG_CONFIG)
LOCK_LIBDIR = $(shell $(LOCK_PC) --variable=libdir gtk-session-lock-0)
LOCK_CFLAGS = $(shell $(LOCK_PC) --cflags gtk-session-lock-0)
LOCK_GTK_LIBS = $(shell $(PKG_CONFIG) --libs $(filter-out gtk-layer-shell-0,$(PKGS))) -lm
LOCK_LIBS = $(shell $(LOCK_PC) --libs gtk-session-lock-0) -lpam
LOCK_BUNDLED = ../../lib/libgtk-session-lock.so.0
PROTO_XML = protocols/wlr-foreign-toplevel-management-unstable-v1.xml
PROTO_H = $(BUILDDIR)/wlr-foreign-toplevel-management-unstable-v1-client-protocol.h
PROTO_C = $(BUILDDIR)/wlr-foreign-toplevel-management-unstable-v1-client-protocol.c

TARGETS = $(BINDIR)/chicago-desktop $(BINDIR)/chicago-taskbar $(BINDIR)/chicago-control-panel $(BINDIR)/chicago-lock $(BINDIR)/chicago-lock-custom $(BINDIR)/chicago-lock-face $(BINDIR)/chicago-capture

.PHONY: all clean

all: $(TARGETS)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

$(PROTO_H): $(PROTO_XML) | $(BUILDDIR)
	$(WAYLAND_SCANNER) client-header $< $@

$(PROTO_C): $(PROTO_XML) | $(BUILDDIR)
	$(WAYLAND_SCANNER) private-code $< $@

$(BINDIR)/chicago-desktop: src/chicago_desktop.c include/chicago_common.h | $(BINDIR)
	$(CC) $(ALL_CFLAGS) src/chicago_desktop.c -o $@ $(LIBS)

$(BINDIR)/chicago-taskbar: src/chicago_taskbar.c $(PROTO_H) $(PROTO_C) include/chicago_common.h include/chicago_power.h | $(BINDIR)
	$(CC) $(ALL_CFLAGS) src/chicago_taskbar.c $(PROTO_C) -o $@ $(LIBS)

$(BINDIR)/chicago-control-panel: src/chicago_control_panel.c include/chicago_common.h include/chicago_power.h | $(BINDIR)
	$(CC) $(ALL_CFLAGS) src/chicago_control_panel.c -o $@ $(LIBS)

$(BINDIR)/chicago-lock: src/chicago_lock.c include/chicago_common.h | $(BINDIR)
	$(CC) $(CFLAGS) src/chicago_lock.c -o $@

$(LOCK_BUNDLED):
	$(LOCK_PC) --atleast-version=0.2.0 gtk-session-lock-0 || { echo "Run ../../scripts/build-lock-dependency.sh or install gtk-session-lock-devel"; exit 1; }
	mkdir -p ../../lib
	cp -L "$(LOCK_LIBDIR)/libgtk-session-lock.so.0" $@

$(BINDIR)/chicago-lock-custom: src/chicago_lock_custom.c src/chicago_lock_auth.c include/chicago_lock_auth.h include/chicago_lock_ready.h include/chicago_common.h $(LOCK_BUNDLED) | $(BINDIR)
	$(CC) $(ALL_CFLAGS) $(LOCK_CFLAGS) src/chicago_lock_custom.c src/chicago_lock_auth.c -o $@ $(LOCK_GTK_LIBS) $(LOCK_LIBS) -Wl,-rpath,'$$ORIGIN/../lib'

$(BINDIR)/chicago-lock-face: src/chicago_lock_face.c src/chicago_lock_auth.c include/chicago_lock_auth.h ../../config/pam/chicago-lock-face | $(BINDIR)
	$(CC) $(CFLAGS) -Iinclude src/chicago_lock_face.c src/chicago_lock_auth.c -o $@ -lpam

$(BINDIR)/chicago-capture: src/chicago_capture.c include/chicago_common.h | $(BINDIR)
	$(CC) $(ALL_CFLAGS) src/chicago_capture.c -o $@ $(LIBS)

clean:
	rm -rf $(BUILDDIR) $(TARGETS) $(LOCK_BUNDLED)
