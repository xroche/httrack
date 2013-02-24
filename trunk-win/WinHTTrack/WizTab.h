#if !defined(__WizTab_H_HSGFYZEGCHXHVCHD52485454857545)
#define __WizTab_H_HSGFYZEGCHXHVCHD52485454857545

// Tab Control Principal

// En-tête pour l'affichage des tabs
#include "NewProj.h"
#include "Wid1.h"
#include "trans.h"
#include "FirstInfo.h"
#include "inprogress.h"
#include "infoend.h"

class CWizTab : public CPropertySheet
{
  DECLARE_DYNAMIC(CWizTab)
  
private:
  int is_inProgress;

  // Construction
public:
  CWizTab(UINT nIDCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);
  CWizTab(LPCTSTR pszCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);
  CWizTab(LPCTSTR pszCaption, int num, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);
  
protected:
  void AddControlPages(void);
  
  // Attributes
public:
  // Déclaration des classes-dialog pour les différents Tab Control
  CFirstInfo*        m_tab0;
  CNewProj*          m_tab1;
  Wid1*              m_tab2;
  Ctrans*            m_tab3;
  Cinprogress*       m_tabprogress;
  Cinfoend*          m_tabend;
  // Operations
public:
	int prec;
	int next;
	int cancel;
  
  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CWizTab)
	//}}AFX_VIRTUAL
  
  // Implementation
public:
  virtual ~CWizTab();
  virtual BOOL OnInitDialog();
  void ApplyAndSave();
  void Apply();
  void LoadPrefs();
  HICON m_hIcon;
  void FinalInProgress();
  void EndInProgress();
  
  // Generated message map functions
protected:
  void DoInits();
  void ClearInits();
  
  char* GetTip(int id);
  //{{AFX_MSG(CWizTab)
	afx_msg BOOL OnHelpInfo(HELPINFO* dummy);
	//}}AFX_MSG
	afx_msg void OnHelpInfo2();
  //afx_msg void OnOK( );
  //afx_msg void OnCancel( );
  afx_msg void OnApplyNow();
  // afx_msg LONG IconRestore(UINT a, LONG b); // LONG msg, LONG b,CPoint point) {
  afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
  //afx_msg LONG IconRestore(UINT a,LONG b);
  DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

#endif

