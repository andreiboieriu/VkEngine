#include <iostream>
#include <dlfcn.h>
#include "script.h"

typedef Script* (*create_script_t)();

int main() {
    void* handle = NULL;

    handle = dlopen("./libderived_script.so", RTLD_LAZY);

    if (!handle) {
        std::cerr << "failed to open lib: " << dlerror() << "\n";
        return 1;
    }

    // int (*add_ptr)(int, int) = (int(*)(int, int))dlsym(handle, "_Z3addii");
    create_script_t create_script = (create_script_t)dlsym(handle, "createScript");

    if (!create_script) {
        std::cerr << "failed to load symbol add: " << dlerror() << "\n";
        dlclose(handle);
        return 1;
    }

    Script* awala = create_script();

    awala->execute();

    std::cout << "Script type: " << typeid(*awala).name() << std::endl;

    dlclose(handle);
    return 0;
}