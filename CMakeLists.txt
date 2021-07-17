cmake_minimum_required(VERSION 3.20)
project(sfml_checkers)

enable_language(CXX)

set(CMAKE_CXX_STANDARD 20)

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")

set(
    SOURCE_FILES
    main.cpp
)

set(
    HEADER_FILES
    utils.h
)

add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})

find_package(SFML REQUIRED COMPONENTS graphics audio window system)

target_link_libraries(${PROJECT_NAME} PUBLIC mynn nn_modules matrix algorithms games utils sfml-graphics sfml-audio sfml-window sfml-system pthread)  # GL X11

add_executable(draw_board draw_board.cpp utils.h)
target_link_libraries(draw_board PUBLIC sfml-graphics sfml-audio sfml-window sfml-system)
