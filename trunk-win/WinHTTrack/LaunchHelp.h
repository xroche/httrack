
#if !defined(LAUNCHHELP_HGDHDGCJHHFIJKCHSOZIOJC5448545245451)
#define LAUNCHHELP_HGDHDGCJHHFIJKCHSOZIOJC5448545245451

#include "stdafx.h"
#include "DialogHtmlHelp.h"
//UINT RunBackHelp( LPVOID pP );

// Lancer aide
class LaunchHelp {
public:
  LaunchHelp::LaunchHelp();
  LaunchHelp::~LaunchHelp();
  void Help();
  void Help(CString page);
  CDialogHtmlHelp b;
  CString page;
private:
  void GoHelp();
};

#endif

