#include "pluginLoad.hpp"

int main(int argc, char **argv) {
    if(argc == 2) {
        PluginLoad::plugin_load(argv[1]);
    }
    return 0;
}