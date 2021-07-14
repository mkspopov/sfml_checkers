cmake_minimum_required(VERSION 3.20)
project(sfml_checkers)

enable_language(CXX)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")

file(
    GLOB SOURCE_FILES
    ${CMAKE_CURRENT_LIST_DIR}/*.cpp
    ${CMAKE_CURRENT_LIST_DIR}/*.c
)

file(
    GLOB HEADER_FILES
    ${CMAKE_CURRENT_LIST_DIR}/*.hpp
    ${CMAKE_CURRENT_LIST_DIR}/*.h
)

add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})

find_package(SFML REQUIRED COMPONENTS graphics audio window system)

target_link_libraries(${PROJECT_NAME} PUBLIC mynn nn_modules matrix algorithms games utils sfml-graphics sfml-audio sfml-window sfml-system)  # GL
