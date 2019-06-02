
#ifndef __PLUGIN__LOAD_HPP__
#define __PLUGIN__LOAD_HPP__
class PluginLoad {
    public:
        static bool plugin_load(const char *filename);
    private:
        PluginLoad() = delete;
};
#endif // __PLUGIN__LOAD_HPP__