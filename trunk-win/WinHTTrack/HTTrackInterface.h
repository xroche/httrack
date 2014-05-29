
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>

#include "httrack-library.h"
/**/
//#include "htsglobal.h"
//#include "htsbase.h"
#include "htsopt.h"
#include "htsdefines.h"
#include "htsstrings.h"
#include "htssafe.h"

extern int linput(FILE* fp,char* s,int max);
extern int linput_trim(FILE* fp,char* s,int max);
extern int linput_cpp(FILE* fp,char* s,int max);
extern void rawlinput(FILE* fp,char* s,int max);
extern int binput(char* buff,char* s,int max);
extern int fexist(const char* s);
extern size_t fsize(const char* s);
extern TStamp time_local(void);

extern char* convtolower(char* catbuff,const char* a);
extern void hts_lowcase(char* s);

extern char* next_token(char* p,int flag);

// Engine internal variables
extern HTSEXT_API hts_stat_struct HTS_STAT;
extern int _DEBUG_HEAD;
extern FILE* ioinfo;

// various
#define copychar(a) concat(catbuff,(a),NULL)
