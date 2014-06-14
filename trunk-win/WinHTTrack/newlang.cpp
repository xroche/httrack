
// Win includes
#include "stdafx.h"
#include "Shell.h"
#include <comdef.h>

// Hash for lang.h
extern "C" {
  #include "HTTrackInterface.h"
#define HTS_INTERNAL_BYTECODE
  #include "coucal.h"
#undef HTS_INTERNAL_BYTECODE
};
#include "newlang.h"

// test
#ifndef _MBCS
#error "MBCS/DBCS support not found"
#endif
#include <locale.h>


int NewLangStrSz=1024;
coucal NewLangStr=NULL;
int NewLangStrKeysSz=1024;
coucal NewLangStrKeys=NULL;
UINT NewLangCP = CP_THREAD_ACP;
UINT NewLangFileCP = CP_THREAD_ACP;

typedef struct WinLangid {
  int langId;
  const char* name;
} WinLangid;

WinLangid WINDOWS_LANGID[] = {
  { 0x0436, "Afrikaans" },
  { 0x041c, "Albanian" },
  { 0x0401, "Arabic (Saudi Arabia)" },
  { 0x0801, "Arabic (Iraq)" },
  { 0x0c01, "Arabic (Egypt)" },
  { 0x1001, "Arabic (Libya)" },
  { 0x1401, "Arabic (Algeria)" },
  { 0x1801, "Arabic (Morocco)" },
  { 0x1c01, "Arabic (Tunisia)" },
  { 0x2001, "Arabic (Oman)" },
  { 0x2401, "Arabic (Yemen)" },
  { 0x2801, "Arabic (Syria)" },
  { 0x2c01, "Arabic (Jordan)" },
  { 0x3001, "Arabic (Lebanon)" },
  { 0x3401, "Arabic (Kuwait)" },
  { 0x3801, "Arabic (U.A.E.)" },
  { 0x3c01, "Arabic (Bahrain)" },
  { 0x4001, "Arabic (Qatar)" },
  { 0x042b, "Armenian" },
  { 0x042c, "Azeri (Latin)" },
  { 0x082c, "Azeri (Cyrillic)" },
  { 0x042d, "Basque" },
  { 0x0423, "Belarusian" },
  { 0x0445, "Bengali (India)" },
  { 0x141a, "Bosnian (Bosnia and Herzegovina)" },
  { 0x0402, "Bulgarian" },
  { 0x0455, "Burmese" },
  { 0x0403, "Catalan" },
  { 0x0404, "Chinese (Taiwan)" },
  { 0x0804, "Chinese (PRC)" },
  { 0x0c04, "Chinese (Hong Kong SAR, PRC)" },
  { 0x1004, "Chinese (Singapore)" },
  { 0x1404, "Chinese (Macao SAR)" },
  { 0x041a, "Croatian" },
  { 0x101a, "Croatian (Bosnia and Herzegovina)" },
  { 0x0405, "Czech" },
  { 0x0406, "Danish" },
  { 0x0465, "Divehi" },
  { 0x0413, "Dutch (Netherlands)" },
  { 0x0813, "Dutch (Belgium)" },
  { 0x0409, "English (United States)" },
  { 0x0809, "English (United Kingdom)" },
  { 0x0c09, "English (Australian)" },
  { 0x1009, "English (Canadian)" },
  { 0x1409, "English (New Zealand)" },
  { 0x1809, "English (Ireland)" },
  { 0x1c09, "English (South Africa)" },
  { 0x2009, "English (Jamaica)" },
  { 0x2409, "English (Caribbean)" },
  { 0x2809, "English (Belize)" },
  { 0x2c09, "English (Trinidad)" },
  { 0x3009, "English (Zimbabwe)" },
  { 0x3409, "English (Philippines)" },
  { 0x0425, "Estonian" },
  { 0x0438, "Faeroese" },
  { 0x0429, "Farsi" },
  { 0x040b, "Finnish" },
  { 0x040c, "French (Standard)" },
  { 0x080c, "French (Belgian)" },
  { 0x0c0c, "French (Canadian)" },
  { 0x100c, "French (Switzerland)" },
  { 0x140c, "French (Luxembourg)" },
  { 0x180c, "French (Monaco)" },
  { 0x0456, "Galician" },
  { 0x0437, "Georgian" },
  { 0x0407, "German (Standard)" },
  { 0x0807, "German (Switzerland)" },
  { 0x0c07, "German (Austria)" },
  { 0x1007, "German (Luxembourg)" },
  { 0x1407, "German (Liechtenstein)" },
  { 0x0408, "Greek" },
  { 0x0447, "Gujarati" },
  { 0x040d, "Hebrew" },
  { 0x0439, "Hindi" },
  { 0x040e, "Hungarian" },
  { 0x040f, "Icelandic" },
  { 0x0421, "Indonesian" },
  { 0x0434, "isiXhosa/Xhosa (South Africa)" },
  { 0x0435, "isiZulu/Zulu (South Africa)" },
  { 0x0410, "Italian (Standard)" },
  { 0x0810, "Italian (Switzerland)" },
  { 0x0411, "Japanese" },
  { 0x044b, "Kannada" },
  { 0x0457, "Konkani" },
  { 0x0412, "Korean" },
  { 0x0812, "Korean (Johab)" },
  { 0x0440, "Kyrgyz" },
  { 0x0426, "Latvian" },
  { 0x0427, "Lithuanian" },
  { 0x0827, "Lithuanian (Classic)" },
  { 0x042f, "FYRO Macedonian" },
  { 0x043e, "Malay (Malaysian)" },
  { 0x083e, "Malay (Brunei Darussalam)" },
  { 0x044c, "Malayalam (India)" },
  { 0x0481, "Maori (New Zealand)" },
  { 0x043a, "Maltese (Malta)" },
  { 0x044e, "Marathi" },
  { 0x0450, "Mongolian" },
  { 0x0414, "Norwegian (Bokmal)" },
  { 0x0814, "Norwegian (Nynorsk)" },
  { 0x0415, "Polish" },
  { 0x0416, "Portuguese (Brazil)" },
  { 0x0816, "Portuguese (Portugal)" },
  { 0x0446, "Punjabi" },
  { 0x046b, "Quechua (Bolivia)" },
  { 0x086b, "Quechua (Ecuador)" },
  { 0x0c6b, "Quechua (Peru)" },
  { 0x0418, "Romanian" },
  { 0x0419, "Russian" },
  { 0x044f, "Sanskrit" },
  { 0x043b, "Sami, Northern (Norway)" },
  { 0x083b, "Sami, Northern (Sweden)" },
  { 0x0c3b, "Sami, Northern (Finland)" },
  { 0x103b, "Sami, Lule (Norway)" },
  { 0x143b, "Sami, Lule (Sweden)" },
  { 0x183b, "Sami, Southern (Norway)" },
  { 0x1c3b, "Sami, Southern (Sweden)" },
  { 0x203b, "Sami, Skolt (Finland)" },
  { 0x243b, "Sami, Inari (Finland)" },
  { 0x0c1a, "Serbian (Cyrillic)" },
  { 0x1c1a, "Serbian (Cyrillic, Bosnia, and Herzegovina)" },
  { 0x081a, "Serbian (Latin)" },
  { 0x181a, "Serbian (Latin, Bosnia, and Herzegovina)" },
  { 0x046c, "Sesotho sa Leboa/Northern Sotho (South Africa)" },
  { 0x0432, "Setswana/Tswana (South Africa)" },
  { 0x041b, "Slovak" },
  { 0x0424, "Slovenian" },
  { 0x040a, "Spanish (Spain, Traditional Sort)" },
  { 0x080a, "Spanish (Mexican)" },
  { 0x0c0a, "Spanish (Spain, Modern Sort)" },
  { 0x100a, "Spanish (Guatemala)" },
  { 0x140a, "Spanish (Costa Rica)" },
  { 0x180a, "Spanish (Panama)" },
  { 0x1c0a, "Spanish (Dominican Republic)" },
  { 0x200a, "Spanish (Venezuela)" },
  { 0x240a, "Spanish (Colombia)" },
  { 0x280a, "Spanish (Peru)" },
  { 0x2c0a, "Spanish (Argentina)" },
  { 0x300a, "Spanish (Ecuador)" },
  { 0x340a, "Spanish (Chile)" },
  { 0x380a, "Spanish (Uruguay)" },
  { 0x3c0a, "Spanish (Paraguay)" },
  { 0x400a, "Spanish (Bolivia)" },
  { 0x440a, "Spanish (El Salvador)" },
  { 0x480a, "Spanish (Honduras)" },
  { 0x4c0a, "Spanish (Nicaragua)" },
  { 0x500a, "Spanish (Puerto Rico)" },
  { 0x0430, "Sutu" },
  { 0x0441, "Swahili (Kenya)" },
  { 0x041d, "Swedish" },
  { 0x081d, "Swedish (Finland)" },
  { 0x045a, "Syriac" },
  { 0x0449, "Tamil" },
  { 0x0444, "Tatar (Tatarstan)" },
  { 0x044a, "Telugu" },
  { 0x041e, "Thai" },
  { 0x041f, "Turkish" },
  { 0x0422, "Ukrainian" },
  { 0x0420, "Urdu (Pakistan)" },
  { 0x0820, "Urdu (India)" },
  { 0x0443, "Uzbek (Latin)" },
  { 0x0843, "Uzbek (Cyrillic)" },
  { 0x042a, "Vietnamese" },
  { 0x0452, "Welsh (United Kingdom)" },
  { 0, NULL }
};

void LANG_LOAD(char* limit_to) {
  CWaitCursor wait;
  //
  extern int NewLangStrSz;
  extern coucal NewLangStr;
  extern int NewLangStrKeysSz;
  extern coucal NewLangStrKeys;
  //
  int selected_lang=LANG_T(-1);
  //
  if (!limit_to) {
    LANG_DELETE();
    NewLangStr=coucal_new(NewLangStrSz);
    NewLangStrKeys=coucal_new(NewLangStrKeysSz);
    if ((NewLangStr==NULL) || (NewLangStrKeys==NULL)) {
      AfxMessageBox("Error in lang.h: not enough memory");
    } else {
      coucal_value_is_malloc(NewLangStr,1);
      coucal_value_is_malloc(NewLangStrKeys,1);
    }
  }

  TCHAR ModulePath[MAX_PATH + 1];
  ModulePath[0] = '\0';
  ::GetModuleFileName(NULL, ModulePath, sizeof(ModulePath)/sizeof(TCHAR) - 1);
  TCHAR* pos = _tcsrchr(ModulePath, '\\');
  if (pos != NULL)
  {
    * ( pos + 1) = '\0';
  } else {
    ModulePath[0] = '\0';
  }

  /* Load master file (list of keys and internal keys) */
  CString app = ModulePath;
  if (!limit_to) {
    CString mname=app+"lang.def";
    if (!fexist((char*)LPCTSTR(mname)))
      mname="lang.def";
    FILE* fp=fopen(mname,"rb");
    if (fp) {
      char intkey[8192];
      char key[8192];
      while(!feof(fp)) {
        linput_cpp(fp,intkey,8000);
        linput_cpp(fp,key,8000);
        if (strnotempty(intkey) && strnotempty(key)) {
          char* test=LANGINTKEY(key);

          /* Increment for multiple definitions */
          if (strnotempty(test)) {
            int increment=0;
            size_t pos = strlen(key);
            do {
              increment++;
              sprintf(key+pos,"%d",increment);
              test=LANGINTKEY(key);
            }  while (strnotempty(test));
          }

          if (!strnotempty(test)) {         // éviter doublons
            // conv_printf(key,key);
            size_t len;
            char* buff;
            len=strlen(intkey);
            buff=(char*)malloc(len+2);
            if (buff) {
              strcpybuff(buff,intkey);
              coucal_add(NewLangStrKeys,key,(intptr_t)buff);
            }
          }
        } // if
      }  // while
      fclose(fp);
    } else {
      AfxMessageBox("FATAL ERROR\r\n'lang.def' file NOT FOUND!\r\nEnsure that the installation was complete!");
      exit(0);
    }
  }
  
  /* Language Name? */
  char* hashname;
  {
    char name[256];
    sprintf(name,"LANGUAGE_%d",selected_lang+1);
    hashname=LANGINTKEY(name);
  }

  /* Get only language name */
  if (limit_to) {
    if (hashname)
      strcpybuff(limit_to,hashname);
    else
      strcpybuff(limit_to,"???");
    return;
  }

  /* Error */
  if (!hashname)
    return;

  // xxc TEST
  /*
  setlocale( LC_ALL, "Japanese");
  _setmbcp(932);    // shift-jis
  setlocale( LC_ALL, ".932" );
  setlocale( LC_ALL, "[.932]" );
  CString st="";
  int lid=SetThreadLocale(MAKELCID(MAKELANGID(LANG_JAPANESE,SUBLANG_NEUTRAL),SORT_DEFAULT ));
  */

  /* Load specific language file */
  {
    int loops;
    CString err_msg="";
    // 2nd loop: load undefined strings
    for(loops=0;loops<2;loops++) {
      CString lbasename;
      
      {
        char name[256];
        sprintf(name,"LANGUAGE_%d",(loops==0)?(selected_lang+1):1);
        hashname=LANGINTKEY(name);
      }
      lbasename.Format("lang/%s.txt",hashname);
      CString lname=app+lbasename;
      if (!fexist((char*)LPCTSTR(lname)))
        lname=lbasename;
      FILE* fp=fopen(lname,"rb");
      if (fp) {
        char extkey[8192];
        TCHAR value[8192];
        while(!feof(fp)) {
          //int ssz;
          linput_cpp(fp,extkey,8000);
          linput_cpp(fp,value,8000);
          /*
          ssz=linput_cpp(fp,value,8000);
          CString st=value;
          AfxMessageBox(st);
          if (ssz>0) {
            int tst=0;
            int test=IsTextUnicode(value,ssz,&tst);
            unsigned short st2[1024];
            int ret=MultiByteToWideChar(CP_UTF8,0,(char*)value,ssz,st2,1024);
            if (ret>0) {
              char st3[1024]="";
              int ret2=WideCharToMultiByte(CP_THREAD_ACP,0,st2,ret,(char*)st3,1024,NULL,FALSE);
              if (ret2>0) {
                AfxMessageBox(st3);
              }
            }
          }
          */

          if (strnotempty(extkey) && strnotempty(value)) {
            int len;
            char* buff;
            char* intkey;
            
            intkey=LANGINTKEY(extkey);
            
            if (strnotempty(intkey)) {
              
              /* Increment for multiple definitions */
              {
                char* test=LANGSEL(intkey);
                if (strnotempty(test)) {
                  if (loops == 0) {
                    int increment=0;
                    size_t pos=strlen(extkey);
                    do {
                      increment++;
                      sprintf(extkey+pos,"%d",increment);
                      intkey=LANGINTKEY(extkey);
                      if (strnotempty(intkey))
                        test=LANGSEL(intkey);
                      else
                        test="";
                    }  while (strnotempty(test));
                  } else
                    intkey="";
                } else {
                  if (loops > 0) {
                    err_msg += intkey;
                    err_msg += " ";
                  }
                }
              }
              
              /* Add key */
              if (strnotempty(intkey)) {
                len = (int) strlen(value);
                buff = (char*)malloc(len+2);
                if (buff) {
                  conv_printf(value,buff);
                  coucal_add(NewLangStr,intkey,(intptr_t)buff);
                }
              }
              
            }
          } // if
        }  // while
        fclose(fp);
      } else {
        AfxMessageBox("FATAL ERROR\r\n'lang.def' file NOT FOUND!\r\nEnsure that the installation was complete!");
        exit(0);
      }
    }
    if (err_msg.GetLength()>0) {
      // AfxMessageBox("Error: undefined strings follows:\r\n"+err_msg);
    }
  }



#if 0
  app=app+"lang.h";
  if (!fexist((char*)LPCTSTR(app)))
    app="lang.h";
  
  FILE* fp=fopen(app,"rb");
  if (fp) {
    char s[8192];
    while(!feof(fp)) {
      linput_cpp(fp,s,8000);
      if (!strncmp(s,"#define ",8)) {
        char* a;
        char* name=s+8;
        a=name;
        while((*a!=' ') && (*a)) a++;
        if ((*a) && (strlen(name)>0) && (((int) a - (int) name)<64)) {
          *a++='\0';
          if (limit_to) {
            if (strcmp(name,limit_to))
              a=NULL;
          }
          if (a) {
            char* data;
            data=a;
            int toggle=0;
            char* start_str=NULL;
            int count=0;
            while(*a) {
              if (*a=='\"') {
                toggle++;
                if ((toggle%2)==1) {
                  if (count==selected_lang) {
                    start_str=a+1;
                  }
                  count++;
                } else {
                  if (start_str) {
                    char* buff;
                    int len;
                    len=(int) a - (int) start_str;
                    if (len) {
                      buff=(char*)malloc(len+2);
                      if (buff) {
                        int i=0,j=0;
                        buff[0]='\0';
                        //strncatbuff(buff,start_str,len);
                        while(i<len) {
                          switch(start_str[i]) {
                          case '\\': 
                            i++;
                            switch(start_str[i]) {
                            case 'a': buff[j]='\a'; break;
                            case 'b': buff[j]='\b'; break;
                            case 'f': buff[j]='\f'; break;
                            case 'n': buff[j]='\n'; break;
                            case 'r': buff[j]='\r'; break;
                            case 't': buff[j]='\t'; break;
                            case 'v': buff[j]='\v'; break;
                            case '\'': buff[j]='\''; break;
                            case '\"': buff[j]='\"'; break;
                            case '\\': buff[j]='\\'; break;
                            case '?': buff[j]='\?'; break;
                            default: buff[j]=start_str[i]; break;
                            }
                            break;
                            default: 
                              buff[j]=start_str[i]; 
                              break;
                          }
                          i++;
                          j++;
                        }
                        buff[j++]='\0';
                        if (!limit_to)
                          coucal_add(NewLangStr,name,(intptr_t)buff);
                        else {
                          strcpybuff(limit_to,buff);
                          free(buff);
                          return;
                        }
                      }
                    }
                    start_str=NULL;
                  }
                }
              }
              a++;
            }
          }
          
          //NewLangStr.SetAt(sname,st);
          /*
          } else {
          CString info;
          info.Format("Error in lang.h: %s",name);
          AfxMessageBox(info);
        */
        }
      }
    }


    fclose(fp);

  } else {
    AfxMessageBox("FATAL ERROR\r\n'lang.h' file NOT FOUND!\r\nEnsure that the installation was complete!");
    exit(0);
  }
#endif

  // Control limit_to
  if (limit_to)
    limit_to[0]='\0';

  // Set locale
  if (!limit_to) {
    CString charset = LANGUAGE_CHARSET;
    charset.TrimLeft();
    charset.TrimRight();
    charset.MakeLower();
    NewLangCP = CP_THREAD_ACP;
    NewLangFileCP = CP_THREAD_ACP;
#if 0
    if (charset.GetLength() > 0) {
      if (charset.Left(9) == "iso-8859-") {
        int iso = 0;
        int isoCP[] = {0, /* 0 */
          1252, /* ISO-8859-1 */
          1250, /* ISO-8859-2 */
          0, /* ISO-8859-3 */
          0, /* ISO-8859-4 */
          1251, /* ISO-8859-5 */
          1256, /* ISO-8859-6 */
          1253, /* ISO-8859-7 */
          1255, /* ISO-8859-8 */
          1254, /* ISO-8859-9 */
        };
        if (sscanf(charset.GetBuffer(0) + 9, "%d", &iso) == 1) {
          if (iso < sizeof(isoCP)/sizeof(isoCP[0])) {
            if (isoCP[iso] != 0) {
              NewLangFileCP = isoCP[iso];
            }
          }
        }
      } else if (charset.Left(8) == "windows-") {
        int windows = 0;
        if (sscanf(charset.GetBuffer(0) + 8, "%d", &windows) == 1) {
          NewLangFileCP = windows;
        }
      } else if (charset == "shift-jis") {
        NewLangFileCP = 932;
      } else if (charset == "big5") {
        NewLangFileCP = 950;
      } else if (charset == "gb2312") {
        NewLangFileCP = 936;
      } else {
        NewLangFileCP = CP_THREAD_ACP;
      }
    }
    WORD acp = GetACP();
    if (NewLangFileCP != CP_THREAD_ACP && NewLangFileCP != acp) {
      char* currName = LANGUAGE_WINDOWSID;
      LCID thl = GetThreadLocale();
      WORD sid = SORTIDFROMLCID(thl);
      WORD lid = 0;
      WinLangid* lids;
      if (currName[0]) {
        for( lids = (WinLangid*)&WINDOWS_LANGID ; lids->name != NULL ; lids++ ) {
          if (strcmp(currName, lids->name) == 0) {
            lid = lids->langId;
            break;
          }
        }
        if (lid != 0) {
          SetThreadLocale(MAKELCID(lid, sid));
        }
      }
    }
#endif

  }

}

void conv_printf(char* from,char* to) {
  int i=0,j=0,len;
  len = (int) strlen(from);
  while(i<len) {
    switch(from[i]) {
    case '\\': 
      i++;
      switch(from[i]) {
      case 'a': to[j]='\a'; break;
      case 'b': to[j]='\b'; break;
      case 'f': to[j]='\f'; break;
      case 'n': to[j]='\n'; break;
      case 'r': to[j]='\r'; break;
      case 't': to[j]='\t'; break;
      case 'v': to[j]='\v'; break;
      case '\'': to[j]='\''; break;
      case '\"': to[j]='\"'; break;
      case '\\': to[j]='\\'; break;
      case '?': to[j]='\?'; break;
      default: to[j]=from[i]; break;
      }
      break;
      default: 
        to[j]=from[i]; 
        break;
    }
    i++;
    j++;
  }
  to[j++]='\0';
}

void LANG_DELETE() {
  extern int NewLangStrSz;
  extern coucal NewLangStr;
  extern int NewLangStrKeysSz;
  extern coucal NewLangStrKeys;
  //
  coucal_delete(&NewLangStr);
  coucal_delete(&NewLangStrKeys);
}

// sélection de la langue
void LANG_INIT() {
  CWinApp* pApp = AfxGetApp();
  if (pApp) {
    int test = pApp->GetProfileInt("Language","IntId",0);
    LANG_T(pApp->GetProfileInt("Language","IntId",0));
  }
}

int LANG_T(int l) {
  if (l>=0) {
    QLANG_T(l);
    CWinApp* pApp = AfxGetApp();
    if (pApp)
      pApp->WriteProfileInt("Language","IntId",l);
    LANG_LOAD(NULL);
  }
  return QLANG_T(-1);  // 0=default (english)
}

int QLANG_T(int l) {
  static int lng=0;
  if (l>=0) {
    lng=l;
  }
  return lng;  // 0=default (english)
}


/*
char* LANGSEL(char* lang0,...) {
  char* lang=lang0;
  char* langalt="";
  int langid=LANG_T(-1);
  //
  va_list argList;
	va_start(argList, lang0);
  while(langid>0) {
    if (lang) {
      if (strlen(langalt)==0) {
        if (strlen(lang)>0)
          langalt=lang;
      }
    }
    langid--;
    lang=va_arg(argList, char*);
  }
  va_end(argList);
  //
  if (!lang)
    return langalt;
  if (strlen(lang)==0)
    return langalt;
  return lang;
}
*/

char* LANGSEL(char* name) {
  intptr_t adr = 0;
  if (NewLangStr)
  if (!coucal_read(NewLangStr,name,&adr))
    adr = NULL;
  if (adr) {
    return (char*)adr;
  }
  return "";
}

char* LANGINTKEY(char* name) {
  intptr_t adr = 0;
  if (NewLangStrKeys)
  if (!coucal_read(NewLangStrKeys,name,&adr))
    adr=NULL;
  if (adr) {
    return (char*)adr;
  }
  return "";
}

static _bstr_t ConvertCodepage(LPCSTR str, UINT codePage)
{
  _bstr_t returnValue;
  BOOL ok = TRUE;
  int mbLength = (int) strlen(str);
  int wideLength = ::MultiByteToWideChar( codePage, 0, str, mbLength, NULL, NULL); 
  if (wideLength > 0) 
  {
    wchar_t *wcharBuffer = new wchar_t[wideLength+1]; 
    ::MultiByteToWideChar( codePage, 0, str, mbLength, wcharBuffer, wideLength); 
    wcharBuffer[wideLength] = '\0';
    returnValue = wcharBuffer;
    delete wcharBuffer;
  }
  return returnValue;
}

BOOL SetDlgItemTextCP(HWND hDlg, int nIDDlgItem, LPCSTR lpString) {
  if (NewLangCP != CP_THREAD_ACP)
    return SetDlgItemTextW(hDlg, nIDDlgItem, ConvertCodepage(lpString, NewLangCP));
  else
    return SetDlgItemTextA(hDlg, nIDDlgItem, lpString);
}

BOOL SetDlgItemTextUTF8(HWND hDlg, int nIDDlgItem, LPCSTR lpString) {
  _bstr_t s = ConvertCodepage(lpString, CP_UTF8);
  if (s.length() != 0)
    return SetDlgItemTextW(hDlg, nIDDlgItem, s);
  else
    return SetDlgItemTextA(hDlg, nIDDlgItem, lpString);
}

BOOL SetDlgItemTextCP(CWnd* wnd, int nIDDlgItem, LPCSTR lpString) {
  return SetDlgItemTextCP(wnd->m_hWnd, nIDDlgItem, lpString);
}

BOOL SetDlgItemTextUTF8(CWnd* wnd, int nIDDlgItem, LPCSTR lpString) {
  return SetDlgItemTextUTF8(wnd->m_hWnd, nIDDlgItem, lpString);
}

BOOL SetWindowTextCP(HWND hWnd, LPCSTR lpString) {
  if (NewLangCP != CP_THREAD_ACP)
    return SetWindowTextW(hWnd, ConvertCodepage(lpString, NewLangCP));
  else
    return SetWindowTextA(hWnd, lpString);
}

BOOL SetWindowTextUTF8(HWND hWnd, LPCSTR lpString) {
  _bstr_t s = ConvertCodepage(lpString, CP_UTF8);
  if (s.length() != 0)
    return SetWindowTextW(hWnd, s);
  else
    return SetWindowTextA(hWnd, lpString);
}

BOOL SetWindowTextCP(CWnd* wnd, LPCSTR lpString) {
  return SetWindowTextCP(wnd->m_hWnd, lpString);
}

BOOL SetWindowTextUTF8(CWnd* wnd, LPCSTR lpString) {
  return SetWindowTextUTF8(wnd->m_hWnd, lpString);
}

BOOL ModifyMenuCP(HMENU hMnu, UINT uPosition, UINT uFlags, UINT uIDNewItem, LPCSTR lpNewItem) {
  if (NewLangCP != CP_THREAD_ACP)
    return ModifyMenuW(hMnu, uPosition, uFlags, uIDNewItem, ConvertCodepage(lpNewItem, NewLangCP));
  else
    return ModifyMenuA(hMnu, uPosition, uFlags, uIDNewItem, lpNewItem);
}

BOOL ModifyMenuCP(CMenu* menu, UINT uPosition, UINT uFlags, UINT uIDNewItem, LPCSTR lpNewItem) {
  return ModifyMenuCP(menu->m_hMenu, uPosition, uFlags, uIDNewItem, lpNewItem);
}
