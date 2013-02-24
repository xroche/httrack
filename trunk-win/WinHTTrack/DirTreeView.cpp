// DirTreeView.cpp : implementation file
//

#include "stdafx.h"
#include "winhttrack.h"
#include "DirTreeView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern "C" {
  #include "HTTrackInterface.h"
  //#include "htsbase.h"
};

#include "MainFrm.h"

CDirTreeView* this_DirTreeView=NULL;

/////////////////////////////////////////////////////////////////////////////
// CDirTreeView

IMPLEMENT_DYNCREATE(CDirTreeView, CTreeView)

CDirTreeView::CDirTreeView()
{
  this_DirTreeView=this;
  redraw_in_progress=0;
  timer=0;
  count_whandle=0;
  docType="<nullType>";
}

CDirTreeView::~CDirTreeView()
{
  WaitThreads();
  this_DirTreeView=NULL;
  if (imagelist.m_hImageList) {
    imagelist.Detach();
    imagelist.m_hImageList=NULL;
  }
}


BEGIN_MESSAGE_MAP(CDirTreeView, CTreeView)
	//{{AFX_MSG_MAP(CDirTreeView)
	ON_WM_TIMER()
	ON_WM_SHOWWINDOW()
	ON_NOTIFY_REFLECT(TVN_ITEMEXPANDING, OnItemexpanding)
	ON_NOTIFY_REFLECT(NM_DBLCLK, OnDblclk)
	ON_NOTIFY_REFLECT(NM_RCLICK, OnRclick)
	ON_NOTIFY_REFLECT(TVN_ENDLABELEDIT, OnEndlabeledit)
	ON_NOTIFY_REFLECT(TVN_KEYDOWN, OnKeydown)
	ON_NOTIFY_REFLECT(NM_CLICK, OnClick)
	ON_WM_KILLFOCUS()
	ON_NOTIFY_REFLECT(TVN_SELCHANGED, OnSelchanged)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDirTreeView drawing

void CDirTreeView::OnDraw(CDC* pDC)
{
	CDocument* pDoc = GetDocument();
	// TODO: add draw code here
}

/////////////////////////////////////////////////////////////////////////////
// CDirTreeView diagnostics

#ifdef _DEBUG
void CDirTreeView::AssertValid() const
{
	CTreeView::AssertValid();
}

void CDirTreeView::Dump(CDumpContext& dc) const
{
	CTreeView::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CDirTreeView message handlers


BOOL CDirTreeView::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext) 
{
  dwStyle|=(TVS_HASLINES|TVS_LINESATROOT|TVS_HASBUTTONS|TVS_SHOWSELALWAYS|TVS_EDITLABELS);
  //dwStyle|=(TVS_HASLINES|TVS_LINESATROOT|TVS_HASBUTTONS|TVS_SHOWSELALWAYS|TVS_DISABLEDRAGDROP);
  //
  dwStyle&=(~(TVS_NOTOOLTIPS));     /* disabled */

/*
  dwStyle|=(WS_DISABLED);
  dwStyle&=(~(WS_VISIBLE));
*/

  int r=CWnd::Create(lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID, pContext);

  GetTreeCtrl().SetImageList(&imagelist,TVSIL_NORMAL);
  /*GetTreeCtrl().SetToolTips(&m_TreeViewToolTip);*/

  RefreshTree();
  return r;
}


void CDirTreeView::RefreshTree() {
  CTreeCtrl& tree=GetTreeCtrl();
  if (!tree) return;   /* error */

  tree.DeleteAllItems();
  int len=32768;
  char* adr=(char*)malloc(len+4);
  if (adr) {
    if (GetLogicalDriveStrings(len,adr)>0) {
      char*a=adr;
      while(*a) {
        char* next=a+strlen(a)+1;
        if (*(a+strlen(a)-1)=='\\')
          *(a+strlen(a)-1)='\0';
        HTREEITEM it=tree.InsertItem(a);
        if (it) {
          tree.SetItemText(it,"");
          tree.InsertItem("expanding..",it,TVI_SORT);
         
          // icone
          char name[256];
          strcpybuff(name,a);
          strcatbuff(name,"\\");
          SHFILEINFO info;
          if (SHGetFileInfo(name,0,&info,sizeof(info),SHGFI_SMALLICON | SHGFI_SYSICONINDEX | SHGFI_SHELLICONSIZE | SHGFI_DISPLAYNAME)) {
            //SHGetFileInfo(name,0,&info,sizeof(info),SHGFI_SMALLICON | SHGFI_SYSICONINDEX | SHGFI_TYPENAME | SHGFI_SHELLICONSIZE | SHGFI_DISPLAYNAME | SHGFI_ATTRIBUTES); 
            tree.SetItemImage(it,info.iIcon,info.iIcon);
            CString disp=info.szDisplayName;
            if (disp.ReverseFind('('))
              disp=disp.Left(disp.Find('('));
            disp+="<"; disp+=a; disp+=">"; 
            tree.SetItemText(it,disp);
          }
        }

        a=next;
      }
    }
    free(adr);
  }
  
  BuildTrackHandles();
}

/* remise à zéro */
void CDirTreeView::ResetTree() {
  if (!GetTreeCtrl()) return;   /* error */
  RefreshTree();
  StartTimer();
}

/* attendre fin des threads refresh */
void CDirTreeView::WaitThreads() {
  /* attendre */
  int w=0;
  while((redraw_in_progress) && (w<10)) {
    Sleep(100);
    w++;
  }
}


void CDirTreeView::OnItemexpanding(NMHDR* pNMHDR, LRESULT* pResult) 
{
  *pResult = 0;
  if (GetTreeCtrl().GetStyle() & WS_VISIBLE) {    /* sinon pas de refresh */
    NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;
    
    // find out what item is getting expanded, and send that to Expand(hItem, TVE_EXPAND)
    if (pNMTreeView->hdr.code == TVN_ITEMEXPANDING)
    {
      HTREEITEM hIT = pNMTreeView->itemNew.hItem;
      DestroyTrackHandles();
      if (!RefreshDir(hIT))
        *pResult = 1;
      BuildTrackHandles();
    }
  }
}

void CDirTreeView::OnSelchanged(NMHDR* pNMHDR, LRESULT* pResult) 
{
  NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;
  // find out what item is getting expanded, and send that to Expand(hItem, TVE_EXPAND)
  if (pNMTreeView->hdr.code == TVN_SELCHANGED)
  {
    HTREEITEM hIT = pNMTreeView->itemNew.hItem;
    CString st=GetItemPath(hIT);
  
    CWnd* top_window=this;
    while(top_window->GetParent())
      top_window=top_window->GetParent();
    SetWindowTextCP(((CMainFrame*)top_window)->m_wndStatusBar, st);
  }	
  *pResult = 0;
}


void CDirTreeView::OnDblclk(NMHDR* pNMHDR, LRESULT* pResult) 
{
  HTREEITEM selected=GetTreeCtrl().GetSelectedItem();
  if (selected) {
    if (!GetTreeCtrl().ItemHasChildren(selected)) {
      CString name=GetItemPath(selected);
      if (name.Right(1)=="\\")
        name=name.Left(name.GetLength()-1);
      DWORD attrb=GetFileAttributes(name);
      if (attrb!=0xFFFFFFFF) {
        if ( !(attrb & (FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_SYSTEM|FILE_ATTRIBUTE_TEMPORARY)) ) {
          CWaitCursor wait;
          if (docType.CompareNoCase(name.Right(docType.GetLength()))==0) {
            CWinApp* app=AfxGetApp();
            POSITION pos;
            pos=app->GetFirstDocTemplatePosition();
            CDocTemplate* templ = app->GetNextDocTemplate(pos);
            pos=templ->GetFirstDocPosition();
            CDocument* doc  = templ->GetNextDoc(pos);
            if (doc) {
              //if (doc->SaveModified()) {
              AfxGetApp()->OpenDocumentFile(name);
              //}
            }
          } else {
            /* Lancer ShellEx en arrière plan */
            AfxBeginThread(CDirTreeViewShellEx,new CString(name));
          }
        }
      }
    }
  }
  *pResult = 0;
}

void CDirTreeView::OnClick(NMHDR* pNMHDR, LRESULT* pResult) 
{
  CTreeCtrl& tree=GetTreeCtrl();
  if (!tree) return;

  HTREEITEM curr_selected=tree.GetDropHilightItem();
  if (!curr_selected)
    curr_selected=tree.GetSelectedItem();
  if (curr_selected) {
    /*
    CString st=GetItemPath(curr_selected);
  
    CWnd* top_window=this;
    while(top_window->GetParent())
      top_window=top_window->GetParent();
    ((CMainFrame*)top_window)->m_wndStatusBar.SetWindowTextCP(this, st);
    */
  }

	*pResult = 0;
}

void CDirTreeView::OnRclick(NMHDR* pNMHDR, LRESULT* pResult) 
{
  CTreeCtrl& tree=GetTreeCtrl();
  if (!tree) return;
  HTREEITEM curr_selected=tree.GetDropHilightItem();
  if (!curr_selected)
    curr_selected=tree.GetSelectedItem();
  if (curr_selected) {
    CString st=GetItemPath(curr_selected);
    AfxMessageBox(st);
  }
  *pResult = 0;
}



CString CDirTreeView::GetItemName(HTREEITEM position) {
  CString st=GetTreeCtrl().GetItemText(position);
  if (st.Find('<')) {
    if (st.Right(2)==":>") {
      st=st.Mid(st.Find('<')+1);
      st=st.Left(st.GetLength()-1);
    }
  }
  return st;
}

CString CDirTreeView::GetItemPath(HTREEITEM position) {
  if (position) {
    HTREEITEM parent_it=GetTreeCtrl().GetParentItem(position);
    CString st=GetItemName(position);
    CString slash="";
    if (GetTreeCtrl().ItemHasChildren(position))
      slash="\\";
    if (st.GetLength())
      return GetItemPath(parent_it)+st+slash;
    else
      return GetItemPath(parent_it);
  } else
    return "";
}

HTREEITEM CDirTreeView::GetPathItem(CString path,BOOL open,BOOL nofail,BOOL nohide,HTREEITEM rootposition) {
  CTreeCtrl& tree=GetTreeCtrl();
  if (!tree) return NULL;   /* error */
  //
  HTREEITEM return_value=NULL;
  //
  CString left="",right="";
  int pos=path.Find('\\');
  if (pos<0)
    left=path;
  else {
    left=path.Left(pos);
    right=path.Mid(pos+1);
  }

  if (!nohide)
    tree.ModifyStyle(WS_VISIBLE,0);
  
  HTREEITEM position;
  if (!rootposition) {
    rootposition=tree.GetRootItem();
    /* position initiale */
    position=rootposition;
  } else {
    // Ouvrir?
    if (open) {
      if (tree.ItemHasChildren(rootposition)) {        /* si enfants */
        if (!(tree.GetItemState(rootposition,TVIF_STATE) & TVIS_EXPANDED)) {    /* si non ouvert */
          /*tree.SetItemState(rootposition,TVIF_STATE,
            tree.GetItemState(rootposition,TVIF_STATE) | TVIS_EXPANDED);
            */
          tree.Expand(rootposition,TVE_EXPAND);
          RefreshDir(rootposition,TRUE);
        }
      }
      
    }

    /* position initiale */
    position=tree.GetChildItem(rootposition);
  }

  left.MakeLower();
  while(position) {
    CString st=GetItemName(position);
    st.MakeLower();
    if (st==left) {
      if (right.GetLength())
        position=GetPathItem(right,open,nofail,TRUE,position);
      else
        return_value=position;      // trouvé
      position=NULL;
    }
    if (position)
      position=tree.GetNextSiblingItem(position);
  }

  // liste visible
  if (!nohide) {
    tree.ModifyStyle(0,WS_VISIBLE);
    //RedrawWindow();
    tree.GetParent()->RedrawWindow();
  }
  if (return_value)
    return return_value;
  else if (nofail)
    return rootposition;
  else
    return NULL;
}

BOOL CDirTreeView::EnsureVisible(CString path) {
  if (!GetTreeCtrl()) return FALSE;   /* error */
#if 0
  /* Lancer refresh en arrière plan */
  refreshPath=path;
  StopTimer();
  AfxBeginThread(CDirTreeViewRefresh,this);
  return TRUE;
#else
  CWaitCursor wait;
  HTREEITEM pos=GetPathItem(path,TRUE,TRUE);
  if (pos) {
    GetTreeCtrl().EnsureVisible(pos);
  } else
    return FALSE;
  return TRUE;
#endif
}

BOOL CDirTreeView::EnsureVisibleBl(CString path) {
  if (!GetTreeCtrl()) return FALSE;   /* error */
  CWaitCursor wait;
  int return_value=TRUE;
  StopTimer();
  HTREEITEM pos=GetPathItem(path,TRUE,TRUE);
  if (pos) {
    GetTreeCtrl().EnsureVisible(pos);
  } else
    return_value=FALSE;
  StartTimer();
  return return_value;
}

void CDirTreeView::RefreshPos(HTREEITEM position) {
  RefreshDir(position);
}

BOOL CDirTreeView::RefreshDir(HTREEITEM position,BOOL nohide) {
  CTreeCtrl& tree=GetTreeCtrl();
  if (!tree) return FALSE;   /* error */

  CWaitCursor wait;
  CString path=GetItemPath(position);
  CString backup_visibles="\n";

  // Pour FindFirstFile/FindNextFile
  WIN32_FIND_DATA find;
  int type;
  HANDLE h = FindFirstFile(path+"*.*",&find);

  // accessible
  if (h!=INVALID_HANDLE_VALUE) {
    // liste invisible
    if (!nohide)
      tree.ModifyStyle(WS_VISIBLE,0);

    { // backuper éléments visibles
      HTREEITEM it=tree.GetChildItem(position);
      int backup_visibles_count=0;
      while(it) {
        if (tree.ItemHasChildren(it)) {        /* si enfants */
          if (tree.GetItemState(it,TVIF_STATE) & TVIS_EXPANDED) {    /* si ouvert */
            backup_visibles+=(GetItemPath(it)+"\n");
            backup_visibles_count++;
          }
        }
        it=tree.GetNextVisibleItem(it);
      }
      if (!backup_visibles_count)
        backup_visibles="";
    }

    {  // effacer la liste
      HTREEITEM it;
      while(it=tree.GetChildItem(position))
        tree.DeleteItem(it);
    }
    
    // chargement de la liste
    HTREEITEM last_dir_it=tree.GetRootItem();
    for(type=0;type<2;type++) {
      if (h != INVALID_HANDLE_VALUE) {
        do {
          if (!(find.dwFileAttributes  & (FILE_ATTRIBUTE_SYSTEM|FILE_ATTRIBUTE_HIDDEN))) {
            if (strcmp(find.cFileName,"..")) {
              if (strcmp(find.cFileName,".")) {
                HTREEITEM it=NULL;
                if (find.dwFileAttributes  & FILE_ATTRIBUTE_DIRECTORY ) {
                  if (type==0) {
                    if (IsWindow(tree.m_hWnd)) {
                      it=tree.InsertItem(find.cFileName,position);
                      tree.InsertItem("expanding..",it,TVI_SORT);
                    }
                  }
                } else {
                  if (type==1) {
                    if (IsWindow(tree.m_hWnd))
                      it=tree.InsertItem(find.cFileName,position,last_dir_it);
                  }
                }
                // changer icone
                if (it) {
                  SHFILEINFO info;
                  SHGetFileInfo(path+find.cFileName,0,&info,sizeof(info),SHGFI_SYSICONINDEX);
                  //SHGetFileInfo(path+find.cFileName,0,&info,sizeof(info),SHGFI_SMALLICON | SHGFI_SYSICONINDEX | SHGFI_TYPENAME | SHGFI_SHELLICONSIZE | SHGFI_DISPLAYNAME | SHGFI_ATTRIBUTES); 
                  if (IsWindow(tree.GetSafeHwnd()))
                    tree.SetItemImage(it,info.iIcon,info.iIcon);
                }
              }
            }
          }
        } while(FindNextFile(h,&find));
        FindClose(h);
      }
      if (type==0)
        h = FindFirstFile(path+"*.*",&find);
    }

    // restaurer éléments visibles
    RestoreVisibles(position,backup_visibles);

    // liste visible
    if (!nohide)
      tree.ModifyStyle(0,WS_VISIBLE);
  }/* **OLD 
  else
    tree.Expand(position,TVE_COLLAPSE);
   */

  // Redessiner
  if (!nohide)
    GetParent()->RedrawWindow();
  return (h!=INVALID_HANDLE_VALUE);
}

void CDirTreeView::RestoreVisibles(HTREEITEM position,CString& backup_visibles) {
  CTreeCtrl& tree=GetTreeCtrl();
  if (!tree) return;   /* error */

  if (backup_visibles.GetLength()) {
    // réouvrir les éléments visibles
    HTREEITEM it=tree.GetChildItem(position);
    while(it) {
      if (backup_visibles.Find("\n"+GetItemPath(it)+"\n")>=0) {
        tree.Expand(it,TVE_EXPAND);
        RefreshDir(it,TRUE);       /* car appelé par RefreshDir lui même */
        RestoreVisibles(it,backup_visibles);
      }
      it=tree.GetNextSiblingItem(it);
    }
  }
}



void CDirTreeView::StartTimer() {
  if (!timer) {
    //whandle=FindFirstChangeNotification("C:\\",TRUE,FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_DIR_NAME);
    BuildTrackHandles();
    timer=SetTimer(WM_TIMER,1000,NULL);
  }
}
void CDirTreeView::StopTimer() {
  if (timer) {
    DestroyTrackHandles();
    KillTimer(timer);
    timer=0;
  }
}

void CDirTreeView::DestroyTrackHandles() 
{
  /* Fermer handles actuels */
  if (count_whandle) {
    int i;
    for(i=0;i<count_whandle;i++)
      FindCloseChangeNotification(whandle[i]);
    count_whandle=0;
  }
}

void CDirTreeView::BuildTrackHandles() 
{
  if (!GetTreeCtrl()) return;   /* error */
  DestroyTrackHandles();
  /* Lecture éléments du root visibles (non, c'est pas récursif vu que ce sont les élts visibles) */
  CTreeCtrl& it=GetTreeCtrl();
  HTREEITEM pos=it.GetFirstVisibleItem();
  while(pos) {
    CString path=GetItemPath(pos);
    CFileStatus status;
    if (it.ItemHasChildren(pos)) {        /* surveiller si enfants */
      if (it.GetItemState(pos,TVIF_STATE) & TVIS_EXPANDED) {    /* si ouvert */
        if (CFile::GetStatus(path.Left(path.GetLength()-1),status)) {   // répertoire (note: path sans le / final)
          if (status.m_attribute & 0x10 ) {       /* directory = 0x10 */
            whandle[count_whandle]=FindFirstChangeNotification(path,FALSE,FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_DIR_NAME);
            if (whandle[count_whandle] != INVALID_HANDLE_VALUE) {
              pos_whandle[count_whandle]=pos;
              count_whandle++;
            }
          }
        }
      }
    }
    pos=it.GetNextVisibleItem(pos);
  }

}

void CDirTreeView::DoTrackHandles() 
{
  CTreeCtrl& tree=GetTreeCtrl();
  if (!tree) return;   /* error */

  if (count_whandle) {
    int r=(WaitForMultipleObjects(count_whandle,whandle,FALSE,0)-WAIT_OBJECT_0);
    if ( (r>=0) && (r<count_whandle) ) {      // un item a changé
      DestroyTrackHandles();
      HTREEITEM pos=pos_whandle[r];
      RefreshPos(pos);
      tree.SetItemState(pos,TVIS_BOLD,TVIS_BOLD);
      BuildTrackHandles();      /* probleme: on va en rater certains.. */ 
      //FindNextChangeNotification(whandle[r]);
    }
    /*
    int i;
    for(i=0;i<count_whandle;i++)
      FindNextChangeNotification(whandle[i]);
    */
  }
}

void CDirTreeView::OnTimer(UINT_PTR nIDEvent) 
{
  DoTrackHandles();
  CTreeView::OnTimer(nIDEvent);
}

void CDirTreeView::OnShowWindow(BOOL bShow, UINT nStatus) 
{
  static BOOL detach_flag=FALSE;
  if (detach_flag == bShow) return;
  detach_flag=bShow;

  if (bShow) {
    HIMAGELIST hSystemImageList;
    SHFILEINFO SHFileInfo;
    hSystemImageList = (HIMAGELIST)SHGetFileInfo("", 0, &SHFileInfo, sizeof(SHFILEINFO), SHGFI_SYSICONINDEX|SHGFI_SMALLICON);
    if (!hSystemImageList) {
      MessageBox("Error: SHGetFileInfo");
      //return;
    } else {
      imagelist.Attach(hSystemImageList);
    }
  } else {
    if (imagelist.m_hImageList) {
      imagelist.Detach();
      imagelist.m_hImageList=NULL;
    }
  }
  //
  CTreeView::OnShowWindow(bShow, nStatus);
  if (bShow)
    StartTimer();
  else
    StopTimer();
}


void CDirTreeView::OnEndlabeledit(NMHDR* pNMHDR, LRESULT* pResult) 
{
	TV_DISPINFO* pTVDispInfo = (TV_DISPINFO*)pNMHDR;
	// TODO: Add your control notification handler code here

  if (pTVDispInfo->item.mask & TVIF_TEXT) {
    if (pTVDispInfo->item.pszText) {
      if (strlen(pTVDispInfo->item.pszText)) {
        CString path=GetItemPath(pTVDispInfo->item.hItem);
        if (path.Right(1)=="\\")
          path=path.Left(path.GetLength()-1);
        CString newpath=path.Left(path.ReverseFind('\\')+1)+pTVDispInfo->item.pszText;
        if (path != newpath) {
          if (!MoveFile(path,newpath)) {
            AfxMessageBox("Unable to rename "+path+" to "+newpath+"!");
          } else {
            GetTreeCtrl().SetItemText(pTVDispInfo->item.hItem,pTVDispInfo->item.pszText);
          }
        }
      }
    }
  }
  

	*pResult = 0;
}

void CDirTreeView::OnKeydown(NMHDR* pNMHDR, LRESULT* pResult) 
{
	TV_KEYDOWN* pTVKeyDown = (TV_KEYDOWN*)pNMHDR;
  HTREEITEM selected=GetTreeCtrl().GetSelectedItem();
	*pResult = 0;
	
  int key=pTVKeyDown->wVKey ;
  switch(key) {
  case 32:
    if (selected) {
      GetTreeCtrl().Expand(selected,TVE_TOGGLE);
 	    *pResult = -1;
    }
    break;
  case 13: case 10:
    if (selected) {
      GetTreeCtrl().Expand(selected,TVE_TOGGLE);
 	    *pResult = -1;
    }
    break;
  /*default:
    printf("\a");
    break;
  */
  }

}



/*
IMPLEMENT_DYNAMIC(CDirTreeViewShellEx, CWinThread)

/////////////////////////////////////////////////////////////////////////////
// CDirTreeViewShellEx
BOOL CDirTreeViewShellEx::InitInstance() {
  if (!ShellExecute(NULL,NULL,File,NULL,NULL,SW_SHOWNORMAL)) {
  }
  return 0;
}
*/
UINT CDirTreeViewShellEx( LPVOID pP ) {
  CWaitCursor wait;
  CString* File=(CString*) pP;
  if (!ShellExecute(NULL,"open",*File,NULL,NULL,SW_SHOWNORMAL)) {
  }
  delete File;
  return 0;
}

UINT CDirTreeViewRefresh( LPVOID pP ) {
  CDirTreeView* _this=(CDirTreeView*) pP;
  if (!_this->redraw_in_progress) {
    _this->redraw_in_progress=1;
    _this->EnsureVisibleBl(_this->refreshPath);
    _this->redraw_in_progress=0;
  }
  return 0;
}



void CDirTreeView::OnKillFocus(CWnd* pNewWnd) 
{
	CTreeView::OnKillFocus(pNewWnd);
	
    CWnd* top_window=this;
    while(top_window->GetParent())
      top_window=top_window->GetParent();
    SetWindowTextCP(((CMainFrame*)top_window)->m_wndStatusBar, "");
    
}

