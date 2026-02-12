cmake_minimum_required(VERSION 3.22)
project(objcurses)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SANITIZE "enable AddressSanitizer with extended checks" OFF)
if(SANITIZE)
    message(STATUS "AddressSanitizer enabled")
    set(ASAN_FLAGS "-fsanitize=address -fno-omit-frame-pointer -g")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ASAN_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${ASAN_FLAGS}")
    add_compile_definitions(ASAN_OPTIONS="detect_leaks=1:strict_string_checks=1:check_initialization_order=1:detect_stack_use_after_return=1:detect_container_overflow=1:abort_on_error=1")
endif()

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/*.cpp")
list(FILTER SOURCES EXCLUDE REGEX ".*/.*build.*/.*")   # 排除 build 目录


add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR})

target_compile_definitions(${PROJECT_NAME} PRIVATE NCURSES_STATIC)


find_package(Curses REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE ${CURSES_LIBRARIES})
target_include_directories(${PROJECT_NAME} PRIVATE ${CURSES_INCLUDE_DIR})

target_link_libraries(${PROJECT_NAME} PRIVATE m)


include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES LICENSE.md
        DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/${PROJECT_NAME})