/*
    HTTrack library example
    .h file
*/

#ifdef __WIN32
#define CDECL __cdecl
#else
#define CDECL
#endif

static void CDECL httrack_wrapper_init(t_hts_callbackarg *carg);
static void CDECL httrack_wrapper_uninit(t_hts_callbackarg *carg);
static int CDECL httrack_wrapper_start(t_hts_callbackarg *carg, httrackp* opt);
static int CDECL httrack_wrapper_chopt(t_hts_callbackarg *carg, httrackp* opt);
static int CDECL httrack_wrapper_end(t_hts_callbackarg *carg, httrackp* opt); 
static int CDECL httrack_wrapper_checkhtml(t_hts_callbackarg *carg, httrackp *opt,  char* html,int len,const char* url_address,const char* url_file);
static int CDECL httrack_wrapper_loop(t_hts_callbackarg *carg, httrackp *opt,  void* _back,int back_max,int back_index,int lien_n,int lien_tot,int stat_time,hts_stat_struct* stats);
static const char* CDECL httrack_wrapper_query(t_hts_callbackarg *carg, httrackp *opt,  const char* question);
static const char* CDECL httrack_wrapper_query2(t_hts_callbackarg *carg, httrackp *opt,  const char* question);
static const char* CDECL httrack_wrapper_query3(t_hts_callbackarg *carg, httrackp *opt,  const char* question);
static int CDECL httrack_wrapper_check(t_hts_callbackarg *carg, httrackp *opt,  const char* adr,const char* fil,int status);
static void CDECL httrack_wrapper_pause(t_hts_callbackarg *carg, httrackp *opt,  const char* lockfile);
static void CDECL httrack_wrapper_filesave(t_hts_callbackarg *carg, httrackp *opt,  const char* file);
static int CDECL httrack_wrapper_linkdetected(t_hts_callbackarg *carg, httrackp *opt,  char* link);
static int CDECL httrack_wrapper_xfrstatus(t_hts_callbackarg *carg, httrackp *opt,  void* back);
static int CDECL httrack_wrapper_preprocesshtml(t_hts_callbackarg *carg, httrackp *opt,  char** html,int* len,const char* url_address,const char* url_file);
static int CDECL httrack_wrapper_postprocesshtml(t_hts_callbackarg *carg, httrackp *opt,  char** html,int* len,const char* url_address,const char* url_file);
static int CDECL httrack_wrapper_check_mime(t_hts_callbackarg *carg, httrackp *opt,  const char* adr,const char* fil,const char* mime,int status);
static void CDECL httrack_wrapper_filesave2(t_hts_callbackarg *carg, httrackp *opt,  const char* adr, const char* fil, const char* save, int is_new, int is_modified,int not_updated);
static int CDECL httrack_wrapper_linkdetected2(t_hts_callbackarg *carg, httrackp *opt,  char* link, const char* start_tag);
static int CDECL httrack_wrapper_savename(t_hts_callbackarg *carg, httrackp *opt,  const char* adr_complete,const char* fil_complete,const char* referer_adr,const char* referer_fil,char* save);
static int CDECL httrack_wrapper_sendheader(t_hts_callbackarg *carg, httrackp *opt,  char* buff, const char* adr, const char* fil, const char* referer_adr, const char* referer_fil, htsblk* outgoing);
static int CDECL httrack_wrapper_receiveheader(t_hts_callbackarg *carg, httrackp *opt,  char* buff, const char* adr, const char* fil, const char* referer_adr, const char* referer_fil, htsblk* incoming);
