CC ?= gcc
WAYLAND_SCANNER ?= wayland-scanner
PKG_CONFIG ?= pkg-config

PKGS = gtk+-3.0 gtk-layer-shell-0 wayland-client gio-unix-2.0 cairo pango
CFLAGS ?= -O2 -Wall -Wextra
ALL_CFLAGS = $(CFLAGS) -Iinclude -Ibuild $(shell $(PKG_CONFIG) --cflags $(PKGS))
LIBS = $(shell $(PKG_CONFIG) --libs $(PKGS)) -lm

BINDIR = ../../bin
BUILDDIR = build
PROTO_XML = protocols/wlr-foreign-toplevel-management-unstable-v1.xml
PROTO_H = $(BUILDDIR)/wlr-foreign-toplevel-management-unstable-v1-client-protocol.h
PROTO_C = $(BUILDDIR)/wlr-foreign-toplevel-management-unstable-v1-client-protocol.c

TARGETS = $(BINDIR)/chicago-desktop $(BINDIR)/chicago-taskbar $(BINDIR)/chicago-control-panel

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

$(BINDIR)/chicago-taskbar: src/chicago_taskbar.c $(PROTO_H) $(PROTO_C) include/chicago_common.h | $(BINDIR)
	$(CC) $(ALL_CFLAGS) src/chicago_taskbar.c $(PROTO_C) -o $@ $(LIBS)

$(BINDIR)/chicago-control-panel: src/chicago_control_panel.c include/chicago_common.h | $(BINDIR)
	$(CC) $(ALL_CFLAGS) src/chicago_control_panel.c -o $@ $(LIBS)

clean:
	rm -rf $(BUILDDIR) $(TARGETS)
