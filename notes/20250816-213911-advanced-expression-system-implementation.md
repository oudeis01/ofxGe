# 고급 표현식 시스템 구현 및 래퍼 함수 완성 - 2025-08-16

## 📋 오늘의 주요 성과

### 🎯 핵심 문제 해결
1. **문자열 절단 버그 완전 해결** - `"sin(time*0.1)"` → `"sin(time*0"` 문제
2. **범용 GLSL 타입 래퍼 시스템 완성** - 모든 GLSL 타입 지원
3. **GLSL Swizzle 구문 완전 지원** - `st.x*sin(time)` 등 복잡한 표현식 처리
4. **지능적 함수 오버로드 선택** - 사용자 인자에 최적화된 함수 매칭

---

## 🔧 구현된 주요 기능들

### 1. 모듈화된 아키텍처 완성 ✅

**분리 전 (ShaderManager: 740줄)**
```cpp
// 모든 기능이 ShaderManager에 집중
- 셰이더 관리 + 코드 생성 + 표현식 파싱
```

**분리 후 (3개 모듈)**
```cpp
ShaderManager.cpp (~400줄)        // 관리 + 캐시
ShaderCodeGenerator.cpp (~520줄)  // 코드 생성 + 래퍼
ExpressionParser.cpp (~200줄)     // 표현식 파싱
```

### 2. 범용 GLSL 타입 래퍼 시스템 ✅

**지원하는 모든 타입 조합:**
```cpp
// 사용자 입력 → 함수 시그니처 자동 매칭
vec2 + float → vec3 cnoise(vec3)
float + float + float → vec3 rgb2srgb(vec3) 
vec2 → vec2 snoise(vec2)
float → float saturate(float)
```

**자동 생성되는 래퍼 함수 예시:**
```glsl
// 사용자: {"st", "time*0.1"} → cnoise(vec3) 매칭
float cnoise_wrapper(vec2 arg0, float arg1) {
    return cnoise(vec3(arg0.xy, arg1));
}
```

### 3. GLSL Swizzle 완전 지원 ✅

**처리 가능한 복잡한 표현식들:**
```cpp
"st.x*sin(time)"                    // swizzle + 함수 호출
"st.x*mix(0.1,10.0,sin(time))"     // 중첩 함수 + swizzle  
"st.y*10.0*sin(time*.5+1000)"      // 복합 수학 연산
```

**muParser vs 수동 파싱 하이브리드:**
```cpp
if (expr.find('.') != std::string::npos) {
    // GLSL swizzle 포함 → 정규식 기반 수동 파싱
    deps = extractDependenciesManually(expr);
} else {
    // 순수 수학 표현식 → muParser 사용
    parser.SetExpr(expr);
}
```

### 4. 지능적 오버로드 선택 시스템 ✅

**컴포넌트 기반 매칭:**
```cpp
// 사용자 인자 분석
vec2 (2 components) + float (1 component) = 3 total components

// 함수 오버로드 매칭
float cnoise(vec2)  // 2 components - 부족
float cnoise(vec3)  // 3 components - 정확한 매칭! ✅
float cnoise(vec4)  // 4 components - 과함
```

### 5. 컨텍스트 기반 의존성 추출 ✅

**함수 vs 변수 자동 구분:**
```cpp
// BEFORE: 하드코딩된 함수 목록
if (match != "sin" && match != "cos" && ...) // 😠

// AFTER: 컨텍스트 기반 분석  
if (match_followed_by_parentheses) {
    // mix(a,b,c) → 함수 호출, 유니폼 생성 안함
} else {
    // time → 변수, 유니폼 생성 ✅
}
```

---

## 🐛 해결된 핵심 버그들

### 1. **문자열 절단 버그** (완전 해결)
```cpp
// 문제: BuiltinVariables::isValidSwizzle()에서 소수점을 swizzle로 인식
"sin(time*0.1)" → extractBaseVariable() → "sin(time*0"

// 해결: 복잡한 표현식은 swizzle 검증 건너뛰기
if (builtins.isComplexExpression(arg)) {
    return true; // 검증 패스, ExpressionParser가 처리
}
```

### 2. **ExpressionParser GLSL 변환 버그** (완전 해결)
```cpp
// 문제: 정규식 중복 적용
"0.1" → "0.0.1", "10.0" → "10.0.0"

// 해결: 복잡한 표현식은 그대로 사용
std::string convertToGLSL(const std::string& expr) {
    return expr; // GLSL이 muParser와 동일한 구문 지원
}
```

### 3. **복잡한 표현식의 유니폼 의존성 누락** (완전 해결)
```cpp
// 문제: "time*0.5"에서 time 의존성 감지 실패
builtins.extractBaseVariable("time*0.5") → "time*0"

// 해결: ExpressionParser 통합
if (builtins.isComplexExpression(arg)) {
    ExpressionParser temp_parser;
    ExpressionInfo expr_info = temp_parser.parseExpression(arg);
    // expr_info.dependencies = ["time"] ✅
}
```

### 4. **셰이더 코드 생성 순서 문제** (완전 해결)
```cpp
// 문제: 임시 변수가 built-in 변수보다 먼저 생성
float _expr0 = st.x*sin(time);  // st가 아직 선언 안됨 ❌
vec2 st = gl_FragCoord.xy / resolution;

// 해결: 코드 생성 순서 수정
vec2 st = gl_FragCoord.xy / resolution;        // 1. Built-in 선언
float _expr0 = st.x*sin(time);                 // 2. 임시 변수
vec3 result = cnoise_wrapper(_expr0, ...);     // 3. 함수 호출
```

---

## 🔬 현재 진행 중인 문제들

### 1. **GLSL 내장 함수의 유니폼 오선언** (진행중)
```glsl
// 현재 문제
uniform float mix;  // ❌ GLSL 내장 함수를 유니폼으로 선언

// 해결 중: 컨텍스트 기반 함수 감지
if (identifier_followed_by_parentheses) {
    // 함수 호출 → 유니폼 생성 안함
}
```

### 2. **반환 타입 불일치** (진행중)  
```glsl
// 현재 문제
float result = rgb2srgb_wrapper(...);  // ❌ vec3를 float에 할당

// 해결 중: 정확한 오버로드 기반 타입 추론
const FunctionOverload* best_overload = findBestOverloadForArguments(...);
std::string return_type = best_overload->returnType;  // "vec3"
```

---

## 📊 성능 및 안정성 지표

### ✅ 테스트 성공 케이스
```cpp
// 단순 표현식
{"st", "time"}                    → ✅ 정상 작동
{"st", "time*0.1"}               → ✅ 정상 작동

// 복잡한 표현식  
{"st.x*sin(time)", "st.y", "time*0.5"}  → ✅ 파싱 성공, 컴파일 단계 진행

// Swizzle 표현식
{"st.x", "st.y", "time"}         → ✅ 타입 추론 정확 (float, float, float)
```

### 🔧 아키텍처 개선사항
- **코드 재사용성**: 모듈 분리로 개별 테스트 가능
- **확장성**: 새로운 GLSL 타입 추가 용이
- **디버깅**: 단계별 로깅으로 문제 지점 정확히 파악
- **유지보수성**: 각 모듈의 책임 명확히 분리

---

## 🎯 다음 단계 (우선순위)

### 즉시 해결 필요 (High Priority)
1. **컨텍스트 기반 함수 감지 완성** - `mix()` 유니폼 오선언 해결
2. **정확한 반환 타입 처리** - 메타데이터 기반 타입 시스템 완성

### 중기 목표 (Medium Priority)  
1. **다중 함수 조합 시스템** - `noise(st) + fbm(st*2.0)` 같은 복합 표현식
2. **성능 최적화** - 캐싱 시스템 확장 및 컴파일 시간 단축
3. **에러 핸들링 강화** - 사용자 친화적 에러 메시지

### 장기 비전 (Low Priority)
1. **비주얼 노드 에디터** - 그래픽 인터페이스로 표현식 조합
2. **실시간 핫 리로드** - 표현식 수정 시 즉시 반영
3. **AI 기반 표현식 추천** - 패턴 학습을 통한 자동 완성

---

## 🏆 기술적 혁신 포인트

### 1. **하이브리드 파싱 시스템**
```cpp
muParser (수학 표현식) + 정규식 (GLSL 구문) = 완벽한 표현식 처리
```

### 2. **자동 타입 추론 및 변환**
```cpp
사용자 인자 분석 → 최적 오버로드 선택 → 자동 래퍼 생성 → GLSL 컴파일
```

### 3. **컨텍스트 인식 의존성 추출**
```cpp
mix(a,b,c) → 함수 (유니폼 생성 안함)
time       → 변수 (유니폼 생성)
```

---

## 📝 개발자 노트

### 배운 교훈
1. **점진적 리팩토링의 중요성** - 한 번에 모든 걸 바꾸려 하지 말고 단계별 접근
2. **디버깅 로그의 가치** - 복잡한 파이프라인에서 각 단계의 상태 확인 필수
3. **타입 시스템의 복잡성** - GLSL의 타입 변환 규칙을 정확히 이해해야 함

### 코드 품질 개선
- **모듈 분리**: 단일 책임 원칙 준수
- **에러 처리**: Try-catch 및 폴백 메커니즘 강화  
- **테스트 커버리지**: 각 표현식 타입별 단위 테스트 필요

---

**마지막 업데이트**: 2025-08-16 18:30  
**다음 세션 목표**: 컨텍스트 기반 함수 감지 완성 및 반환 타입 시스템 완성