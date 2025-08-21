# 핑퐁 FBO 기반 누적 블러 효과 구현 방안

**작성일**: 2025-08-19  
**상태**: 설계 단계  
**작성자**: Claude Code  

## 1. 요구사항 분석: 핑퐁 FBO 블러 효과

### 1.1 목표 효과

사용자가 구현하고자 하는 **핑퐁 FBO 블러**는 다음과 같은 프로세스입니다:

```
Frame N:   FBO_A ← 원형 도형 그리기
Frame N+1: FBO_B ← 원형 도형 그리기 + FBO_A * 0.001 (누적)  
Frame N+2: FBO_A ← 원형 도형 그리기 + FBO_B * 0.001 (누적)
...
```

**핵심 특징**:
- 🔄 **핑퐁 버퍼**: 프레임마다 FBO_A ↔ FBO_B 전환
- 🌟 **누적 효과**: 이전 프레임 결과를 극히 낮은 알파(0.001)로 누적
- 🎨 **모션 블러**: 움직이는 원형이 꼬리를 남기며 블러 효과 생성

## 2. 현재 엔진 구조 분석

### 2.1 현재 렌더링 파이프라인

```cpp
// ofApp.cpp:86-91
if (ge.current_shader->isReady()) {
    ge.current_shader->compiled_shader.begin();
    ge.current_shader->updateAutoUniforms();
    plane.draw();                    // 직접 스크린에 렌더링
    ge.current_shader->compiled_shader.end();
}
```

**현재 구조의 특징**:
- ✅ **간단함**: 직접 `ofPlanePrimitive`에 셰이더 적용
- ❌ **FBO 부재**: 오프스크린 렌더링 기능 없음
- ❌ **단일 패스**: 멀티패스나 누적 렌더링 불가능
- ❌ **상태 관리**: 프레임 간 텍스처 전환 메커니즘 없음

### 2.2 필요한 시스템 확장

#### A. **FBO 관리 시스템**
현재 시스템에는 `ofFbo` 관련 코드가 전혀 없습니다. 필요한 컴포넌트:

```cpp
class PingPongFBOManager {
private:
    ofFbo fbo_a, fbo_b;              // 핑퐁 버퍼 쌍
    bool current_is_a;               // 현재 활성 버퍼 추적
    int width, height;               // FBO 해상도
    
public:
    void setup(int w, int h);        // FBO 초기화
    ofFbo& getCurrentFBO();          // 현재 쓰기용 FBO
    ofFbo& getPreviousFBO();         // 이전 프레임 읽기용 FBO  
    void swap();                     // 버퍼 교체
    void clear();                    // 버퍼 클리어
};
```

#### B. **누적 렌더링 파이프라인**
```cpp
class AccumulativeRenderer {
private:
    PingPongFBOManager fbo_manager;
    std::shared_ptr<ShaderNode> accumulation_shader;  // 누적용 셰이더
    std::shared_ptr<ShaderNode> content_shader;       // 원형 그리기 셰이더
    
public:
    void renderFrame();              // 한 프레임 렌더링
    void setAccumulationFactor(float factor);  // 누적 비율 설정
};
```

## 3. TypeScript API 설계

### 3.1 핑퐁 FBO를 위한 새로운 API

현재 단순한 체이닝 API를 확장하여 **버퍼 기반 연산**을 지원해야 합니다:

```typescript
// 기존 API (단일 패스)
const simple = shader.snoise(st, time).connect();

// 새로운 API (핑퐁 FBO 지원)
const pingpong = shader
  .createPingPongBuffer("blur_buffer", 1024, 1024)    // FBO 쌍 생성
  .beginFrame()                                        // 프레임 시작
    .clear(0.0, 0.0, 0.0, 0.0)                        // 투명으로 클리어
    .drawCircle(mouse.x, mouse.y, 50.0, time)          // 움직이는 원형
    .accumulate("blur_buffer", 0.001)                  // 이전 프레임 누적
  .endFrame()                                          // 프레임 완료
  .connectPingPong("blur_buffer");                     // 결과를 화면에 출력
```

### 3.2 OSC 메시지 확장

핑퐁 FBO 지원을 위한 새로운 OSC 명령어들:

```
/create_pingpong_buffer [buffer_id] [width] [height]
/begin_frame [buffer_id]
/clear_buffer [r] [g] [b] [a]  
/draw_geometry [type] [params...]
/accumulate [buffer_id] [factor]
/end_frame [buffer_id]
/connect_pingpong [buffer_id]
/free_pingpong_buffer [buffer_id]
```

**예제 OSC 시퀀스**:
```
/create_pingpong_buffer "blur_buffer" 1024 1024
/begin_frame "blur_buffer"
/clear_buffer 0.0 0.0 0.0 0.0
/draw_geometry "circle" "mouse.x,mouse.y,50.0"
/accumulate "blur_buffer" 0.001
/end_frame "blur_buffer"  
/connect_pingpong "blur_buffer"
```

## 4. 구체적 구현 방안

### 4.1 PingPongFBOManager 클래스

```cpp
// 새 파일: src/renderSystem/PingPongFBOManager.h
#pragma once
#include "ofMain.h"
#include <string>
#include <unordered_map>

struct PingPongBuffer {
    ofFbo fbo_a, fbo_b;
    bool current_is_a;
    int width, height;
    std::string buffer_id;
    
    PingPongBuffer(const std::string& id, int w, int h);
    ofFbo& getCurrentWriteFBO();
    ofFbo& getPreviousReadFBO();
    void swap();
    void clear(float r, float g, float b, float a);
};

class PingPongFBOManager {
private:
    std::unordered_map<std::string, std::unique_ptr<PingPongBuffer>> buffers;
    
public:
    bool createBuffer(const std::string& buffer_id, int width, int height);
    PingPongBuffer* getBuffer(const std::string& buffer_id);
    bool removeBuffer(const std::string& buffer_id);
    void swapBuffer(const std::string& buffer_id);
    std::vector<std::string> getAllBufferIds();
};
```

### 4.2 누적 렌더링 셰이더

**누적 합성용 Fragment Shader**:
```glsl
// accumulation.frag
uniform sampler2D currentContent;   // 현재 프레임 내용
uniform sampler2D previousFrame;    // 이전 프레임 결과
uniform float accumulationFactor;   // 누적 비율 (0.001)
uniform vec2 resolution;

void main() {
    vec2 st = gl_FragCoord.xy / resolution;
    
    vec4 current = texture2D(currentContent, st);
    vec4 previous = texture2D(previousFrame, st);
    
    // 누적 합성: 현재 + 이전 * 극소 비율
    vec4 result = current + previous * accumulationFactor;
    
    gl_FragColor = result;
}
```

### 4.3 GlobalOutputNode 확장

현재 `GlobalOutputNode`는 단일 셰이더만 지원하므로 확장이 필요합니다:

```cpp
// GlobalOutputNode.h에 추가
class GlobalOutputNode {
private:
    // 기존 멤버들...
    std::unique_ptr<PingPongFBOManager> pingpong_manager;  // FBO 관리자
    std::string active_pingpong_buffer;                     // 활성 버퍼 ID
    
public:
    // 핑퐁 버퍼 관리
    bool createPingPongBuffer(const std::string& buffer_id, int width, int height);
    bool connectPingPongBuffer(const std::string& buffer_id);
    void renderPingPongFrame(const std::string& buffer_id);
    
    // 렌더링 모드
    enum class RenderMode { DIRECT_SHADER, PINGPONG_BUFFER };
    RenderMode current_render_mode;
};
```

### 4.4 프레임별 렌더링 프로세스

```cpp
void GlobalOutputNode::renderPingPongFrame(const std::string& buffer_id) {
    auto* buffer = pingpong_manager->getBuffer(buffer_id);
    if (!buffer) return;
    
    // 1. 현재 FBO에 그리기 시작
    buffer->getCurrentWriteFBO().begin();
    
    // 2. 배경 클리어 (투명)
    ofClear(0, 0, 0, 0);
    
    // 3. 현재 프레임 콘텐츠 렌더링 (원형 등)
    if (connected_shader && connected_shader->isReady()) {
        connected_shader->compiled_shader.begin();
        connected_shader->updateAutoUniforms();
        
        // 기본 도형 그리기 (원형, 사각형 등)
        renderGeometry();
        
        connected_shader->compiled_shader.end();
    }
    
    // 4. 이전 프레임 누적
    if (accumulation_shader && accumulation_shader->isReady()) {
        accumulation_shader->compiled_shader.begin();
        accumulation_shader->compiled_shader.setUniformTexture("previousFrame", 
            buffer->getPreviousReadFBO().getTexture(), 1);
        accumulation_shader->compiled_shader.setUniform1f("accumulationFactor", 0.001f);
        
        // 전체 화면 쿼드에 누적 셰이더 적용
        renderFullScreenQuad();
        
        accumulation_shader->compiled_shader.end();
    }
    
    buffer->getCurrentWriteFBO().end();
    
    // 5. 버퍼 교체
    buffer->swap();
}
```

## 5. 성능 및 메모리 고려사항

### 5.1 FBO 메모리 사용량

**1024×1024 RGBA FBO 쌍**:
- 메모리 사용량: `1024 × 1024 × 4 bytes × 2 = 8.4MB`
- GPU VRAM: 고해상도에서 상당한 메모리 필요
- **해결책**: 동적 해상도 조정, FBO 풀링

### 5.2 렌더링 성능

**병목 지점**:
- FBO 전환 오버헤드
- 텍스처 업로드/다운로드
- 브랜딩 및 알파 블렌딩

**최적화 방안**:
```cpp
class FBOPerformanceOptimizer {
    // FBO 상태 캐싱
    void cacheFBOState();
    
    // 불필요한 클리어 방지  
    void smartClear();
    
    // 텍스처 형식 최적화
    void optimizeTextureFormat();
};
```

## 6. 구현 로드맵

### Phase 1: 기본 FBO 시스템 (1주)
- [ ] `PingPongFBOManager` 클래스 구현
- [ ] 기본 FBO 생성, 교체, 해제 기능
- [ ] OSC 명령어 `/create_pingpong_buffer`, `/free_pingpong_buffer`

### Phase 2: 누적 렌더링 (1주)  
- [ ] 누적 합성용 셰이더 작성
- [ ] `AccumulativeRenderer` 구현
- [ ] OSC 명령어 `/begin_frame`, `/end_frame`, `/accumulate`

### Phase 3: 도형 그리기 시스템 (1주)
- [ ] 기본 도형 렌더링 (원형, 사각형, 선)
- [ ] 마우스/시간 기반 애니메이션
- [ ] OSC 명령어 `/draw_geometry`

### Phase 4: TypeScript API 통합 (1주)
- [ ] TypeScript 인터프리터에서 핑퐁 API 지원
- [ ] AST 파서에서 `.createPingPongBuffer()` 등 인식
- [ ] 완전한 end-to-end 워크플로우

### Phase 5: 성능 최적화 (1주)
- [ ] FBO 풀링 시스템
- [ ] 메모리 사용량 최적화
- [ ] 실시간 성능 모니터링

## 7. 사용 예제

### 7.1 TypeScript 스크립트

```typescript
// pingpong-blur-example.ts
const blurEffect = shader
  .createPingPongBuffer("motion_blur", 1024, 1024)
  .beginFrame()
    .clear(0.0, 0.0, 0.0, 0.0)
    .drawCircle(
      mouse.x, 
      mouse.y, 
      50.0 + sin(time * 2.0) * 20.0,  // 크기 애니메이션
      [1.0, 0.5, 0.2, 0.8]             // 오렌지 색상
    )
    .accumulate("motion_blur", 0.001)   // 0.1% 누적
  .endFrame()
  .connectPingPong("motion_blur");

// 실시간 파라미터 조정
blurEffect.setAccumulationFactor(time_based_factor);
```

### 7.2 결과 OSC 메시지

```
/create_pingpong_buffer "motion_blur" 1024 1024
/begin_frame "motion_blur"  
/clear_buffer 0.0 0.0 0.0 0.0
/draw_geometry "circle" "mouse.x,mouse.y,50.0+sin(time*2.0)*20.0"
/set_color 1.0 0.5 0.2 0.8
/accumulate "motion_blur" 0.001
/end_frame "motion_blur"
/connect_pingpong "motion_blur"
```

## 8. 결론

핑퐁 FBO 기반 누적 블러 효과는 **현재 엔진에 상당한 확장**이 필요한 고급 기능입니다.

### 핵심 요구사항
1. **🎭 오프스크린 렌더링**: FBO 기반 멀티패스 시스템
2. **🔄 상태 관리**: 프레임별 버퍼 전환 및 추적
3. **🎨 도형 렌더링**: 기본 지오메트리 그리기 시스템  
4. **📡 API 확장**: TypeScript 및 OSC 프로토콜 확장

### 예상 효과
- **시각적 품질**: 고급 모션 블러 및 잔상 효과
- **창작 자유도**: 실시간 인터랙티브 시각화 가능
- **성능 영향**: 메모리 사용량 증가, GPU 부하 증가

**다음 단계**: Phase 1의 `PingPongFBOManager` 프로토타입부터 시작하여 점진적으로 확장하는 것을 권장합니다.

---

**참고**: 이는 현재 단일 패스 셰이더 시스템에서 **멀티패스 렌더링 파이프라인**으로의 중대한 아키텍처 변화를 의미합니다.