#pragma once
#include <string>
#include <unordered_map>
#include <set>

/**
 * @brief 빌트인 변수 정보 구조체
 */
struct BuiltinVariable {
    std::string name;             // 변수명 (예: "st")
    std::string glsl_type;        // GLSL 타입 (예: "vec2", "float")
    int component_count;          // 성분 수 (vec2=2, float=1)
    bool needs_uniform;           // uniform 선언이 필요한지
    bool needs_declaration;       // 메인 함수에서 선언이 필요한지
    std::string declaration_code; // 선언 코드 (예: "vec2 st = gl_FragCoord.xy / resolution;")
    
    BuiltinVariable() = default;
    
    BuiltinVariable(const std::string& n, const std::string& type, int comp, 
                   bool uniform, bool decl, const std::string& decl_code = "")
        : name(n), glsl_type(type), component_count(comp), 
          needs_uniform(uniform), needs_declaration(decl), declaration_code(decl_code) {}
};

/**
 * @brief 빌트인 변수 관리 클래스
 */
class BuiltinVariables {
public:
    // 싱글톤 인스턴스 접근
    static BuiltinVariables& getInstance();
    
    // 빌트인 변수 정보 조회
    const BuiltinVariable* getBuiltinInfo(const std::string& name) const;
    
    // 변수가 빌트인인지 확인
    bool isBuiltin(const std::string& name) const;
    
    // swizzle에서 기본 변수명 추출 (예: "st.x" -> "st")
    std::string extractBaseVariable(const std::string& variable) const;
    
    // swizzle이 있는지 확인 (예: "st.x" -> true)
    bool hasSwizzle(const std::string& variable) const;
    
    // swizzle 부분 추출 (예: "st.xy" -> "xy")
    std::string extractSwizzle(const std::string& variable) const;
    
    // 모든 빌트인 변수 이름 목록
    std::set<std::string> getAllBuiltinNames() const;
    
    // swizzle 검증 및 에러 메시지 생성
    bool isValidSwizzle(const std::string& variable, std::string& errorMessage) const;
    
    // 지원하는 swizzle 성분들 반환
    std::string getSupportedSwizzleComponents(const std::string& baseVariable) const;
    
    // 지원하는 성분들을 포맷팅
    std::string formatSupportedComponents(const std::string& glslType, int componentCount) const;
    
    // 리터럴 값인지 확인
    bool isFloatLiteral(const std::string& str) const;

private:
    BuiltinVariables();
    void initializeBuiltins();
    
    std::unordered_map<std::string, BuiltinVariable> builtins;
};