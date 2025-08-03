#pragma once
#include "ofMain.h"
#include <vector>
#include <string>
#include <memory>
#include <map>

/**
 * @brief 셰이더의 생명주기를 관리하는 구조체
 * GLSL 함수와 인자들로부터 생성된 셰이더의 상태와 데이터를 포함
 */
struct ShaderNode {
    // 셰이더 메타데이터
    std::string function_name;           // 사용된 GLSL 함수명
    std::vector<std::string> arguments;  // 함수에 전달된 인자들
    std::string shader_key;              // 캐싱을 위한 고유 키
    
    // 셰이더 코드
    std::string vertex_shader_code;      // 생성된 vertex 셰이더 코드
    std::string fragment_shader_code;    // 생성된 fragment 셰이더 코드
    std::string glsl_function_code;      // 원본 GLSL 함수 코드
    std::string source_directory_path;   // GLSL include 해결용 소스 디렉토리 경로
    
    // OpenFrameworks 셰이더 객체
    ofShader compiled_shader;            // 컴파일된 셰이더
    
    // 유니폼 관리
    std::map<std::string, float> float_uniforms;    // float 유니폼들
    std::map<std::string, ofVec2f> vec2_uniforms;   // vec2 유니폼들
    bool auto_update_time;               // 시간 유니폼 자동 업데이트 여부
    bool auto_update_resolution;         // 해상도 유니폼 자동 업데이트 여부
    
    // 상태 관리
    bool is_compiled;                    // 컴파일 성공 여부
    bool has_error;                      // 에러 발생 여부
    std::string error_message;           // 에러 메시지
    
    // 생성자
    ShaderNode();
    ShaderNode(const std::string& func_name, const std::vector<std::string>& args);
    
    // 소멸자
    ~ShaderNode();
    
    // 생명주기 관리 메서드
    bool compile();                      // 셰이더 컴파일
    void cleanup();                      // 리소스 정리
    bool isReady() const;                // 사용 가능 상태 확인
    
    // 유틸리티 메서드
    std::string generateShaderKey() const;           // 캐싱 키 생성
    void setShaderCode(const std::string& vertex, const std::string& fragment);
    void setError(const std::string& error);
    
    // 유니폼 관리 메서드
    void setFloatUniform(const std::string& name, float value);
    void setVec2Uniform(const std::string& name, const ofVec2f& value);
    void setAutoUpdateTime(bool enable);             // 시간 자동 업데이트 설정
    void setAutoUpdateResolution(bool enable);       // 해상도 자동 업데이트 설정
    void updateUniforms();                           // 모든 유니폼 업데이트
    void updateAutoUniforms();                       // 자동 유니폼만 업데이트
    
    // 디버깅 메서드
    void printDebugInfo() const;
    std::string getStatusString() const;
};