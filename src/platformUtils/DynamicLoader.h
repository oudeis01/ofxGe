#pragma once
#include "PlatformUtils.h"
#include <string>

class DynamicLoader {
public:
    struct LibraryHandle {
        void* handle;
        std::string path;
        bool is_valid;
        
        LibraryHandle() : handle(nullptr), is_valid(false) {}
    };
    
    static LibraryHandle loadLibrary(const std::string& path);
    static void* getSymbol(const LibraryHandle& lib, const std::string& symbol_name);
    static bool unloadLibrary(LibraryHandle& lib);
    static std::string getLastError();
    
private:
    static std::string formatError(const std::string& operation, const std::string& detail);
};