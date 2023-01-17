# Install script for directory: /home/ivddorrka/proxy_ghs/IXWebSocket

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
    set(CMAKE_INSTALL_CONFIG_NAME "")
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

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/home/ivddorrka/proxy_ghs/IXWebSocket/libixwebsocket.a")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ixwebsocket" TYPE FILE FILES
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXBase64.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXBench.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXCancellationRequest.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXConnectionState.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXDNSLookup.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXExponentialBackoff.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXGetFreePort.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXGzipCodec.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXHttp.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXHttpClient.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXHttpServer.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXNetSystem.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXProgressCallback.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXSelectInterrupt.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXSelectInterruptFactory.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXSelectInterruptPipe.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXSelectInterruptEvent.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXSetThreadName.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXSocket.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXSocketConnect.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXSocketFactory.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXSocketServer.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXSocketTLSOptions.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXStrCaseCompare.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXUdpSocket.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXUniquePtr.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXUrlParser.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXUuid.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXUtf8Validator.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXUserAgent.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXWebSocket.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXWebSocketCloseConstants.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXWebSocketCloseInfo.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXWebSocketErrorInfo.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXWebSocketHandshake.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXWebSocketHandshakeKeyGen.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXWebSocketHttpHeaders.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXWebSocketInitResult.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXWebSocketMessage.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXWebSocketMessageType.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXWebSocketOpenInfo.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXWebSocketPerMessageDeflate.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXWebSocketPerMessageDeflateCodec.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXWebSocketPerMessageDeflateOptions.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXWebSocketProxyServer.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXWebSocketSendData.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXWebSocketSendInfo.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXWebSocketServer.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXWebSocketTransport.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXWebSocketVersion.h"
    "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket/IXSocketOpenSSL.h"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/ixwebsocket" TYPE FILE FILES "/home/ivddorrka/proxy_ghs/IXWebSocket/ixwebsocket-config.cmake")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/ixwebsocket/ixwebsocket-targets.cmake")
    file(DIFFERENT EXPORT_FILE_CHANGED FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/ixwebsocket/ixwebsocket-targets.cmake"
         "/home/ivddorrka/proxy_ghs/IXWebSocket/CMakeFiles/Export/lib/cmake/ixwebsocket/ixwebsocket-targets.cmake")
    if(EXPORT_FILE_CHANGED)
      file(GLOB OLD_CONFIG_FILES "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/ixwebsocket/ixwebsocket-targets-*.cmake")
      if(OLD_CONFIG_FILES)
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/ixwebsocket/ixwebsocket-targets.cmake\" will be replaced.  Removing files [${OLD_CONFIG_FILES}].")
        file(REMOVE ${OLD_CONFIG_FILES})
      endif()
    endif()
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/ixwebsocket" TYPE FILE FILES "/home/ivddorrka/proxy_ghs/IXWebSocket/CMakeFiles/Export/lib/cmake/ixwebsocket/ixwebsocket-targets.cmake")
  if("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^()$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/ixwebsocket" TYPE FILE FILES "/home/ivddorrka/proxy_ghs/IXWebSocket/CMakeFiles/Export/lib/cmake/ixwebsocket/ixwebsocket-targets-noconfig.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT)
  set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
file(WRITE "/home/ivddorrka/proxy_ghs/IXWebSocket/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
