/*
 *  IXWebSocketExport.h
 *  Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 */

#pragma once

// CMake propagates IXWEBSOCKET_SHARED only for Windows shared-library targets.
// Static builds and projects compiling the sources directly need no decoration.
#if defined(_WIN32) && defined(IXWEBSOCKET_SHARED)
#    if defined(ixwebsocket_EXPORTS)
#        define IXWEBSOCKET_EXPORT __declspec(dllexport)
#    else
#        define IXWEBSOCKET_EXPORT __declspec(dllimport)
#    endif
#else
#    define IXWEBSOCKET_EXPORT
#endif
