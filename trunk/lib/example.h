/*
    HTTrack library example
    .h file
*/

#if __WIN32
#else
#define __cdecl
#endif

void __cdecl httrack_wrapper_init(void);
void __cdecl httrack_wrapper_uninit(void);
int __cdecl httrack_wrapper_start(httrackp* opt);
int __cdecl httrack_wrapper_chopt(httrackp* opt);
int  __cdecl httrack_wrapper_end(void); 
int __cdecl httrack_wrapper_checkhtml(char* html,int len,char* url_adresse,char* url_fichier);
int __cdecl httrack_wrapper_loop(void* _back,int back_max,int back_index,int lien_n,int lien_tot,int stat_time,hts_stat_struct* stats);
char* __cdecl httrack_wrapper_query(char* question);
char* __cdecl httrack_wrapper_query2(char* question);
char* __cdecl httrack_wrapper_query3(char* question);
int __cdecl httrack_wrapper_check(char* adr,char* fil,int status);
void __cdecl httrack_wrapper_pause(char* lockfile);
void __cdecl httrack_wrapper_filesave(char* file);
int __cdecl httrack_wrapper_linkdetected(char* link);
int __cdecl httrack_wrapper_xfrstatus(void* back);
void __cdecl httrack_wrapper_savename(char* adr_complete,char* fil_complete,char* referer_adr,char* referer_fil,char* save);

