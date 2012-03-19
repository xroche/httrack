/*
    HTTrack library example
    .h file
*/

#ifdef __WIN32
#define CDECL __cdecl
#else
#define CDECL
#endif

void  CDECL httrack_wrapper_init(void);
void  CDECL httrack_wrapper_uninit(void);
int   CDECL httrack_wrapper_start(httrackp* opt);
int   CDECL httrack_wrapper_chopt(httrackp* opt);
int   CDECL httrack_wrapper_end(void); 
int   CDECL httrack_wrapper_checkhtml(char* html,int len,char* url_adresse,char* url_fichier);
int   CDECL httrack_wrapper_loop(void* _back,int back_max,int back_index,int lien_n,int lien_tot,int stat_time,hts_stat_struct* stats);
char* CDECL httrack_wrapper_query(char* question);
char* CDECL httrack_wrapper_query2(char* question);
char* CDECL httrack_wrapper_query3(char* question);
int   CDECL httrack_wrapper_check(char* adr,char* fil,int status);
void  CDECL httrack_wrapper_pause(char* lockfile);
void  CDECL httrack_wrapper_filesave(char* file);
int   CDECL httrack_wrapper_linkdetected(char* link);
int   CDECL httrack_wrapper_xfrstatus(void* back);

