#include "oscHandler.h"

OscHandler::OscHandler() {
    // OSC 수신기 초기화
    receiver.setup(12345); // 포트 번호는 필요에 따라 변경
    // OSC 송신기 초기화 (예시)
    sender.setup("localhost", 54321); // 대상 호스트와 포트 번호
}
OscHandler::~OscHandler() {
    // 소멸자에서 특별한 작업은 필요 없음
}

