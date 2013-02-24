// projectInfo.h : Declaration of the CprojectInfo

#pragma once

#include "stdafx.h"

#include "resource.h"       // main symbols
#include <atlhost.h>


// CprojectInfo

class CprojectInfo : 
	public CAxDialogImpl<CprojectInfo>
{
private:
	_bstr_t* title;
public:
	CprojectInfo(_bstr_t* title)
	{
		this->title = title;
	}

	~CprojectInfo()
	{
	}

	enum { IDD = IDD_PROJECTINFO };

BEGIN_MSG_MAP(CprojectInfo)
	MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
	COMMAND_HANDLER(IDOK, BN_CLICKED, OnClickedOK)
	COMMAND_HANDLER(IDCANCEL, BN_CLICKED, OnClickedCancel)
	CHAIN_MSG_MAP(CAxDialogImpl<CprojectInfo>)
END_MSG_MAP()

// Handler prototypes:
//  LRESULT MessageHandler(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
//  LRESULT CommandHandler(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
//  LRESULT NotifyHandler(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);

	LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		CAxDialogImpl<CprojectInfo>::OnInitDialog(uMsg, wParam, lParam, bHandled);
		SetDlgItemText(IDC_EDIT1, *title);
		return 1;  // Let the system set the focus
	}

	LRESULT OnClickedOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
	{
		GetDlgItemText(IDC_EDIT1, (*title).GetBSTR());
		EndDialog(wID);
		return 0;
	}

	LRESULT OnClickedCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
	{
		EndDialog(wID);
		return 0;
	}
};


