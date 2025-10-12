# Install script for directory: /Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/Library/Developer/CommandLineTools/usr/bin/objdump")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/build/libs/capnproto/c++/src/capnp/libcapnp.a")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcapnp.a" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcapnp.a")
    execute_process(COMMAND "/Library/Developer/CommandLineTools/usr/bin/ranlib" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcapnp.a")
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/capnp" TYPE FILE FILES
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/c++.capnp.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/common.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/blob.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/endian.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/layout.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/orphan.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/list.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/any.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/message.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/capability.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/membrane.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/dynamic.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/schema.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/schema.capnp.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/stream.capnp.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/schema-lite.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/schema-loader.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/schema-parser.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/pretty-print.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/serialize.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/serialize-async.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/serialize-packed.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/serialize-text.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/pointer-helpers.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/generated-header-support.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/raw-schema.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/c++.capnp"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/schema.capnp"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/stream.capnp"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/capnp/compat" TYPE FILE FILES "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/compat/std-iterator.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/build/libs/capnproto/c++/src/capnp/libcapnp-rpc.a")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcapnp-rpc.a" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcapnp-rpc.a")
    execute_process(COMMAND "/Library/Developer/CommandLineTools/usr/bin/ranlib" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcapnp-rpc.a")
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/capnp" TYPE FILE FILES
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/rpc-prelude.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/rpc.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/rpc-twoparty.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/rpc.capnp.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/rpc-twoparty.capnp.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/persistent.capnp.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/ez-rpc.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/rpc.capnp"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/rpc-twoparty.capnp"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/persistent.capnp"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/build/libs/capnproto/c++/src/capnp/libcapnp-json.a")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcapnp-json.a" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcapnp-json.a")
    execute_process(COMMAND "/Library/Developer/CommandLineTools/usr/bin/ranlib" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcapnp-json.a")
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/capnp/compat" TYPE FILE FILES
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/compat/json.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/compat/json.capnp.h"
    "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/compat/json.capnp"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/build/libs/capnproto/c++/src/capnp/libcapnp-websocket.a")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcapnp-websocket.a" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcapnp-websocket.a")
    execute_process(COMMAND "/Library/Developer/CommandLineTools/usr/bin/ranlib" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcapnp-websocket.a")
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/capnp/compat" TYPE FILE FILES "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/libs/capnproto/c++/src/capnp/compat/websocket-rpc.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/build/libs/capnproto/c++/src/capnp/libcapnpc.a")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcapnpc.a" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcapnpc.a")
    execute_process(COMMAND "/Library/Developer/CommandLineTools/usr/bin/ranlib" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcapnpc.a")
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/build/libs/capnproto/c++/src/capnp/capnp")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/capnp" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/capnp")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/Library/Developer/CommandLineTools/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/capnp")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/build/libs/capnproto/c++/src/capnp/capnpc-c++")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/capnpc-c++" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/capnpc-c++")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/Library/Developer/CommandLineTools/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/capnpc-c++")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/xiaofs/opengpt/ai-manual/oxygen/Potter/build/libs/capnproto/c++/src/capnp/capnpc-capnp")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/capnpc-capnp" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/capnpc-capnp")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/Library/Developer/CommandLineTools/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/capnpc-capnp")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  execute_process(COMMAND "/usr/local/Cellar/cmake/3.19.1/bin/cmake" -E create_symlink capnp "$ENV{DESTDIR}/usr/local/bin/capnpc")
endif()

