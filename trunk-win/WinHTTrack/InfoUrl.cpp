// InfoUrl.cpp : implementation file
//

#include "stdafx.h"

/* Externe C */
#include <WS2tcpip.h>  // Note: weird C2894 error if not included here
extern "C" {
  #include "HTTrackInterface.h"
  #include "htscore.h"
}

#include "Shell.h"
#include "InfoUrl.h"
#include "NewLang.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern HICON httrack_icon;

/////////////////////////////////////////////////////////////////////////////
// CInfoUrl dialog


CInfoUrl::CInfoUrl(CWnd* pParent /*=NULL*/)
	: CDialog(CInfoUrl::IDD, pParent)
{
	//{{AFX_DATA_INIT(CInfoUrl)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CInfoUrl::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CInfoUrl)
	DDX_Control(pDX, IDC_backlist, m_ctl_backlist);
	DDX_Control(pDX, IDC_slider, m_slider);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CInfoUrl, CDialog)
	//{{AFX_MSG_MAP(CInfoUrl)
	ON_WM_TIMER()
	ON_WM_CLOSE()
	ON_BN_CLICKED(IDC_Freeze, OnFreeze)
	ON_WM_CREATE()
	ON_CBN_SELCHANGE(IDC_backlist, OnSelchangebacklist)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CInfoUrl message handlers

BOOL CInfoUrl::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
  SetIcon(httrack_icon,false);
  SetIcon(httrack_icon,true);  
  SetForegroundWindow();   // yop en premier plan!

  // Patcher l'interface pour les Français ;-)
  if (LANG_T(-1)) {    // Patcher en français
    SetWindowTextCP(this, LANG_X1);
    SetDlgItemTextCP(this, IDC_Freeze,LANG_X2);
    SetDlgItemTextCP(this, IDC_STATIC_moreinfos,LANG_X3);
    SetDlgItemTextCP(this, IDOK,LANG_CLOSE);
  }

  StartTimer();
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CInfoUrl::StartTimer() {
  if (!timer) {
    timer=SetTimer(WM_TIMER,100,NULL);
  }
}
void CInfoUrl::StopTimer() {
  if (timer) {
    KillTimer(timer);
    timer=0;
    StatsBufferback=NULL;
  }
}

char* ToBool(int i) {
  if (i)
    return "yes";
  else
    return "no";
}

char* ToStatuscode(int statuscode) {
  if (statuscode<0) {
    return "unknown status";
  } else {
    static char msg[1024];
    msg[0]='\0';
    infostatuscode(msg,statuscode);
    return msg;
  }
}

char* ToStatus(int i) {
  switch(i) {
  case 0:
    return "ready";
    break;
  case 1:
    return "receiving";
    break;
  case 101:
    return "searching dns";
    break;
  case 100:
    return "waiting for connection";
    break;
  case 99:
    return "receiving header";
    break;
  case 98:
    return "receiving chunk header";
    break;
  case 1000:
    return "ftp session";
    break;
  case -1:
    return "empty";
    break;
  default:
    return "unknown";
    break;
  }
}

void CInfoUrl::OnTimer(UINT_PTR nIDEvent) 
{
  static char old_info[8192]="";
  //
  if (termine) {
    EndDialog(IDOK);
    return;
  }
  if (!StatsBufferback) return;
  //
  lien_back* back=(lien_back*) StatsBufferback;
  //
  if (m_ctl_backlist.GetDroppedState()==0) {
    m_ctl_backlist.Clear();
    m_ctl_backlist.ResetContent();
    int i;
    int back_max=StatsBufferback_max;
    for(i=0;i<back_max;i++) {
      if (termine) {
        EndDialog(IDOK);
        return;
      }
      if (back[i].status != -1) {
        char st[HTS_URLMAXSIZE*4];
        sprintf(st,"%02d: ",i);
        strncatbuff(st,back[i].url_adr,256);
        strncatbuff(st,back[i].url_fil,256);
        m_ctl_backlist.AddString(st);
      }
    }
  }
  //
  if (back) {
    CInfoUrl box;
    char info[8192]; info[0]='\0';
    char total[256]; total[0]='\0';
    char info100[256]; info100[0]='\0';
    int offset=0;
    if (back[id].status != -1) {        // utilisé
      if (back[id].r.totalsize>0) {
        sprintf(total,LLintP,(LLint)back[id].r.totalsize);
        offset=(int) ((LONGLONG) ((LONGLONG) back[id].r.size*(LONGLONG) 100)/((LONGLONG) back[id].r.totalsize));
        sprintf(info100,"(%d%%)",offset);
      } else {
        sprintf(total,"unknown");
        sprintf(info100,"");
      }
      sprintf(info,"File: %s\r\nTotal length: %s\r\nBytes downloaded: "LLintP" %s\r\nCurrent state: %s",back[id].url_sav,total,back[id].r.size,info100,ToStatus(back[id].status));
    }
    //
    if (strcmp(old_info,info)) {
      char moreinfo[8192]; moreinfo[0]='\0';
      lien_back* backitem=&back[id];
      if (backitem) {
        if (back[id].status != -1) {        // utilisé
          sprintf(moreinfo+strlen(moreinfo),"Host: %s\r\n",backitem->url_adr);
          sprintf(moreinfo+strlen(moreinfo),"File: %s\r\n",backitem->url_fil);
          sprintf(moreinfo+strlen(moreinfo),"Name: %s\r\n",backitem->url_sav);
          if (backitem->location_buffer)
            if (backitem->location_buffer[0])
              sprintf(moreinfo+strlen(moreinfo),"MoveToLocation: %s\r\n",backitem->location_buffer);
            //
            sprintf(moreinfo+strlen(moreinfo),"ContentType: %s\r\n",backitem->r.contenttype);
            //
            sprintf(moreinfo+strlen(moreinfo),"StatusCode: %d (%s)\r\n",backitem->r.statuscode,ToStatuscode(backitem->r.statuscode));
            sprintf(moreinfo+strlen(moreinfo),"InternalStatus: %d (%s)\r\n",backitem->status,ToStatus(backitem->status));
            if (backitem->r.msg[0])
              sprintf(moreinfo+strlen(moreinfo),"StatusMessage: %s\r\n",backitem->r.msg);
            //
            sprintf(moreinfo+strlen(moreinfo),"HTTP/1.1: %s\r\n",ToBool(backitem->r.req.http11));
            sprintf(moreinfo+strlen(moreinfo),"ChunkMode: %s\r\n",ToBool(backitem->r.is_chunk));
            if (backitem->is_chunk)
              sprintf(moreinfo+strlen(moreinfo),"CurrentChunkSize: "LLintP"\r\n",backitem->chunk_size);
            //
            sprintf(moreinfo+strlen(moreinfo),"TestMode: %s\r\n",ToBool(backitem->testmode));
            sprintf(moreinfo+strlen(moreinfo),"HeadRequest: %s\r\n",ToBool(backitem->head_request));
            sprintf(moreinfo+strlen(moreinfo),"NotModified: %s\r\n",ToBool(backitem->r.notmodified));
            //
            sprintf(moreinfo+strlen(moreinfo),"WriteToDisk: %s\r\n",ToBool(backitem->r.is_write));
            sprintf(moreinfo+strlen(moreinfo),"LocalFile: %s\r\n",ToBool(backitem->r.is_file));
            //
            sprintf(moreinfo+strlen(moreinfo),"Size: "LLintP"\r\n",backitem->r.size);
            sprintf(moreinfo+strlen(moreinfo),"TotalSize: "LLintP"\r\n",backitem->r.totalsize);
        } else
          strcpybuff(moreinfo,"Transfer complete in this buffer, waiting for next file");
      }
      //
      SetDlgItemTextUTF8(this, IDC_InfoUrl,info);
      SetDlgItemTextUTF8(this, IDC_Info100,info100);
      SetDlgItemTextUTF8(this, IDC_MoreInfoUrl,moreinfo);
      //
      m_slider.SetRange(0,100);
      m_slider.SetPos(offset);
      m_slider.RedrawWindow();
      //
      strcpybuff(old_info,info);
    }
  }
  //
  CDialog::OnTimer(nIDEvent);
}

void CInfoUrl::OnClose() 
{
  StopTimer();
	CDialog::OnClose();
}

void CInfoUrl::OnFreeze() 
{
  if (this->IsDlgButtonChecked(IDC_Freeze)) {
    StopTimer();
  } else {
    StartTimer();
  }
}

int CInfoUrl::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CDialog::OnCreate(lpCreateStruct) == -1)
		return -1;
	
  timer=0;	

	return 0;
}

void CInfoUrl::OnSelchangebacklist() 
{
  CString st;
  int index=m_ctl_backlist.GetCurSel();
  if (index!=CB_ERR) {
    m_ctl_backlist.GetLBText(index,st);
    int a=st.Find(':');
    if (a>=0) {
      char s[256]; s[0]='\0';
      strncatbuff(s,st,a);
      sscanf(s,"%d",&id);
      this->OnTimer(0);
    }
  }
}
