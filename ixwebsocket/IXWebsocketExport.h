#ifndef IXWEBSOCKET_EXPORT_H
#define IXWEBSOCKET_EXPORT_H

#ifdef _WIN32

#ifdef IXWEBSOCKET_STATIC_DEFINE 
   /* Building and using static library */
#  define IXWEBSOCKET_EXPORT
#  define IXWEBSOCKET_NO_EXPORT
#else
#  ifndef IXWEBSOCKET_EXPORT
#    ifdef ixwebsocket_EXPORTS /* Comes from cmake, documented in DEFINE_SYMBOL */
       /* Building dynamic library */
#      define IXWEBSOCKET_EXPORT __declspec(dllexport)
#    else
        /* Using dynamic library */
#      define IXWEBSOCKET_EXPORT __declspec(dllimport)
#    endif
#  endif

#  ifndef IXWEBSOCKET_NO_EXPORT
#    define IXWEBSOCKET_NO_EXPORT 
#  endif
#endif

#else /* _WIN32 */

#  define IXWEBSOCKET_EXPORT
#  define IXWEBSOCKET_NO_EXPORT

#endif /* _WIN32 */

#endif /* IXWEBSOCKET_EXPORT_H */
