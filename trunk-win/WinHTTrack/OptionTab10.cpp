// OptionTab10.cpp : implementation file
//

#include "stdafx.h"
#include "Shell.h"
#include "OptionTab10.h"
#include "ProxyId.h"

#include <ws2tcpip.h>
#include <Wspiapi.h>
//#include <winsock2.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/* Externe C */
extern "C" {
  #include "HTTrackInterface.h"
  //#include "htsbase.h"
  //#include "htslib.h"
}

  //extern char* jump_identification(char*);

/////////////////////////////////////////////////////////////////////////////
// COptionTab10 property page

IMPLEMENT_DYNCREATE(COptionTab10, CPropertyPage)

COptionTab10::COptionTab10() : CPropertyPage(COptionTab10::IDD)
{
  // Patcher titre
  if (LANG_T(-1)) {    // Patcher en français
    m_psp.pszTitle=LANG(LANG_IOPT10); // titre
    m_psp.dwFlags|=PSP_USETITLE;
  }
  m_psp.dwFlags|=PSP_HASHELP;
  //
	//{{AFX_DATA_INIT(COptionTab10)
	m_proxy = _T("");
	m_port = _T("");
	m_ftpprox = FALSE;
	//}}AFX_DATA_INIT
}

COptionTab10::~COptionTab10()
{
}

void COptionTab10::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptionTab10)
	DDX_Control(pDX, IDC_prox, m_ctl_prox);
	DDX_Control(pDX, IDC_PWDHIDE, m_ctl_pwdhide);
	DDX_Control(pDX, IDC_ftpprox, m_ctl_ftpprox);
	DDX_Control(pDX, IDC_portprox, m_ctl_portprox);
	DDX_Control(pDX, IDC_proxtitle, m_ctl_proxtitle);
	DDX_Text(pDX, IDC_prox, m_proxy);
	DDX_Text(pDX, IDC_portprox, m_port);
	DDX_Check(pDX, IDC_ftpprox, m_ftpprox);
	//}}AFX_DATA_MAP
}

#define wm_ProxySearch (WM_USER + 1)
BEGIN_MESSAGE_MAP(COptionTab10, CPropertyPage)
	//{{AFX_MSG_MAP(COptionTab10)
	ON_BN_CLICKED(IDC_proxyconfigure, Onproxyconfigure)
	ON_EN_CHANGE(IDC_prox, OnChangeprox)
	ON_BN_CLICKED(IDC_PWDHIDE, OnPwdhide)
	ON_CBN_SELCHANGE(IDC_prox, OnChangeprox)
	ON_CBN_EDITCHANGE(IDC_prox, OnChangeprox)
	ON_CBN_EDITUPDATE(IDC_prox, OnChangeprox)
	//}}AFX_MSG_MAP
  ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
  ON_MESSAGE( wm_ProxySearch, ProxySearch0 )
  ON_MESSAGE( wm_ProxySearch+1, ProxySearch1 )
  ON_MESSAGE( wm_ProxySearch+2, ProxySearch2 )
  ON_MESSAGE( wm_ProxySearch+3, ProxySearch3 )
  ON_MESSAGE( wm_ProxySearch+4, ProxySearch4 )
  ON_MESSAGE( wm_ProxySearch+5, ProxySearch5 )
  ON_MESSAGE( wm_ProxySearch+6, ProxySearch6 )
  ON_MESSAGE( wm_ProxySearch+7, ProxySearch7 )
  ON_MESSAGE( wm_ProxySearch+8, ProxySearch8 )
  ON_MESSAGE( wm_ProxySearch+9, ProxySearch9 )
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptionTab10 message handlers


BOOL COptionTab10::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();
	
  EnableToolTips(true);     // TOOL TIPS

  /* hide password */
  CString st;
  GetDlgItemText(IDC_prox,st);
  if (st.Find('@')>=0) {
    m_ctl_pwdhide.SetCheck(1);
    OnPwdhide();
  } else {
    m_ctl_pwdhide.SetCheck(0);
    OnPwdhide();
  }

  if (LANG_T(-1)) {    // Patcher en français
    SetDlgItemTextCP(this, IDC_proxyconfigure,LANG(LANG_I47b)); // "Configurer"
    SetDlgItemTextCP(this, IDC_ftpprox,LANG(LANG_I47c));
    SetDlgItemTextCP(this, IDC_PWDHIDE,LANG_HIDEPWD);  /* Hide password */
  }  

  // mode modif à la volée
  if (modify==1) {
    GetDlgItem(IDC_prox           ) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_portprox       ) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_proxyconfigure ) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_proxtitle      ) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_ftpprox        ) ->ModifyStyle(0,WS_DISABLED);
  } else {
    GetDlgItem(IDC_prox           ) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_portprox       ) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_proxyconfigure ) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_proxtitle      ) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_ftpprox        ) ->ModifyStyle(WS_DISABLED,0);
  }

  CString str;
  GetDlgItemText(IDC_prox,str);
  m_ctl_prox.ResetContent();
  m_ctl_prox.AddString("");

  HKEY phkResult;
  if (RegOpenKeyEx(HKEY_CURRENT_USER,"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",0,KEY_READ,&phkResult)==ERROR_SUCCESS) {
    DWORD type=0;
    DWORD datasize=1000;
    char data[1024];
    data[0]='\0';
    if (RegQueryValueEx(phkResult,"ProxyServer",0,&type,(unsigned char*)&data,&datasize)==ERROR_SUCCESS) {
      if (datasize) {
        char* a=strchr(data,':');
        if (a)
          *a='\0';
        m_ctl_prox.AddString(data);
      }
    }
    RegCloseKey(phkResult);
  }
  SetDlgItemTextCP(this, IDC_prox,str);

  // Do not search for a proxy everytime
  CWinApp* pApp = AfxGetApp();
  if (pApp->GetProfileInt("Interface","FirstProxySearch",0) != 1) {
    pApp->WriteProfileInt("Interface","FirstProxySearch",1);
    
    // Launch proxy name search
    int i=0;
    WSAAsyncGetHostByName(this->m_hWnd,wm_ProxySearch+i,(ProxyDetectName[i]="proxy")   ,this->ProxyDetectBuff[i],sizeof(this->ProxyDetectBuff[i])); i++;
    WSAAsyncGetHostByName(this->m_hWnd,wm_ProxySearch+i,(ProxyDetectName[i]="www")     ,this->ProxyDetectBuff[i],sizeof(this->ProxyDetectBuff[i])); i++;
    WSAAsyncGetHostByName(this->m_hWnd,wm_ProxySearch+i,(ProxyDetectName[i]="ns")      ,this->ProxyDetectBuff[i],sizeof(this->ProxyDetectBuff[i])); i++;
    WSAAsyncGetHostByName(this->m_hWnd,wm_ProxySearch+i,(ProxyDetectName[i]="web")     ,this->ProxyDetectBuff[i],sizeof(this->ProxyDetectBuff[i])); i++;
    WSAAsyncGetHostByName(this->m_hWnd,wm_ProxySearch+i,(ProxyDetectName[i]="ntserv")  ,this->ProxyDetectBuff[i],sizeof(this->ProxyDetectBuff[i])); i++;
    WSAAsyncGetHostByName(this->m_hWnd,wm_ProxySearch+i,(ProxyDetectName[i]="gate")    ,this->ProxyDetectBuff[i],sizeof(this->ProxyDetectBuff[i])); i++;
    WSAAsyncGetHostByName(this->m_hWnd,wm_ProxySearch+i,(ProxyDetectName[i]="gateway") ,this->ProxyDetectBuff[i],sizeof(this->ProxyDetectBuff[i])); i++;
    WSAAsyncGetHostByName(this->m_hWnd,wm_ProxySearch+i,(ProxyDetectName[i]="firewall"),this->ProxyDetectBuff[i],sizeof(this->ProxyDetectBuff[i])); i++;
    WSAAsyncGetHostByName(this->m_hWnd,wm_ProxySearch+i,(ProxyDetectName[i]="cache")   ,this->ProxyDetectBuff[i],sizeof(this->ProxyDetectBuff[i])); i++;
    
  }
  
  return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}


void COptionTab10::Onproxyconfigure() 
{
  CProxyId proxy;
  char adr[256];
  CString s;
  GetDlgItemText(IDC_portprox,s);
  proxy.m_proxport=s;
  GetDlgItemText(IDC_prox,s);
  strcpybuff(adr,s);
  proxy.m_proxadr=jump_identification(adr);
  if (jump_identification(adr)!=adr) {
    char user_pass[256]; user_pass[0]='\0';
    char* a;
    size_t nsize = (size_t) ( ( jump_identification(adr) - adr ) - 1 );
    strncatbuff(user_pass,adr,nsize);
    a=strchr(user_pass,':');
    if (a)
      *a='\0';
    else
      a=user_pass+strlen(user_pass);
    proxy.m_proxlogin=user_pass;
    proxy.m_proxpass=a+1;
  }
  if (proxy.DoModal() == IDOK) {
    if (proxy.m_proxlogin.GetLength()==0) {
      SetDlgItemTextCP(this, IDC_prox,proxy.m_proxadr);
      m_ctl_pwdhide.SetCheck(0);
      OnPwdhide();
    } else {
      SetDlgItemTextCP(this, IDC_prox,proxy.m_proxlogin+":"+proxy.m_proxpass+"@"+proxy.m_proxadr);
      m_ctl_pwdhide.SetCheck(1);
      OnPwdhide();
    }
    SetDlgItemTextCP(this, IDC_portprox,proxy.m_proxport);
  }
}



// ------------------------------------------------------------
// TOOL TIPS
//
// ajouter dans le .cpp:
// remplacer les deux Wid1:: par le nom de la classe::
// dans la message map, ajouter
// ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
// dans initdialog ajouter
// EnableToolTips(true);     // TOOL TIPS
//
// ajouter dans le .h:
// char* GetTip(int id);
// et en generated message map
// afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
BOOL COptionTab10::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
{
  TOOLTIPTEXT *pTTT = (TOOLTIPTEXT *)pNMHDR;
  UINT_PTR nID =pNMHDR->idFrom;
  if (pTTT->uFlags & TTF_IDISHWND)
  {
    // idFrom is actually the HWND of the tool
    nID = ::GetDlgCtrlID((HWND)nID);
    if(nID)
    {
      char* st=GetTip((int)nID);
      if (st != "") {
        pTTT->lpszText = st;
        pTTT->hinst = AfxGetResourceHandle();
        return(TRUE);
      }
    }
  }
  return(FALSE);
}
char* COptionTab10::GetTip(int ID)
{
  switch(ID) {
    case IDC_prox:  return LANG(LANG_G14); break; // "Proxy if needed","Proxy si besoin"); break;
    case IDC_portprox: return LANG(LANG_G15); break; // "Proxy port","Port du proxy"); break;
    case IDC_proxyconfigure: return LANG_G15b; break;
    case IDC_ftpprox: return LANG_G15c; break;
  }
  return "";
}
// TOOL TIPS
// ------------------------------------------------------------



void COptionTab10::OnChangeprox() 
{
  CString st="";
  char tempo[8192];
  CString port="";

  GetDlgItemText(IDC_prox,st);

  int pos=st.Find(':');
  if (pos>=0) {
    port=st.Mid(pos+1);
    st=st.Mid(0,pos);
    SetDlgItemTextCP(this, IDC_prox,st);
  }
  strcpybuff(tempo,st);
  
  int ex=0;
  do {
    if (strlen(tempo)>0) {
      switch (tempo[strlen(tempo)-1]) {
      case 10: case 13: case 32: tempo[strlen(tempo)-1]='\0';
        break;
      default: ex=1;
        break;
      }
    } else ex=1;
  } while(!ex);
  
  if ((strlen(tempo)>0)!=prox_status) {
    prox_status=!prox_status;
    
    if (strlen(tempo)>0) {
      CString st="";
      GetDlgItemText(IDC_portprox,st);
      if (strlen(st)==0) {
        SetDlgItemTextCP(this, IDC_portprox,"8080");
        m_ctl_portprox.RedrawWindow();
      }
      m_ctl_proxtitle.ModifyStyle(WS_DISABLED,0);
    }
    else {
      SetDlgItemTextCP(this, IDC_portprox,"");
      m_ctl_portprox.RedrawWindow();
      m_ctl_proxtitle.ModifyStyle(0,WS_DISABLED);
    }
    m_ctl_proxtitle.RedrawWindow();
  }  

  if (port.GetLength())
    SetDlgItemTextCP(this, IDC_portprox,port);
}

void COptionTab10::OnPwdhide() 
{
  /* ES_PASSWORD */
  if (IsDlgButtonChecked(IDC_PWDHIDE)) {
    GetDlgItem(IDC_prox)->ModifyStyle(WS_VISIBLE,0);
    GetDlgItem(IDC_prox)->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_portprox)->ModifyStyle(WS_VISIBLE,0);
    GetDlgItem(IDC_portprox)->ModifyStyle(0,WS_DISABLED);
  } else {
    GetDlgItem(IDC_prox)->ModifyStyle(0,WS_VISIBLE);
    GetDlgItem(IDC_prox)->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_portprox)->ModifyStyle(0,WS_VISIBLE);
    GetDlgItem(IDC_portprox)->ModifyStyle(WS_DISABLED,0);
  }
  RedrawWindow();
} 


LRESULT COptionTab10::ProxySearch0(WPARAM wParam,LPARAM lParam) { return ProxySearch(0,wParam,lParam); }
LRESULT COptionTab10::ProxySearch1(WPARAM wParam,LPARAM lParam) { return ProxySearch(1,wParam,lParam); }
LRESULT COptionTab10::ProxySearch2(WPARAM wParam,LPARAM lParam) { return ProxySearch(2,wParam,lParam); }
LRESULT COptionTab10::ProxySearch3(WPARAM wParam,LPARAM lParam) { return ProxySearch(3,wParam,lParam); }
LRESULT COptionTab10::ProxySearch4(WPARAM wParam,LPARAM lParam) { return ProxySearch(4,wParam,lParam); }
LRESULT COptionTab10::ProxySearch5(WPARAM wParam,LPARAM lParam) { return ProxySearch(5,wParam,lParam); }
LRESULT COptionTab10::ProxySearch6(WPARAM wParam,LPARAM lParam) { return ProxySearch(6,wParam,lParam); }
LRESULT COptionTab10::ProxySearch7(WPARAM wParam,LPARAM lParam) { return ProxySearch(7,wParam,lParam); }
LRESULT COptionTab10::ProxySearch8(WPARAM wParam,LPARAM lParam) { return ProxySearch(8,wParam,lParam); }
LRESULT COptionTab10::ProxySearch9(WPARAM wParam,LPARAM lParam) { return ProxySearch(9,wParam,lParam); }

// Search dor a proxy
#define WSAGETASYNCERROR(lParam)            HIWORD(lParam)
#define WSAGETASYNCBUFLEN(lParam)           LOWORD(lParam)
LRESULT COptionTab10::ProxySearch(int id,WPARAM wParam,LPARAM lParam) {
  int errcode=WSAGETASYNCERROR(lParam);
  int bufflen=WSAGETASYNCBUFLEN(lParam);

  if ((id>=0) && (id<10)) {
    if (!errcode) {
      //HOSTENT* host=(HOSTENT*) ProxyDetectBuff[id];
      //if (host->h_name) {
      m_ctl_prox.AddString(ProxyDetectName[id]);
      //}
    }
  }

  return 0;
}
