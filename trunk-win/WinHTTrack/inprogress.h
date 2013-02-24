#if !defined(AFX_INPROGRESS_H__BAF427E1_1910_11D2_A290_502047C1F032__INCLUDED_)
#define AFX_INPROGRESS_H__BAF427E1_1910_11D2_A290_502047C1F032__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// inprogress.h : header file
//

#include "Shell.h"
#include "iplog.h"
#include "EasyDropTarget.h"


/////////////////////////////////////////////////////////////////////////////
// Cinprogress dialog

class Cinprogress : public CPropertyPage
{
private:
  CEasyDropTarget* drag;
// Construction
public:
	Cinprogress();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(Cinprogress)
public:
  void StatsBuffer_cancel(int id);
  void StatsBuffer_info(int id);
  void StopTimer();

  CWnd* element[5][NStatsBuffer];    // ici 10=NStatsBuffer -- les éléments (status nom slide bouton)
	//Cinprogress(CWnd* pParent = NULL);   // standard constructor
  CWinThread * BackAffLog;
  Ciplog form;
  char pathlog[256];

// Dialog Data
	//{{AFX_DATA(Cinprogress)
	enum { IDD = IDD_inprogress };
	CStatic	m_nn9;
	CStatic	m_nn8;
	CStatic	m_nn7;
	CStatic	m_nn6;
	CStatic	m_nn5;
	CStatic	m_nn4;
	CStatic	m_nn3;
	CStatic	m_nn2;
	CStatic	m_nn13;
	CStatic	m_nn12;
	CStatic	m_nn11;
	CStatic	m_nn10;
	CStatic	m_nn1;
	CStatic	m_nn0;
	CEdit	m_nm13;
	CEdit	m_nm12;
	CEdit	m_nm11;
	CEdit	m_nm10;
	CEdit	m_nm9;
	CEdit	m_nm8;
	CEdit	m_nm7;
	CEdit	m_nm6;
	CEdit	m_nm5;
	CEdit	m_nm4;
	CEdit	m_nm3;
	CEdit	m_nm2;
	CEdit	m_nm1;
	CEdit	m_nm0;
	CEdit	m_st13;
	CEdit	m_st12;
	CEdit	m_st11;
	CEdit	m_st10;
	CEdit	m_st9;
	CEdit	m_st8;
	CEdit	m_st7;
	CEdit	m_st6;
	CEdit	m_st5;
	CEdit	m_st4;
	CEdit	m_st3;
	CEdit	m_st2;
	CEdit	m_st1;
	CEdit	m_st0;
	CButton	m_sk0;
	CButton	m_sk1;
	CButton	m_sk2;
	CButton	m_sk3;
	CButton	m_sk4;
	CButton	m_sk5;
	CButton	m_sk6;
	CButton	m_sk7;
	CButton	m_sk8;
	CButton	m_sk9;
	CButton	m_sk10;
	CButton	m_sk11;
	CButton	m_sk12;
	CButton	m_sk13;
	CProgressCtrl	m_sl0;
	CProgressCtrl	m_sl1;
	CProgressCtrl	m_sl2;
	CProgressCtrl	m_sl3;
	CProgressCtrl	m_sl4;
	CProgressCtrl	m_sl5;
	CProgressCtrl	m_sl6;
	CProgressCtrl	m_sl7;
	CProgressCtrl	m_sl8;
	CProgressCtrl	m_sl9;
	CProgressCtrl	m_sl10;
	CProgressCtrl	m_sl11;
	CProgressCtrl	m_sl12;
	CProgressCtrl	m_sl13;
	BOOL	m_inphide;
	//}}AFX_DATA

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(Cinprogress)
	public:
	virtual BOOL DestroyWindow();
	virtual BOOL OnSetActive();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
  char* GetTip(int id);
  void OnHelpInfo2();
  UINT_PTR timer;
  void StartTimer();
  void OnStopall();

public:
	// Generated message map functions
	//{{AFX_MSG(Cinprogress)
	afx_msg void OnClose();
	afx_msg void Onsk0();
	afx_msg void Onsk1();
	afx_msg void Onsk2();
	afx_msg void Onsk3();
	afx_msg void Onsk4();
	afx_msg void Onsk5();
	afx_msg void Onsk6();
	afx_msg void Onsk7();
	afx_msg void Onsk8();
	afx_msg void Onsk9();
	afx_msg void OnEscape();
	afx_msg void Onipabout();
	afx_msg void OnDestroy();
	virtual BOOL OnInitDialog();
	afx_msg BOOL OnHelpInfo(HELPINFO* dummy);
	afx_msg void Onsk10();
	afx_msg void Onsk11();
	afx_msg void Onsk12();
	afx_msg void Onsk13();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void Onnm0();
	afx_msg void Onnm1();
	afx_msg void Onnm2();
	afx_msg void Onnm3();
	afx_msg void Onnm4();
	afx_msg void Onnm5();
	afx_msg void Onnm6();
	afx_msg void Onnm7();
	afx_msg void Onnm8();
	afx_msg void Onnm9();
	afx_msg void Onnm10();
	afx_msg void Onnm11();
	afx_msg void Onnm12();
	afx_msg void Onnm13();
	afx_msg void Onst0();
	afx_msg void Onst1();
	afx_msg void Onst2();
	afx_msg void Onst3();
	afx_msg void Onst4();
	afx_msg void Onst5();
	afx_msg void Onst6();
	afx_msg void Onst7();
	afx_msg void Onst8();
	afx_msg void Onst9();
	afx_msg void Onst10();
	afx_msg void Onst11();
	afx_msg void Onst12();
	afx_msg void Onst13();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void Oninphide();
	//}}AFX_MSG
  afx_msg LRESULT DragDropText(WPARAM wParam,LPARAM lParam);
  afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
  //afx_msg LONG IconRestore(HWND,UINT,WPARAM,LPARAM);
  afx_msg void OnPause();
  afx_msg void OnViewTransfers();
  afx_msg void OnModifyOpt();
  LRESULT OnEndMirror(WPARAM,LPARAM);
	DECLARE_MESSAGE_MAP()
  BOOL OnQueryCancel( );

  public:
	afx_msg void Oniplog(int);
	afx_msg void OniplogLog();
	afx_msg void OniplogErr();
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_INPROGRESS_H__BAF427E1_1910_11D2_A290_502047C1F032__INCLUDED_)
