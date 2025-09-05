#pragma once
#include "ofMain.h"
#include <string>
#include <vector>

class PlatformUtils {
public:
    enum Platform {
        LINUX,
        MACOS,
        UNKNOWN
    };
    
    static Platform getCurrentPlatform();
    static std::string getDynamicLibraryExtension();
    static std::string getDynamicLibraryPrefix();
    static std::vector<std::string> getAllSupportedExtensions();
    static std::string normalizePath(const std::string& path);
    static std::string getPlatformName();
    
private:
    static Platform platform_cache;
    static bool platform_detected;
};