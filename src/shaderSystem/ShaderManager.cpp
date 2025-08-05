#include "ShaderManager.h"
#include "BuiltinVariables.h"
#include "ofLog.h"
#include <algorithm>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>

//--------------------------------------------------------------
ShaderManager::ShaderManager(PluginManager * pm)
	: plugin_manager(pm)
	, debug_mode(true) {

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
	const std::string & function_name,
	const std::vector<std::string> & arguments) {

	if (debug_mode) {
		ofLogNotice("ShaderManager") << "Creating shader for function: " << function_name
									 << " with " << arguments.size() << " arguments";
	}

	// 인자 검증 - swizzle 유효성 확인
	BuiltinVariables& builtins = BuiltinVariables::getInstance();
	for (const auto& arg : arguments) {
		std::string errorMessage;
		if (!builtins.isValidSwizzle(arg, errorMessage)) {
			return createErrorShader(function_name, arguments, errorMessage);
		}
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
	const GLSLFunction * function_metadata = plugin_manager->findFunction(function_name);
	if (!function_metadata) {
		std::string error = "Function '" + function_name + "' not found in any loaded plugin";
		return createErrorShader(function_name, arguments, error);
	}

	// GLSL 함수 코드 로드
	std::string plugin_name = ""; // 함수를 찾은 플러그인 이름을 얻어야 함
	auto loaded_plugins = plugin_manager->getLoadedPlugins();
	for (const auto & plugin_info : loaded_plugins) {
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

	// st 기반 변수 확인 (st, st.x, st.y, st.xy 등 모든 swizzle 포함)
	bool has_st = false;
	for (const auto & arg : arguments) {
		std::string base_var = builtins.extractBaseVariable(arg);
		if (base_var == "st") {
			has_st = true;
			break;
		}
	}

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
std::string ShaderManager::loadGLSLFunction(const GLSLFunction * function_metadata, const std::string & plugin_name) {
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
std::string ShaderManager::resolveGLSLFilePath(const std::string & plugin_name, const std::string & function_file_path) {
	// 플러그인의 데이터 디렉토리 경로 가져오기
	auto plugin_paths = plugin_manager->getPluginPaths();
	std::string plugin_data_dir;

	for (const auto & [name, path] : plugin_paths) {
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
std::string ShaderManager::readFileContent(const std::string & file_path) {
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
	const std::string & glsl_function_code,
	const std::string & function_name,
	const std::vector<std::string> & arguments) {

	std::string fragment_code = default_fragment_shader_template;

	// Uniforms 생성
	std::string uniforms = generateUniforms(arguments);

	// 래핑 함수 생성 (필요한 경우)
	std::string wrapper_functions = "";
	const GLSLFunction * function_metadata = plugin_manager->findFunction(function_name);
	if (function_metadata && !function_metadata->overloads.empty()) {
		const FunctionOverload * best_overload = findBestOverload(function_metadata, arguments);
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
bool ShaderManager::isFloatLiteral(const std::string & str) {
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

//--------------------------------------------------------------
bool ShaderManager::canCombineToVector(const std::vector<std::string> & arguments, const std::string & target_type) {
	// 벡터 타입별 필요한 성분 수
	int required_components = 0;
	if (target_type == "vec2") required_components = 2;
	else if (target_type == "vec3") required_components = 3;
	else if (target_type == "vec4") required_components = 4;
	else if (target_type == "float") required_components = 1;
	else return false;
	
	// 총 성분 수 계산
	int total_components = 0;
	BuiltinVariables & builtins = BuiltinVariables::getInstance();
	
	for (const auto & arg : arguments) {
		if (isFloatLiteral(arg)) {
			total_components += 1; // 리터럴은 1개 성분
		} else {
			std::string errorMessage; // 에러 메시지 저장용 (사용하지 않음)
			if (builtins.isValidSwizzle(arg, errorMessage)) {
				// swizzle의 성분 수 계산
				size_t dot_pos = arg.find('.');
				if (dot_pos != std::string::npos) {
					std::string swizzle_part = arg.substr(dot_pos + 1);
					total_components += swizzle_part.length();
				} else {
					// 기본 변수 (st 등)
					if (arg == "st") total_components += 2;
					else total_components += 1;
				}
			} else {
				// 일반 변수 (time 등)
				total_components += 1;
			}
		}
	}
	
	return total_components == required_components;
}

std::string ShaderManager::generateUniforms(const std::vector<std::string> & arguments) {
	std::stringstream uniforms;
	BuiltinVariables & builtins = BuiltinVariables::getInstance();
	std::set<std::string> needed_uniforms;

	// 사용자 인자들을 분석하여 필요한 유니폼 수집
	for (const auto & arg : arguments) {
		// 리터럴은 스킵
		if (isFloatLiteral(arg)) {
			continue;
		}

		// swizzle이 있으면 기본 변수명 추출
		std::string base_var = builtins.extractBaseVariable(arg);
		const BuiltinVariable * builtin_info = builtins.getBuiltinInfo(base_var);

		if (builtin_info) {
			// 빌트인 변수가 유니폼이 필요한 경우
			if (builtin_info->needs_uniform) {
				if (base_var == "st") {
					needed_uniforms.insert("resolution");
				} else if (base_var == "time") {
					needed_uniforms.insert("time");
				} else if (base_var == "resolution") {
					needed_uniforms.insert("resolution");
				}
				// gl_FragCoord는 유니폼 불필요
			}
		} else {
			// 일반 변수는 유니폼으로 추가
			needed_uniforms.insert(arg);
		}
	}

	// 필요한 유니폼들 생성
	for (const auto & uniform_name : needed_uniforms) {
		if (uniform_name == "time") {
			uniforms << "uniform float time;\n";
		} else if (uniform_name == "resolution") {
			uniforms << "uniform vec2 resolution;\n";
		} else {
			uniforms << "uniform float " << uniform_name << ";\n";
		}
	}

	return uniforms.str();
}

//--------------------------------------------------------------
std::string ShaderManager::generateMainFunction(const std::string & function_name, const std::vector<std::string> & arguments) {
	std::stringstream main_func;
	BuiltinVariables & builtins = BuiltinVariables::getInstance();
	std::set<std::string> needed_declarations;

	// 사용자 인자들을 분석하여 필요한 선언 수집
	for (const auto & arg : arguments) {
		// 리터럴은 스킵
		if (isFloatLiteral(arg)) {
			continue;
		}

		// swizzle이 있으면 기본 변수명 추출
		std::string base_var = builtins.extractBaseVariable(arg);
		const BuiltinVariable * builtin_info = builtins.getBuiltinInfo(base_var);

		if (builtin_info && builtin_info->needs_declaration) {
			needed_declarations.insert(base_var);
		}
	}

	// 필요한 선언들 생성
	for (const auto & var_name : needed_declarations) {
		const BuiltinVariable * builtin_info = builtins.getBuiltinInfo(var_name);
		if (builtin_info) {
			main_func << "    " << builtin_info->declaration_code << "\n";
		}
	}

	if (!needed_declarations.empty()) {
		main_func << "    \n"; // 빈 줄 추가
	}

	// 래핑 함수 존재 여부 확인
	const GLSLFunction * function_metadata = plugin_manager->findFunction(function_name);
	bool needs_wrapper = false;
	if (function_metadata && !function_metadata->overloads.empty()) {
		const FunctionOverload * best_overload = findBestOverload(function_metadata, arguments);
		needs_wrapper = (best_overload != nullptr);
	}

	// 함수 호출 코드 생성
	main_func << "    // Call the GLSL function\n";
	main_func << "    float result = ";

	if (needs_wrapper) {
		// 래핑 함수 호출 - 오버로드된 함수 호출
		main_func << function_name << "(";
	} else {
		// 직접 함수 호출
		main_func << function_name << "(";
	}

	// 사용자 인자들 전달 (실제 값 전달)
	// builtins 변수는 이미 위에서 선언됨
	int arg_index = 0;

	for (size_t i = 0; i < arguments.size(); ++i) {
		if (arg_index > 0) main_func << ", ";

		// 리터럴과 변수 모두 원본 그대로 전달 (실제 값)
		main_func << arguments[i]; // st.x, time, 1.0 그대로

		arg_index++;
	}
	main_func << ");\n";
	main_func << "    \n";

	// 노이즈 결과 정규화 및 시각화 개선
	// if (function_name == "snoise") {
	// 	main_func << "    // Normalize noise result from [-1,1] to [0,1] for better visibility\n";
	// 	main_func << "    result = result * 0.5 + 0.5;\n";
	// 	main_func << "    \n";
	// }

	// 결과를 색상으로 출력
	main_func << "    outputColor = vec4(vec3(result), 1.0);\n";

	return main_func.str();
}

//--------------------------------------------------------------
std::string ShaderManager::generateCacheKey(const std::string & function_name, const std::vector<std::string> & arguments) {
	std::string key = function_name;
	for (const auto & arg : arguments) {
		key += "_" + arg;
	}
	return key;
}

//--------------------------------------------------------------
std::shared_ptr<ShaderNode> ShaderManager::getCachedShader(const std::string & shader_key) {
	auto it = shader_cache.find(shader_key);
	return (it != shader_cache.end()) ? it->second : nullptr;
}

//--------------------------------------------------------------
void ShaderManager::cacheShader(const std::string & shader_key, std::shared_ptr<ShaderNode> shader_node) {
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
	const std::string & function_name,
	const std::vector<std::string> & arguments,
	const std::string & error_message) {

	auto error_shader = std::make_shared<ShaderNode>(function_name, arguments);
	error_shader->setError(error_message);

	ofLogError("ShaderManager") << "Created error shader: " << error_message;

	return error_shader;
}

//--------------------------------------------------------------
void ShaderManager::printCacheInfo() const {
	ofLogNotice("ShaderManager") << "=== Shader Cache Info ===";
	ofLogNotice("ShaderManager") << "Total cached shaders: " << shader_cache.size();

	for (const auto & [key, shader] : shader_cache) {
		ofLogNotice("ShaderManager") << "  " << key << " -> " << shader->getStatusString();
	}
}

//--------------------------------------------------------------
void ShaderManager::setDebugMode(bool debug) {
	debug_mode = debug;
	ofLogNotice("ShaderManager") << "Debug mode: " << (debug ? "ON" : "OFF");
}

//--------------------------------------------------------------
const FunctionOverload * ShaderManager::findBestOverload(
	const GLSLFunction * function_metadata,
	const std::vector<std::string> & user_arguments) {

	if (!function_metadata || function_metadata->overloads.empty()) {
		return nullptr;
	}

	// 사용자 인자들의 총 성분 수 계산 (빌트인 변수 시스템 사용)
	BuiltinVariables & builtins = BuiltinVariables::getInstance();
	int total_components = 0;

	for (const auto & arg : user_arguments) {
		// 리터럴은 스킵
		if (isFloatLiteral(arg)) {
			total_components += 1;
			continue;
		}

		// swizzle이 있으면 기본 변수명 추출
		std::string base_var = builtins.extractBaseVariable(arg);
		const BuiltinVariable * builtin_info = builtins.getBuiltinInfo(base_var);

		if (builtin_info) {
			// swizzle이 있으면 swizzle 길이로 성분 수 결정
			if (builtins.hasSwizzle(arg)) {
				std::string swizzle = builtins.extractSwizzle(arg);
				total_components += swizzle.length(); // 예: "xy" = 2개 성분
			} else {
				total_components += builtin_info->component_count;
			}
		} else {
			total_components += 1; // 일반 float 변수
		}
	}

	if (debug_mode) {
		ofLogNotice("ShaderManager") << "User arguments need " << total_components << " components total";
	}

	// 1. 정확히 매치되는 단일 벡터 오버로드 찾기
	for (const auto & overload : function_metadata->overloads) {
		if (overload.paramTypes.size() == 1) {
			const std::string & param_type = overload.paramTypes[0];
			int required_components = 0;

			if (param_type == "vec2")
				required_components = 2;
			else if (param_type == "vec3")
				required_components = 3;
			else if (param_type == "vec4")
				required_components = 4;
			else if (param_type == "float")
				required_components = 1;

			// 총 성분 수가 같고, 인자들이 실제로 조합 가능한지 확인
			if (required_components == total_components && canCombineToVector(user_arguments, param_type)) {
				if (debug_mode) {
					ofLogNotice("ShaderManager") << "Found perfect match: " << param_type
												 << " for " << total_components << " components";
				}
				return &overload;
			}
		}
	}

	// 2. 다중 매개변수 오버로드에서 인자 수가 정확히 매치되는 것 찾기
	for (const auto & overload : function_metadata->overloads) {
		if (overload.paramTypes.size() == user_arguments.size()) {
			if (debug_mode) {
				ofLogNotice("ShaderManager") << "Found multi-param match: " << overload.paramTypes.size() << " params";
			}
			return &overload;
		}
	}

	// 3. 가장 유사한 단일 벡터 오버로드 찾기
	const FunctionOverload * best_match = nullptr;
	int min_component_diff = INT_MAX;

	for (const auto & overload : function_metadata->overloads) {
		if (overload.paramTypes.size() == 1) {
			const std::string & param_type = overload.paramTypes[0];
			int required_components = 0;

			if (param_type == "vec2")
				required_components = 2;
			else if (param_type == "vec3")
				required_components = 3;
			else if (param_type == "vec4")
				required_components = 4;
			else if (param_type == "float")
				required_components = 1;

			int component_diff = abs(required_components - total_components);
			if (component_diff < min_component_diff) {
				min_component_diff = component_diff;
				best_match = &overload;
			}
		}
	}

	if (debug_mode && best_match) {
		ofLogNotice("ShaderManager") << "Best component match for " << function_metadata->name
									 << ": " << best_match->paramTypes[0];
	}

	return best_match;
}

//--------------------------------------------------------------
std::string ShaderManager::generateWrapperFunction(
	const std::string & function_name,
	const std::vector<std::string> & user_arguments,
	const FunctionOverload * target_overload) {

	if (!target_overload) {
		return ""; // 래핑이 필요 없음
	}

	std::stringstream wrapper;

	// 래핑 함수 시그니처 생성 (오버로드된 함수로 생성)
	wrapper << target_overload->returnType << " " << function_name << "(";
	int param_index = 0;
	int literal_index = 0;
	BuiltinVariables & builtins = BuiltinVariables::getInstance();

	for (size_t i = 0; i < user_arguments.size(); ++i) {
		if (param_index > 0) wrapper << ", ";

		// 리터럴 상수를 위한 매개변수 생성
		if (isFloatLiteral(user_arguments[i])) {
			wrapper << "float literal_" << literal_index; // literal_0, literal_1 등
			literal_index++;
		} else {
			// swizzle이 있으면 점을 언더바로 변경
			std::string param_name = user_arguments[i];
			std::replace(param_name.begin(), param_name.end(), '.', '_'); // st.x -> st_x

			// swizzle이 있으면 결과 타입에 따라 매개변수 타입 결정
			if (builtins.hasSwizzle(user_arguments[i])) {
				std::string swizzle = builtins.extractSwizzle(user_arguments[i]);
				if (swizzle.length() == 1) {
					wrapper << "float " << param_name; // st.x -> float st_x
				} else if (swizzle.length() == 2) {
					wrapper << "vec2 " << param_name; // st.xy -> vec2 st_xy
				} else if (swizzle.length() == 3) {
					wrapper << "vec3 " << param_name; // st.xyz -> vec3 st_xyz
				} else {
					wrapper << "vec4 " << param_name; // st.xyzw -> vec4 st_xyzw
				}
			} else {
				// 일반 변수는 빌트인 정보 또는 float로 처리
				std::string base_var = builtins.extractBaseVariable(user_arguments[i]);
				const BuiltinVariable * builtin_info = builtins.getBuiltinInfo(base_var);

				if (builtin_info) {
					wrapper << builtin_info->glsl_type << " " << user_arguments[i];
				} else {
					wrapper << "float " << user_arguments[i];
				}
			}
		}
		param_index++;
	}
	wrapper << ") {\n";

	// 원본 함수 호출 코드 생성
	wrapper << "    return " << function_name << "(";

	// 타겟 오버로드에 따라 인자 매핑
	if (target_overload->paramTypes.size() == 1) {
		// 단일 벡터 매개변수로 조합 (vec2/vec3/vec4)
		const std::string & param_type = target_overload->paramTypes[0];

		// 특별 처리: st.x와 같은 1차원 swizzle을 사용하는 노이즈 함수의 경우
		// 2차원 패턴을 생성하도록 수정
		bool has_1d_swizzle = false;
		std::string swizzle_base = "";
		for (const auto & arg : user_arguments) {
			if (builtins.hasSwizzle(arg)) {
				std::string swizzle = builtins.extractSwizzle(arg);
				if (swizzle.length() == 1) {
					has_1d_swizzle = true;
					swizzle_base = builtins.extractBaseVariable(arg);
					break;
				}
			}
		}

		if (has_1d_swizzle && swizzle_base == "st" && function_name == "snoise") {
			// st.x 노이즈의 경우: target_overload에서 지정된 매개변수 타입 사용
			const std::string& target_param_type = target_overload->paramTypes[0];
			
			wrapper << target_param_type << "(";

			int literal_index = 0;
			for (size_t i = 0; i < user_arguments.size(); ++i) {
				if (i > 0) wrapper << ", ";

				if (isFloatLiteral(user_arguments[i])) {
					wrapper << "literal_" << literal_index;
					literal_index++;
				} else {
					std::string param_name = user_arguments[i];
					std::replace(param_name.begin(), param_name.end(), '.', '_');
					wrapper << param_name;
				}
			}
			wrapper << ")";
		} else {
			// 기존 로직: 벡터 생성자 사용
			wrapper << param_type << "(";

			int literal_index = 0;
			for (size_t i = 0; i < user_arguments.size(); ++i) {
				if (i > 0) wrapper << ", ";

				// 리터럴은 literal_N으로, swizzle은 st_x로 변환하여 매개변수명 사용
				if (isFloatLiteral(user_arguments[i])) {
					wrapper << "literal_" << literal_index;
					literal_index++;
				} else {
					// swizzle이 있으면 점을 언더바로 변경하여 매개변수명 사용
					std::string param_name = user_arguments[i];
					std::replace(param_name.begin(), param_name.end(), '.', '_'); // st.x -> st_x
					wrapper << param_name;
				}
			}

			wrapper << ")";
		}
	} else {
		// 다중 매개변수 함수 - 직접 매핑
		for (size_t i = 0; i < user_arguments.size() && i < target_overload->paramTypes.size(); ++i) {
			if (i > 0) wrapper << ", ";
			wrapper << user_arguments[i];
		}
		// 부족한 매개변수는 0.0으로 채움
		for (size_t i = user_arguments.size(); i < target_overload->paramTypes.size(); ++i) {
			wrapper << ", 0.0";
		}
	}

	wrapper << ");\n";
	wrapper << "}\n";

	if (debug_mode) {
		ofLogNotice("ShaderManager") << "Generated wrapper function:\n"
									 << wrapper.str();
	}

	return wrapper.str();
}