#include "DynamicLoader.h"
#include "ofLog.h"
#include <dlfcn.h>

DynamicLoader::LibraryHandle DynamicLoader::loadLibrary(const std::string& path) {
    LibraryHandle handle;
    handle.path = path;
    handle.is_valid = false;
    
    // Linux와 macOS 모두 dlopen 사용
    void* lib = dlopen(path.c_str(), RTLD_LAZY);
    if (lib) {
        handle.handle = lib;
        handle.is_valid = true;
        ofLogNotice("DynamicLoader") << "Successfully loaded library: " << path;
    } else {
        ofLogError("DynamicLoader") << "Failed to load library: " << path 
                                   << " - " << getLastError();
    }
    
    return handle;
}

void* DynamicLoader::getSymbol(const LibraryHandle& lib, const std::string& symbol_name) {
    if (!lib.is_valid) {
        ofLogError("DynamicLoader") << "Cannot get symbol from invalid library handle";
        return nullptr;
    }
    
    void* symbol = dlsym(lib.handle, symbol_name.c_str());
    if (!symbol) {
        ofLogError("DynamicLoader") << "Symbol '" << symbol_name << "' not found in " << lib.path
                                   << " - " << getLastError();
    }
    
    return symbol;
}

bool DynamicLoader::unloadLibrary(LibraryHandle& lib) {
    if (!lib.is_valid) {
        return true;
    }
    
    bool success = (dlclose(lib.handle) == 0);
    
    if (success) {
        ofLogNotice("DynamicLoader") << "Successfully unloaded library: " << lib.path;
        lib.handle = nullptr;
        lib.is_valid = false;
    } else {
        ofLogError("DynamicLoader") << "Failed to unload library: " << lib.path 
                                   << " - " << getLastError();
    }
    
    return success;
}

std::string DynamicLoader::getLastError() {
    const char* error = dlerror();
    return error ? std::string(error) : "No error";
}

std::string DynamicLoader::formatError(const std::string& operation, const std::string& detail) {
    return operation + " failed: " + detail;
}