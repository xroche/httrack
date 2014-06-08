// Shell.h : main header file for the SHELL application
//

#if !defined(AFX_SHELL_H__ED81E637_E017_11D1_A97E_006097BC6150__INCLUDED_)
#define AFX_SHELL_H__ED81E637_E017_11D1_A97E_006097BC6150__INCLUDED_
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

/* basic HTTrack defs */
#include "htsnet.h"
#include "htsopt.h"
#include "htsdefines.h"

//
// connecté via HTTrack? (défini dans projet)
#define USE_RAS 1
//

// sleep (taux de refresh) en ms
#define HTS_SLEEP_WIN 100

#include "resource.h"       // main symbols
#include "cpp_lang.h"
// Ras
#if USE_RAS
#include "RasLoad.h"
#endif
#include "MainTab.h"

// helper launcher
#include "LaunchHelp.h"

// lang.h
#include "newlang.h"

// message requests
#define wm_ViewRestart (WM_USER + 100)
// test 
#define wm_WizRequest1 (WM_USER + 101)
#define wm_WizRequest2 (WM_USER + 102)
#define wm_WizRequest3 (WM_USER + 103)

// char[] dynamiques
#define dynstrcpy(dest,src) { \
  if (dest) free(dest);\
  dest=(char*) malloc(strlen(src)+1);\
  if (dest)\
    strcpy(dest,src);\
  }
#define dynclear(dest) { if (dest) { free(dest); dest=NULL; }}

typedef struct t_StatsBuffer {
  char nom[HTS_URLMAXSIZE*2];
  char fichier[HTS_URLMAXSIZE];
  char etat[256];
  char url_sav[HTS_URLMAXSIZE*2];    // pour cancel
  char url_adr[HTS_URLMAXSIZE*2];
  char url_fil[HTS_URLMAXSIZE*2];
  LLint size;
  LLint sizetot;
  int offset;
  //
  int back;
  //
  int actived;    // pour disabled
} t_StatsBuffer;

typedef struct InpInfo {
  int ask_refresh;
  int refresh;
  LLint stat_bytes;
  int stat_time;
  int lien_n;
  int lien_tot;
  int stat_nsocket;
  int rate;
  int irate;
  int ft;
  LLint stat_written;
  int stat_updated;
  int stat_errors;
  int stat_warnings;
  int stat_infos;
  TStamp stat_timestart;
  int stat_back;
} InpInfo;


/* WinHTTrack mutex */
extern HANDLE WhttMutex;
#define WHTT_LOCK() WaitForSingleObject(WhttMutex,INFINITE)
#define WHTT_UNLOCK() ReleaseMutex(WhttMutex)

/* Location */
extern char* WhttLocation;
#define WHTT_LOCATION(a) WhttLocation=(a)

#ifndef HTS_DEF_FWSTRUCT_lien_back
#define HTS_DEF_FWSTRUCT_lien_back
typedef struct lien_back lien_back;
#endif

// fonctions moteur
int   __cdecl httrackengine_check(t_hts_callbackarg *carg, httrackp *opt, const char* adr,const char* fil,int status);
int   __cdecl httrackengine_check_mime(t_hts_callbackarg *carg, httrackp *opt, const char* adr,const char* fil,const char* mime,int status);
void  __cdecl httrackengine_init();
void  __cdecl httrackengine_uninit();
int   __cdecl httrackengine_start(httrackp *opt);
int   __cdecl httrackengine_end(httrackp *opt);
int   __cdecl httrackengine_htmlpreprocess(t_hts_callbackarg *carg, httrackp *opt, char** html,int* len,const char* url_address,const char* url_file);
int   __cdecl httrackengine_htmlpostprocess(t_hts_callbackarg *carg, httrackp *opt, char** html,int* len,const char* url_address,const char* url_file);
int   __cdecl httrackengine_htmlcheck(t_hts_callbackarg *carg, httrackp *opt, char* html,int len,const char* url_address,const char* url_file);
int   __cdecl httrackengine_chopt(t_hts_callbackarg *carg, httrackp *opt);
int   __cdecl httrackengine_loop(t_hts_callbackarg *carg, httrackp *opt, lien_back* back,int back_max,int back_index,int lien_n,int lien_tot,int stat_time,hts_stat_struct* stats);
const char* __cdecl httrackengine_query(t_hts_callbackarg *carg, httrackp *opt, const char* question);
const char* __cdecl httrackengine_query2(t_hts_callbackarg *carg, httrackp *opt, const char* question);
const char* __cdecl httrackengine_query3(t_hts_callbackarg *carg, httrackp *opt, const char* question);
void  __cdecl httrackengine_pause(t_hts_callbackarg *carg, httrackp *opt, const char* lockfile);
void  __cdecl httrackengine_filesave(t_hts_callbackarg *carg, httrackp *opt, const char* file);
void  __cdecl httrackengine_filesave2(t_hts_callbackarg *carg, httrackp *opt, const char* adr, const char* fil, const char* save, int is_new, int is_modified, int not_updated);

extern httrackp *global_opt;

// profile
int Save_current_profile(int ask);
//
int MyGetProfileInt(CString path,CString dummy,CString name,int value);
int MyGetProfileIntFile(FILE* fp,CString dummy,CString name,int value);
int MyWriteProfileString(CString path,CString dummy,CString name,CString value);
int MyWriteProfileStringFile(FILE* fp,CString dummy,CString name,CString value);
CString MyGetProfileStringFile(FILE* fp,CString dummy,CString name,CString value);
CString MyGetProfileString(CString path,CString dummy,CString name,CString value="");
void Write_profile(CString path,int load_path);
void Read_profile(CString path,int load_path);
int MyWriteProfileInt(CString path,CString dummy,CString name,int value);
int MyWriteProfileIntFile(FILE* fp,CString dummy,CString name,int value);

void Build_TopIndex(BOOL check_empty=TRUE);

void InitRAS();

// Gestion répertoires
int CheckDirInfo(CString path);
BOOL RemoveEmptyDir(CString path);


// RAS //
/*
#include "Ras.h"
typedef unsigned long (* t_RasHangUp)(HRASCONN);
*/
// RAS END //

/* lang extensions */
void SetCombo(CWnd* _this,int id,char* lang_string);


// HTTrack params - pour le multithread interface/robot
class HTTrack_Params {
public:
  int argc;
  char** argv;
  //
  int* result;
};
typedef struct Robot_params {
  int argc;
  char** argv;
} Robot_params;

// Lancement du miroir
BOOL LaunchMirror();


#define NStatsBuffer                    14

/* Class */
class CShellOptions {
public:
  CString url, filelist, proxy, proxyftp, port, depth, 
    extdepth, get, where, meth, maxfile, max, frag, 
    conn, tog, cache, robots, choixdeb, build, filtre, htmlfirst, 
    index, index2, index_mail, dos, time, rate, hostquit, ka, 
    user, footer, log, testall, parseall, link, path, 
    retry, errpage, external, nopurge, hidepwd, hidequery, 
    cookies, checktype, parsejava, Cache2, logtype, norecatch, 
    toler, updhack, urlhack, http10, waittime, maxtime, maxrate, 
    maxconn, maxlinks, hh, mm, ss, buff_filtres, buff_MIME, 
    _RasString, accept_language, other_headers, default_referer;
  CString LINE_back;
  RASDIALPARAMS _dial;
};

/* Class options */
extern CShellOptions* ShellOptions;


/////////////////////////////////////////////////////////////////////////////
// CShellApp:
// See Shell.cpp for the implementation of this class
//
//class CShellApp : public CWinApp
class CShellApp
{
public:
	CShellApp();
  void init_lance();
  CString end_path;
  CString end_path_complete;
  /*
  int suivant0(void);
  int suivant1(void);
  int suivant2(void);
  //int suivant3(void);
  int suivant4(void);
  */
  void OptPannel();

  //BOOL InitInstance();
  
  /*
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CShellApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CShellApp)
	afx_msg void OnAppAbout();
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
  */
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SHELL_H__ED81E637_E017_11D1_A97E_006097BC6150__INCLUDED_)
