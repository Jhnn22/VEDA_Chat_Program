cmake_minimum_required(VERSION 3.10)

# 프로젝트 이름 설정
project(VedaChat)

# C 표준 버전 설정
set(CMAKE_C_STANDARD 11)

# 소스 파일 추가
set(SERVER_SOURCES
    ${CMAKE_SOURCE_DIR}/server.h
    ${CMAKE_SOURCE_DIR}/server.c
    ${CMAKE_SOURCE_DIR}/main.c
)

add_executable(server ${SERVER_SOURCES})
