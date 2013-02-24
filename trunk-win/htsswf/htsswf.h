
// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the HTSSWF_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// HTSSWF_API functions as being imported from a DLL, wheras this DLL sees symbols
// defined with this macro as being exported.
#ifdef HTSSWF_EXPORTS
#define HTSSWF_API __declspec(dllexport)
#else
#define HTSSWF_API __declspec(dllimport)
#endif

extern "C" {
  #include "htsmodules.h"
  #include "swf/swf2html_interface.h"
};
extern "C" HTSSWF_API int hts_detect_swf(htsmoduleStruct* str);
extern "C" HTSSWF_API int hts_parse_swf(htsmoduleStruct* str);
