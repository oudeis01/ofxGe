# OSC 셰이더 노드 트리 쿼리 시스템 구현 로드맵

**작성일**: 2025-08-19 23:08:44  
**상태**: 설계 단계  
**작성자**: Claude Code  

## 1. 개요

현재 그래픽스 엔진은 OSC 클라이언트가 개별 셰이더 노드를 생성(`/create`), 연결(`/connect`), 해제(`/free`)할 수 있지만, **전체 노드 트리의 현재 상태를 조회하는 기능**이 없습니다. 

이 문서는 클라이언트가 `/query` OSC 메시지를 통해 현재 활성 셰이더 노드들의 전체 관계도를 **JSON 형태로 조회**할 수 있는 시스템의 구현 로드맵을 제시합니다.

## 2. 현재 시스템 분석

### 2.1 기존 OSC 명령어 구조

```
/create [function] [args]     → shader_id 반환
/connect [shader_id]          → 글로벌 출력에 연결  
/free [shader_id]             → 셰이더 메모리 해제
```

### 2.2 현재 노드 관리 구조

**ShaderManager**:
```cpp
std::unordered_map<std::string, std::shared_ptr<ShaderNode>> active_shaders;
std::vector<std::string> getAllActiveShaderIds();  // 이미 구현됨
```

**graphicsEngine**:
```cpp
std::map<std::string, std::shared_ptr<ShaderNode>> active_shaders;  // 중복
std::shared_ptr<ShaderNode> current_shader;  // 현재 연결된 노드
```

### 2.3 현재 구조의 한계

1. **노드 관계 추적 부재**: 셰이더 간 의존성 정보 없음
2. **상태 조회 불가**: 클라이언트가 현재 상태를 알 수 없음  
3. **디버깅 어려움**: 복잡한 셰이더 그래프의 시각화 불가
4. **중복 데이터**: ShaderManager와 graphicsEngine에서 동일한 데이터 관리

## 3. 새로운 쿼리 시스템 설계

### 3.1 새로운 OSC 명령어

```
/query                        → 전체 노드 트리 조회
/query/response [json_string] → JSON 형태의 응답
```

### 3.2 JSON 응답 구조

#### 기본 구조
```json
{
  "timestamp": "2025-08-19T23:08:44.627Z",
  "engine_state": {
    "total_nodes": 4,
    "connected_node": "shader_004",
    "global_output_state": "CONNECTED"
  },
  "nodes": {
    "shader_001": {
      "id": "shader_001",
      "function_name": "snoise", 
      "arguments": ["st", "time*0.2"],
      "status": "idle",
      "created_at": "2025-08-19T23:05:12.123Z",
      "compilation_status": "compiled",
      "error_message": null,
      "dependencies": [],
      "dependents": ["shader_003"],
      "is_connected_to_output": false
    },
    "shader_002": {
      "id": "shader_002", 
      "function_name": "fbm",
      "arguments": ["st*3.0"],
      "status": "idle",
      "created_at": "2025-08-19T23:05:13.456Z",
      "compilation_status": "compiled", 
      "error_message": null,
      "dependencies": [],
      "dependents": ["shader_003"],
      "is_connected_to_output": false
    },
    "shader_003": {
      "id": "shader_003",
      "function_name": "mix", 
      "arguments": ["$shader_001", "$shader_002", "0.5"],
      "status": "idle",
      "created_at": "2025-08-19T23:05:14.789Z",
      "compilation_status": "compiled",
      "error_message": null,
      "dependencies": ["shader_001", "shader_002"],
      "dependents": ["shader_004"],
      "is_connected_to_output": false
    },
    "shader_004": {
      "id": "shader_004",
      "function_name": "smoothstep",
      "arguments": ["$shader_003", "0.3", "0.7"],
      "status": "connected", 
      "created_at": "2025-08-19T23:05:15.012Z",
      "compilation_status": "compiled",
      "error_message": null,
      "dependencies": ["shader_003"],
      "dependents": [],
      "is_connected_to_output": true
    }
  },
  "execution_graph": [
    {
      "from": "shader_001",
      "to": "shader_003", 
      "parameter_index": 0,
      "connection_type": "shader_reference"
    },
    {
      "from": "shader_002",
      "to": "shader_003",
      "parameter_index": 1, 
      "connection_type": "shader_reference"
    },
    {
      "from": "shader_003",
      "to": "shader_004",
      "parameter_index": 0,
      "connection_type": "shader_reference"
    }
  ],
  "performance_stats": {
    "total_shader_compilations": 4,
    "total_memory_usage_mb": 12.5,
    "average_compile_time_ms": 45.2
  }
}
```

### 3.3 확장 가능한 메타데이터

```json
{
  "shader_metadata": {
    "shader_001": {
      "source_plugin": "lygia",
      "glsl_function_path": "/noise/snoise.glsl",
      "uniform_bindings": {
        "time": "auto_builtin",
        "st": "vertex_attribute" 
      },
      "render_passes": 1,
      "texture_dependencies": []
    }
  }
}
```

## 4. 구현 아키텍처

### 4.1 새로운 컴포넌트

#### A. NodeTreeAnalyzer 클래스
```cpp
// 새 파일: src/querySystem/NodeTreeAnalyzer.h
class NodeTreeAnalyzer {
private:
    ShaderManager* shader_manager;
    graphicsEngine* engine;
    
public:
    struct NodeDependency {
        std::string from_node;
        std::string to_node; 
        int parameter_index;
        std::string connection_type;
    };
    
    struct NodeTreeSnapshot {
        std::vector<std::string> all_node_ids;
        std::map<std::string, std::vector<std::string>> dependencies;
        std::map<std::string, std::vector<std::string>> dependents;  
        std::vector<NodeDependency> execution_graph;
        std::string connected_output_node;
    };
    
    NodeTreeSnapshot analyzeCurrentTree();
    std::vector<NodeDependency> extractDependencies(const std::string& node_id);
    std::vector<std::string> topologicalSort(const NodeTreeSnapshot& snapshot);
};
```

#### B. JSONSerializer 클래스
```cpp
// 새 파일: src/querySystem/JSONSerializer.h  
#include <nlohmann/json.hpp>
using json = nlohmann::json;

class JSONSerializer {
private:
    NodeTreeAnalyzer* analyzer;
    
public:
    json serializeNodeTree(const NodeTreeAnalyzer::NodeTreeSnapshot& snapshot);
    json serializeShaderNode(const std::shared_ptr<ShaderNode>& node);
    json serializeEngineState();
    json serializePerformanceStats();
    
    std::string generateTimestamp();
    std::string formatErrorMessage(const std::string& error);
};
```

### 4.2 OSC Handler 확장

```cpp
// oscHandler.h에 추가
struct OscQueryMessage {
    std::string query_type;      // "tree", "node", "stats"
    std::string target_id;       // 특정 노드 조회시
    bool is_valid_format;
    std::string format_error;
};

class OscHandler {
private:
    std::queue<OscQueryMessage> query_message_queue;  // 새로 추가
    
public:
    bool hasQueryMessage();
    OscQueryMessage getNextQueryMessage();
    
    // JSON 응답 전송 (큰 데이터 처리)
    void sendQueryResponse(const std::string& json_data);
    
private:
    OscQueryMessage parseQueryMessage(const ofxOscMessage& osc_message);
    
    // 대용량 JSON 데이터를 여러 OSC 메시지로 분할 전송
    void sendLargeJsonResponse(const std::string& json_data);
};
```

### 4.3 graphicsEngine 확장

```cpp
// geMain.h에 추가  
#include "querySystem/NodeTreeAnalyzer.h"
#include "querySystem/JSONSerializer.h"

class graphicsEngine {
private:
    std::unique_ptr<NodeTreeAnalyzer> tree_analyzer;    // 새로 추가
    std::unique_ptr<JSONSerializer> json_serializer;    // 새로 추가
    
public:
    // 쿼리 처리 메서드
    std::string generateNodeTreeJSON();
    void processQueryMessages();  // updateOSC()에서 호출
    
private:
    void handleQueryMessage(const OscQueryMessage& msg);
};
```

## 5. 의존성 분석 알고리즘

### 5.1 셰이더 참조 파싱

현재 셰이더의 `arguments` 벡터에서 `$shader_xxx` 형태의 참조를 찾아 의존성을 추출합니다:

```cpp
std::vector<std::string> NodeTreeAnalyzer::extractShaderReferences(
    const std::vector<std::string>& arguments) {
    
    std::vector<std::string> references;
    std::regex shader_ref_pattern(R"(\$shader_\d+)");
    
    for (const auto& arg : arguments) {
        std::sregex_iterator iter(arg.begin(), arg.end(), shader_ref_pattern);
        std::sregex_iterator end;
        
        for (; iter != end; ++iter) {
            std::string ref = iter->str().substr(1);  // '$' 제거
            references.push_back(ref);
        }
    }
    
    return references;
}
```

### 5.2 순환 참조 검사

```cpp
bool NodeTreeAnalyzer::detectCycles(const NodeTreeSnapshot& snapshot) {
    std::set<std::string> visited;
    std::set<std::string> recursion_stack;
    
    for (const auto& node_id : snapshot.all_node_ids) {
        if (visited.find(node_id) == visited.end()) {
            if (hasCycleDFS(node_id, snapshot, visited, recursion_stack)) {
                return true;  // 순환 참조 발견
            }
        }
    }
    
    return false;
}
```

## 6. 대용량 JSON 데이터 OSC 전송

### 6.1 문제점

- OSC 메시지 크기 제한 (일반적으로 64KB)
- 복잡한 셰이더 그래프의 JSON은 수백 KB 가능
- 네트워크 패킷 분할 필요

### 6.2 해결책: 청크 기반 전송

```cpp
void OscHandler::sendLargeJsonResponse(const std::string& json_data) {
    const size_t chunk_size = 32768;  // 32KB 청크
    const size_t total_chunks = (json_data.size() + chunk_size - 1) / chunk_size;
    const std::string session_id = generateSessionId();
    
    // 전송 시작 신호
    ofxOscMessage start_msg;
    start_msg.setAddress("/query/response/start");
    start_msg.addStringArg(session_id);
    start_msg.addIntArg(total_chunks);
    start_msg.addIntArg(json_data.size());
    sender.sendMessage(start_msg);
    
    // 청크별 전송
    for (size_t i = 0; i < total_chunks; i++) {
        size_t offset = i * chunk_size;
        size_t length = std::min(chunk_size, json_data.size() - offset);
        std::string chunk = json_data.substr(offset, length);
        
        ofxOscMessage chunk_msg;
        chunk_msg.setAddress("/query/response/chunk");
        chunk_msg.addStringArg(session_id);
        chunk_msg.addIntArg(i);
        chunk_msg.addStringArg(chunk);
        sender.sendMessage(chunk_msg);
        
        // 전송 간 딜레이 (네트워크 오버로드 방지)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    // 전송 완료 신호
    ofxOscMessage end_msg;
    end_msg.setAddress("/query/response/end");
    end_msg.addStringArg(session_id);
    sender.sendMessage(end_msg);
}
```

## 7. 클라이언트 사용 예제

### 7.1 단순 쿼리

```python
# Python OSC 클라이언트 예제
import json
from pythonosc import osc
from pythonosc.dispatcher import Dispatcher
from pythonosc.server import osc

# 쿼리 전송
client = osc.SimpleUDPClient("127.0.0.1", 12345)
client.send_message("/query", [])

# 응답 수신
def handle_query_response(unused_addr, json_string):
    data = json.loads(json_string)
    print(f"Total nodes: {data['engine_state']['total_nodes']}")
    
    for node_id, node_info in data['nodes'].items():
        print(f"Node {node_id}: {node_info['function_name']}")
        print(f"  Status: {node_info['status']}")
        print(f"  Dependencies: {node_info['dependencies']}")

dispatcher = Dispatcher()
dispatcher.map("/query/response", handle_query_response)
```

### 7.2 대용량 데이터 조립

```python
class ChunkedResponseHandler:
    def __init__(self):
        self.sessions = {}  # session_id -> {chunks: {}, total_chunks: int}
    
    def handle_start(self, unused_addr, session_id, total_chunks, total_size):
        self.sessions[session_id] = {
            'chunks': {},
            'total_chunks': total_chunks,
            'total_size': total_size
        }
    
    def handle_chunk(self, unused_addr, session_id, chunk_index, chunk_data):
        if session_id in self.sessions:
            self.sessions[session_id]['chunks'][chunk_index] = chunk_data
    
    def handle_end(self, unused_addr, session_id):
        if session_id in self.sessions:
            session = self.sessions[session_id]
            
            # 모든 청크 수신 확인
            if len(session['chunks']) == session['total_chunks']:
                # 청크들을 순서대로 합성
                full_json = ""
                for i in range(session['total_chunks']):
                    full_json += session['chunks'][i]
                
                # JSON 파싱 및 처리
                data = json.loads(full_json)
                self.process_node_tree(data)
            
            del self.sessions[session_id]
```

## 8. 성능 고려사항

### 8.1 쿼리 주기 제한

```cpp
class QueryThrottler {
private:
    std::chrono::steady_clock::time_point last_query_time;
    std::chrono::milliseconds min_interval{100};  // 최소 100ms 간격
    
public:
    bool canProcessQuery() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = now - last_query_time;
        
        if (elapsed >= min_interval) {
            last_query_time = now;
            return true;
        }
        
        return false;
    }
};
```

### 8.2 캐싱 전략

```cpp
class NodeTreeCache {
private:
    std::string cached_json;
    std::chrono::steady_clock::time_point cache_time;
    std::chrono::milliseconds cache_ttl{50};  // 50ms TTL
    
public:
    bool isCacheValid() {
        auto now = std::chrono::steady_clock::now();
        return (now - cache_time) < cache_ttl;
    }
    
    void updateCache(const std::string& new_json) {
        cached_json = new_json;
        cache_time = std::chrono::steady_clock::now();
    }
};
```

## 9. 구현 로드맵

### Phase 1: 기본 쿼리 시스템 (1주)
- [ ] `NodeTreeAnalyzer` 클래스 구현
- [ ] 기본 의존성 분석 알고리즘
- [ ] OSC `/query` 메시지 파싱
- [ ] 단순 JSON 응답 (작은 데이터)

### Phase 2: JSON 직렬화 (1주)
- [ ] `nlohmann/json` 라이브러리 통합
- [ ] `JSONSerializer` 클래스 구현  
- [ ] 완전한 노드 메타데이터 직렬화
- [ ] 타임스탬프 및 성능 통계 포함

### Phase 3: 대용량 데이터 전송 (1주)  
- [ ] 청크 기반 OSC 전송 시스템
- [ ] 세션 관리 및 오류 복구
- [ ] 클라이언트 예제 코드 작성
- [ ] 전송 성능 최적화

### Phase 4: 고급 기능 (1주)
- [ ] 실시간 캐싱 시스템
- [ ] 쿼리 스로틀링 
- [ ] 순환 참조 검사
- [ ] 성능 프로파일링

### Phase 5: 디버깅 및 시각화 도구 (1주)
- [ ] 웹 기반 노드 트리 시각화 도구
- [ ] 실시간 상태 모니터링 대시보드
- [ ] 오류 진단 및 복구 도구
- [ ] 성능 병목 분석 리포트

## 10. 예상 활용 사례

### 10.1 개발 및 디버깅
- **셰이더 그래프 시각화**: 복잡한 노드 관계를 그래프로 표시
- **성능 병목 찾기**: 컴파일 시간이 긴 노드 식별
- **메모리 사용량 추적**: 노드별 리소스 사용량 모니터링

### 10.2 라이브 코딩 환경
- **실시간 상태 표시**: IDE에서 현재 셰이더 상태 시각화
- **자동 완성**: 기존 노드 ID를 자동 완성에 활용
- **오류 하이라이팅**: 참조 불가능한 노드 실시간 표시

### 10.3 교육 도구
- **학습 과정 시각화**: 셰이더 구성 과정을 단계별로 표시
- **예제 시연**: 복잡한 셰이더 그래프의 구조 설명
- **인터랙티브 튜토리얼**: 노드 생성부터 연결까지 실습

## 11. 결론

OSC 셰이더 노드 트리 쿼리 시스템은 **현재 엔진의 관찰 가능성(Observability)**을 크게 향상시킬 것입니다.

### 핵심 이점
1. **🔍 완전한 상태 추적**: 모든 셰이더 노드의 실시간 상태 조회
2. **🎯 디버깅 효율성**: 복잡한 셰이더 그래프의 문제점 신속 파악  
3. **📊 성능 최적화**: 노드별 성능 지표를 통한 병목 식별
4. **🎨 라이브 코딩 지원**: IDE와의 완벽한 통합으로 개발 경험 향상

이 시스템을 통해 **SuperCollider 수준의 강력한 인트로스펙션 기능**을 제공하여, 사용자가 복잡한 셰이더 시스템을 직관적으로 이해하고 제어할 수 있게 될 것입니다.

---

**다음 단계**: Phase 1의 `NodeTreeAnalyzer` 프로토타입 구현부터 시작하여 점진적으로 기능을 확장하는 것을 권장합니다.