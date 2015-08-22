/*
 * eventd-plugin-fedmsg - Collect events from fedmsg
 *
 * Copyright 2015 Ben Boeckel
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see http://www.gnu.org/licenses/.
 */

#include <glib.h>
#include <glib-object.h>
#include <glib-unix.h>

#include <libeventd-config.h>
#include <libeventd-event.h>
#include <eventd-plugin.h>

struct _EventdPluginContext {
    EventdPluginCoreContext *core;
    EventdPluginCoreInterface *core_interface;
};

static EventdPluginContext *
_eventd_fedmsg_init(EventdPluginCoreContext *core, EventdPluginCoreInterface *core_interface)
{
    EventdPluginContext *context;

    context = g_new0(EventdPluginContext, 1);

    context->core = core;
    context->core_interface= core_interface;

    return context;
}

static void
_eventd_fedmsg_uninit(EventdPluginContext *context)
{
    g_free(context);
}

static void
_eventd_fedmsg_start(EventdPluginContext *context)
{
    /* TODO: implement */
}

static void
_eventd_fedmsg_stop(EventdPluginContext *context)
{
    /* TODO: implement */
}

static void
_eventd_fedmsg_global_parse(EventdPluginContext *context, GKeyFile *config_file)
{
    gchar **subscribe = NULL;
    gchar **publish = NULL;

    if (!g_key_file_has_group(config_file, "Fedmsg"))
        return;

    libeventd_config_key_file_get_string_list(config_file, "Fedmsg", "SubscribeBuses", &subscribe, NULL);
    libeventd_config_key_file_get_string_list(config_file, "Fedmsg", "PublishBuses", &publish, NULL);

    if (!subscribe && !publish)
        g_warning("no buses configured");

    /* TODO: parse out the bus objects */
}

static void
_eventd_fedmsg_event_parse(EventdPluginContext *context, const gchar *config_id, GKeyFile *config_file)
{
    /* TODO: implement */
}

static void
_eventd_fedmsg_config_reset(EventdPluginContext *context)
{
    /* TODO: implement */
}

static void
_eventd_fedmsg_event_action(EventdPluginContext *context, const gchar *config_id, EventdEvent *event)
{
    /* TODO: implement */
}

#define FEDMSG_EXPORT __attribute__((__visibility__("default")))

FEDMSG_EXPORT const gchar *eventd_plugin_id = "eventd-fedmsg";
FEDMSG_EXPORT
void
eventd_plugin_get_interface(EventdPluginInterface *interface)
{
    eventd_plugin_interface_add_init_callback(interface, _eventd_fedmsg_init);
    eventd_plugin_interface_add_uninit_callback(interface, _eventd_fedmsg_uninit);

    eventd_plugin_interface_add_start_callback(interface, _eventd_fedmsg_start);
    eventd_plugin_interface_add_stop_callback(interface, _eventd_fedmsg_stop);

    eventd_plugin_interface_add_global_parse_callback(interface, _eventd_fedmsg_global_parse);
    eventd_plugin_interface_add_event_parse_callback(interface, _eventd_fedmsg_event_parse);
    eventd_plugin_interface_add_config_reset_callback(interface, _eventd_fedmsg_config_reset);

    eventd_plugin_interface_add_event_action_callback(interface, _eventd_fedmsg_event_action);
}
