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
    // st: 화면 좌표 (0.0~1.0)
    builtins["st"] = BuiltinVariable(
        "st", 
        "vec2", 
        2,           // 2개 성분
        true,        // resolution uniform 필요
        true,        // 메인함수에서 선언 필요
        "vec2 st = gl_FragCoord.xy / resolution;"
    );
    
    // time: 경과 시간
    builtins["time"] = BuiltinVariable(
        "time",
        "float",
        1,           // 1개 성분
        true,        // time uniform 필요
        false,       // 선언 불필요 (직접 uniform 사용)
        ""
    );
    
    // resolution: 화면 해상도
    builtins["resolution"] = BuiltinVariable(
        "resolution",
        "vec2",
        2,           // 2개 성분
        true,        // resolution uniform 필요
        false,       // 선언 불필요 (직접 uniform 사용)
        ""
    );
    
    // gl_FragCoord: GLSL 내장 변수
    builtins["gl_FragCoord"] = BuiltinVariable(
        "gl_FragCoord",
        "vec4",
        4,           // 4개 성분
        false,       // uniform 불필요 (GLSL 내장)
        false,       // 선언 불필요 (GLSL 내장)
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
    // 리터럴 값은 항상 유효
    if (isFloatLiteral(variable)) {
        return true;
    }
    
    // swizzle이 없는 경우 항상 유효
    if (!hasSwizzle(variable)) {
        return true;
    }
    
    std::string baseVar = extractBaseVariable(variable);
    std::string swizzle = extractSwizzle(variable);
    
    // 기본 변수가 builtin인지 확인
    const BuiltinVariable* info = getBuiltinInfo(baseVar);
    if (!info) {
        errorMessage = "Unknown variable '" + baseVar + "'";
        return false;
    }
    
    // swizzle 성분들이 유효한지 확인
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
    
    // component_count에 따라 지원하는 성분 반환
    switch (info->component_count) {
        case 1: return "x";           // float: x
        case 2: return "xy";          // vec2: x, y
        case 3: return "xyz";         // vec3: x, y, z
        case 4: return "xyzw";        // vec4: x, y, z, w
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
    // 숫자로만 구성되어 있거나 소수점이 포함된 경우 리터럴로 판단
    if (str.empty()) return false;

    bool has_dot = false;
    for (char c : str) {
        if (c == '.') {
            if (has_dot) return false; // 소수점이 두 번 나오면 안됨
            has_dot = true;
        } else if (!std::isdigit(c)) {
            return false; // 숫자가 아닌 문자가 있으면 안됨
        }
    }
    return true;
}