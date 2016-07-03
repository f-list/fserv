# taken from tarantool at https://github.com/tarantool/tarantool/blob/master/cmake/FindLibEV.cmake
# Copyright (C) 2010-2015 Tarantool AUTHORS, see https://github.com/tarantool/tarantool/blob/master/LICENSE

find_path(LIBEV_INCLUDE_DIR NAMES ev.h)
find_library(LIBEV_LIBRARIES NAMES ev)

if(LIBEV_INCLUDE_DIR AND LIBEV_LIBRARIES)
    set(LIBEV_FOUND ON)
endif(LIBEV_INCLUDE_DIR AND LIBEV_LIBRARIES)

if(LIBEV_FOUND)
    if (NOT LIBEV_FIND_QUIETLY)
        message(STATUS "Found libev includes: ${LIBEV_INCLUDE_DIR}/ev.h")
        message(STATUS "Found libev library: ${LIBEV_LIBRARIES}")
    endif (NOT LIBEV_FIND_QUIETLY)
else(LIBEV_FOUND)
    if (LIBEV_FIND_REQUIRED)
        message(FATAL_ERROR "Could not find libev development files")
    endif (LIBEV_FIND_REQUIRED)
endif (LIBEV_FOUND)
