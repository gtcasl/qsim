#define CATCH_CONFIG_MAIN

#include <catch2/catch.hpp>
#include "pluginLoad.hpp"

TEST_CASE("plugin demos") {
    REQUIRE(PluginLoad::plugin_load("../../qemu/plugins/qsimPlugin/qsim-plugin.so"));
    REQUIRE(PluginLoad::plugin_load("../../qemu/plugins/testPlugin/test-plugin.so"));
}