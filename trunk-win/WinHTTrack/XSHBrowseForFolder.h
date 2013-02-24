// ----------------------------------------------------------------------
// 'extended' SHBrowseForFolder routine. ('New folder' button added)
// Written by Xavier Roche, with the help of Gil Rosin, 
// Todd Fast's routines from Pencilneck Software and other Usenet contributors.
// 
// Usage: (example)
// CString path = XSHBrowseForFolder(this->m_hWnd,"Select path","c:\\") {
//
// Freeware, but no warranty!
//
// #include "NewFolder.h"             for Input dialog (new folder)
// #include <direct.h>                for _mkdir
// ----------------------------------------------------------------------

#if !defined(__XSHBrowseForFolder_routines)
#define __XSHBrowseForFolder_routines 

// TODO: Put here your ressource definition
#include "resource.h"
#include "NewFolder.h"

#include <direct.h>
#include "shlobj.h"

#define XSHBrowseForFolder_SETSTRING 1234
#define XSHBrowseForFolder_OK 1

CString        XSHBrowseForFolder (HWND hwnd,char* title,char* _path);
LRESULT __stdcall XSHBFF_WndProc     (HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam);
int __stdcall  XSHBFF_CallbackProc(HWND hwnd,UINT uMsg,LPARAM lParam,LPARAM lpData);
LPITEMIDLIST   XSHBFF_PathConvert (HWND hwnd,char* _path);

#endif

