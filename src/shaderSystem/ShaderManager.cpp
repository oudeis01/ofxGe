#include "ShaderManager.h"
#include "ofLog.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <set>
#include <algorithm>

//--------------------------------------------------------------
ShaderManager::ShaderManager(PluginManager* pm) 
    : plugin_manager(pm), debug_mode(true) {
    
    if (!plugin_manager) {
        ofLogError("ShaderManager") << "PluginManager pointer is null";
        return;
    }
    
    initializeShaderTemplates();
    
    ofLogNotice("ShaderManager") << "ShaderManager initialized";
}

//--------------------------------------------------------------
ShaderManager::~ShaderManager() {
    clearCache();
}

//--------------------------------------------------------------
void ShaderManager::initializeShaderTemplates() {
    // 기본 vertex 셰이더 템플릿
    default_vertex_shader = R"(
#version 150

uniform mat4 modelViewProjectionMatrix;

in vec4 position;
in vec2 texcoord;

out vec2 vTexCoord;

void main() {
    vTexCoord = texcoord;
    gl_Position = modelViewProjectionMatrix * position;
}
)";

    // 기본 fragment 셰이더 템플릿 (placeholder 포함)
    default_fragment_shader_template = R"(
#version 150

in vec2 vTexCoord;
out vec4 outputColor;

// Uniforms will be inserted here
{UNIFORMS}

// GLSL function will be inserted here
{GLSL_FUNCTION}

void main() {
    // Main function content will be inserted here
    {MAIN_CONTENT}
}
)";
    
    if (debug_mode) {
        ofLogNotice("ShaderManager") << "Shader templates initialized";
    }
}

//--------------------------------------------------------------
std::shared_ptr<ShaderNode> ShaderManager::createShader(
    const std::string& function_name, 
    const std::vector<std::string>& arguments) {
    
    if (debug_mode) {
        ofLogNotice("ShaderManager") << "Creating shader for function: " << function_name 
                                     << " with " << arguments.size() << " arguments";
    }
    
    // 캐시 확인
    std::string cache_key = generateCacheKey(function_name, arguments);
    auto cached_shader = getCachedShader(cache_key);
    if (cached_shader && cached_shader->isReady()) {
        if (debug_mode) {
            ofLogNotice("ShaderManager") << "Returning cached shader: " << cache_key;
        }
        return cached_shader;
    }
    
    // 새 셰이더 노드 생성
    auto shader_node = std::make_shared<ShaderNode>(function_name, arguments);
    
    // 플러그인에서 함수 검색
    const GLSLFunction* function_metadata = plugin_manager->findFunction(function_name);
    if (!function_metadata) {
        std::string error = "Function '" + function_name + "' not found in any loaded plugin";
        return createErrorShader(function_name, arguments, error);
    }
    
    // GLSL 함수 코드 로드
    std::string plugin_name = ""; // 함수를 찾은 플러그인 이름을 얻어야 함
    auto loaded_plugins = plugin_manager->getLoadedPlugins();
    for (const auto& plugin_info : loaded_plugins) {
        // 플러그인명 추출 (형식: "alias (name version)")
        size_t start = plugin_info.find('(');
        if (start != std::string::npos) {
            plugin_name = plugin_info.substr(0, start);
            // 끝 공백 제거
            while (!plugin_name.empty() && plugin_name.back() == ' ') {
                plugin_name.pop_back();
            }
            break;
        }
    }
    
    std::string glsl_function_code = loadGLSLFunction(function_metadata, plugin_name);
    if (glsl_function_code.empty()) {
        std::string error = "Failed to load GLSL code for function: " + function_name;
        return createErrorShader(function_name, arguments, error);
    }
    
    shader_node->glsl_function_code = glsl_function_code;
    
    // GLSL 파일의 디렉토리 경로 설정 (include 해결용)
    std::string glsl_file_path = resolveGLSLFilePath(plugin_name, function_metadata->filePath);
    std::string directory_path = glsl_file_path;
    size_t last_slash = directory_path.find_last_of('/');
    if (last_slash != std::string::npos) {
        directory_path = directory_path.substr(0, last_slash);
    }
    shader_node->source_directory_path = directory_path;
    
    // 셰이더 코드 생성
    std::string vertex_code = generateVertexShader();
    std::string fragment_code = generateFragmentShader(glsl_function_code, function_name, arguments);
    
    shader_node->setShaderCode(vertex_code, fragment_code);
    
    // 셰이더 컴파일
    if (!shader_node->compile()) {
        ofLogError("ShaderManager") << "Failed to compile shader for function: " << function_name;
        return shader_node; // 에러 상태의 노드 반환
    }
    
    // 자동 유니폼 설정
    bool has_time = std::find(arguments.begin(), arguments.end(), "time") != arguments.end();
    bool has_st = std::find(arguments.begin(), arguments.end(), "st") != arguments.end();
    
    if (has_time) {
        shader_node->setAutoUpdateTime(true);
    }
    if (has_st) {
        shader_node->setAutoUpdateResolution(true);
    }
    
    // 캐시에 저장
    cacheShader(cache_key, shader_node);
    
    if (debug_mode) {
        ofLogNotice("ShaderManager") << "Successfully created shader: " << cache_key;
        std::cout << "=== VERTEX SHADER ===" << std::endl;
        std::cout << shader_node->vertex_shader_code << std::endl;
        std::cout << "=== FRAGMENT SHADER ===" << std::endl;
        std::cout << shader_node->fragment_shader_code << std::endl;
        std::cout << "===================" << std::endl;
    }
    
    return shader_node;
}

//--------------------------------------------------------------
std::string ShaderManager::loadGLSLFunction(const GLSLFunction* function_metadata, const std::string& plugin_name) {
    if (!function_metadata) {
        ofLogError("ShaderManager") << "Function metadata is null";
        return "";
    }
    
    std::string file_path = resolveGLSLFilePath(plugin_name, function_metadata->filePath);
    
    if (debug_mode) {
        ofLogNotice("ShaderManager") << "Loading GLSL file: " << file_path;
    }
    
    std::string content = readFileContent(file_path);
    
    // ofShader가 자체적으로 include를 처리하므로 전처리하지 않음
    // ofShader는 파일의 디렉토리를 기준으로 상대 경로를 자동 해결함
    
    return content;
}

//--------------------------------------------------------------
std::string ShaderManager::resolveGLSLFilePath(const std::string& plugin_name, const std::string& function_file_path) {
    // 플러그인의 데이터 디렉토리 경로 가져오기
    auto plugin_paths = plugin_manager->getPluginPaths();
    std::string plugin_data_dir;
    
    for (const auto& [name, path] : plugin_paths) {
        if (name == plugin_name) {
            plugin_data_dir = path;
            break;
        }
    }
    
    if (plugin_data_dir.empty()) {
        ofLogError("ShaderManager") << "Plugin data directory not found for: " << plugin_name;
        return "";
    }
    
    // 절대 경로 생성
    std::string full_path = plugin_data_dir + function_file_path;
    
    if (debug_mode) {
        ofLogNotice("ShaderManager") << "Resolved GLSL path: " << full_path;
    }
    
    return full_path;
}

// resolveGLSLIncludes 함수 제거됨 - ofShader가 자동으로 include를 처리함

//--------------------------------------------------------------
std::string ShaderManager::readFileContent(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        ofLogError("ShaderManager") << "Failed to open file: " << file_path;
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    std::string content = buffer.str();
    
    if (debug_mode) {
        ofLogNotice("ShaderManager") << "Read " << content.length() << " characters from: " << file_path;
    }
    
    return content;
}

//--------------------------------------------------------------
std::string ShaderManager::generateVertexShader() {
    return default_vertex_shader;
}

//--------------------------------------------------------------
std::string ShaderManager::generateFragmentShader(
    const std::string& glsl_function_code,
    const std::string& function_name,
    const std::vector<std::string>& arguments) {
    
    std::string fragment_code = default_fragment_shader_template;
    
    // Uniforms 생성
    std::string uniforms = generateUniforms(arguments);
    
    // 래핑 함수 생성 (필요한 경우)
    std::string wrapper_functions = "";
    const GLSLFunction* function_metadata = plugin_manager->findFunction(function_name);
    if (function_metadata && !function_metadata->overloads.empty()) {
        const FunctionOverload* best_overload = findBestOverload(function_metadata, arguments);
        if (best_overload) {
            wrapper_functions = generateWrapperFunction(function_name, arguments, best_overload);
            if (debug_mode && !wrapper_functions.empty()) {
                ofLogNotice("ShaderManager") << "Generated wrapper function for " << function_name;
            }
        }
    }
    
    // 메인 함수 내용 생성
    std::string main_content = generateMainFunction(function_name, arguments);
    
    // 원본 GLSL 함수와 래핑 함수를 조합
    std::string combined_functions = glsl_function_code;
    if (!wrapper_functions.empty()) {
        combined_functions += "\n\n// Generated wrapper function\n" + wrapper_functions;
    }
    
    // 템플릿 치환
    size_t pos = fragment_code.find("{UNIFORMS}");
    if (pos != std::string::npos) {
        fragment_code.replace(pos, 10, uniforms);
    }
    
    pos = fragment_code.find("{GLSL_FUNCTION}");
    if (pos != std::string::npos) {
        fragment_code.replace(pos, 15, combined_functions);
    }
    
    pos = fragment_code.find("{MAIN_CONTENT}");
    if (pos != std::string::npos) {
        fragment_code.replace(pos, 14, main_content);
    }
    
    return fragment_code;
}

//--------------------------------------------------------------
std::string ShaderManager::generateUniforms(const std::vector<std::string>& arguments) {
    std::stringstream uniforms;
    
    // 특별 변수들 확인
    bool has_st = std::find(arguments.begin(), arguments.end(), "st") != arguments.end();
    bool has_time = std::find(arguments.begin(), arguments.end(), "time") != arguments.end();
    
    // 기본 유니폼들 (필요한 경우에만)
    if (has_time) {
        uniforms << "uniform float time;\n";
    }
    if (has_st) {
        uniforms << "uniform vec2 resolution;\n";
    }
    
    // 특별 변수들과 중복되지 않는 사용자 유니폼 생성
    std::set<std::string> special_vars = {"time", "resolution", "st"}; // st는 gl_FragCoord에서 생성
    
    for (const auto& arg : arguments) {
        if (special_vars.find(arg) == special_vars.end()) {
            uniforms << "uniform float " << arg << ";\n";
        }
    }
    
    return uniforms.str();
}

//--------------------------------------------------------------
std::string ShaderManager::generateMainFunction(const std::string& function_name, const std::vector<std::string>& arguments) {
    std::stringstream main_func;
    
    // st 변수가 있으면 자동으로 해상도 기반 좌표 생성
    bool has_st = std::find(arguments.begin(), arguments.end(), "st") != arguments.end();
    if (has_st) {
        main_func << "    vec2 st = gl_FragCoord.xy / resolution;\n";
        main_func << "    \n";
    }
    
    // 함수 호출 코드 생성 (사용자 인자만 사용)
    main_func << "    // Call the GLSL function\n";
    main_func << "    float result = " << function_name << "(";
    
    // 사용자 인자들만 전달 (uv/pos 제거)
    for (size_t i = 0; i < arguments.size(); ++i) {
        if (i > 0) main_func << ", ";
        main_func << arguments[i];
    }
    main_func << ");\n";
    main_func << "    \n";
    
    // 결과를 색상으로 출력
    main_func << "    outputColor = vec4(vec3(result), 1.0);\n";
    
    return main_func.str();
}

//--------------------------------------------------------------
std::string ShaderManager::generateCacheKey(const std::string& function_name, const std::vector<std::string>& arguments) {
    std::string key = function_name;
    for (const auto& arg : arguments) {
        key += "_" + arg;
    }
    return key;
}

//--------------------------------------------------------------
std::shared_ptr<ShaderNode> ShaderManager::getCachedShader(const std::string& shader_key) {
    auto it = shader_cache.find(shader_key);
    return (it != shader_cache.end()) ? it->second : nullptr;
}

//--------------------------------------------------------------
void ShaderManager::cacheShader(const std::string& shader_key, std::shared_ptr<ShaderNode> shader_node) {
    shader_cache[shader_key] = shader_node;
    
    if (debug_mode) {
        ofLogNotice("ShaderManager") << "Cached shader: " << shader_key;
    }
}

//--------------------------------------------------------------
void ShaderManager::clearCache() {
    shader_cache.clear();
    
    if (debug_mode) {
        ofLogNotice("ShaderManager") << "Shader cache cleared";
    }
}

//--------------------------------------------------------------
std::shared_ptr<ShaderNode> ShaderManager::createErrorShader(
    const std::string& function_name, 
    const std::vector<std::string>& arguments,
    const std::string& error_message) {
    
    auto error_shader = std::make_shared<ShaderNode>(function_name, arguments);
    error_shader->setError(error_message);
    
    ofLogError("ShaderManager") << "Created error shader: " << error_message;
    
    return error_shader;
}

//--------------------------------------------------------------
void ShaderManager::printCacheInfo() const {
    ofLogNotice("ShaderManager") << "=== Shader Cache Info ===";
    ofLogNotice("ShaderManager") << "Total cached shaders: " << shader_cache.size();
    
    for (const auto& [key, shader] : shader_cache) {
        ofLogNotice("ShaderManager") << "  " << key << " -> " << shader->getStatusString();
    }
}

//--------------------------------------------------------------
void ShaderManager::setDebugMode(bool debug) {
    debug_mode = debug;
    ofLogNotice("ShaderManager") << "Debug mode: " << (debug ? "ON" : "OFF");
}

//--------------------------------------------------------------
const FunctionOverload* ShaderManager::findBestOverload(
    const GLSLFunction* function_metadata,
    const std::vector<std::string>& user_arguments) {
    
    if (!function_metadata || function_metadata->overloads.empty()) {
        return nullptr;
    }
    
    // 사용자가 원하는 총 매개변수 수: user_arguments만 (uv/pos 제거)
    size_t desired_param_count = user_arguments.size();
    
    // 1. 정확히 매치되는 오버로드 찾기
    for (const auto& overload : function_metadata->overloads) {
        if (overload.paramTypes.size() == desired_param_count) {
            if (debug_mode) {
                ofLogNotice("ShaderManager") << "Found exact parameter match for " << function_metadata->name;
            }
            return &overload;
        }
    }
    
    // 2. 가장 유사한 매개변수 수를 가진 오버로드 찾기
    const FunctionOverload* best_match = nullptr;
    int min_param_diff = INT_MAX;
    
    for (const auto& overload : function_metadata->overloads) {
        int param_diff = abs((int)overload.paramTypes.size() - (int)desired_param_count);
        if (param_diff < min_param_diff) {
            min_param_diff = param_diff;
            best_match = &overload;
        }
    }
    
    if (debug_mode && best_match) {
        ofLogNotice("ShaderManager") << "Best match for " << function_metadata->name 
                                     << ": " << best_match->paramTypes.size() << " params";
    }
    
    return best_match;
}

//--------------------------------------------------------------
std::string ShaderManager::generateWrapperFunction(
    const std::string& function_name,
    const std::vector<std::string>& user_arguments,
    const FunctionOverload* target_overload) {
    
    if (!target_overload) {
        return ""; // 래핑이 필요 없음
    }
    
    std::stringstream wrapper;
    
    // 래핑 함수 시그니처 생성 (사용자 변수명에 언더바 추가하여 유니폼과 구분)
    wrapper << target_overload->returnType << " " << function_name << "(";
    for (size_t i = 0; i < user_arguments.size(); ++i) {
        if (i > 0) wrapper << ", ";
        wrapper << "float _" << user_arguments[i]; // 매개변수명에 언더바 추가
    }
    wrapper << ") {\n";
    
    // 원본 함수 호출 코드 생성
    wrapper << "    return " << function_name << "(";
    
    // 타겟 오버로드에 따라 인자 매핑
    if (target_overload->paramTypes.size() == 1) {
        // vec2/vec3/vec4 단일 인자로 조합
        const std::string& param_type = target_overload->paramTypes[0];
        if (param_type == "vec2") {
            if (user_arguments.size() >= 2) {
                wrapper << "vec2(_" << user_arguments[0] << ", _" << user_arguments[1] << ")";
            } else if (user_arguments.size() >= 1) {
                wrapper << "vec2(_" << user_arguments[0] << ", 0.0)";
            } else {
                wrapper << "vec2(0.0)";
            }
        } else if (param_type == "vec3") {
            if (user_arguments.size() >= 3) {
                wrapper << "vec3(_" << user_arguments[0] << ", _" << user_arguments[1] << ", _" << user_arguments[2] << ")";
            } else if (user_arguments.size() >= 2) {
                wrapper << "vec3(_" << user_arguments[0] << ", _" << user_arguments[1] << ", 0.0)";
            } else if (user_arguments.size() >= 1) {
                wrapper << "vec3(_" << user_arguments[0] << ", 0.0, 0.0)";
            } else {
                wrapper << "vec3(0.0)";
            }
        } else if (param_type == "vec4") {
            if (user_arguments.size() >= 4) {
                wrapper << "vec4(_" << user_arguments[0] << ", _" << user_arguments[1] << ", _" << user_arguments[2] << ", _" << user_arguments[3] << ")";
            } else if (user_arguments.size() >= 3) {
                wrapper << "vec4(_" << user_arguments[0] << ", _" << user_arguments[1] << ", _" << user_arguments[2] << ", 0.0)";
            } else if (user_arguments.size() >= 2) {
                wrapper << "vec4(_" << user_arguments[0] << ", _" << user_arguments[1] << ", 0.0, 0.0)";
            } else if (user_arguments.size() >= 1) {
                wrapper << "vec4(_" << user_arguments[0] << ", 0.0, 0.0, 0.0)";
            } else {
                wrapper << "vec4(0.0)";
            }
        }
    } else {
        // 다중 매개변수 함수 - 직접 매핑 (언더바 추가)
        for (size_t i = 0; i < user_arguments.size() && i < target_overload->paramTypes.size(); ++i) {
            if (i > 0) wrapper << ", ";
            wrapper << "_" << user_arguments[i];
        }
        // 부족한 매개변수는 0.0으로 채움
        for (size_t i = user_arguments.size(); i < target_overload->paramTypes.size(); ++i) {
            wrapper << ", 0.0";
        }
    }
    
    wrapper << ");\n";
    wrapper << "}\n";
    
    if (debug_mode) {
        ofLogNotice("ShaderManager") << "Generated wrapper function:\n" << wrapper.str();
    }
    
    return wrapper.str();
}