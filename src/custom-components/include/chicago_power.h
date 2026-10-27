#ifndef CHICAGO_POWER_H
#define CHICAGO_POWER_H

#include <gtk/gtk.h>
#include <string.h>

/* Read the same D-Bus property we change. Dell thermal modes are not PPD profiles. */
static GDBusProxy *chicago_power_proxy = NULL;

static inline GDBusProxy *chicago_power_connect(void) {
    if (!chicago_power_proxy) {
        GError *error = NULL;
        chicago_power_proxy = g_dbus_proxy_new_for_bus_sync(
            G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, NULL,
            "net.hadess.PowerProfiles", "/net/hadess/PowerProfiles",
            "net.hadess.PowerProfiles", NULL, &error);
        if (error) {
            g_warning("Power profiles unavailable: %s", error->message);
            g_error_free(error);
        }
    }
    return chicago_power_proxy;
}

static inline void chicago_power_get(char *profile, size_t size) {
    if (!profile || !size) return;
    g_strlcpy(profile, "unknown", size);
    GDBusProxy *proxy = chicago_power_connect();
    GVariant *value = proxy ? g_dbus_proxy_get_cached_property(proxy, "ActiveProfile") : NULL;
    if (value) {
        g_strlcpy(profile, g_variant_get_string(value, NULL), size);
        g_variant_unref(value);
    }
}

static void chicago_power_set_finished(GObject *source, GAsyncResult *result, gpointer data) {
    (void)data;
    GError *error = NULL;
    GVariant *reply = g_dbus_proxy_call_finish(G_DBUS_PROXY(source), result, &error);
    if (reply) g_variant_unref(reply);
    if (error) {
        GtkWidget *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "Could not change power profile");
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", error->message);
        g_signal_connect_swapped(dialog, "response", G_CALLBACK(gtk_widget_destroy), dialog);
        gtk_widget_show(dialog);
        g_error_free(error);
    }
}

static inline void chicago_power_set(const char *profile) {
    if (!profile || (strcmp(profile, "power-saver") && strcmp(profile, "balanced") && strcmp(profile, "performance"))) return;
    GDBusProxy *proxy = chicago_power_connect();
    if (!proxy) return;
    g_dbus_proxy_call(proxy, "org.freedesktop.DBus.Properties.Set",
        g_variant_new("(ssv)", "net.hadess.PowerProfiles", "ActiveProfile", g_variant_new_string(profile)),
        G_DBUS_CALL_FLAGS_NONE, 5000, NULL, chicago_power_set_finished, NULL);
}

static inline void chicago_power_cycle(void) {
    char current[64];
    chicago_power_get(current, sizeof(current));
    const char *next = !strcmp(current, "power-saver") ? "balanced" :
                       !strcmp(current, "balanced") ? "performance" : "power-saver";
    GDBusProxy *proxy = chicago_power_connect();
    GVariant *profiles = proxy ? g_dbus_proxy_get_cached_property(proxy, "Profiles") : NULL;
    gboolean supported = FALSE;
    if (profiles) {
        GVariantIter iter;
        GVariant *entry;
        g_variant_iter_init(&iter, profiles);
        while ((entry = g_variant_iter_next_value(&iter))) {
            const char *name = NULL;
            if (g_variant_lookup(entry, "Profile", "&s", &name) && !strcmp(name, next)) supported = TRUE;
            g_variant_unref(entry);
        }
        g_variant_unref(profiles);
    }
    chicago_power_set(!strcmp(next, "performance") && !supported ? "power-saver" : next);
}
#endif
