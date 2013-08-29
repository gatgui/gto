#ifndef __gto_config_h_
#define __gto_config_h_

#ifdef _WIN32
#  ifdef GTO_STATIC
#    define GTO_API
#  else
#    ifdef GTO_EXPORT
#      define GTO_API __declspec(dllexport)
#    else
#      define GTO_API __declspec(dllimport)
#    endif
#  endif
#else
#  define GTO_API
#endif

#endif
