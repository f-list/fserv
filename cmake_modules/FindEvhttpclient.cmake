# adapted from Findlibjansson.cmake

find_path(EVHTTPCLIENT_INCLUDE_DIR evhttpclient.h
  PATHS /usr/include
)

find_library(EVHTTPCLIENT_LIBRARY
  NAMES evhttpclient
  PATHS /usr/lib /usr/local/lib
)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(EVHTTPCLIENT
  DEFAULT_MSG
  EVHTTPCLIENT_INCLUDE_DIR
  EVHTTPCLIENT_LIBRARY
)

include_directories(${EVHTTPCLIENT_INCLUDE_DIR})
