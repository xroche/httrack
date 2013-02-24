#if !defined(AFX_HTMLHELP_H__CA6ABCFD_0D37_4DC2_A214_AD0BB7AFBBD5__INCLUDED_)
#define AFX_HTMLHELP_H__CA6ABCFD_0D37_4DC2_A214_AD0BB7AFBBD5__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// HTMLHelp.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CHTMLHelp html view

#ifndef __AFXEXT_H__
#include <afxext.h>
#endif
#include <afxhtml.h>

class CHTMLHelp : public CHtmlView
{
public:
	CHTMLHelp();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CHTMLHelp)

// html Data
public:
	//{{AFX_DATA(CHTMLHelp)
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CHTMLHelp)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CHTMLHelp();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
	//{{AFX_MSG(CHTMLHelp)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_HTMLHELP_H__CA6ABCFD_0D37_4DC2_A214_AD0BB7AFBBD5__INCLUDED_)
