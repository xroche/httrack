// OptionTab7.cpp : implementation file
//

#include "stdafx.h"
#include "Shell.h"
#include "OptionTab7.h"
#include "AddFilter.h"

/* basic HTTrack defs */
extern "C" {
  #include "HTTrackInterface.h"
  //#include "htsglobal.h"
  //#include "htsbase.h"
}

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// COptionTab7 property page

IMPLEMENT_DYNCREATE(COptionTab7, CPropertyPage)

COptionTab7::COptionTab7() : CPropertyPage(COptionTab7::IDD)
{
  // Patcher titre
  if (LANG_T(-1)) {    // Patcher en français
    m_psp.pszTitle=LANG(LANG_IOPT7); // titre
    m_psp.dwFlags|=PSP_USETITLE;
  }
  m_psp.dwFlags|=PSP_HASHELP;
  //
	//{{AFX_DATA_INIT(COptionTab7)
	m_url2 = _T("");
	//}}AFX_DATA_INIT
}

COptionTab7::~COptionTab7()
{
}

void COptionTab7::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptionTab7)
	DDX_Text(pDX, IDC_URL2, m_url2);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptionTab7, CPropertyPage)
	//{{AFX_MSG_MAP(COptionTab7)
	ON_BN_CLICKED(IDC_ADD1, OnAdd1)
	ON_BN_CLICKED(IDC_ADD2, OnAdd2)
	ON_BN_CLICKED(IDC_CHECK1, OnCheck1)
	ON_BN_CLICKED(IDC_CHECK2, OnCheck2)
	ON_BN_CLICKED(IDC_CHECK3, OnCheck3)
	//}}AFX_MSG_MAP
  ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptionTab7 message handlers

void NewFilter(int i,char* s) {  // 0: forbid 1: accept
  CAddFilter AddF;
  AddF.type=i;
  if (AddF.type==0)
    AddF.m_addtype=LANG(LANG_B20); /*"Links following this rule will be forbidden:"*/
  else
    AddF.m_addtype=LANG(LANG_B21); // "Links following this rule will be accepted:";
  if (AddF.DoModal()==IDOK) {
    char query[2048],t[256],as[256];
    char* q;

    // error
    if (AddF.m_afquery.GetLength() >= sizeof(query) - 2 ) {
      return;
    }

    strcpybuff(s,"");
    strcpybuff(query,AddF.m_afquery);
    q=query;
    
    if (AddF.m_aftype==10) {
      if (i==0)
        strcpybuff(s,"-");
      else
        strcpybuff(s,"+");
      strcatbuff(s,"*");
    } else {
      while(strlen(q)>0) {
        while ((*q==' ') || (*q==',')) q++;
        strcpybuff(t,"");
        {  // prochain (séparé par des ,)
          char *a,*b;
          a=strchr(q,' ');
          b=strchr(q,',');
          if (a && b) {  // départager
            if ( b < a)
              a=b;
          } else if (b) a=b;
          
          if (a) {
            strcpybuff (t,"");
            strncat(t,q,a-q);
            q=a+1;
          } else {
            strcpybuff(t,q);
            strcpybuff(q,"");
          }
        }
        
        if (strlen(t)>0) {
          strcpybuff(as,"");
          switch (AddF.m_aftype) {
          case 0:  // ext
            sprintf(as,"*.%s",t);
            break;
          case 1:  // contient
            sprintf(as,"*/*%s*",t);
            break;
          case 2:  // this one
            sprintf(as,"*/%s",t);
            break;
          case 3:  // folder contains
            sprintf(as,"*/*%s*/*",t);
            break;
          case 4:  // this folder
            sprintf(as,"*/%s/*",t);
            break;
          case 5:  // domaine
            sprintf(as,"*[name].%s/*",t);
            break;
          case 6:  // contien
            sprintf(as,"*[name].*[name]%s*[name].*[name]/*",t);
            break;
          case 7:  // host
            sprintf(as,"%s/*",t);
            break;
          case 8:  // link contient
            sprintf(as,"*%s*",t);
            break;
          case 9:  // lien exact
            sprintf(as,"%s",t);
            break;
          }
          if (strlen(as)>0) {
            if (i==0)
              strcatbuff(s,"-");
            else
              strcatbuff(s,"+");
            strcatbuff(s,as);
            strcatbuff(s,"\x0d\x0a");
          }
        }
      }
      
    }

  } else strcpybuff(s,"");
}


void COptionTab7::OnAdd1() 
{
  char s[1024]; s[0]='\0';
  NewFilter(0,s);
  if (strlen(s)>0) {
    char tempo[HTS_URLMAXSIZE*16];
    {
      CString st;
      GetDlgItemText(IDC_URL2,st);
      if (st.GetLength() < sizeof(tempo) - 2)
        strcpybuff(tempo,st);
      else
        tempo[0] = '\0';
    }
    if (strlen(tempo)>0) {
      if ((tempo[strlen(tempo)-1]!=' ') && (tempo[strlen(tempo)-1]!='\n') && (tempo[strlen(tempo)-1]!=13))
        strcatbuff(tempo,"\x0d\x0a");
    }
    strcatbuff(tempo,s);
    //	m_url2=tempo;
    SetDlgItemTextCP(this, IDC_URL2,tempo);
  }
}

void COptionTab7::OnAdd2() 
{
  char s[1024]; s[0]='\0';
  NewFilter(1,s);
  if (strlen(s)>0) {
    char tempo[HTS_URLMAXSIZE*16];
    {
      CString st;
      GetDlgItemText(IDC_URL2,st);
      if (st.GetLength() < sizeof(tempo) - 2)
        strcpybuff(tempo,st);
      else
        tempo[0] = '\0';
    }
    if (strlen(tempo)>0) {
      if ((tempo[strlen(tempo)-1]!=' ') && (tempo[strlen(tempo)-1]!='\n') && (tempo[strlen(tempo)-1]!=13))
        strcatbuff(tempo,"\x0d\x0a");
    }
    strcatbuff(tempo,s);
    //	m_url2=tempo;
    SetDlgItemTextCP(this, IDC_URL2,tempo);
  }
}

BOOL COptionTab7::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();
  EnableToolTips(true);     // TOOL TIPS

  // mode modif à la volée
  if (modify==1) {
    GetDlgItem(IDC_ADD1) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_ADD2) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_URL2) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_STATIC_finfo) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_STATIC_tip) ->ModifyStyle(0,WS_DISABLED);
  } else {
    GetDlgItem(IDC_ADD1) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_ADD2) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_URL2) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_STATIC_finfo) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_STATIC_tip) ->ModifyStyle(WS_DISABLED,0);
  }

  // Patcher l'interface pour les Français ;-)
  if (LANG_T(-1)) {    // Patcher en français
    SetWindowTextCP(this, LANG(LANG_B9)); // "Filtres");
    SetDlgItemTextCP(this, IDC_STATIC_finfo,LANG(LANG_B10)); // "Vous pouvez exclure ou accepter plusieurs URLs ou liens, en utilisant les jokers\nVous pouvez utiliser les virgules ou les espaces entre les filtres\nExemple: +*.zip,-www.*.com,-www.*.edu/cgi-bin/*.cgi");
    SetDlgItemTextCP(this, IDC_ADD1,LANG(LANG_B11)); // "Exclure lien(s)..");
    SetDlgItemTextCP(this, IDC_ADD2,LANG(LANG_B12)); // "Accepter lien(s)..");
    SetDlgItemTextCP(this, IDC_STATIC_tip,LANG(LANG_B13)); // "Conseil: Si vous voulez accepter tous les fichiers gif d'un site, utilisez quelque chose comme +www.monweb.com/*.gif\n(+*.gif autorisera TOUS les fichiers gif sur TOUS les sites)");
  }

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
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
BOOL COptionTab7::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
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
char* COptionTab7::GetTip(int ID)
{
  switch(ID) {
    case IDC_ADD1: return LANG(LANG_C1);   /*"Add refuse filter"*/ break;
    case IDC_ADD2: return LANG(LANG_C2);   /*"Add accept filter"*/ break;
    case IDC_URL2: return LANG(LANG_C3);   /*"Extra filters"*/ break;
  }
  return "";
}
// TOOL TIPS
// ------------------------------------------------------------

void COptionTab7::EnsureIncluded(BOOL state, CString filter)  {
  /* wipe all selected filters */
  CString st;
  CString ftok=filter;
  GetDlgItemText(IDC_URL2,st);
  ftok+=" ";
  while(ftok.Find(' ')>=0) {
    CString token=ftok.Mid(0,ftok.Find(' '));
    ftok=ftok.Mid(ftok.Find(' ')+1);
    ftok.TrimLeft();
    st.Replace(token,"");
    st.Replace("\r"," ");
    st.Replace("\t"," ");
    st.Replace("  "," ");
    st.Replace(" \n","\n");
    st.Replace("\n ","\n");
    st.Replace("\n\n","\n");
  }
  st.TrimLeft();
  st.TrimRight();
  /* add ? */
  if (state) {
    st+="\n";
    st+=filter;
  }
  st.Replace("\n","\r\n");      // W32 compatible
  SetDlgItemTextCP(this, IDC_URL2,st);
}


void COptionTab7::OnCheck1() 
{
  EnsureIncluded(this->IsDlgButtonChecked(IDC_CHECK1),"+*.gif +*.jpg +*.jpeg +*.png +*.tif +*.bmp");
}

void COptionTab7::OnCheck2() 
{
  EnsureIncluded(this->IsDlgButtonChecked(IDC_CHECK2),"+*.zip +*.tar +*.tgz +*.gz +*.rar +*.z +*.exe");
}

void COptionTab7::OnCheck3() 
{
  EnsureIncluded(this->IsDlgButtonChecked(IDC_CHECK3),"+*.mov +*.mpg +*.mpeg +*.avi +*.asf +*.mp3 +*.mp2 +*.rm +*.wav +*.vob +*.qt +*.vid +*.ac3 +*.wma +*.wmv");
}
