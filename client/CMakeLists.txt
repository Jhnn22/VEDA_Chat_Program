cmake_minimum_required(VERSION 3.10)

# 프로젝트 이름 설정
project(VedaChat)

# C 표준 버전 설정
set(CMAKE_C_STANDARD 11)

# 소스 파일 추가
set(CLIENT_SOURCES
    ${CMAKE_SOURCE_DIR}/client.h
    ${CMAKE_SOURCE_DIR}/client.c
    ${CMAKE_SOURCE_DIR}/main.c   
)

add_executable(client ${CLIENT_SOURCES})
