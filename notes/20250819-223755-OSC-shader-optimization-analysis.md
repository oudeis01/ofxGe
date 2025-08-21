# OSC 셰이더 처리 효율성 분석 및 멀티패스 최적화 방안

**작성일**: 2025-08-19  
**상태**: 분석 완료  
**작성자**: Claude Code  

## 1. 현재 엔진의 셰이더 처리 구조 분석

### 1.1 개별 셰이더 노드 생성 방식

현재 엔진은 TypeScript 인터프리터에서 생성된 각 OSC `/create` 메시지에 대해 **독립적인 ShaderNode를 생성**합니다.

**예제 분석**: 
```typescript
const pattern = shader
  .snoise(st, time.multiply(0.2))        // shader_001 생성
  .mix(shader.fbm(st.multiply(3.0)), 0.5) // shader_002, shader_003 생성  
  .smoothstep(0.3, 0.7);                 // shader_004 생성

pattern.connect();                       // shader_004만 GlobalOutputNode에 연결
```

**OSC 메시지 시퀀스**:
```
/create "snoise" "st,time*0.2"      → shader_001 (사용되지 않음)
/create "fbm" "st*3.0"              → shader_002 (사용되지 않음) 
/create "mix" "$shader_001,$shader_002,0.5" → shader_003 (사용되지 않음)
/create "smoothstep" "$shader_003,0.3,0.7"  → shader_004 (연결됨)
/connect "shader_004"
```

### 1.2 현재 구조의 문제점

#### 💸 **리소스 낭비**
- **GPU 메모리**: 중간 셰이더 노드들(shader_001~003)이 각각 완전한 OpenGL 셰이더 프로그램을 생성
- **CPU 오버헤드**: 각 노드마다 별도의 컴파일, 링킹, 유니폼 관리
- **캐시 효율성**: 실제로는 하나의 셰이더로 통합 가능한 연산을 여러 개로 분할

#### 🔧 **아키텍처 한계**  
- **GlobalOutputNode**: 단일 셰이더만 연결 가능 (`connected_shader` 멤버)
- **중간 결과 전달**: 셰이더 간 데이터 전달이 텍스처 기반이 아닌 ID 참조 방식
- **실시간 성능**: 불필요한 렌더 패스 증가

## 2. 최적화된 처리 방안

### 2.1 지연 컴파일 (Deferred Compilation) 시스템

**핵심 아이디어**: `.connect()` 호출 시점까지 실제 셰이더 컴파일을 지연하고, 전체 체인을 **단일 fragment shader로 통합**

#### 구현 전략

```cpp
class ShaderCompositionEngine {
private:
    // 셰이더 그래프 노드
    struct CompositionNode {
        std::string function_name;
        std::vector<std::string> arguments;
        std::vector<CompositionNode*> input_nodes;
        std::string node_id;
    };
    
    std::unordered_map<std::string, CompositionNode> pending_nodes;
    
public:
    // OSC /create 시 노드만 등록, 컴파일하지 않음
    std::string registerNode(const std::string& function_name, 
                             const std::vector<std::string>& arguments);
    
    // OSC /connect 시 전체 그래프를 단일 셰이더로 컴파일
    std::shared_ptr<ShaderNode> compileGraph(const std::string& output_node_id);
};
```

#### 통합 셰이더 생성 과정

1. **의존성 그래프 구축**: 각 노드의 입력 의존성 분석
2. **함수 인라인화**: 중간 함수 호출을 직접 코드로 치환
3. **변수 최적화**: 불필요한 중간 변수 제거
4. **단일 main() 함수 생성**: 전체 연산을 하나의 fragment shader로 통합

**결과**:
```glsl
// 기존: 4개의 개별 셰이더
// 최적화: 단일 통합 셰이더
void main() {
    vec2 st = gl_FragCoord.xy / resolution.xy;
    
    // 모든 연산이 하나의 셰이더에서 처리
    float snoise_result = snoise(st, time * 0.2);
    float fbm_result = fbm(st * 3.0);
    float mix_result = mix(snoise_result, fbm_result, 0.5);
    vec3 final_color = vec3(smoothstep(0.3, 0.7, mix_result));
    
    gl_FragColor = vec4(final_color, 1.0);
}
```

### 2.2 스마트 캐싱 시스템

```cpp
class ShaderGraphCache {
private:
    // 그래프 구조 기반 캐시 키
    std::string generateGraphKey(const CompositionNode& root_node);
    
    // 컴파일된 셰이더 캐시
    std::unordered_map<std::string, std::shared_ptr<ShaderNode>> compiled_cache;
    
public:
    // 동일한 그래프 구조 재사용
    std::shared_ptr<ShaderNode> getCachedShader(const std::string& graph_key);
    void cacheCompiledShader(const std::string& graph_key, 
                           std::shared_ptr<ShaderNode> shader);
};
```

## 3. 멀티패스 셰이더 아키텍처 설계

### 3.1 FBO 기반 렌더링 파이프라인

**현재 제약**: GlobalOutputNode는 단일 셰이더만 지원  
**해결책**: Frame Buffer Object(FBO) 기반 멀티패스 시스템

#### 아키텍처 개요

```cpp
class MultiPassRenderPipeline {
private:
    // 렌더 패스 정의
    struct RenderPass {
        std::shared_ptr<ShaderNode> shader;
        std::vector<ofFbo*> input_textures;
        ofFbo output_texture;
        std::string pass_id;
    };
    
    // DAG 기반 패스 순서
    std::vector<RenderPass> render_passes;
    std::unordered_map<std::string, size_t> pass_indices;
    
public:
    // 멀티패스 그래프 구성
    void addRenderPass(const std::string& pass_id, 
                      std::shared_ptr<ShaderNode> shader,
                      const std::vector<std::string>& input_pass_ids);
    
    // 전체 파이프라인 실행
    void executeAllPasses();
    
    // 최종 결과 텍스처
    ofTexture& getFinalOutput();
};
```

#### TypeScript API 확장

```typescript
// 멀티패스 지원을 위한 새로운 API
const pass1 = shader.snoise(st, time).toTexture("noise_pass");
const pass2 = shader.fbm(st.multiply(2.0)).toTexture("fbm_pass"); 
const final = shader.mix(pass1.sample(), pass2.sample(), 0.5);

final.connect();
```

#### OSC 메시지 확장

```
/create_pass "noise_pass" "snoise" "st,time"
/create_pass "fbm_pass" "fbm" "st*2.0"  
/create_pass "final_pass" "mix" "texture:noise_pass,texture:fbm_pass,0.5"
/connect_pipeline "final_pass"
```

### 3.2 텍스처 버퍼 관리

```cpp
class TexturePoolManager {
private:
    // 재사용 가능한 FBO 풀
    std::vector<std::unique_ptr<ofFbo>> available_fbos;
    std::unordered_map<std::string, ofFbo*> active_textures;
    
    // 텍스처 해상도 관리
    struct TextureSpec {
        int width, height;
        ofTextureFormat format;
    };
    
public:
    // FBO 할당 및 해제
    ofFbo* acquireFBO(const TextureSpec& spec);
    void releaseFBO(const std::string& texture_id);
    
    // 자동 리사이징
    void handleResolutionChange(int new_width, int new_height);
};
```

## 4. 성능 최적화 예상 효과

### 4.1 리소스 사용량 비교

| 방식 | GPU 셰이더 | 텍스처 메모리 | 렌더 패스 | 컴파일 시간 |
|------|------------|---------------|-----------|-------------|
| **현재 (개별 노드)** | 4개 | 3×텍스처 | 4회 | 4×컴파일 |
| **최적화 (통합)** | 1개 | 0×텍스처 | 1회 | 1×컴파일 |
| **멀티패스 (필요시)** | 2-3개 | 1-2×텍스처 | 2-3회 | 2-3×컴파일 |

### 4.2 성능 향상 지표

- **메모리 사용량**: 75% 감소 (단순 체이닝의 경우)
- **렌더링 성능**: 2-3배 향상 (GPU 상태 변경 최소화)
- **컴파일 시간**: 70% 단축 (캐시 활용)
- **배터리 효율**: 향상 (모바일 환경)

## 5. 구현 로드맵

### Phase 1: 지연 컴파일 시스템 (2주)
- [ ] `ShaderCompositionEngine` 구현
- [ ] OSC 메시지 처리 로직 수정  
- [ ] 그래프 기반 캐싱 시스템
- [ ] 기존 API 호환성 유지

### Phase 2: 스마트 최적화 (1주)  
- [ ] 함수 인라인화 엔진
- [ ] 중간 변수 최적화
- [ ] 컴파일 성능 프로파일링

### Phase 3: 멀티패스 지원 (3주)
- [ ] FBO 기반 렌더 파이프라인
- [ ] 텍스처 풀 관리자
- [ ] TypeScript API 확장
- [ ] OSC 프로토콜 확장

### Phase 4: 고급 기능 (2주)
- [ ] 조건부 렌더링 (if문 지원)
- [ ] 루프 언롤링 최적화
- [ ] 실시간 성능 모니터링

## 6. 결론

현재 TypeScript 인터프리터가 생성하는 OSC 메시지는 **개념적으로는 정확하지만, 실행 효율성 측면에서 개선의 여지**가 있습니다.

### 핵심 개선사항

1. **지연 컴파일**: 최종 연결 시점에 전체 그래프를 단일 셰이더로 통합
2. **스마트 캐싱**: 그래프 구조 기반의 고급 캐싱 시스템  
3. **멀티패스 지원**: FBO 기반 복잡한 렌더링 파이프라인
4. **리소스 최적화**: 불필요한 중간 노드 생성 방지

이러한 개선을 통해 **SuperCollider 수준의 성능과 유연성**을 제공하면서도, 사용자에게는 직관적인 Fluent API를 유지할 수 있을 것입니다.

---

**다음 단계**: Phase 1의 `ShaderCompositionEngine` 프로토타입 구현을 시작하여 실제 성능 향상을 검증해보는 것을 제안합니다.