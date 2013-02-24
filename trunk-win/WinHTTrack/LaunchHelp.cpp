// LaunchHelp.cpp : implementation file
//

#define VIEW_HELP 0

#include "stdafx.h"
#include "LaunchHelp.h"
#include "DialogHtmlHelp.h"
#include "process.h"

#if VIEW_HELP
#include "htmlfrm.h"
#endif

LaunchHelp::LaunchHelp() {
  page="";
}
LaunchHelp::~LaunchHelp() {
  if (b.m_hWnd) {
    b.EndDialog(IDCANCEL);
  }
}

void LaunchHelp::Help(CString page) {
#if VIEW_HELP
#else
  if (!b)
    this->page=page;
  else {
    if (b.m_hWnd)
      this->b.Go(page);
    else
      this->page=page;
  }
#endif
  GoHelp();
}

void LaunchHelp::Help() {
  Help("index.html");
}
  
void LaunchHelp::GoHelp() {
#if VIEW_HELP
  CHtmlFrame* frm=new CHtmlFrame;
	if (!frm->LoadFrame(IDR_HELPFRM))
		return;
  frm->ShowWindow(SW_SHOWNORMAL);
	frm->UpdateWindow();
#else
  if (!b.m_hWnd) {
    b.page=page;
    RECT rect;
    rect.bottom=rect.left=rect.right=rect.top=0;
    b.Create(NULL,NULL,WS_OVERLAPPEDWINDOW,rect,NULL,0);
    b.ShowWindow(SW_SHOWNORMAL);
  } else {
    b.SetForegroundWindow();
  }
#endif
}

