// Classe de sauvegarde de clés (identifiées par leur nom)
// à la manière de la base de registre (mais en plus basique)

#include "stdafx.h"
#include "MemRegister.h"


void MemRegister::deleteAll() {
  Mem_index.RemoveAll();
  Mem_value.RemoveAll();
  Mem_valueint.RemoveAll();
}
CString MemRegister::getString(CString name,CString defval) {
  int i;
  for(i=0;i<Mem_index.GetUpperBound()+1;i++) {
    if (Mem_index[i] == name)
      return Mem_value[i];
  }
  return defval;
}
int MemRegister::getInt(CString name,int defval) {
  int i;
  for(i=0;i<Mem_index.GetUpperBound()+1;i++) {
    if (Mem_index[i] == name)
      return Mem_valueint[i];
  }
  return defval;
}
bool MemRegister::setString(CString name,CString val) {
  int i;
  for(i=0;i<Mem_index.GetUpperBound()+1;i++) {
    if (Mem_index[i] == name) {
      Mem_value[i]=val;
      return true;
    }
  }
  Mem_index.Add(name);
  Mem_value.SetAtGrow(Mem_index.GetUpperBound(),val);
  return true;
}
bool MemRegister::setInt(CString name,int val) {
  int i;
  for(i=0;i<Mem_index.GetUpperBound()+1;i++) {
    if (Mem_index[i] == name) {
      Mem_valueint[i]=val;
      return true;
    }
  }
  Mem_index.Add(name);
  Mem_valueint.SetAtGrow(Mem_index.GetUpperBound(),val);
  return true;
}
