cmake_minimum_required(VERSION 3.16)
project(pubsub_quic C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_executable(broker_quic broker_quic.c)
add_executable(publisher_quic publisher_quic.c)
add_executable(subscriber_quic subscriber_quic.c)

# Try pkg-config first (recommended on Linux)
find_package(PkgConfig QUIET)
if (PkgConfig_FOUND)
  pkg_check_modules(MSQUIC QUIET msquic)
endif()

if (MSQUIC_FOUND)
  target_include_directories(broker_quic PRIVATE ${MSQUIC_INCLUDE_DIRS})
  target_include_directories(publisher_quic PRIVATE ${MSQUIC_INCLUDE_DIRS})
  target_include_directories(subscriber_quic PRIVATE ${MSQUIC_INCLUDE_DIRS})
  target_link_libraries(broker_quic PRIVATE ${MSQUIC_LDFLAGS} ${MSQUIC_LIBRARIES})
  target_link_libraries(publisher_quic PRIVATE ${MSQUIC_LDFLAGS} ${MSQUIC_LIBRARIES})
  target_link_libraries(subscriber_quic PRIVATE ${MSQUIC_LDFLAGS} ${MSQUIC_LIBRARIES})
else()
  # Fallback: simple find for header and library
  find_path(MSQUIC_INCLUDE msquic.h)
  find_library(MSQUIC_LIB msquic)
  if (NOT MSQUIC_INCLUDE OR NOT MSQUIC_LIB)
    message(FATAL_ERROR "MsQuic not found. Install libmsquic-dev (apt) or provide vcpkg toolchain.")
  endif()
  target_include_directories(broker_quic PRIVATE ${MSQUIC_INCLUDE})
  target_include_directories(publisher_quic PRIVATE ${MSQUIC_INCLUDE})
  target_include_directories(subscriber_quic PRIVATE ${MSQUIC_INCLUDE})
  target_link_libraries(broker_quic PRIVATE ${MSQUIC_LIB})
  target_link_libraries(publisher_quic PRIVATE ${MSQUIC_LIB})
  target_link_libraries(subscriber_quic PRIVATE ${MSQUIC_LIB})
endif()


