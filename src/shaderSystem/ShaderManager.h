#pragma once
#include "ShaderNode.h"
#include "BuiltinVariables.h"
#include "../pluginSystem/PluginManager.h"
#include "ofMain.h"
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief GLSL 셰이더의 생성, 관리, 캐싱을 담당하는 매니저 클래스
 * PluginManager와 협력하여 GLSL 함수를 실제 컴파일된 셰이더로 변환
 */
class ShaderManager {
private:
    // 의존성
    PluginManager* plugin_manager;
    
    // 셰이더 캐시 (함수명+인자 조합을 키로 사용)
    std::unordered_map<std::string, std::shared_ptr<ShaderNode>> shader_cache;
    
    // 셰이더 템플릿
    std::string default_vertex_shader;
    std::string default_fragment_shader_template;
    
public:
    // 생성자/소멸자
    ShaderManager(PluginManager* pm);
    ~ShaderManager();
    
    // === 핵심 기능 ===
    // GLSL 함수명과 인자들로부터 셰이더 노드 생성
    std::shared_ptr<ShaderNode> createShader(
        const std::string& function_name, 
        const std::vector<std::string>& arguments
    );
    
    // === GLSL 파일 처리 ===
    // 플러그인에서 GLSL 파일 로드
    std::string loadGLSLFile(const std::string& plugin_name, const std::string& file_path);
    
    // 함수 메타데이터를 기반으로 GLSL 파일 로드
    std::string loadGLSLFunction(const GLSLFunction* function_metadata, const std::string& plugin_name);
    
    // === 셰이더 코드 생성 ===
    // 기본 vertex 셰이더 생성
    std::string generateVertexShader();
    
    // GLSL 함수와 인자를 기반으로 fragment 셰이더 생성
    std::string generateFragmentShader(
        const std::string& glsl_function_code,
        const std::string& function_name,
        const std::vector<std::string>& arguments
    );
    
    // === 래핑 함수 생성 시스템 ===
    // 사용자 인자와 가장 근사한 오버로드 찾기
    const FunctionOverload* findBestOverload(
        const GLSLFunction* function_metadata,
        const std::vector<std::string>& user_arguments
    );
    
    // 래핑 함수 코드 생성
    std::string generateWrapperFunction(
        const std::string& function_name,
        const std::vector<std::string>& user_arguments,
        const FunctionOverload* target_overload
    );
    
    // === 캐시 관리 ===
    // 셰이더 캐시에서 검색
    std::shared_ptr<ShaderNode> getCachedShader(const std::string& shader_key);
    
    // 캐시에 셰이더 저장
    void cacheShader(const std::string& shader_key, std::shared_ptr<ShaderNode> shader_node);
    
    // 캐시 정리
    void clearCache();
    
    // === 유틸리티 ===
    // 파일 경로 해결 (플러그인 디렉토리 + 함수 파일 경로)
    std::string resolveGLSLFilePath(const std::string& plugin_name, const std::string& function_file_path);
    
    // GLSL include는 ofShader가 자동으로 처리함
    
    // 캐시 키 생성
    std::string generateCacheKey(const std::string& function_name, const std::vector<std::string>& arguments);
    
    // 리터럴 상수 판별
    bool isFloatLiteral(const std::string& str);
    
    // 인자들이 벡터로 결합 가능한지 확인
    bool canCombineToVector(const std::vector<std::string>& arguments, const std::string& target_type);
    
    // === 디버깅 및 정보 ===
    // 캐시 상태 출력
    void printCacheInfo() const;
    
    // 셰이더 생성 과정 로깅
    void setDebugMode(bool debug);
    
private:
    // 내부 상태
    bool debug_mode;
    
    // === 내부 헬퍼 메서드 ===
    // 기본 셰이더 템플릿 초기화
    void initializeShaderTemplates();
    
    // 함수 인자를 uniform으로 변환
    std::string generateUniforms(const std::vector<std::string>& arguments);
    
    // 메인 함수 내용 생성
    std::string generateMainFunction(const std::string& function_name, const std::vector<std::string>& arguments);
    
    // 파일 읽기 헬퍼
    std::string readFileContent(const std::string& file_path);
    
    // 에러 처리 헬퍼
    std::shared_ptr<ShaderNode> createErrorShader(const std::string& function_name, 
                                                   const std::vector<std::string>& arguments,
                                                   const std::string& error_message);
};