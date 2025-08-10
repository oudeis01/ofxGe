#include "BuiltinVariables.h"

//--------------------------------------------------------------
BuiltinVariables& BuiltinVariables::getInstance() {
    static BuiltinVariables instance;
    return instance;
}

//--------------------------------------------------------------
BuiltinVariables::BuiltinVariables() {
    initializeBuiltins();
}

//--------------------------------------------------------------
void BuiltinVariables::initializeBuiltins() {
    // 'st': Normalized screen coordinates (0.0 to 1.0)
    builtins["st"] = BuiltinVariable(
        "st", 
        "vec2", 
        2,           // 2 components (x, y)
        true,        // requires 'resolution' uniform
        true,        // needs declaration in main()
        "vec2 st = gl_FragCoord.xy / resolution;"
    );
    
    // 'time': Elapsed time in seconds
    builtins["time"] = BuiltinVariable(
        "time",
        "float",
        1,           // 1 component
        true,        // requires 'time' uniform
        false,       // no local declaration needed, use uniform directly
        ""
    );
    
    // 'resolution': The dimensions of the viewport in pixels
    builtins["resolution"] = BuiltinVariable(
        "resolution",
        "vec2",
        2,           // 2 components (width, height)
        true,        // requires 'resolution' uniform
        false,       // no local declaration needed, use uniform directly
        ""
    );
    
    // 'gl_FragCoord': Fragment coordinates in window space (a standard GLSL built-in)
    builtins["gl_FragCoord"] = BuiltinVariable(
        "gl_FragCoord",
        "vec4",
        4,           // 4 components (x, y, z, w)
        false,       // no uniform needed
        false,       // no declaration needed
        ""
    );
}

//--------------------------------------------------------------
const BuiltinVariable* BuiltinVariables::getBuiltinInfo(const std::string& name) const {
    auto it = builtins.find(name);
    return (it != builtins.end()) ? &it->second : nullptr;
}

//--------------------------------------------------------------
bool BuiltinVariables::isBuiltin(const std::string& name) const {
    return builtins.find(name) != builtins.end();
}

//--------------------------------------------------------------
std::string BuiltinVariables::extractBaseVariable(const std::string& variable) const {
    size_t dot_pos = variable.find('.');
    if (dot_pos != std::string::npos) {
        return variable.substr(0, dot_pos);
    }
    return variable;
}

//--------------------------------------------------------------
bool BuiltinVariables::hasSwizzle(const std::string& variable) const {
    return variable.find('.') != std::string::npos;
}

//--------------------------------------------------------------
std::string BuiltinVariables::extractSwizzle(const std::string& variable) const {
    size_t dot_pos = variable.find('.');
    if (dot_pos != std::string::npos && dot_pos + 1 < variable.length()) {
        return variable.substr(dot_pos + 1);
    }
    return "";
}

//--------------------------------------------------------------
std::set<std::string> BuiltinVariables::getAllBuiltinNames() const {
    std::set<std::string> names;
    for (const auto& [name, info] : builtins) {
        names.insert(name);
    }
    return names;
}

//--------------------------------------------------------------
bool BuiltinVariables::isValidSwizzle(const std::string& variable, std::string& errorMessage) const {
    // A float literal is always considered valid.
    if (isFloatLiteral(variable)) {
        return true;
    }
    
    // If there's no swizzle, it's valid by default in this context.
    if (!hasSwizzle(variable)) {
        return true;
    }
    
    std::string baseVar = extractBaseVariable(variable);
    std::string swizzle = extractSwizzle(variable);
    
    // Check if the base variable is a known built-in.
    const BuiltinVariable* info = getBuiltinInfo(baseVar);
    if (!info) {
        errorMessage = "Unknown variable '" + baseVar + "'";
        return false;
    }
    
    // Check if the swizzle characters are valid for the base variable's type.
    std::string validComponents = getSupportedSwizzleComponents(baseVar);
    
    for (char c : swizzle) {
        if (validComponents.find(c) == std::string::npos) {
            std::string supportedList = formatSupportedComponents(info->glsl_type, info->component_count);
            errorMessage = "Invalid swizzle '" + variable + "': base variable '" + baseVar + 
                          "' supports components [" + supportedList + "]";
            return false;
        }
    }
    
    return true;
}

//--------------------------------------------------------------
std::string BuiltinVariables::getSupportedSwizzleComponents(const std::string& baseVariable) const {
    const BuiltinVariable* info = getBuiltinInfo(baseVariable);
    if (!info) return "";
    
    // Return valid swizzle characters based on the number of components.
    switch (info->component_count) {
        case 1: return "x";      // float: .x
        case 2: return "xy";     // vec2: .x, .y
        case 3: return "xyz";    // vec3: .x, .y, .z
        case 4: return "xyzw";   // vec4: .x, .y, .z, .w
        default: return "";
    }
}

//--------------------------------------------------------------
std::string BuiltinVariables::formatSupportedComponents(const std::string& glslType, int componentCount) const {
    std::string supportedComponents = "";
    
    switch (componentCount) {
        case 1:
            supportedComponents = "x";
            break;
        case 2:
            supportedComponents = "x, y";
            break;
        case 3:
            supportedComponents = "x, y, z";
            break;
        case 4:
            supportedComponents = "x, y, z, w";
            break;
    }
    
    return supportedComponents;
}

//--------------------------------------------------------------
bool BuiltinVariables::isFloatLiteral(const std::string& str) const {
    if (str.empty()) return false;

    bool has_dot = false;
    for (char c : str) {
        if (c == '.') {
            if (has_dot) return false; // A maximum of one decimal point is allowed.
            has_dot = true;
        } else if (!std::isdigit(c)) {
            return false; // Must contain only digits and an optional decimal point.
        }
    }
    return true;
}
