// ----------------------------------------------------------------------
// 'extended' SHBrowseForFolder routine. ('New folder' button added)
// Written by Xavier Roche, with the help of Gil Rosin, 
// Todd Fast's routines from Pencilneck Software and other Usenet contributors.
// Freeware, but no warranty!
// 
// Usage: (example)
// CString path = XSHBrowseForFolder(this->m_hWnd,"Select path","c:\\") {
//
// To DO:
//
// #include "XSHBrowseForFolder.h"
//
// Then Add to the .rc file:
//
// IDD_NewFolder DIALOG DISCARDABLE  0, 0, 237, 46
// STYLE DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU
// CAPTION "Create New Folder"
// FONT 8, "MS Sans Serif"
// BEGIN
//     EDITTEXT        IDC_Folder,7,7,167,14,ES_AUTOHSCROLL
//     DEFPUSHBUTTON   "OK",IDOK,180,7,50,14
//     PUSHBUTTON      "Cancel",IDCANCEL,180,24,50,14
// END
// ----------------------------------------------------------------------

// TODO: Put in XSHBrowseForFolder.h your ressource definition

#include "stdafx.h"
#include "shlobj.h"
#include "XSHBrowseForFolder.h"

// our button ID
int XSHBFF_button1 = -1;

// Main routine
CString XSHBrowseForFolder(HWND hwnd,char* title,char* _path) {
  char path[MAX_PATH]; path[0]='\0';

  // Remove the last slash bar
  if (strlen(_path)>0)
  if (_path[strlen(_path)-1]=='\\')
    _path[strlen(_path)-1]='\0';

  // Get IMalloc Interface
  LPMALLOC Mymal;
  if (SHGetMalloc(&Mymal)!=NOERROR)
    return path;
  
  // Build Root directory (My Computer)
  LPITEMIDLIST Mylist;
  if (SHGetSpecialFolderLocation(hwnd,CSIDL_DRIVES,&Mylist)==NOERROR) {

    // Convert _path into browse data
    LPITEMIDLIST MyItemlist=XSHBFF_PathConvert(hwnd,_path);
    
    // Parameter structure for callback
    char Thispath[MAX_PATH]; Thispath[0]='\0';
    LPARAM CParam[2];
    CParam[0]=(LPARAM) MyItemlist;
    CParam[1]=(LPARAM) Thispath;
    // Fill the BROWSEINFO structure
    BROWSEINFO br;
    br.hwndOwner=hwnd;       // hwnd
    br.pidlRoot=NULL;        // root
    br.pszDisplayName=path;  // buffer
    br.lpszTitle=title;      // title
    br.ulFlags=BIF_RETURNONLYFSDIRS;  // dir
    br.lpfn=XSHBFF_CallbackProc;      // callback
    br.lParam=(LPARAM) CParam;          // callback params
    br.iImage=0;             // image
    // And Call SHBrowseForFolder
    LPITEMIDLIST UserList;
    if ( (UserList = SHBrowseForFolder(&br)) != NULL) {
      if (strlen(Thispath)==0) {  // No value in path
        // Convert UserList to a string
        if (SHGetPathFromIDList(UserList,path)==FALSE)
          path[0]='\0';
      } else
        strcpy(path,Thispath);
      Mymal->Free(UserList);
    }
    if (MyItemlist) Mymal->Free(MyItemlist);
    Mymal->Free(Mylist);
  }
  return path;
}

// XSHBFF_WndProc function type 
typedef LRESULT (__stdcall * XSHBFF_WndProc_type)(HWND ,UINT ,WPARAM ,LPARAM);

// Window Routine
LRESULT __stdcall XSHBFF_WndProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam) {
  static char StringSelection[MAX_PATH]="";
  static char* DirectReturnValue=NULL;
  static XSHBFF_WndProc_type Ladr=DefDlgProc;
  int wNotifyCode = HIWORD(wParam);  // notification code 
  int wID = LOWORD(wParam);          // item, control, or accelerator identifier 
  HWND hwndCtl = (HWND) lParam;      // handle of control
  //
  if (uMsg==WM_COMMAND) {            // WM_COMMAND message received
    if (wID == XSHBFF_button1 ) {   // our button! 
      if (strlen(StringSelection)>0) {  // there is a selection
        CNewFolder f;
        f.m_folder=StringSelection;
        if (StringSelection[strlen(StringSelection)-1]!='\\')
          f.m_folder+="\\";  // add a /
        if (f.DoModal()==IDOK) {
          char st[MAX_PATH];
          strcpy(st,f.m_folder);
          // Remove the last slash bar
          if (strlen(st)>0)
          if (st[strlen(st)-1]=='\\')
            st[strlen(st)-1]='\0';
          // create dir
          if (_mkdir(st))              // error
            AfxMessageBox("Folder already exists, or can not be created",MB_OK+MB_ICONEXCLAMATION);
          else {    // Select the new path
            if (DirectReturnValue) {
              strcpy(DirectReturnValue,st);
              wParam = (wParam & 0xFFFF0000) | XSHBrowseForFolder_OK;    // 'OK'
              return Ladr(hwnd,uMsg,wParam,lParam); // former window routine
            }
          }
        }
      } else
        AfxMessageBox("Please select a path first!",MB_OK);
      return 0;
    } else {
      return Ladr(hwnd,uMsg,wParam,lParam); // former window routine
    }
  } else if (uMsg==XSHBrowseForFolder_SETSTRING) {  // received from our XSHBFF_CallbackProc routine
    DirectReturnValue=(char*) lParam;
    return 0;
  } else if (uMsg==XSHBrowseForFolder_SETSTRING+1) {
    Ladr = (XSHBFF_WndProc_type) lParam;  // store former address
    return 0;
  } else if (uMsg==BFFM_SELCHANGED) {  // received from our XSHBFF_CallbackProc routine
    LPITEMIDLIST ItemSelection=(LPITEMIDLIST) lParam;  // catch the Item selection data
    if (ItemSelection) {            // valid data (not null)
      if (!SHGetPathFromIDList(ItemSelection,StringSelection)) {  // immedialtly converted into char string
        StringSelection[0]='\0';    // clear, invalid
      }
    } else
      StringSelection[0]='\0';      // clear, invalid
    return 0;
  } else {
    return Ladr(hwnd,uMsg,wParam,lParam); // former window routine
  }
}

// Callback function
int __stdcall XSHBFF_CallbackProc(HWND hwnd,UINT uMsg,LPARAM lParam,LPARAM lpData) {
  if (uMsg==BFFM_SELCHANGED) {  // selection changed
    XSHBFF_WndProc(hwnd,BFFM_SELCHANGED,0,lParam);
  } 
  else if (uMsg==BFFM_INITIALIZED) {  // init
// DO NOT ADD BUTTON
#if 0000
    int x,y,w=0,h=0;
    HWND ok = GetDlgItem(hwnd,XSHBrowseForFolder_OK);     // 'OK' button
    if (ok) {
      RECT rect;
      GetWindowRect(ok,&rect);
      
      // screen coord -> client coord
      POINT a,b;
      a.x=rect.left; a.y=rect.top; b.x=rect.right; b.y=rect.bottom;
      ScreenToClient(hwnd,&a); ScreenToClient(hwnd,&b);
      rect.left=a.x; rect.top=a.y; rect.right=b.x; rect.bottom=b.y;
      
      // button's coordinates and size
      x=rect.left;
      y=rect.top;
      w=rect.right-rect.left+1;
      h=rect.bottom-rect.top+1;
      x-=w+10;  // nice shift

      // Okay, then create the button
      if (w*h != 0) {
        // button's styles
        int style = WS_CHILD | WS_CLIPSIBLINGS | BS_PUSHBUTTON | WS_TABSTOP | WS_VISIBLE;
        // create button, parent=hwnd
        HWND b = CreateWindow("BUTTON","New folder",style,x,y,w,h,hwnd,NULL,0,NULL);
        LONG Ladr=NULL;
        if (b!=NULL) {
          // Send Font notification so that the font be the same as default font
          HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
          ::SendMessage(b, WM_SETFONT, (WPARAM) hFont, MAKELPARAM(1,0));
          // Enable input
          EnableWindow(b,true);
          // Get the control ID of our button
          XSHBFF_button1 = GetDlgCtrlID(b);
          // Place our function
          Ladr = SetWindowLong(hwnd,GWL_WNDPROC,(LONG) XSHBFF_WndProc);
        }
        
        // initial selection
        LONG* Param = (LONG*) lpData;
        if (Param) {
          // Send init dir
          SendMessage(hwnd,BFFM_SETSELECTION,WPARAM(false),Param[0]);  // init dir
          // Send string return address
          XSHBFF_WndProc(hwnd,XSHBrowseForFolder_SETSTRING,0,Param[1]);             // return string
          // Send former function address
          if (Ladr) {
            XSHBFF_WndProc(hwnd,XSHBrowseForFolder_SETSTRING+1,0,Ladr);             // return string
          }
        }
      }
    }
#endif
  }
  return 0;
}

// Convert a char* into a LPITEMIDLIST
LPITEMIDLIST XSHBFF_PathConvert(HWND hwnd,char* _path) {
  // Retrieves the IShellFolder interface for the desktop folder
  LPSHELLFOLDER MyShellFolder;
  if (SHGetDesktopFolder(&MyShellFolder) != NO_ERROR)
    return NULL;
  
  // Initial path
  ULONG pchEaten;
  LPITEMIDLIST MyItemlist=NULL;
  ULONG pdwAttributes;
  if (strlen(_path)>0) {
    char* lpszA = _path;
    // -- Unicode conversion--
    // we want to convert an MBCS string into lpszA    
    int nLen = MultiByteToWideChar(CP_ACP, 0,lpszA, -1, NULL, NULL);
    LPWSTR lpszW = new WCHAR[nLen];
    MultiByteToWideChar(CP_ACP, 0, lpszA, -1, lpszW, nLen);
    // parse string
    MyShellFolder->ParseDisplayName(hwnd,NULL,lpszW,&pchEaten,&MyItemlist,&pdwAttributes);
    // free the string
    delete[] lpszW;
    // -- Unicode conversion--    
  }
  return MyItemlist;
}

// ----------------------------------------------------------------------
// 'extended' SHBrowseForFolder routine. 'New folder' button added.
// ----------------------------------------------------------------------

