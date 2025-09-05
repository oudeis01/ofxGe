#include "PlatformUtils.h"

PlatformUtils::Platform PlatformUtils::platform_cache = PlatformUtils::UNKNOWN;
bool PlatformUtils::platform_detected = false;

PlatformUtils::Platform PlatformUtils::getCurrentPlatform() {
    if (!platform_detected) {
#ifdef TARGET_OSX
        platform_cache = MACOS;
#elif defined(TARGET_LINUX)
        platform_cache = LINUX;
#else
        platform_cache = UNKNOWN;
        ofLogWarning("PlatformUtils") << "Unsupported platform detected - only Linux and macOS are supported";
#endif
        platform_detected = true;
    }
    return platform_cache;
}

std::string PlatformUtils::getDynamicLibraryExtension() {
    switch (getCurrentPlatform()) {
        case LINUX:
            return "so";
        case MACOS:
            return "dylib";
        default:
            ofLogWarning("PlatformUtils") << "Unknown platform, defaulting to .so";
            return "so";
    }
}

std::string PlatformUtils::getDynamicLibraryPrefix() {
    switch (getCurrentPlatform()) {
        case LINUX:
        case MACOS:
            return "lib";
        default:
            return "lib";
    }
}

std::vector<std::string> PlatformUtils::getAllSupportedExtensions() {
    std::vector<std::string> extensions;
    
    // 현재 플랫폼의 기본 확장자 추가
    extensions.push_back(getDynamicLibraryExtension());
    
    // macOS의 경우 .so도 지원 (호환성)
    if (getCurrentPlatform() == MACOS) {
        extensions.push_back("so");
    }
    
    return extensions;
}

std::string PlatformUtils::normalizePath(const std::string& path) {
    // Linux와 macOS 모두 UNIX 계열이므로 / 사용
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    return normalized;
}

std::string PlatformUtils::getPlatformName() {
    switch (getCurrentPlatform()) {
        case LINUX:
            return "Linux";
        case MACOS:
            return "macOS";
        default:
            return "Unknown";
    }
}