#------------------------------------------------------------------------------#
# CMakeLists.txt
# Rémi Attab (remi.attab@gmail.com), 03 Mar 2016
# FreeBSD-style copyright and disclaimer apply
#------------------------------------------------------------------------------#

cmake_minimum_required(VERSION 2.6)
project(optics)
set(VERSION 1.1.1)

enable_testing()

#------------------------------------------------------------------------------#
# BASICS
#------------------------------------------------------------------------------#

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_definitions("-pipe -g -O3 -Werror -march=native")
add_definitions("-Wall")
add_definitions("-Wextra")
add_definitions("-Wundef")
add_definitions("-Wcast-align")
add_definitions("-Wwrite-strings")
add_definitions("-Wunreachable-code")
add_definitions("-Wformat=2")
add_definitions("-Wswitch-enum")
add_definitions("-Wswitch-default")
add_definitions("-Winit-self")
add_definitions("-Wno-strict-aliasing")

# clang rejects standard compliant `{0}` struct initializers. It's a known bug.
add_definitions("-Wno-missing-field-initializers")

# Our log utils needs the format to be passed through a function and I really
# don't see the issue with that so...
add_definitions("-Wno-format-nonliteral")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -std=gnu11 -pthread -D_GNU_SOURCE")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DOPTICS_VERSION=\"${VERSION}\"")


#------------------------------------------------------------------------------#
# DEPS
#------------------------------------------------------------------------------#

include(FindPkgConfig)

pkg_check_modules(MHD REQUIRED libmicrohttpd)
pkg_check_modules(DAEMON REQUIRED libdaemon)

# Not installed on prod machine and is therefore optional
pkg_check_modules(CMOCKA cmocka)
if(CMOCKA_FOUND)
    message("CMOCKA: ON")
else()
    message("CMOCKA: OFF")
endif()


#------------------------------------------------------------------------------#
# LIBS
#------------------------------------------------------------------------------#

include_directories("${PROJECT_SOURCE_DIR}/src")

set(OPTICS_SOURCES
    src/optics.c
    src/utils/utils.c)

set(OPTICS_DEPS
    optics_static
    bsd
    rt)

add_library(optics_static STATIC ${OPTICS_SOURCES})
add_library(optics SHARED ${OPTICS_SOURCES})
target_link_libraries(optics ${MHD_LIBRARIES} bsd rt)


set(OPTICS_POLLER_SOURCES
    src/poller.c
    src/backend_stdout.c
    src/backend_carbon.c
    src/backend_rest.c
    src/backend_prometheus.c
    src/utils/crest/crest.c)

set(OPTICS_POLLER_DEPS
    optics_poller_static
    ${MHD_LIBRARIES}
    ${OPTICS_DEPS})

add_library(optics_poller_static STATIC ${OPTICS_POLLER_SOURCES})


#------------------------------------------------------------------------------#
# BINS
#------------------------------------------------------------------------------#

add_executable(example test/example.c)
target_link_libraries(example optics_static ${OPTICS_DEPS})

add_executable(opticsd src/opticsd.c)
target_link_libraries(opticsd ${OPTICS_POLLER_DEPS} ${DAEMON_LIBRARIES})


#------------------------------------------------------------------------------#
# INSTALL
#------------------------------------------------------------------------------#

install(TARGETS optics optics_static opticsd
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)

install(FILES src/optics.h DESTINATION include)

set(pc_version ${VERSION})
set(pc_prefix ${CMAKE_INSTALL_PREFIX})

function(optics_pc name)
    configure_file(src/${name}.pc.in ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${name}.pc @ONLY)
    install(FILES ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${name}.pc DESTINATION lib/pkgconfig)
endfunction()

optics_pc(optics)
optics_pc(optics_static)


#------------------------------------------------------------------------------#
# TESTS
#------------------------------------------------------------------------------#

if(CMOCKA_FOUND)
    add_library(optics_tests STATIC test/bench.c test/test.c)
    set(OPTICS_TESTS_DEPS optics_tests ${OPTICS_POLLER_DEPS} ${CMOCKA_LIBRARIES})
endif()


find_program(VALGRIND_EXEC "valgrind")
if(VALGRIND_EXEC)
    message("VALGRIND: ${VALGRIND_EXEC}")
    set(VALGRIND_ARGS --leak-check=full --track-origins=yes)
else()
    message("VALGRIND: DISABLED - install valgrind to enable")
endif()

function(optics_cmocka_test name)
    if(CMOCKA_FOUND)
        add_executable(${name}_test test/${name}_test.c)
        target_link_libraries(${name}_test ${OPTICS_TESTS_DEPS})
        set_property(TARGET ${name}_test APPEND PROPERTY COMPILE_OPTIONS ${CMOCKA_CFLAGS})

        add_test(${name}_test bin/${name}_test)
        set_tests_properties(${name}_test PROPERTIES LABELS "test")

        if(VALGRIND_EXEC)
            add_test(${name}_valgrind ${VALGRIND_EXEC} ${VALGRIND_ARGS} bin/${name}_test)
            set_tests_properties(${name}_valgrind PROPERTIES LABELS "valgrind")
        endif()
    endif()
endfunction()

optics_cmocka_test(key)
optics_cmocka_test(htable)
optics_cmocka_test(buffer)
optics_cmocka_test(region)
optics_cmocka_test(timer)
optics_cmocka_test(lens)
optics_cmocka_test(lens_gauge)
optics_cmocka_test(lens_counter)
optics_cmocka_test(lens_dist)
optics_cmocka_test(lens_histo)
optics_cmocka_test(poller)
optics_cmocka_test(poller_lens)
optics_cmocka_test(backend_carbon)
optics_cmocka_test(backend_prometheus)
optics_cmocka_test(backend_rest)
optics_cmocka_test(crest)


#------------------------------------------------------------------------------#
# BENCHS
#------------------------------------------------------------------------------#

function(optics_cmocka_bench name)
    if(CMOCKA_FOUND)
        add_executable(${name}_bench test/${name}_bench.c)
        target_link_libraries(${name}_bench optics_tests ${OPTICS_TESTS_DEPS})
        set_property(TARGET ${name}_bench APPEND PROPERTY COMPILE_OPTIONS ${CMOCKA_CFLAGS})

        add_test(${name}_bench bin/${name}_bench)
        set_tests_properties(${name}_bench PROPERTIES LABELS "bench")
    endif()
endfunction()

optics_cmocka_bench(bench)
optics_cmocka_bench(timer)
optics_cmocka_bench(htable)
optics_cmocka_bench(lens)
optics_cmocka_bench(lens_gauge)
optics_cmocka_bench(lens_counter)
optics_cmocka_bench(lens_dist)
optics_cmocka_bench(lens_histo)


#------------------------------------------------------------------------------#
# UBSAN
#------------------------------------------------------------------------------#

option(ENABLE_UBSAN
    "enable undefined behaviour sanitizer (affects performance)" OFF)
if(ENABLE_UBSAN)
    message("UBSAN: ENABLED")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=undefined")
else()
    message("UBSAN: DISABLED")
endif()


#------------------------------------------------------------------------------#
# CLANG-TIDY
#------------------------------------------------------------------------------#

find_program(CLANG_TIDY_EXEC "clang-tidy")
if(CLANG_TIDY_EXEC)
    message("CLANG-TIDY: ${CLANG_TIDY_EXEC}")
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    add_custom_target(clang-tidy
        COMMAND ${CLANG_TIDY_EXEC} -p ${CMAKE_BINARY_DIR} -header-filter='.*' src/*.c src/utils/*.c test/*.c
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
else()
    message("CLANG-TIDY: DISABLED - install clang to enable")
endif()
