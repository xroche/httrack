// Classe de sauvegarde de clés (identifiées par leur nom)
// à la manière de la base de registre (mais en plus basique)

#if !defined(MEMREGISTER_LIB_JHGFHIV25489654156HJRZDSCIOUJ5648654651)
#define MEMREGISTER_LIB_JHGFHIV25489654156HJRZDSCIOUJ5648654651

class MemRegister
{
private:
  CStringArray Mem_index;
  CStringArray Mem_value;
  CWordArray Mem_valueint;
public:
  void deleteAll();
  CString getString(CString name,CString defval);
  int getInt(CString name,int defval);
  bool setString(CString name,CString val);
  bool setInt(CString name,int val);
};


#endif

