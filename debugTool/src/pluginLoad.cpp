#include <stdio.h>
#include <gmodule.h>
#include "pluginLoad.hpp"


#ifdef VERBOSE
#define DB_PRINTF(f_, ...) printf((f_), ##__VA_ARGS__)
#else
#define DB_PRINTF(f_, ...) (void)0
#endif

//#include "/usr/include/glib-2.0/gmodule.h"
typedef bool (*PluginInitFunc)(const char *);
typedef bool (*PluginNeedsBeforeInsnFunc)(u_int64_t, void *);
typedef void (*PluginBeforeInsnFunc)(u_int64_t, void *);
typedef void (*PluginAfterMemFunc)(void *, u_int64_t, int, int);
typedef void (*PluginBeeforeInterupt)(void *, u_int64_t);

PluginInitFunc* init;
PluginNeedsBeforeInsnFunc* needs_before_insn;
PluginBeforeInsnFunc* before_insn;
PluginAfterMemFunc* after_mem;
bool enable_instrumentation = true;

bool PluginLoad::plugin_load(const char *filename)
{
    GModule *g_module;
    bool retValue = true;
    if (!filename) {
        DB_PRINTF("plugin name was not specified");
        return false;
    }
    g_module = g_module_open(filename,
        G_MODULE_BIND_LAZY);
    if (!g_module) {
        DB_PRINTF("can't load plugin '%s'", filename);
        DB_PRINTF("error: %s",g_module_error ());
        return false;
    }
    DB_PRINTF("plugin '%s' Loaded!", filename);

    if (!g_module_symbol(g_module, "plugin_init",  (gpointer*)&init) ) {
        DB_PRINTF("plugin_init failed to load is: 0x%p !", init);
        DB_PRINTF("plugin_init error: %s",g_module_error ());
        retValue = false;

    }
    /* Get the instrumentation callbacks */
    if (! g_module_symbol(g_module, "plugin_needs_before_insn", (gpointer*)&needs_before_insn) ) {
            DB_PRINTF("needs_before_insn is: 0x%p !", needs_before_insn);
            DB_PRINTF("needs_before_insn error: %s",g_module_error ());
            retValue = false;
    }
    if (! g_module_symbol(g_module, "plugin_before_insn",(gpointer*)&before_insn) ) {
            DB_PRINTF("before_insn is: 0x%p !", before_insn);
            DB_PRINTF("before_insn error: %s",g_module_error ());
            retValue = false;
        }

    if (! g_module_symbol(g_module, "plugin_after_mem",
        (gpointer*)&after_mem) ) {

            DB_PRINTF("after_mem is: 0x%p !", after_mem);
            DB_PRINTF("after_mem error: %s",g_module_error ());
            retValue = false;
        }
    
     g_module_close (g_module);
    return retValue;
}
