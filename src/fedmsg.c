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

#include <Python.h>

struct _EventdPluginContext {
    EventdPluginCoreContext *core;
    EventdPluginCoreInterface *core_interface;

    GList *buses;
    GHashTable *events;

    /* Contains references into `buses`. */
    GList *publish_buses;

    /* Python bits */
    gboolean initialized_python;
    PyObject *fedmsg_core;
};

typedef struct {
    EventdPluginContext *context;
    gchar *name;

    PyObject *fedmsg_ctx;
} EventdFedmsgBus;

typedef struct {
    EventdFedmsgBus *bus;
    FormatString *modname;
    FormatString *topic;
} EventdFedmsgEventBus;

static PyObject *
_eventd_fedmsg_python_call(PyObject *obj, const char *name, PyObject *args, PyObject *kwargs)
{
    PyObject *method = PyObject_GetAttrString(obj, name);
    PyObject *ret = NULL;
    if (method && PyCallable_Check(method)) {
        ret = PyObject_Call(method, args, kwargs);
    }
    Py_XDECREF(method);

    return ret;
}

static void
_eventd_fedmsg_python_call_ignore(PyObject *obj, const char *name, PyObject *args, PyObject *kwargs)
{
    Py_XDECREF(_eventd_fedmsg_python_call(obj, name, args, kwargs));
}

static void
_eventd_fedmsg_bus_free(gpointer data)
{
    EventdFedmsgBus *bus = data;

    g_free(bus->name);

    PyObject *args = PyTuple_New(0);
    _eventd_fedmsg_python_call_ignore(bus->fedmsg_ctx, "destroy", args, NULL);
    Py_DECREF(args);

    Py_DECREF(bus->fedmsg_ctx);

    g_free(bus);
}

static void
_eventd_fedmsg_event_bus_free(gpointer data)
{
    EventdFedmsgEventBus *bus = data;

    libeventd_format_string_unref(bus->modname);
    libeventd_format_string_unref(bus->topic);

    g_slice_free(EventdFedmsgEventBus, data);
}

static void
_eventd_fedmsg_event_free(gpointer data)
{
    g_list_free_full(data, _eventd_fedmsg_event_bus_free);
}

static void
_eventd_fedmsg_uninit(EventdPluginContext *context);

static EventdPluginContext *
_eventd_fedmsg_init(EventdPluginCoreContext *core, EventdPluginCoreInterface *core_interface)
{
    EventdPluginContext *context;

    context = g_new0(EventdPluginContext, 1);

    context->core = core;
    context->core_interface= core_interface;

    context->events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, _eventd_fedmsg_event_free);

    if (!Py_IsInitialized()) {
        Py_Initialize();
        context->initialized_python = TRUE;
    }

    PyObject *fedmsg = PyImport_ImportModule("fedmsg");
    if (!fedmsg)
        goto python_bail;

    context->fedmsg_core = PyObject_GetAttrString(fedmsg, "core");
    Py_DECREF(fedmsg);
    if (!context->fedmsg_core)
        goto python_bail;

    return context;

python_bail:
    _eventd_fedmsg_uninit(context);
    return NULL;
}

static void
_eventd_fedmsg_uninit(EventdPluginContext *context)
{
    g_hash_table_unref(context->events);
    g_list_free_full(context->buses, _eventd_fedmsg_bus_free);
    g_list_free(context->publish_buses);

    Py_XDECREF(context->fedmsg_core);

    if (context->initialized_python)
        Py_Finalize();

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
    if (g_key_file_has_group(config_file, "Fedmsg")) {
        gboolean disable;
        if (libeventd_config_key_file_get_boolean(config_file, "Fedmsg", "Disable", &disable) < 0)
            return;

        if (disable) {
            g_hash_table_insert(context->events, g_strdup(config_id), NULL);
            return;
        }
    }

    gboolean have_bus = FALSE;
    GList *event_buses = NULL;

    GList *bus_;
    gchar *section = NULL;
    FormatString *modname = NULL;
    FormatString *topic = NULL;
    for (bus_ = context->publish_buses; bus_; bus_ = g_list_next(bus_)) {
        EventdFedmsgBus *bus = bus_->data;

        g_free(section);
        section = g_strconcat("FedmsgBus ", config_id, NULL);
        if (!g_key_file_has_group(config_file, section))
            continue;

        have_bus = TRUE;

        libeventd_format_string_unref(modname);
        modname = NULL;
        if (libeventd_config_key_file_get_locale_format_string(config_file, section, "ModName", NULL, &modname) < 0)
            continue;

        libeventd_format_string_unref(topic);
        topic = NULL;
        if (libeventd_config_key_file_get_locale_format_string(config_file, section, "Topic", NULL, &topic) < 0)
            continue;

        EventdFedmsgEventBus *event_bus;
        event_bus = g_slice_new0(EventdFedmsgEventBus);
        event_bus->bus = bus;
        event_bus->modname = modname;
        modname = NULL;
        event_bus->topic = topic;
        topic = NULL;

        event_buses = g_list_prepend(event_buses, event_bus);
    }

    g_free(section);
    libeventd_format_string_unref(modname);
    libeventd_format_string_unref(topic);

    if (have_bus)
        g_hash_table_insert(context->events, g_strdup(config_id), event_buses);
}

static void
_eventd_fedmsg_config_reset(EventdPluginContext *context)
{
    g_hash_table_remove_all(context->events);

    g_list_free(context->publish_buses);
    context->publish_buses = NULL;

    g_list_free_full(context->buses, _eventd_fedmsg_bus_free);
    context->buses = NULL;
}

static void
_eventd_fedmsg_python_add_data_to_add_dict(gpointer key, gpointer value, gpointer data)
{
    gchar *skey = key;
    gchar *svalue = value;
    PyObject *dict = data;

    PyObject *py_value = PyUnicode_FromString(svalue);
    if (PyDict_SetItemString(dict, skey, py_value))
        g_warning("failed to add %s data into the dictionary", skey);
    Py_DECREF(py_value);
}

static PyObject *
_eventd_fedmsg_event_to_python_dict(EventdEvent *event)
{
    PyObject *dict = PyDict_New();
    GHashTable *data = eventd_event_get_all_data(event);

    g_hash_table_foreach(data, _eventd_fedmsg_python_add_data_to_add_dict, dict);

    return dict;
}

static void
_eventd_fedmsg_event_action(EventdPluginContext *context, const gchar *config_id, EventdEvent *event)
{
    GList *buses;

    buses = g_hash_table_lookup(context->events, config_id);
    if (!buses)
        return;

    GList *bus_;
    for (bus_ = buses; bus_; bus_ = g_list_next(bus_)) {
        EventdFedmsgEventBus *bus = bus_->data;

        gchar *modname = libeventd_format_string_get_string(bus->modname, event, NULL, NULL);
        gchar *topic = libeventd_format_string_get_string(bus->topic, event, NULL, NULL);

        PyObject *py_modname = PyUnicode_FromString(modname);
        PyObject *py_topic = PyUnicode_FromString(topic);

        PyObject *args = PyTuple_New(0);
        PyObject *kwargs = PyDict_New();
        int ret = 0;
        ret |= PyDict_SetItemString(kwargs, "modname", py_modname);
        ret |= PyDict_SetItemString(kwargs, "topic", py_topic);
        ret |= PyDict_SetItemString(kwargs, "msg", _eventd_fedmsg_event_to_python_dict(event));

        if (!ret)
            _eventd_fedmsg_python_call(bus->bus->fedmsg_ctx, "publish", args, kwargs);
        else
            g_warning("failed to construct arguments for publishing");
        Py_DECREF(args);
        Py_DECREF(kwargs);
    }
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
