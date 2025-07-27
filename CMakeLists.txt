cmake_minimum_required(VERSION 3.10)

# set project name
project(scratch)

# load openFrameworks core settings
include("${CMAKE_CURRENT_SOURCE_DIR}/../../../libs/openFrameworks/cmake/openFrameworks.cmake")

# 추가 소스 파일들을 수집
file(GLOB_RECURSE ADDITIONAL_SOURCES
    "src/pluginSystem/*.cpp"
    "src/oscHandler/*.cpp"
)

# openFrameworks 프로젝트 설정 (한 줄이면 끝!)
of_setup_project()

# 추가 소스 파일들을 명시적으로 포함
target_sources(${PROJECT_NAME} PRIVATE ${ADDITIONAL_SOURCES})

# 플러그인 시스템을 위한 추가 라이브러리
target_link_libraries(${PROJECT_NAME} PRIVATE dl)

# ========================================
# 외부 라이브러리 추가 (선택사항)
# ========================================
# 예시: OpenCV 추가
# of_add_library(${PROJECT_NAME} OpenCV)

# 예시: Boost 라이브러리 추가
# of_add_library(${PROJECT_NAME} Boost system filesystem)

# 예시: 커스텀 라이브러리 추가
# of_add_custom_library(${PROJECT_NAME} "MyLib" "/path/to/lib.a" "/path/to/headers")

# 예시: 추가 컴파일 정의
# target_compile_definitions(${PROJECT_NAME} PRIVATE MY_CUSTOM_DEFINE=1)

# 예시: 추가 헤더 경로
# target_include_directories(${PROJECT_NAME} PRIVATE "custom_headers/")
