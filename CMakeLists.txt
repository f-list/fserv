cmake_minimum_required(VERSION 2.8.7)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake_modules")


## compiler flags

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fno-strict-aliasing -Werror")


project(fserv)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY release)

find_package(Threads REQUIRED)

find_package(LibEV REQUIRED)
find_package(LuaJIT REQUIRED)
find_package(Glog REQUIRED)
find_package(tcmalloc REQUIRED)
find_package(libjansson REQUIRED)
find_package(Hiredis REQUIRED)
find_package(ICU REQUIRED)
find_package(CURL REQUIRED)
find_package(Evhttpclient REQUIRED)
find_package(Boost REQUIRED)

include_directories(
    "${LUAJIT_INCLUDE_DIR}"
    "${LIBEV_INCLUDE_DIR}"
    "${GLOG_INCLUDE_DIR}"
    "${Tcmalloc_INCLUDE_DIR}"
    "${JANSSON_INCLUDE_DIR}"
    "${LIBHIREDIS_INCLUDE_DIR}"
    "${ICU_INCLUDE_DIRS}"
    "${CURL_INCLUDE_DIR}"
    "${EVHTTPCLIENT_INCLUDE_DIR}"
    "${Boost_INCLUDE_DIRS}")


## compatibility for pre-2.0 gperftools

find_file(HAVE_NEW_TCMALLOC gperftools/malloc_extension.h ${Tcmalloc_INCLUDE_DIR} NO_DEFAULT_PATH)


## generate the build config file

configure_file(src/cmake_config.hpp.in "${CMAKE_SOURCE_DIR}/src/cmake_config.hpp")


## fserv binary

set(fserv_SOURCES
    src/channel.cpp
    src/connection.cpp
    src/fserv.cpp
    src/login_evhttp.cpp
    src/lua_channel.cpp
    src/lua_chat.cpp
    src/lua_connection.cpp
    src/lua_constants.cpp
    src/messagebuffer.cpp
    src/native_command.cpp
    src/redis.cpp
    src/server.cpp
    src/server_state.cpp
    src/startup_config.cpp
    src/unicode_tools.cpp
    src/websocket.cpp
    src/base64.cpp
    src/md5.cpp
    src/modp_b64.cpp
    src/sha1.cpp)

set(fserv_LIBS ${fserv_LIBS}
    "${CMAKE_THREAD_LIBS_INIT}"
    "${LUAJIT_LIBRARIES}"
    "${LIBEV_LIBRARIES}"
    "${GLOG_LIBRARY}"
    "${Tcmalloc_LIBRARY}"
    "${JANSSON_LIBRARY}"
    "${LIBHIREDIS_LIBRARIES}"
    "${ICU_LIBRARIES}"
    "${CURL_LIBRARY}"
    "${EVHTTPCLIENT_LIBRARY}")

add_executable(fserv ${fserv_SOURCES})
target_link_libraries(fserv ${fserv_LIBS})

if(IS_DIRECTORY "${CMAKE_SOURCE_DIR}/cmake_modules/cotire/CMake")
    message("## Found cotire; using precompiled header")
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake_modules/cotire/CMake")
    include(cotire)
    set_target_properties(fserv PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT "src/precompiled_headers.hpp")
    cotire(fserv)
else()
    message("## Could not find cotire; not using precompiled header")
    message("## If you've cloned from git, run `git submodule init && git submodule update` to fetch cotire")
endif()


## facceptor binary

set(facceptor_SOURCES
    src/facceptor.cpp)

set(facceptor_LIBS
    "${LIBEV_LIBRARIES}")

add_executable(facceptor ${facceptor_SOURCES})
target_link_libraries(facceptor ${facceptor_LIBS})


## copy scripts over to the release dir
add_custom_command(
    TARGET fserv POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/script ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/script
    
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/logs)
