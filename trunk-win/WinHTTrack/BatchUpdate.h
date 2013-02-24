#if !defined(AFX_BATCHUPDATE_H__27459ED9_CCB3_4BA7_B2E3_442733B41467__INCLUDED_)
#define AFX_BATCHUPDATE_H__27459ED9_CCB3_4BA7_B2E3_442733B41467__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// BatchUpdate.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CBatchUpdate dialog

class CBatchUpdate : public CPropertyPage
{
	DECLARE_DYNCREATE(CBatchUpdate)

// Construction
public:
	CBatchUpdate();
	~CBatchUpdate();

// Dialog Data
	//{{AFX_DATA(CBatchUpdate)
	enum { IDD = IDD_BatchUpdate };
		// NOTE - ClassWizard will add data members here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CBatchUpdate)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CBatchUpdate)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_BATCHUPDATE_H__27459ED9_CCB3_4BA7_B2E3_442733B41467__INCLUDED_)
