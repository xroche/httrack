/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) Xavier Roche and other contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.


Important notes:

- We hereby ask people using this source NOT to use it in purpose of grabbing
emails addresses, or collecting any other private information on persons.
This would disgrace our work, and spoil the many hours we spent on it.


Please visit our Website: http://www.httrack.com
*/


/* ------------------------------------------------------------ */
/* File: Main source                                            */
/* DIRECT INCLUDE TO httrack.c                                  */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */


#if HTS_ANALYSTE
if (hts_htmlcheck(r.adr,(int)r.size,urladr,urlfil)) {
#endif          
  FILE* fp=NULL;      // fichier écrit localement 
  char* adr=r.adr;    // pointeur (on parcourt)
  char* lastsaved;    // adresse du dernier octet sauvé + 1
  if ( (opt.debug>1) && (opt.log!=NULL) ) {
    fspc(opt.log,"debug"); fprintf(opt.log,"scan file.."LF); test_flush;
  }


  // Indexing!
#if HTS_MAKE_KEYWORD_INDEX
  if (opt.kindex) {
    if (index_keyword(r.adr,r.size,r.contenttype,savename,opt.path_html)) {
      if ( (opt.debug>1) && (opt.log!=NULL) ) {
        fspc(opt.log,"debug"); fprintf(opt.log,"indexing file..done"LF); test_flush;
      }
    } else {
      if ( (opt.debug>1) && (opt.log!=NULL) ) {
        fspc(opt.log,"debug"); fprintf(opt.log,"indexing file..error!"LF); test_flush;
      }
    }
  }
#endif

  // Now, parsing
  if ((opt.getmode & 1) && (ptr>0)) {  // récupérer les html sur disque       
    // créer le fichier html local
    HT_ADD_FOP;   // écrire peu à peu le fichier
  }
  
  if (!error) {
    int detect_title=0;  // détection  du title
    //
    char* in_media=NULL; // in other media type (real media and so..)
    int intag=0;         // on est dans un tag
    int incomment=0;     // dans un <!--
    int inscript=0;      // dans un scipt pour applets javascript)
    int inscript_tag=0;  // on est dans un <body onLoad="... terminé par >
    char inscript_tag_lastc='\0';  
                           // terminaison (" ou ') du "<body onLoad=.."
    int inscriptgen=0;     // on est dans un code générant, ex après obj.write("..
    char scriptgen_q='\0'; // caractère faisant office de guillemet (' ou ")
    int no_esc_utf=0;      // ne pas echapper chars > 127
    int nofollow=0;        // ne pas scanner
    //
    int parseall_lastc='\0';    // dernier caractère parsé pour parseall
    int parseall_incomment=0;   // dans un /* */ (exemple: a = /* URL */ "img.gif";)
    //
    char* intag_start=adr;
    char* intag_startattr=NULL;
    int intag_start_valid=0;
    HT_ADD_START;    // débuter


    /* statistics */
    if ((opt.getmode & 1) && (ptr>0)) { 
      /*
      HTS_STAT.stat_files++;
      HTS_STAT.stat_bytes+=r.size;
      */
    }

    /* Primary list or URLs */
    if (ptr == 0) {
      intag=1;
      intag_start_valid=0;
    }
    /* Check is the file is a .js file */
    else if (
      (strfield2(r.contenttype,"application/x-javascript")!=0)
      || (strfield2(r.contenttype,"text/css")!=0)
      ) {      /* JavaScript js file */
      inscript=1;
      intag=1;     // because après <script> on y est .. - pas utile
      intag_start_valid=0;    // OUI car nous sommes dans du code, plus dans du "vrai" tag
      if ((opt.debug>1) && (opt.log!=NULL)) {
        fspc(opt.log,"debug"); fprintf(opt.log,"note: this file is a javascript file"LF); test_flush;
      }
    }
    /* Or a real audio */
    else if (strfield2(r.contenttype,"audio/x-pn-realaudio")!=0) {      /* realaudio link file */
      inscript=intag=1;
      intag_start_valid=0;
      in_media="RAM";       // real media!
    }
    // Detect UTF8 format
    if (is_unicode_utf8((unsigned char*) r.adr, (unsigned int) r.size) == 1) {
      no_esc_utf=1;
    } else {
      no_esc_utf=0;
    }
    // Hack to prevent any problems with ram files of other files
    * ( r.adr + r.size ) = '\0';


    // ------------------------------------------------------------
    // analyser ce qu'il y a en mémoire (fichier html)
    // on scanne les balises
    // ------------------------------------------------------------
#if HTS_ANALYSTE
    _hts_in_html_done=0;     // 0% scannés
    _hts_cancel=0;           // pas de cancel
    _hts_in_html_parsing=1;  // flag pour indiquer un parsing
#endif
    base[0]='\0';    // effacer base-href
    lastsaved=adr;
    do {
      int p=0;
      int valid_p=0;      // force to take p even if == 0
      int ending_p='\0';  // ending quote?
      error=0;

      /* Hack to avoid NULL char problems with C syntax */
      /* Yes, some bogus HTML pages can embed null chars
         and therefore can not be properly handled if this hack is not done
      */
      if ( ! (*adr) ) {
        if ( ((int) (adr - r.adr)) < r.size)
          *adr=' ';
      }



      /*
      index.html built here
      */
      // Construction index.html (sommaire)
      // Avant de tester les a href,
      // Ici on teste si l'on doit construire l'index vers le(s) site(s) miroir(s)
      if (!makeindex_done) {  // autoriation d'écrire un index
        if (!detect_title) {
          if (opt.depth == liens[ptr]->depth) {    // on note toujours les premiers liens
            if (!in_media) {
              if (opt.makeindex && (ptr>0)) {
                if (opt.getmode & 1) {  // autorisation d'écrire
                  p=strfield(adr,"title");  
                  if (p) {
                    if (*(adr-1)=='/') p=0;    // /title
                  } else {
                    if (strfield(adr,"/html"))
                      p=-1;                    // noter, mais sans titre
                    else if (strfield(adr,"body"))
                      p=-1;                    // noter, mais sans titre
                    else if ( ((int) (adr - r.adr) ) >= (r.size-1) )
                      p=-1;                    // noter, mais sans titre
                    else if ( (int) (adr - r.adr) >= r.size - 2)   // we got to hurry
                      p=-1; // xxc xxc xxc
                  }
                } else
                  p=0;
                
                if (p) {    // ok center                            
                  if (makeindex_fp==NULL) {
                    verif_backblue(opt.path_html);    // générer gif
                    makeindex_fp=filecreate(fconcat(opt.path_html,"index.html"));
                    if (makeindex_fp!=NULL) {

                      // Header
                      fprintf(makeindex_fp,template_header,
                        "<!-- Mirror and index made by HTTrack Website Copier/"HTTRACK_VERSION" "HTTRACK_AFF_AUTHORS" -->"
                        );

                    } else makeindex_done=-1;    // fait, erreur
                  }
                  
                  if (makeindex_fp!=NULL) {
                    char tempo[HTS_URLMAXSIZE*2];
                    char s[HTS_URLMAXSIZE*2];
                    char* a=NULL;
                    char* b=NULL;
                    s[0]='\0';
                    if (p>0) {
                      a=strchr(adr,'>');
                      if (a!=NULL) {
                        a++;
                        while(is_space(*a)) a++;    // sauter espaces & co
                        b=strchr(a,'<');   // prochain tag
                      }
                    }
                    if (lienrelatif(tempo,liens[ptr]->sav,concat(opt.path_html,"index.html"))==0) {
                      detect_title=1;      // ok détecté pour cette page!
                      makeindex_links++;   // un de plus
                      strcpy(makeindex_firstlink,tempo);
                      //
                      if ((b==a) || (a==NULL) || (b==NULL)) {    // pas de titre
                        strcpy(s,tempo);
                      } else if ((b-a)<256) {
                        b--;
                        while(is_space(*b)) b--;
                        strncpy(s,a,b-a+1);
                        *(s+(b-a)+1)='\0';
                      }

                      // Body
                      fprintf(makeindex_fp,template_body,
                        tempo,
                        s
                        );

                    }
                  }
                }
              }
            }
            
          } else if (liens[ptr]->depth<opt.depth) {   // on a sauté level1+1 et level1
            HT_INDEX_END;
          }
        } // if (opt.makeindex)
      }
      // FIN Construction index.html (sommaire)
      /*
      end -- index.html built here
      */
      


      /* Parse */
      if (
           (*adr=='<')    /* No starting tag */
        && (!inscript)    /* Not in (java)script */
        && (!incomment)   /* Not in comment (<!--) */
      ) { 
        intag=1;
        parseall_incomment=0;
        //inquote=0;  // effacer quote
        intag_start=adr; intag_start_valid=1;
        codebase[0]='\0';    // effacer éventuel codebase
        
        if (opt.getmode & 1) {  // sauver html
          p=strfield(adr,"</html");
          if (p==0) p=strfield(adr,"<head>");
          // if (p==0) p=strfield(adr,"<doctype");
          if (p) {
            if (strnotempty(opt.footer)) {
              char tempo[1024+HTS_URLMAXSIZE*2];
              char gmttime[256];
              char* eol="\n";
              tempo[0]='\0';
              if (strchr(r.adr,'\r'))
                eol="\r\n";
              time_gmt_rfc822(gmttime);
              strcat(tempo,eol);
              sprintf(tempo+strlen(tempo),opt.footer,jump_identification(urladr),urlfil,gmttime,"","","","","","","","");
              strcat(tempo,eol);
              //fwrite(tempo,1,strlen(tempo),fp);
              HT_ADD(tempo);
            }
          }
        }        
        
        // éliminer les <!-- (commentaires) : intag dévalidé
        if (*(adr+1)=='!')
          if (*(adr+2)=='-')
            if (*(adr+3)=='-') {
              intag=0;
              incomment=1;
              intag_start_valid=0;
            }
            
      }
      else if (
           (*adr=='>')                        /* ending tag */
        && ( (!inscript) || (inscript_tag) )  /* and in tag (or in script) */
      ) {
        if (inscript_tag) {
          inscript_tag=inscript=0;
          intag=0;
          incomment=0;
          intag_start_valid=0;
        } else if (!incomment) {
          intag=0; //inquote=0;
          
          // entrée dans du javascript?
          // on parse ICI car il se peut qu'on ait eu a parser les src=.. dedans
          //if (!inscript) {  // sinon on est dans un obj.write("..
          if ((intag_start_valid) && 
            (
            check_tag(intag_start,"script")
            ||
            check_tag(intag_start,"style")
            )
            ) {
            char* a=intag_start;    // <
            // ** while(is_realspace(*(--a)));
            if (*a=='<') {  // sûr que c'est un tag?
              inscript=1;
              intag=1;     // because après <script> on y est .. - pas utile
              intag_start_valid=0;    // OUI car nous sommes dans du code, plus dans du "vrai" tag
            }
          }
        } else {                               /* end of comment? */
          // vérifier fermeture correcte
          if ( (*(adr-1)=='-') && (*(adr-2)=='-') ) {
            intag=0;
            incomment=0;
            intag_start_valid=0;
          }
#if GT_ENDS_COMMENT
          /* wrong comment ending */
          else {
            /* check if correct ending does not exists
               <!-- foo > example <!-- bar > is sometimes accepted by browsers
               when no --> is used somewhere else.. darn those browsers are dirty
            */
            if (!strstr(adr,"-->")) {
              intag=0;
              incomment=0;
              intag_start_valid=0;
            }
          }
#endif
        }
        //}
      }
      //else if (*adr==34) {
      //  inquote=(inquote?0:1);
      //}
      else if (intag || inscript) {    // nous sommes dans un tag/commentaire, tester si on recoit un tag
        int p_type=0;
        int p_nocatch=0;
        int p_searchMETAURL=0;  // chercher ..URL=<url>
        int add_class=0;        // ajouter .class
        int add_class_dots_to_patch=0;   // number of '.' in code="x.y.z<realname>"
        char* p_flush=NULL;
        
        
        // ------------------------------------------------------------
        // parsing évolé
        // ------------------------------------------------------------
        if (((isalpha((unsigned char)*adr)) || (*adr=='/') || (inscript) || (inscriptgen))) {  // sinon pas la peine de tester..


          /* caractère de terminaison pour "miniparsing" javascript=.. ? 
             (ex: <a href="javascript:()" action="foo"> ) */
          if (inscript_tag) {
            if (inscript_tag_lastc) {
              if (*adr == inscript_tag_lastc) {
                /* sortir */
                inscript_tag=inscript=0;
                incomment=0;
              }
            }
          }
          
          
          // Note:
          // Certaines pages ne respectent pas le html
          // notamment les guillements ne sont pas fixés
          // Nous sommes dans un tag, donc on peut faire un test plus
          // large pour pouvoi prendre en compte ces particularités
          
          // à vérifier: ACTION, CODEBASE, VRML
          
          if (in_media) {
            if (strcmp(in_media,"RAM")==0) { // real media
              p=0;
              valid_p=1;
            }
          } else if (ptr>0) {        /* pas première page 0 (primary) */
            p=0;  // saut pour le nom de fichier: adresse nom fichier=adr+p
            
            // ------------------------------
            // détection d'écriture JavaScript.
            // osons les obj.write et les obj.href=.. ! osons!
            // note: inscript==1 donc on sautera après les \"
            if (inscript) {
              if (inscriptgen) {          // on est déja dans un objet générant..
                if (*adr==scriptgen_q) {  // fermeture des " ou '
                  if (*(adr-1)!='\\') {   // non
                    inscriptgen=0;        // ok parsing terminé
                  }
                }
              } else {
                char* a=NULL;
                char check_this_fking_line=0;  // parsing code javascript..
                char must_be_terminated=0;     // caractère obligatoire de terminaison!
                int token_size;
                if (!(token_size=strfield(adr,".writeln"))) // détection ...objet.write[ln]("code html")...
                  token_size=strfield(adr,".write");
                if (token_size) {
                  a=adr+token_size;
                  while(is_realspace(*a)) a++; // sauter espaces
                  if (*a=='(') {  // début parenthèse
                    check_this_fking_line=2;  // à parser!
                    must_be_terminated=')';
                    a++;  // sauter (
                  }
                }
                // euhh ??? ???
                /* else if (strfield(adr,".href")) {  // détection ...objet.href="...
                a=adr+5;
                while(is_realspace(*a)) a++; // sauter espaces
                if (*a=='=') {  // ohh un égal
                check_this_fking_line=1;  // à noter!
                must_be_terminated=';';   // et si t'as oublié le ; tu sais pas coder
                a++;   // sauter =
                }
                
                }*/
                
                // on a un truc du genre instruction"code généré" dont on parse le code
                if (check_this_fking_line) {
                  while(is_realspace(*a)) a++;
                  if ((*a=='\'') || (*a=='"')) {  // départ de '' ou ""
                    char *b;
                    int ex=0;
                    scriptgen_q=*a;    // quote
                    b=a+1;      // départ de la chaîne
                    // vérifier forme ("code") et pas ("code"+var), ingérable
                    do {
                      a++;  // caractère suivant
                      if (*a==scriptgen_q) if (*(a-1)!='\\')  // quote non slash
                        ex=1;            // sortie
                      if ((*a==10) || (*a==13))
                        ex=1;
                    } while(!ex);
                    if (*a==scriptgen_q) {  // fin du quote
                      a++;
                      while(is_realspace(*a)) a++;
                      if (*a==must_be_terminated) {  // parenthèse fermante: ("..")
                        
                        // bon, on doit parser une ligne javascript
                        // 1) si check.. ==1 alors c'est un nom de fichier direct, donc
                        // on fixe p sur le saut nécessaire pour atteindre le nom du fichier
                        // et le moteur se débrouillera ensuite tout seul comme un grand
                        // 2) si check==2 c'est un peu plus tordu car là on génére du
                        // code html au sein de code javascript au sein de code html
                        // dans ce cas on doit fixer un flag à un puis ensuite dans la boucle
                        // on devra parser les instructions standard comme <a href etc
                        // NOTE: le code javascript autogénéré n'est pas pris en compte!!
                        // (et ne marche pas dans 50% des cas de toute facon!)
                        if (check_this_fking_line==1) {
                          p=(int) (b - adr);    // calculer saut!
                        } else {
                          inscriptgen=1;        // SCRIPTGEN actif
                          adr=b;                // jump
                        }
                        
                        if ((opt.debug>1) && (opt.log!=NULL)) {
                          char str[512];
                          str[0]='\0';
                          strncat(str,b,minimum((int) (a - b + 1), 32));
                          fspc(opt.log,"debug"); fprintf(opt.log,"active code (%s) detected in javascript: %s"LF,(check_this_fking_line==2)?"parse":"pickup",str); test_flush;
                        }
                      }
                      
                    }
                    
                  }
                  
                  
                }
              }
            }
            // fin detection code générant javascript vers html
            // ------------------------------
            
            
            // analyse proprement dite, A HREF=.. etc..
            if (!p) {
              // si dans un tag, et pas dans un script - sauf si on analyse un obj.write("..
              if ((intag && (!inscript)) || inscriptgen) {
                if ( (*(adr-1)=='<') || (is_space(*(adr-1))) ) {   // <tag < tag etc
                  // <A HREF=.. pour les liens HTML
                  p=rech_tageq(adr,"href");
                  if (p) {    // href.. tester si c'est une bas href!
                    if ((intag_start_valid) && check_tag(intag_start,"base")) {  // oui!
                      // ** note: base href et codebase ne font pas bon ménage..
                      p_type=2;    // c'est un chemin
                    }
                  }
                  
                  /* Tags supplémentaires à vérifier (<img src=..> etc) */
                  if (p==0) {
                    int i=0;
                    while( (p==0) && (strnotempty(hts_detect[i])) ) {
                      p=rech_tageq(adr,hts_detect[i]);
                      i++;
                    }
                  }

                  /* Tags supplémentaires en début à vérifier (<object .. hotspot1=..> etc) */
                  if (p==0) {
                    int i=0;
                    while( (p==0) && (strnotempty(hts_detectbeg[i])) ) {
                      p=rech_tageqbegdigits(adr,hts_detectbeg[i]);
                      i++;
                    }
                  }
                  
                  /* Tags supplémentaires à vérifier : URL=.. */
                  if (p==0) {
                    int i=0;
                    while( (p==0) && (strnotempty(hts_detectURL[i])) ) {
                      p=rech_tageq(adr,hts_detectURL[i]);
                      i++;
                    }
                    if (p)
                      p_searchMETAURL=1;
                  }
                  
                  /* Tags supplémentaires à vérifier, mais à ne pas capturer */
                  if (p==0) {
                    int i=0;
                    while( (p==0) && (strnotempty(hts_detectandleave[i])) ) {
                      p=rech_tageq(adr,hts_detectandleave[i]);
                      i++;
                    }
                    if (p)
                      p_nocatch=1;      /* ne pas rechercher */
                  }
                  
                  /* Evénements */
                  if (p==0) {
                    int i=0;
                    /* détection onLoad etc */
                    while( (p==0) && (strnotempty(hts_detect_js[i])) ) {
                      p=rech_tageq(adr,hts_detect_js[i]);
                      i++;
                    }
                    /* non détecté - détecter également les onXxxxx= */
                    if (p==0) {
                      if ( (*adr=='o') && (*(adr+1)=='n') && isUpperLetter(*(adr+2)) ) {
                        p=0;
                        while(isalpha((unsigned char)adr[p]) && (p<64) ) p++;
                        if (p<64) {
                          while(is_space(adr[p])) p++;
                          if (adr[p]=='=')
                            p++;
                          else p=0;
                        } else p=0;
                      }
                    }
                    /* OK, événement repéré */
                    if (p) {
                      inscript_tag_lastc=*(adr+p);     /* à attendre à la fin */
                      adr+=p;     /* saut */
                                  /*
                                  On est désormais dans du code javascript
                      */
                      inscript_tag=inscript=1;
                    }
                    p=0;        /* quoi qu'il arrive, ne rien démarrer ici */
                  }
                  
                  // <APPLET CODE=.. pour les applet java.. [CODEBASE (chemin..) à faire]
                  if (p==0) {
                    p=rech_tageq(adr,"code");
                    if (p) {
                      if ((intag_start_valid) && check_tag(intag_start,"applet")) {  // dans un <applet !
                        p_type=-1;  // juste le nom de fichier+dossier, écire avant codebase 
                        add_class=1;   // ajouter .class au besoin                         
                        
                        // vérifier qu'il n'y a pas de codebase APRES
                        // sinon on swappe les deux.
                        // pas très propre mais c'est ce qu'il y a de plus simple à faire!!
                        
                        {
                          char *a;
                          a=adr;
                          while((*a) && (*a!='>') && (!rech_tageq(a,"codebase"))) a++;
                          if (rech_tageq(a,"codebase")) {  // banzai! codebase=
                            char* b;
                            b=strchr(a,'>');
                            if (b) {
                              if (((int) (b - adr)) < 1000) {    // au total < 1Ko
                                char tempo[HTS_URLMAXSIZE*2];
                                tempo[0]='\0';
                                strncat(tempo,a,(int) (b - a) );
                                strcat( tempo," ");
                                strncat(tempo,adr,(int) (a - adr - 1));
                                // éventuellement remplire par des espaces pour avoir juste la taille
                                while((int) strlen(tempo)<((int) (b - adr)))
                                  strcat(tempo," ");
                                // pas d'erreur?
                                if ((int) strlen(tempo) == ((int) (b - adr) )) {
                                  strncpy(adr,tempo,strlen(tempo));   // PAS d'octet nul à la fin!
                                  p=0;    // DEVALIDER!!
                                  p_type=0;
                                  add_class=0;
                                }
                              }
                            }
                          }
                        }
                        
                      }
                    }
                  }
                  
                  // liens à patcher mais pas à charger (ex: codebase)
                  if (p==0) {  // note: si non chargé (ex: ignorer .class) patché tout de même
                    p=rech_tageq(adr,"codebase");
                    if (p) {
                      if ((intag_start_valid) && check_tag(intag_start,"applet")) {  // dans un <applet !
                        p_type=-2;
                      } else p=-1;   // ne plus chercher
                    }
                  }
                  
                  
                  // Meta tags pour robots
                  if (p==0) {
                    if (opt.robots) {
                      if ((intag_start_valid) && check_tag(intag_start,"meta")) {
                        if (rech_tageq(adr,"name")) {    // name=robots.txt
                          char tempo[1100];
                          char* a;
                          tempo[0]='\0';
                          a=strchr(adr,'>');
#if DEBUG_ROBOTS
                          printf("robots.txt meta tag detected\n");
#endif
                          if (a) {
                            if (((int) (a - adr)) < 999 ) {
                              strncat(tempo,adr,(int) (a - adr));
                              if (strstrcase(tempo,"content")) {
                                if (strstrcase(tempo,"robots")) {
                                  if (strstrcase(tempo,"nofollow")) {
#if DEBUG_ROBOTS
                                    printf("robots.txt meta tag: nofollow in %s%s\n",urladr,urlfil);
#endif
                                    nofollow=1;       // NE PLUS suivre liens dans cette page
                                    if (opt.errlog) {
                                      fspc(opt.errlog,"warning"); fprintf(opt.errlog,"Link %s%s not scanned (follow robots meta tag)"LF,urladr,urlfil);
                                      test_flush;
                                    }
                                  }
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                  
                  // entrée dans une applet javascript
                  /*if (!inscript) {  // sinon on est dans un obj.write("..
                  if (p==0)
                  if (rech_sampletag(adr,"script"))
                  if (check_tag(intag_start,"script")) {
                  inscript=1;
                  }
                        }*/
                  
                  // Ici on procède à une analyse du code javascript pour tenter de récupérer
                  // certains fichiers évidents.
                  // C'est devenu obligatoire vu le nombre de pages qui intègrent
                  // des images réactives par exemple
                }
              } else if (inscript) {
                if (
                  (
                  (strfield(adr,"/script"))
                  ||
                  (strfield(adr,"/style"))
                  )
                  ) {
                  char* a=adr;
                  //while(is_realspace(*(--a)));
                  while( is_realspace(*a) ) a--;
                  a--;
                  if (*a=='<') {  // sûr que c'est un tag?
                    inscript=0;
                  }
                } else {
                  /*
                  Script Analyzing - different types supported:
                    foo="url"
                    foo("url") or foo(url)
                    foo "url"
                  */
                  int nc;
                  char  expected     = '=';          // caractère attendu après
                  char* expected_end = ";";
                  int can_avoid_quotes=0;
                  char quotes_replacement='\0';
                  if (inscript_tag)
                    expected_end=";\"\'";            // voir a href="javascript:doc.location='foo'"
                  nc = strfield(adr,".src");  // nom.src="image";
                  if (!nc) nc = strfield(adr,".location");  // document.location="doc"
                  if (!nc) nc = strfield(adr,".href");  // document.location="doc"
                  if (!nc) if ( (nc = strfield(adr,".open")) ) { // window.open("doc",..
                    expected='(';    // parenthèse
                    expected_end="),";  // fin: virgule ou parenthèse
                  }
                  if (!nc) if ( (nc = strfield(adr,".replace")) ) { // window.replace("url")
                    expected='(';    // parenthèse
                    expected_end=")";  // fin: parenthèse
                  }
                  if (!nc) if ( (nc = strfield(adr,".link")) ) { // window.link("url")
                    expected='(';    // parenthèse
                    expected_end=")";  // fin: parenthèse
                  }
                  if (!nc) if ( (nc = strfield(adr,"url")) ) { // url(url)
                    expected='(';    // parenthèse
                    expected_end=")";  // fin: parenthèse
                    can_avoid_quotes=1;
                    quotes_replacement=')';
                  }
                  if (!nc) if ( (nc = strfield(adr,"import")) ) { // import "url"
                    if (is_space(*(adr+nc))) {
                      expected=0;    // no char expected
                    } else
                      nc=0;
                  }
                  if (nc) {
                    char *a;
                    a=adr+nc;
                    while(is_realspace(*a)) a++;
                    if ((*a == expected) || (!expected)) {
                      if (expected)
                        a++;
                      while(is_realspace(*a)) a++;
                      if ((*a==34) || (*a=='\'') || (can_avoid_quotes)) {
                        char *b,*c;
                        int ndelim=1;
                        if ((*a==34) || (*a=='\''))
                          a++;
                        else
                          ndelim=0;
                        b=a;
                        if (ndelim) {
                          while((*b!=34) && (*b!='\'') && (*b!='\0')) b++;
                        }
                        else {
                          while((*b != quotes_replacement) && (*b!='\0')) b++;
                        }
                        c=b--; c+=ndelim;
                        while(*c==' ') c++;
                        if ((strchr(expected_end,*c)) || (*c=='\n') || (*c=='\r')) {
                          c-=(ndelim+1);
                          if ((int) (c - a + 1)) {
                            if ((opt.debug>1) && (opt.log!=NULL)) {
                              char str[512];
                              str[0]='\0';
                              strncat(str,a,minimum((int) (c - a + 1),32));
                              fspc(opt.log,"debug"); fprintf(opt.log,"link detected in javascript: %s"LF,str); test_flush;
                            }
                            p=(int) (a - adr);    // p non nul: TRAITER CHAINE COMME FICHIER
                            if (can_avoid_quotes) {
                              ending_p=quotes_replacement;
                            }
                          }
                        }
                        
                        
                      }
                    }
                  }
                  
                }
              }
            }
            
          } else {      // ptr == 0
            //p=rech_tageq(adr,"primary");    // lien primaire, yeah
            p=0;          // No stupid tag anymore, raw link
            valid_p=1;    // Valid even if p==0
            while ((adr[p] == '\r') || (adr[p] == '\n'))
              p++;
            //can_avoid_quotes=1;
            ending_p='\r';
          }       
          
        } else if (isspace((unsigned char)*adr)) {
          intag_startattr=adr+1;        // attribute in tag (for dirty parsing)
        }
          
          
          // ------------------------------------------------------------
          // dernier recours - parsing "sale" : détection systématique des .gif, etc.
          // risque: générer de faux fichiers parazites
          // fix: ne parse plus dans les commentaires
          // ------------------------------------------------------------
          if ( (opt.parseall) && (ptr>0) && (!in_media) ) {           // option parsing "brut"
            int incomment_justquit=0;
            if (!is_realspace(*adr)) {
              int noparse=0;

              // Gestion des /* */
              if (inscript) {
                if (parseall_incomment) {
                  if ((*adr=='/') && (*(adr-1)=='*'))
                    parseall_incomment=0;
                  incomment_justquit=1;       // ne pas noter dernier caractère
                } else {
                  if ((*adr=='/') && (*(adr+1)=='*'))
                    parseall_incomment=1;
                }
              } else
                parseall_incomment=0;

              /* vérifier que l'on est pas dans un <!-- --> pur */
              if ( (!intag) && (incomment) && (!inscript))
                noparse=1;        /* commentaire */

              // recherche d'URLs
              if ((!parseall_incomment) && (!noparse)) {
                if (!p) {                   // non déja trouvé
                  if (adr != r.adr) {     // >1 caractère
                    // scanner les chaines
                    if ((*adr == '\"') || (*adr=='\'')) {         // "xx.gif" 'xx.gif'
                      if (strchr("=(,",parseall_lastc)) {    // exemple: a="img.gif..
                        char *a=adr;
                        char stop=*adr;  // " ou '
                        int count=0;
                        
                        // sauter caractères
                        a++;
                        // copier
                        while((*a) && (*a!='\'') && (*a!='\"') && (count<HTS_URLMAXSIZE)) { count++; a++; }
                        
                        // ok chaine terminée par " ou '
                        if ((*a == stop) && (count<HTS_URLMAXSIZE) && (count>0)) {
                          char c;
                          char* aend;
                          //
                          aend=a;     // sauver début
                          a++;
                          while(is_taborspace(*a)) a++;
                          c=*a;
                          if (strchr("),;>/+\r\n",c)) {     // exemple: ..img.gif";
                            // le / est pour funct("img.gif" /* URL */);
                            char tempo[HTS_URLMAXSIZE*2];
                            char type[256];
                            int url_ok=0;      // url valide?
                            tempo[0]='\0'; type[0]='\0';
                            //
                            strncat(tempo,adr+1,count);
                            //
                            if ((!strchr(tempo,' ')) || inscript) {   // espace dedans: méfiance! (sauf dans code javascript)
                              int invalid_url=0;

                              // escape                              
                              unescape_amp(tempo);

                              // Couper au # ou ? éventuel
                              {
                                char* a=strchr(tempo,'#');
                                if (a)
                                  *a='\0';
                                a=strchr(tempo,'?');
                                if (a)
                                  *a='\0';
                              }

                              // vérifier qu'il n'y a pas de caractères spéciaux
                              if (!strnotempty(tempo))
                                invalid_url=1;
                              else if (strchr(tempo,'*')
                                || strchr(tempo,'<')
                                || strchr(tempo,'>'))
                                invalid_url=1;
                              
                              /* non invalide? */
                              if (!invalid_url) {
                                // Un plus à la fin? Alors ne pas prendre sauf si extension ("/toto.html#"+tag)
                                if (c!='+') {    // PAS de plus à la fin
                                  char* a;
                                  // "Comparisons of scheme names MUST be case-insensitive" (RFC2616)                                  
                                  //if ((strncmp(tempo,"http://",7)==0) || (strncmp(tempo,"ftp://",6)==0))  // ok pas de problème
                                  if (
                                       (strfield(tempo,"http:")) 
                                    || (strfield(tempo,"ftp:"))
#if HTS_USEOPENSSL
                                    || (strfield(tempo,"https:"))
#endif
                                    )  // ok pas de problème
                                    url_ok=1;
                                  else if (tempo[strlen(tempo)-1]=='/') {        // un slash: ok..
                                    if (inscript)   // sinon si pas javascript, méfiance (répertoire style base?)
                                      url_ok=1;
                                  } else if ((a=strchr(tempo,'/'))) {        // un slash: ok..
                                    if (inscript) {    // sinon si pas javascript, méfiance (style "text/css")
                                      if (strchr(a+1,'/'))  // un seul / : abandon (STYLE type='text/css')
                                        url_ok=1;
                                    }
                                  }
                                }
                                // Prendre si extension reconnue
                                if (!url_ok) {
                                  get_httptype(type,tempo,0);
                                  if (strnotempty(type))     // type reconnu!
                                    url_ok=1;
                                  else if (is_dyntype(get_ext(tempo)))  // reconnu php,cgi,asp..
                                    url_ok=1;
                                  // MAIS pas les foobar@aol.com !!
                                  if (strchr(tempo,'@'))
                                    url_ok=0;
                                }
                                //
                                // Ok, cela pourrait être une URL
                                if (url_ok) {
                                  
                                  // Check if not fodbidden tag (id,name..)
                                  if (intag_start_valid) {
                                    if (intag_start)
                                      if (intag_startattr)
                                        if (intag)
                                          if (!inscript)
                                            if (!incomment) {
                                              int i=0,nop=0;
                                              while( (nop==0) && (strnotempty(hts_nodetect[i])) ) {
                                                nop=rech_tageq(intag_startattr,hts_nodetect[i]);
                                                i++;
                                              }
                                              // Forbidden tag
                                              if (nop) {
                                                url_ok=0;
                                                if ((opt.debug>1) && (opt.log!=NULL)) {
                                                  fspc(opt.log,"debug"); fprintf(opt.log,"dirty parsing: bad tag avoided: %s"LF,hts_nodetect[i-1]); test_flush;
                                                }
                                              }
                                            }
                                  }
                                  
                                  
                                  // Accepter URL, on la traitera comme une URL normale!!
                                  if (url_ok)
                                    p=1;

                                }
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                }  // p == 0
                
                // plus dans un commentaire
                if (!incomment_justquit)
                  parseall_lastc=*adr;             // caractère avant le prochain
                
              } // not in comment
              
            }  // if realspace
          }  // if parseall
          
          
          // ------------------------------------------------------------
          // p!=0 : on a repéré un éventuel lien
          // ------------------------------------------------------------
          //
          if ((p>0) || (valid_p)) {    // on a repéré un lien
            //int lien_valide=0;
            char* eadr=NULL;          /* fin de l'URL */
            char* quote_adr=NULL;     /* adresse du ? dans l'adresse */
            int ok=1;
            char quote='\0';
            
            // si nofollow ou un stop a été déclenché, réécrire tous les liens en externe
            if ((nofollow) || (opt.state.stop))
              p_nocatch=1;

            // écrire codebase avant, flusher avant code
            if ((p_type==-1) || (p_type==-2)) {
              if ((opt.getmode & 1) && (ptr>0)) {
                HT_ADD_ADR;    // refresh
              }
              lastsaved=adr;    // dernier écrit+1
            }
            
            // sauter espaces
            adr+=p;
            while((is_space(*adr)) && (quote=='\0')) {
              if (!quote)
                if ((*adr=='\"') || (*adr=='\''))
                  quote=*adr;                     // on doit attendre cela à la fin
                                                  // puis quitter
                adr++;    // sauter les espaces, "" et cie
            }

            /* Stop at \n (LF) if primary links*/
            if (ptr == 0)
              quote='\n';
            /* s'arrêter que ce soit un ' ou un " : pour document.write('<img src="foo'+a); par exemple! */
            else if (inscript)
              quote='\0';
            
            // sauter éventuel \" ou \' javascript
            if (inscript) {    // on est dans un obj.write("..
              if (*adr=='\\') {
                if ((*(adr+1)=='\'') || (*(adr+1)=='"')) {  // \" ou \'
                  adr+=2;    // sauter
                }
              }
            }
            
            // sauter content="1;URL=http://..
            if (p_searchMETAURL) {
              int l=0;
              while(
                (adr + l + 4 < r.adr + r.size)
                && (!strfield(adr+l,"URL=")) 
                && (l<128) ) l++;
              if (!strfield(adr+l,"URL="))
                ok=-1;
              else
                adr+=(l+4);
            }

            /* éviter les javascript:document.location=.. : les parser, plutôt */
            if (ok!=-1) {
              if (strfield(adr,"javascript:")) {
                ok=-1;
                /*
                On est désormais dans du code javascript
                */
                inscript_tag=inscript=1;
                inscript_tag_lastc=quote;     /* à attendre à la fin */
              }
            }
            
            if (p_type==1) {
              if (*adr=='#') {
                adr++;           // sauter # pour usemap etc
              }
            }
            eadr=adr;
            
            // ne pas flusher après code si on doit écrire le codebase avant!
            if ((p_type!=-1) && (p_type!=2) && (p_type!=-2)) {
              if ((opt.getmode & 1) && (ptr>0)) {
                HT_ADD_ADR;    // refresh
              }
              lastsaved=adr;    // dernier écrit+1
              // après on écrira soit les données initiales,
              // soir une URL/lien modifié!
            } else if (p_type==-1) p_flush=adr;    // flusher jusqu'à adr ensuite
            
            if (ok!=-1) {    // continuer
              // découper le lien
              do {
                if ((* (unsigned char*) eadr)<32) {   // caractère de contrôle (ou \0)
                  if (!is_space(*eadr))
                    ok=0; 
                }
                if ( ( ((int) (eadr - adr)) ) > HTS_URLMAXSIZE)  // ** trop long, >HTS_URLMAXSIZE caractères (on prévoit HTS_URLMAXSIZE autres pour path)
                  ok=-1;    // ne pas traiter ce lien
                
                if (ok > 0) {
                  //if (*eadr!=' ') {  
                  if (is_space(*eadr)) {   // guillemets,CR, etc
                    if ((!quote) || (*eadr==quote))     // si pas d'attente de quote spéciale ou si quote atteinte
                      ok=0; 
                  } else if (ending_p && (*eadr==ending_p))
                    ok=0;
                  else {
                    switch(*eadr) {
                    case '>': 
                      if (!quote) {
                        if (!inscript) {
                          intag=0;    // PLUS dans un tag!
                          intag_start_valid=0;
                        }
                        ok=0;
                      }
                      break;
                      /*case '<':*/ 
                    case '#': 
                      if (*(eadr-1) != '&')       // &#40;
                        ok=0; 
                      break;
                      // case '?': non!
                    case '\\': if (inscript) ok=0; break;     // \" ou \' point d'arrêt
                    case '?': quote_adr=adr; break;           // noter position query
                    }
                  }
                  //}
                } 
                eadr++;
              } while(ok==1);     
              
              // Empty link detected
              if ( (((int) (eadr - adr))) <= 1) {       // link empty
                ok=-1;        // No
                if (*adr != '#') {        // Not empty+unique #
                  if ( (((int) (eadr - adr)) == 1)) {       // 1=link empty with delim (end_adr-start_adr)
                    if (quote) {
                      if ((opt.getmode & 1) && (ptr>0)) { 
                        HT_ADD("#");        // We add this for a <href="">
                      }
                    }
                  }
                }
              }
              
            }
            
            if (ok==0) {    // tester un lien
              char lien[HTS_URLMAXSIZE*2];
              int meme_adresse=0;      // 0 par défaut pour primary
              //char *copie_de_adr=adr;
              //char* p;
              
              // construire lien (découpage)
              if ( (((int) (eadr -  adr))-1) < HTS_URLMAXSIZE  ) {    // pas trop long?
                strncpy(lien,adr,((int) (eadr - adr))-1);
                *(lien+  (((int) (eadr -  adr)))-1  )='\0';
                //printf("link: %s\n",lien);          
                // supprimer les espaces
                while((lien[strlen(lien)-1]==' ') && (strnotempty(lien))) lien[strlen(lien)-1]='\0';

                
#if HTS_STRIP_DOUBLE_SLASH
                // supprimer les // en / (sauf pour http://)
                {
                  char *a,*p,*q;
                  int done=0;
                  a=strchr(lien,':');    // http://
                  if (a) {
                    a++;
                    while(*a=='/') a++;    // position après http://
                  } else {
                    a=lien;                // début
                    while(*a=='/') a++;    // position après http://
                  }
                  q=strchr(a,'?');     // ne pas traiter après '?'
                  if (!q)
                    q=a+strlen(a)-1;
                  while(( p=strstr(a,"//")) && (!done) ) {    // remplacer // par /
                    if ((int) p>(int) q) {   // après le ? (toto.cgi?param=1//2.3)
                      done=1;    // stopper
                    } else {
                      char tempo[HTS_URLMAXSIZE*2];
                      tempo[0]='\0';
                      strncat(tempo,a,(int) p - (int) a);
                      strcat (tempo,p+1);
                      strcpy(a,tempo);    // recopier
                    }
                  }
                }
#endif

              } else
                lien[0]='\0';    // erreur
              
              // ------------------------------------------------------
              // Lien repéré et extrait
              if (strnotempty(lien)>0) {           // construction du lien
                char adr[HTS_URLMAXSIZE*2],fil[HTS_URLMAXSIZE*2];          // ATTENTION adr cache le "vrai" adr
                int forbidden_url=-1;              // lien non interdit (mais non autorisé..)
                int just_test_it=0;                // mode de test des liens
                int set_prio_to=0;                 // pour capture de page isolée
                int import_done=0;                 // lien importé (ne pas scanner ensuite *à priori*)
                //
                adr[0]='\0'; fil[0]='\0';
                //
                // 0: autorisé
                // 1: interdit (patcher tout de même adresse)
                
                if ((opt.debug>1) && (opt.log!=NULL)) {
                  fspc(opt.log,"debug"); fprintf(opt.log,"link detected in html: %s"LF,lien); test_flush;
                }

                // external check
#if HTS_ANALYSTE
                if (!hts_htmlcheck_linkdetected(lien)) {
                  error=1;    // erreur
                  if (opt.errlog) {
                    fspc(opt.errlog,"error"); fprintf(opt.errlog,"Link %s refused by external wrapper"LF,lien);
                    test_flush;
                  }
                }
#endif
                
                // purger espaces de début et fin, CR,LF résiduels
                // (IMG SRC="foo.<\n>gif")
                {
                  char* a;
                  while (is_realspace(lien[0])) {
                    char tempo[HTS_URLMAXSIZE*2];
                    tempo[0]='\0';
                    strcpy(tempo,lien+1);
                    strcpy(lien,tempo);
                  }
                  while(strnotempty(lien)
                        && (is_realspace(lien[max(0,(int)(strlen(lien))-1)])) ) {
                    lien[strlen(lien)-1]='\0';
                  } 
                  while ((a=strchr(lien,'\n'))) {
                    char tempo[HTS_URLMAXSIZE*2];
                    tempo[0]='\0';
                    strncat(tempo,lien,(int) (a - lien));
                    strcat(tempo,a+1);
                    strcpy(lien,tempo);
                  }
                  while ((a=strchr(lien,'\r'))) {
                    char tempo[HTS_URLMAXSIZE*2];
                    tempo[0]='\0';
                    strncat(tempo,lien,(int) (a - lien));
                    strcat(tempo,a+1);
                    strcpy(lien,tempo);
                  }
                }
                
                /* Unescape/escape %20 and other &nbsp; */
                {
                  char query[HTS_URLMAXSIZE*2];
                  char* a=strchr(lien,'?');
                  if (a) {
                    strcpy(query,a);
                    *a='\0';
                  } else
                    query[0]='\0';
                  // conversion &amp; -> & et autres joyeusetés
                  unescape_amp(lien);
                  unescape_amp(query);
                  // décoder l'inutile (%2E par exemple) et coder espaces
                  // XXXXXXXXXXXXXXXXX strcpy(lien,unescape_http(lien));
                  strcpy(lien,unescape_http_unharm(lien, (no_esc_utf)?0:1));
                  escape_spc_url(lien);
                  strcat(lien,query);     /* restore */
                }
                
                // convertir les éventuels \ en des / pour éviter des problèmes de reconnaissance!
                {
                  char* a=jump_identification(lien);
                  while( (a=strchr(a,'\\')) ) *a='/';
                }
                
                // supprimer le(s) ./
                while ((lien[0]=='.') && (lien[1]=='/')) {
                  char tempo[HTS_URLMAXSIZE*2];
                  strcpy(tempo,lien+2);
                  strcpy(lien,tempo);
                }
                if (strnotempty(lien)==0)  // sauf si plus de nom de fichier
                  strcpy(lien,"./");
                
                // vérifie les /~machin -> /~machin/
                // supposition dangereuse?
                // OUI!!
#if HTS_TILDE_SLASH
                if (lien[strlen(lien)-1]!='/') {
                  char *a=lien+strlen(lien)-1;
                  // éviter aussi index~1.html
                  while (((int) a>(int) lien) && (*a!='~') && (*a!='/') && (*a!='.')) a--;
                  if (*a=='~') {
                    strcat(lien,"/");    // ajouter slash
                  }
                }
#endif
                
                // APPLET CODE="mixer.MixerApplet.class" --> APPLET CODE="mixer/MixerApplet.class"
                // yes, this is dirty
                // but I'm so lazzy..
                // and besides the java "code" convention is really a pain in html code
                if (p_type==-1) {
                  char* a=strrchr(lien,'.');
                  add_class_dots_to_patch=0;
                  if (a) {
                    char* b;
                    do {
                      b=strchr(lien,'.');
                      if ((b != a) && (b)) {
                        add_class_dots_to_patch++;
                        *b='/';
                      }
                    } while((b != a) && (b));
                  }
                }
                
                // éliminer les éventuels :80 (port par défaut!)
                if (link_has_authority(lien)) {
                  char * a;
                  a=strstr(lien,"//");    // "//" authority
                  if (a)
                    a+=2;
                  else
                    a=lien;
                  // while((*a) && (*a!='/') && (*a!=':')) a++;
                  a=jump_toport(a);
                  if (a) {  // port
                    int port=0;
                    int defport=80;
                    char* b=a+1;
#if HTS_USEOPENSSL
                    // FIXME
                    //if (strfield(adr, "https:")) {
                    //}
#endif
                    while(isdigit((unsigned char)*b)) { port*=10; port+=(int) (*b-'0'); b++; }
                    if (port==defport) {  // port 80, default - c'est débile
                      char tempo[HTS_URLMAXSIZE*2];
                      tempo[0]='\0';
                      strncat(tempo,lien,(int) (a - lien));
                      strcat(tempo,a+3);  // sauter :80
                      strcpy(lien,tempo);
                    }
                  }
                }
                
                // filtrer les parazites (mailto & cie)
                /*
                if (strfield(lien,"mailto:")) {  // ne pas traiter
                  error=1;
                } else if (strfield(lien,"news:")) {  // ne pas traiter
                  error=1;
                }
                */
                
                // vérifier que l'on ne doit pas ajouter de .class
                if (!error) {
                  if (add_class) {
                    char *a = lien+strlen(lien)-1;
                    while(( a > lien) && (*a!='/') && (*a!='.')) a--;
                    if (*a != '.')
                      strcat(lien,".class");    // ajouter .class
                    else if (!strfield2(a,".class"))
                      strcat(lien,".class");    // idem
                  }
                }
                
                // si c'est un chemin, alors vérifier (toto/toto.html -> http://www/toto/)
                if (!error) {
                  if ((opt.debug>1) && (opt.log!=NULL)) {
                    fspc(opt.log,"debug"); fprintf(opt.log,"position link check %s"LF,lien); test_flush;
                  }
                  
                  if ((p_type==2) || (p_type==-2)) {   // code ou codebase                        
                    // Vérifier les codebase=applet (au lieu de applet/)
                    if (p_type==-2) {    // codebase
                      if (strnotempty(lien)) {
                        if (fil[strlen(lien)-1]!='/') {  // pas répertoire
                          strcat(lien,"/");
                        }
                      }
                    }
                    /* only one ending / (bug on some pages) */
                    if ((int)strlen(lien)>2) {
                      while( (lien[strlen(lien)-2]=='/') && ((int)strlen(lien)>2) )    /* double // (bug) */
                        lien[strlen(lien)-1]='\0';
                    }
                    // copier nom host si besoin est
                    if (!link_has_authority(lien)) {  // pas de http://
                      char adr2[HTS_URLMAXSIZE*2],fil2[HTS_URLMAXSIZE*2];  // ** euh ident_url_relatif??
                      if (ident_url_relatif(lien,urladr,urlfil,adr2,fil2)<0) {                        
                        error=1;
                      } else {
                        strcpy(lien,"http://");
                        strcat(lien,adr2);
                        if (*fil2!='/')
                          strcat(lien,"/");
                        strcat(lien,fil2);
                        {
                          char* a;
                          a=lien+strlen(lien)-1;
                          while((*a) && (*a!='/') && ( a> lien)) a--;
                          if (*a=='/') {
                            *(a+1)='\0';
                          }
                        }
                        //char tempo[HTS_URLMAXSIZE*2];
                        //strcpy(tempo,"http://");
                        //strcat(tempo,urladr);    // host
                        //if (*lien!='/')
                        //  strcat(tempo,"/");
                        //strcat(tempo,lien);
                        //strcpy(lien,tempo);
                      }
                    }
                    
                    if (!error) {  // pas d'erreur?
                      if (p_type==2) {   // code ET PAS codebase      
                        char* a=lien+strlen(lien)-1;
                        while( (a > lien) && (*a) && (*a!='/')) a--;
                        if (*a=='/')     // ok on a repéré le dernier /
                          *(a+1)='\0';   // couper
                        else {
                          *lien='\0';    // éliminer
                          error=1;   // erreur, ne pas poursuivre
                        }      
                      }
                      
                      // stocker base ou codebase?
                      switch(p_type) {
                      case 2: { 
                        //if (*lien!='/') strcat(base,"/");
                        strcpy(base,lien);
                              }
                        break;      // base
                      case -2: {
                        //if (*lien!='/') strcat(codebase,"/");
                        strcpy(codebase,lien); 
                               }
                        break;  // base
                      }
                      
                      if ((opt.debug>1) && (opt.log!=NULL)) {
                        fspc(opt.log,"debug"); fprintf(opt.log,"code/codebase link %s base %s"LF,lien,base); test_flush;
                      }
                      //printf("base code: %s - %s\n",lien,base);
                    }
                    
                  } else {
                    char* _base;
                    if (p_type==-1)   // code (applet)
                      _base=codebase;
                    else
                      _base=base;
                    
                    
                    // ajouter chemin de base href..
                    if (strnotempty(_base)) {       // considérer base
                      if (!link_has_authority(lien)) {    // non absolue
                        //if (*lien!='/') {           // non absolu sur le site (/)
                        if ( ((int) strlen(_base)+(int) strlen(lien))<HTS_URLMAXSIZE) {
                          // mailto: and co: do NOT add base
                          if (ident_url_relatif(lien,urladr,urlfil,adr,fil)>=0) {
                            char tempo[HTS_URLMAXSIZE*2];
                            // base est absolue
                            strcpy(tempo,_base);
                            strcat(tempo,lien + ((*lien=='/')?1:0) );
                            strcpy(lien,tempo);        // patcher en considérant base
                            // ** vérifier que ../ fonctionne (ne doit pas arriver mais bon..)
                            
                            if ((opt.debug>1) && (opt.log!=NULL)) {
                              fspc(opt.log,"debug"); fprintf(opt.log,"link modified with code/codebase %s"LF,lien); test_flush;
                            }
                          }
                        } else {
                          error=1;    // erreur
                          if (opt.errlog) {
                            fspc(opt.errlog,"error"); fprintf(opt.errlog,"Link %s too long with base href"LF,lien);
                            test_flush;
                          }
                        }
                        //}
                      }
                    }
                    
                    
                  }
                  }
                  
                  
                  // transformer lien quelconque (http, relatif, etc) en une adresse
                  // et un chemin+fichier (adr,fil)
                  if (!error) {
                    int reponse;
                    if ((opt.debug>1) && (opt.log!=NULL)) {
                      fspc(opt.log,"debug"); fprintf(opt.log,"build relative link %s with %s%s"LF,lien,urladr,urlfil); test_flush;
                    }
                    if ((reponse=ident_url_relatif(lien,urladr,urlfil,adr,fil))<0) {                        
                      adr[0]='\0';    // erreur
                      if (reponse==-2) {
                        if (opt.errlog) {
                          fspc(opt.errlog,"warning"); fprintf(opt.errlog,"Link %s not caught (unknown ftp:// protocol)"LF,lien);
                          test_flush;
                        }
                      } else {
                        if ((opt.debug>1) && (opt.errlog!=NULL)) {
                          fspc(opt.errlog,"debug"); fprintf(opt.errlog,"ident_url_relatif failed for %s with %s%s"LF,lien,urladr,urlfil); test_flush;
                        }
                      }
                    }
                  } else {
                    if ((opt.debug>1) && (opt.log!=NULL)) {
                      fspc(opt.log,"debug"); fprintf(opt.log,"link %s not build, error detected before"LF,lien); test_flush;
                    }
                    adr[0]='\0';
                  }
                  
#if HTS_CHECK_STRANGEDIR
                  // !ATTENTION!
                  // Ici on teste les exotiques du genre www.truc.fr/machin (sans slash à la fin)
                  // je n'ai pas encore trouvé le moyen de faire la différence entre un répertoire
                  // et un fichier en http A PRIORI : je fais donc un test
                  // En cas de moved xxx, on recalcule adr et fil, tout simplement
                  // DEFAUT: test effectué plusieurs fois! à revoir!!!
                  if ((adr[0]!='\0') && (strcmp(adr,"file://") && (p_type!=2) && (p_type!=-2)) {
                    //## if ((adr[0]!='\0') && (adr[0]!=lOCAL_CHAR) && (p_type!=2) && (p_type!=-2)) {
                    if (fil[strlen(fil)-1]!='/') {  // pas répertoire
                      if (ishtml(fil)==-2) {    // pas d'extension
                        char loc[HTS_URLMAXSIZE*2];  // éventuelle nouvelle position
                        loc[0]='\0';
                        if ((opt.debug>1) && (opt.log!=NULL)) {
                          fspc(opt.log,"debug"); fprintf(opt.log,"link-check-directory: %s%s"LF,adr,fil);
                          test_flush;
                        }
                        
                        // tester éventuelle nouvelle position
                        switch (http_location(adr,fil,loc).statuscode) {
                        case 200: // ok au final
                          if (strnotempty(loc)) {  // a changé d'adresse
                            if (opt.errlog) {
                              fspc(opt.errlog,"warning"); fprintf(opt.errlog,"Link %s%s has moved to %s for %s%s"LF,adr,fil,loc,urladr,urlfil);
                              test_flush;
                            }
                            
                            // recalculer adr et fil!
                            if (ident_url_absolute(loc,adr,fil)==-1) {
                              adr[0]='\0';  // cancel
                              if ((opt.debug>1) && (opt.log!=NULL)) {
                                fspc(opt.log,"debug"); fprintf(opt.log,"link-check-dir: %s%s"LF,adr,fil);
                                test_flush;
                              }
                            }
                            
                          }
                          break;
                        case -2: case -3:  // timeout ou erreur grave
                          if (opt.errlog) {
                            fspc(opt.errlog,"warning"); fprintf(opt.errlog,"Connection too slow for testing link %s%s (from %s%s)"LF,adr,fil,urladr,urlfil);
                            test_flush;
                          }
                          
                          break;
                        }
                        
                      }
                    } 
                  }
#endif
                  
                  // Le lien doit juste être réécrit, mais ne doit pas générer un lien
                  // exemple: <FORM ACTION="url_cgi">
                  if (p_nocatch) {
                    forbidden_url=1;    // interdire récupération du lien
                    if ((opt.debug>1) && (opt.log!=NULL)) {
                      fspc(opt.log,"debug"); fprintf(opt.log,"link forced external at %s%s"LF,adr,fil);
                      test_flush;
                    }
                  }
                  
                  // Tester si un lien doit être accepté ou refusé (wizard)
                  // forbidden_url=1 : lien refusé
                  // forbidden_url=0 : lien accepté
                  //if ((ptr>0) && (p_type!=2) && (p_type!=-2)) {    // tester autorisations?
                  if ((p_type!=2) && (p_type!=-2)) {    // tester autorisations?
                    if (!p_nocatch) {
                      if (adr[0]!='\0') {          
                        if ((opt.debug>1) && (opt.log!=NULL)) {
                          fspc(opt.log,"debug"); fprintf(opt.log,"wizard link test at %s%s.."LF,adr,fil);
                          test_flush;
                        }
                        forbidden_url=hts_acceptlink(&opt,ptr,lien_tot,liens,
                          adr,fil,
                          &filters,&filptr,opt.maxfilter,
                          &robots,
                          &set_prio_to,
                          &just_test_it);
                        if ((opt.debug>1) && (opt.log!=NULL)) {
                          fspc(opt.log,"debug"); fprintf(opt.log,"result for wizard link test: %d"LF,forbidden_url);
                          test_flush;
                        }
                      }
                    }
                  }
                  
                  // calculer meme_adresse
                  meme_adresse=strfield2(jump_identification(adr),jump_identification(urladr));
                  
                  
                  
                  // Début partie sauvegarde
                  
                  // ici on forme le nom du fichier à sauver, et on patche l'URL
                  if (adr[0]!='\0') {
                    // savename: simplifier les ../ et autres joyeusetés
                    char save[HTS_URLMAXSIZE*2];
                    int r_sv=0;
                    // En cas de moved, adresse première
                    char former_adr[HTS_URLMAXSIZE*2];
                    char former_fil[HTS_URLMAXSIZE*2];
                    //
                    save[0]='\0'; former_adr[0]='\0'; former_fil[0]='\0';
                    //
                    
                    // nom du chemin à sauver si on doit le calculer
                    // note: url_savename peut décider de tester le lien si il le trouve
                    // suspect, et modifier alors adr et fil
                    // dans ce cas on aura une référence directe au lieu des traditionnels
                    // moved en cascade (impossible à reproduire à priori en local, lorsque des fichiers
                    // gif sont impliqués par exemple)
                    if ((p_type!=2) && (p_type!=-2)) {  // pas base href ou codebase
                      if (forbidden_url!=1) {
                        char last_adr[HTS_URLMAXSIZE*2];
                        last_adr[0]='\0';
                        //char last_fil[HTS_URLMAXSIZE*2]="";
                        strcpy(last_adr,adr);    // ancienne adresse
                        //strcpy(last_fil,fil);    // ancien chemin
                        r_sv=url_savename(adr,fil,save,former_adr,former_fil,liens[ptr]->adr,liens[ptr]->fil,&opt,liens,lien_tot,back,back_max,&cache,&hash,ptr,numero_passe);
                        if (strcmp(jump_identification(last_adr),jump_identification(adr)) != 0) {  // a changé
                          
                          // 2e test si moved
                          
                          // Tester si un lien doit être accepté ou refusé (wizard)
                          // forbidden_url=1 : lien refusé
                          // forbidden_url=0 : lien accepté
                          if ((ptr>0) && (p_type!=2) && (p_type!=-2)) {    // tester autorisations?
                            if (!p_nocatch) {
                              if (adr[0]!='\0') {          
                                if ((opt.debug>1) && (opt.log!=NULL)) {
                                  fspc(opt.log,"debug"); fprintf(opt.log,"wizard moved link retest at %s%s.."LF,adr,fil);
                                  test_flush;
                                }
                                forbidden_url=hts_acceptlink(&opt,ptr,lien_tot,liens,
                                  adr,fil,
                                  &filters,&filptr,opt.maxfilter,
                                  &robots,
                                  &set_prio_to,
                                  &just_test_it);
                                if ((opt.debug>1) && (opt.log!=NULL)) {
                                  fspc(opt.log,"debug"); fprintf(opt.log,"result for wizard moved link retest: %d"LF,forbidden_url);
                                  test_flush;
                                }
                              }
                            }
                          }
                          
                          //import_done=1;    // c'est un import!
                          meme_adresse=0;   // on a changé
                        }
                      } else {
                        strcpy(save,"");  // dummy
                      }
                    }
                    if (r_sv!=-1) {  // pas d'erreur, on continue
                      /* log */
                      if ((opt.debug>1) && (opt.log!=NULL)) {
                        fspc(opt.log,"debug");
                        if (forbidden_url!=1) {    // le lien va être chargé
                          if ((p_type==2) || (p_type==-2)) {  // base href ou codebase, pas un lien
                            fprintf(opt.log,"Code/Codebase: %s%s"LF,adr,fil);
                          } else if ((opt.getmode & 4)==0) {
                            fprintf(opt.log,"Record: %s%s -> %s"LF,adr,fil,save);
                          } else {
                            if (!ishtml(fil))
                              fprintf(opt.log,"Record after: %s%s -> %s"LF,adr,fil,save);
                            else
                              fprintf(opt.log,"Record: %s%s -> %s"LF,adr,fil,save);
                          } 
                        } else
                          fprintf(opt.log,"External: %s%s"LF,adr,fil);
                        test_flush;
                      }
                      /* FIN log */
                      
                      // écrire lien
                      if ((p_type==2) || (p_type==-2)) {  // base href ou codebase, sauter
                        lastsaved=eadr-1+1;  // sauter "
                      }
                      /* */
                      else if (opt.urlmode==0) {    // URL absolue dans tous les cas
                        if ((opt.getmode & 1) && (ptr>0)) {    // ecrire les html
                          if (!link_has_authority(adr)) {
                            HT_ADD("http://");
                          } else {
                            char* aut = strstr(adr, "//");
                            if (aut) {
                              char tmp[256];
                              tmp[0]='\0';
                              strncat(tmp, adr, (int) (aut - adr));   // scheme
                              HT_ADD(tmp);          // Protocol
                              HT_ADD("//");
                            }
                          }

                          if (!opt.passprivacy) {
                            HT_ADD(jump_protocol(adr));           // Password
                          } else {
                            HT_ADD(jump_identification(adr));     // No Password
                          }
                          if (*fil!='/')
                            HT_ADD("/");
                          HT_ADD(fil);
                        }
                        lastsaved=eadr-1;    // dernier écrit+1 (enfin euh apres on fait un ++ alors hein)
                      /* */
                      } else if (opt.urlmode >= 4) {    // ne rien faire dans tous les cas!
                      /* */
                      /* leave the link 'as is' */
                      /* Sinon, dépend de interne/externe */
                      } else if (forbidden_url==1) {    // le lien ne sera pas chargé, référence externe!
                        if ((opt.getmode & 1) && (ptr>0)) {
                          if (p_type!=-1) {     // pas que le nom de fichier (pas classe java)
                            if (!opt.external) {
                              if (!link_has_authority(adr)) {
                                HT_ADD("http://");
                                if (!opt.passprivacy) {
                                  HT_ADD(adr);     // Password
                                } else {
                                  HT_ADD(jump_identification(adr));     // No Password
                                }
                                if (*fil!='/')
                                  HT_ADD("/");
                                HT_ADD(fil);
                              } else {
                                char* aut = strstr(adr, "//");
                                if (aut) {
                                  char tmp[256];
                                  tmp[0]='\0';
                                  strncat(tmp, adr, (int) (aut - adr));   // scheme
                                  HT_ADD(tmp);          // Protocol
                                  HT_ADD("//");
                                  if (!opt.passprivacy) {
                                    HT_ADD(jump_protocol(adr));          // Password
                                  } else {
                                    HT_ADD(jump_identification(adr));     // No Password
                                  }
                                  if (*fil!='/')
                                    HT_ADD("/");
                                  HT_ADD(fil);
                                }
                              }
                              //
                            } else {    // fichier/page externe, mais on veut générer une erreur
                              //
                              int patch_it=0;
                              int add_url=0;
                              char* cat_name=NULL;
                              char* cat_data=NULL;
                              int cat_nb=0;
                              int cat_data_len=0;
                              
                              // ajouter lien external
                              switch ( (link_has_authority(adr)) ? 1 : ( (fil[strlen(fil)-1]=='/')?1:(ishtml(fil))  ) ) {
                              case 1: case -2:       // html ou répertoire
                                if (opt.getmode & 1) {  // sauver html
                                  patch_it=1;   // redirect
                                  add_url=1;    // avec link?
                                  cat_name="external.html";
                                  cat_nb=0;
                                  cat_data=HTS_DATA_UNKNOWN_HTML;
                                  cat_data_len=HTS_DATA_UNKNOWN_HTML_LEN;
                                }
                                break;
                              default:    // inconnu
                                // asp, cgi..
                                if (is_dyntype(get_ext(fil))) {
                                  patch_it=1;   // redirect
                                  add_url=1;    // avec link?
                                  cat_name="external.html";
                                  cat_nb=0;
                                  cat_data=HTS_DATA_UNKNOWN_HTML;
                                  cat_data_len=HTS_DATA_UNKNOWN_HTML_LEN;
                                } else if ( (strfield2(fil+max(0,(int)strlen(fil)-4),".gif")) 
                                  || (strfield2(fil+max(0,(int)strlen(fil)-4),".jpg")) 
                                  || (strfield2(fil+max(0,(int)strlen(fil)-4),".xbm")) 
                                  || (ishtml(fil)!=0) ) {
                                  patch_it=1;   // redirect
                                  add_url=1;    // avec link aussi
                                  cat_name="external.gif";
                                  cat_nb=1;
                                  cat_data=HTS_DATA_UNKNOWN_GIF;
                                  cat_data_len=HTS_DATA_UNKNOWN_GIF_LEN;
                                }
                                break;
                              }// html,gif
                              
                              if (patch_it) {
                                char save[HTS_URLMAXSIZE*2];
                                char tempo[HTS_URLMAXSIZE*2];
                                strcpy(save,opt.path_html);
                                strcat(save,cat_name);
                                if (lienrelatif(tempo,save,savename)==0) {
                                  if (!no_esc_utf)
                                    escape_uri(tempo);     // escape with %xx
                                  else
                                    escape_uri_utf(tempo);     // escape with %xx
                                  HT_ADD(tempo);    // page externe
                                  if (add_url) {
                                    HT_ADD("?link=");    // page externe
                                    
                                    // same as above
                                    if (!link_has_authority(adr)) {
                                      HT_ADD("http://");
                                      if (!opt.passprivacy) {
                                        HT_ADD(adr);     // Password
                                      } else {
                                        HT_ADD(jump_identification(adr));     // No Password
                                      }
                                      if (*fil!='/')
                                        HT_ADD("/");
                                      HT_ADD(fil);
                                    } else {
                                      char* aut = strstr(adr, "//");
                                      if (aut) {
                                        char tmp[256];
                                        tmp[0]='\0';
                                        strncat(tmp, adr, (int) (aut - adr) + 2);   // scheme
                                        HT_ADD(tmp);
                                        if (!opt.passprivacy) {
                                          HT_ADD(jump_protocol(adr));          // Password
                                        } else {
                                          HT_ADD(jump_identification(adr));     // No Password
                                        }
                                        if (*fil!='/')
                                          HT_ADD("/");
                                        HT_ADD(fil);
                                      }
                                    }
                                    //

                                  }
                                }
                                
                                // écrire fichier?
                                if (verif_external(cat_nb,1)) {
                                //if (!fexist(fconcat(opt.path_html,cat_name))) {
                                  FILE* fp = filecreate(fconcat(opt.path_html,cat_name));
                                  if (fp) {
                                    if (cat_data_len==0) {   // texte
                                      verif_backblue(opt.path_html);
                                      fprintf(fp,"%s%s","<!-- Created by HTTrack Website Copier/"HTTRACK_VERSION" "HTTRACK_AFF_AUTHORS" -->"LF,cat_data);
                                    } else {                    // data
                                      fwrite(cat_data,cat_data_len,1,fp);
                                    }
                                    fclose(fp);
                                    usercommand(0,NULL,fconcat(opt.path_html,cat_name));
                                  }
                                }
                              }  else {    // écrire normalement le nom de fichier
                                HT_ADD("http://");
                                if (!opt.passprivacy) {
                                  HT_ADD(adr);       // Password
                                } else {
                                  HT_ADD(jump_identification(adr));       // No Password
                                }
                                if (*fil!='/')
                                  HT_ADD("/");
                                HT_ADD(fil);
                              }// patcher?
                            }  // external
                          } else {  // que le nom de fichier (classe java)
                            // en gros recopie de plus bas: copier codebase et base
                            if (p_flush) {
                              char tempo[HTS_URLMAXSIZE*2];    // <-- ajouté
                              char tempo_pat[HTS_URLMAXSIZE*2];

                              // Calculer chemin
                              tempo_pat[0]='\0';
                              strcpy(tempo,fil);  // <-- ajouté
                              {
                                char* a=strrchr(tempo,'/');

                                // Example: we converted code="x.y.z.foo.class" into "x/y/z/foo.class"
                                // we have to do the contrary now
                                if (add_class_dots_to_patch>0) {
                                  while( (add_class_dots_to_patch>0) && (a) ) {
                                    *a='.';     // convert "false" java / into .
                                    add_class_dots_to_patch--;
                                    a=strrchr(tempo,'/');
                                  }
                                  // if add_class_dots_to_patch, this is because there is a problem!!
                                  if (add_class_dots_to_patch) {
                                    if (opt.errlog) {
                                      fspc(opt.errlog,"warning"); fprintf(opt.errlog,"Error: can not rewind java path %s, check html code"LF,tempo);
                                      test_flush;
                                    }
                                  }
                                }

                                // Cut path/filename
                                if (a) {
                                  char tempo2[HTS_URLMAXSIZE*2];
                                  strcpy(tempo2,a+1);         // FICHIER
                                  strncat(tempo_pat,tempo,(int) (a - tempo)+1);  // chemin
                                  strcpy(tempo,tempo2);                     // fichier
                                }
                              }
                              
                              // érire codebase="chemin"
                              if ((opt.getmode & 1) && (ptr>0)) {
                                char tempo4[HTS_URLMAXSIZE*2];
                                tempo4[0]='\0';
                                
                                if (strnotempty(tempo_pat)) {
                                  HT_ADD("codebase=\"http://");
                                  if (!opt.passprivacy) {
                                    HT_ADD(adr);  // Password
                                  } else {
                                    HT_ADD(jump_identification(adr));  // No Password
                                  }
                                  if (*tempo_pat!='/') HT_ADD("/");
                                  HT_ADD(tempo_pat);
                                  HT_ADD("\" ");
                                }
                                
                                strncat(tempo4,lastsaved,(int) (p_flush - lastsaved));
                                HT_ADD(tempo4);    // refresh code="
                                HT_ADD(tempo);
                              }
                            }
                          }
                        }
                        lastsaved=eadr-1;
                      }
                      /*
                      else if (opt.urlmode==1) {    // ABSOLU, c'est le cas le moins courant
                      //  NE FONCTIONNE PAS!!  (et est inutile)
                      if ((opt.getmode & 1) && (ptr>0)) {    // ecrire les html
                      // écrire le lien modifié, absolu
                      HT_ADD("file:");
                      if (*save=='/')
                      HT_ADD(save+1)
                      else
                      HT_ADD(save)
                      }
                      lastsaved=eadr-1;    // dernier écrit+1 (enfin euh apres on fait un ++ alors hein)
                      }
                      */
                      else if (opt.urlmode==3) {    // URI absolue /
                        if ((opt.getmode & 1) && (ptr>0)) {    // ecrire les html
                          HT_ADD(fil);
                        }
                        lastsaved=eadr-1;    // dernier écrit+1 (enfin euh apres on fait un ++ alors hein)
                      }
                      else if (opt.urlmode==2) {  // RELATIF
                        char tempo[HTS_URLMAXSIZE*2];
                        tempo[0]='\0';
                        // calculer le lien relatif
                        
                        if (lienrelatif(tempo,save,savename)==0) {
                          if (!no_esc_utf)
                            escape_uri(tempo);     // escape with %xx
                          else
                            escape_uri_utf(tempo);     // escape with %xx
                          if ((opt.debug>1) && (opt.log!=NULL)) {
                            fspc(opt.log,"debug"); fprintf(opt.log,"relative link at %s build with %s and %s: %s"LF,adr,save,savename,tempo);
                            test_flush;
                          }
                          
                          // lien applet (code) - il faut placer un codebase avant
                          if (p_type==-1) {  // que le nom de fichier
                            
                            if (p_flush) {
                              char tempo_pat[HTS_URLMAXSIZE*2];
                              tempo_pat[0]='\0';
                              {
                                char* a=strrchr(tempo,'/');

                                // Example: we converted code="x.y.z.foo.class" into "x/y/z/foo.class"
                                // we have to do the contrary now
                                if (add_class_dots_to_patch>0) {
                                  while( (add_class_dots_to_patch>0) && (a) ) {
                                    *a='.';     // convert "false" java / into .
                                    add_class_dots_to_patch--;
                                    a=strrchr(tempo,'/');
                                  }
                                  // if add_class_dots_to_patch, this is because there is a problem!!
                                  if (add_class_dots_to_patch) {
                                    if (opt.errlog) {
                                      fspc(opt.errlog,"warning"); fprintf(opt.errlog,"Error: can not rewind java path %s, check html code"LF,tempo);
                                      test_flush;
                                    }
                                  }
                                }

                                if (a) {
                                  char tempo2[HTS_URLMAXSIZE*2];
                                  strcpy(tempo2,a+1);
                                  strncat(tempo_pat,tempo,(int) (a - tempo)+1);  // chemin
                                  strcpy(tempo,tempo2);                     // fichier
                                }
                              }
                              
                              // érire codebase="chemin"
                              if ((opt.getmode & 1) && (ptr>0)) {
                                char tempo4[HTS_URLMAXSIZE*2];
                                tempo4[0]='\0';
                                
                                if (strnotempty(tempo_pat)) {
                                  HT_ADD("codebase=\"");
                                  HT_ADD(tempo_pat);
                                  HT_ADD("\" ");
                                }
                                
                                strncat(tempo4,lastsaved,(int) (p_flush - lastsaved));
                                HT_ADD(tempo4);    // refresh code="
                              }
                            }
                            //lastsaved=adr;    // dernier écrit+1
                          }                              
                          
                          if ((opt.getmode & 1) && (ptr>0)) {
                            // écrire le lien modifié, relatif
                            HT_ADD(tempo);

                            // Add query-string, for informational purpose only
                            // Useless, because all parameters-pages are saved into different targets
                            if (opt.includequery) {
                              char* a=strchr(lien,'?');
                              if (a) {
                                HT_ADD(a);
                              }
                            }
                          }
                          lastsaved=eadr-1;    // dernier écrit+1 (enfin euh apres on fait un ++ alors hein)
                        } else {
                          if (opt.errlog) {
                            fprintf(opt.errlog,"Error building relative link %s and %s"LF,save,savename);
                            test_flush;
                          }
                        }
                      }  // sinon le lien sera écrit normalement
                      
                      
#if 0
                      if (fexist(save)) {    // le fichier existe..
                        adr[0]='\0';
                        //if ((opt.debug>0) && (opt.log!=NULL)) {
                        if (opt.errlog) {
                          fspc(opt.errlog,"warning"); fprintf(opt.errlog,"Link has already been written on disk, cancelled: %s"LF,save);
                          test_flush;
                        }
                      }
#endif                            
                      
                      /* Security check */
                      if (strlen(save) >= HTS_URLMAXSIZE) {
                        adr[0]='\0';
                        if (opt.errlog) {
                          fspc(opt.errlog,"warning"); fprintf(opt.errlog,"Link is too long: %s"LF,save);
                          test_flush;
                        }
                      }

                      if ((adr[0]!='\0') && (p_type!=2) && (p_type!=-2) && ( (forbidden_url!=1) || (just_test_it))) {  // si le fichier n'existe pas, ajouter à la liste                            
                        // n'y a-t-il pas trop de liens?
                        if (lien_tot+1 >= lien_max-4) {    // trop de liens!
                          printf("PANIC! : Too many URLs : >%d [%d]\n",lien_tot,__LINE__);
                          if (opt.errlog) {
                            fprintf(opt.errlog,LF"Too many URLs, giving up..(>%d)"LF,lien_max);
                            fprintf(opt.errlog,"To avoid that: use #L option for more links (example: -#L1000000)"LF);
                            test_flush;
                          }
                          if ((opt.getmode & 1) && (ptr>0)) { if (fp) { fclose(fp); fp=NULL; } }
                          XH_uninit;   // désallocation mémoire & buffers
                          return 0;
                          
                        } else {    // noter le lien sur la listes des liens à charger
                          int pass_fix,dejafait=0;
                          
                          // Calculer la priorité de ce lien
                          if ((opt.getmode & 4)==0) {    // traiter html après
                            pass_fix=0;
                          } else {    // vérifier que ce n'est pas un !html
                            if (!ishtml(fil))
                              pass_fix=1;        // priorité inférieure (traiter après)
                            else
                              pass_fix=max(0,numero_passe);    // priorité normale
                          }
                          
                          /* If the file seems to be an html file, get depth-1 */
                          /*
                          if (strnotempty(save)) {
                            if (ishtml(save) == 1) {
                              // descore_prio = 2;
                            } else {
                              // descore_prio = 1;
                            }
                          }
                          */
                          
                          // vérifier que le lien n'a pas déja été noté
                          // si c'est le cas, alors il faut s'assurer que la priorité associée
                          // au fichier est la plus grande des deux priorités
                          //
                          // On part de la fin et on essaye de se presser (économise temps machine)
#if HTS_HASH
                          {
                            int i=hash_read(&hash,save,"",0);      // lecture type 0 (sav)
                            if (i>=0) {
                              liens[i]->depth=maximum(liens[i]->depth,liens[ptr]->depth - 1);
                              dejafait=1;
                            }
                          }
#else
                          {
                            int l;
                            int i;
                            l=strlen(save);  // opti
                            for(i=lien_tot-1;(i>=0) && (dejafait==0);i--) {
                              if (liens[i]->sav_len==l) {    // même taille de chaîne
                                if (strcmp(liens[i]->sav,save)==0) {    // existe déja
                                  liens[i]->depth=maximum(liens[i]->depth,liens[ptr]->depth - 1);
                                  dejafait=1;
                                }
                              }
                            }
                          }
#endif
                          
                          // le lien n'a jamais été créé.
                          // cette fois ci, on le crée!
                          if (!dejafait) {                                
                            //
                            // >>>> CREER LE LIEN <<<<
                            //
                            // enregistrer lien à charger
                            //liens[lien_tot]->adr[0]=liens[lien_tot]->fil[0]=liens[lien_tot]->sav[0]='\0';
                            // même adresse: l'objet père est l'objet père de l'actuel
                            
                            // DEBUT ROBOTS.TXT AJOUT
                            if (!just_test_it) {
                              if (
                                (!strfield(adr,"ftp://"))         // non ftp
                             && (!strfield(adr,"file://")) ) {    // non file
                                if (opt.robots) {    // récupérer robots
                                  if (ishtml(fil)!=0) {                       // pas la peine pour des fichiers isolés
                                    if (checkrobots(&robots,adr,"") != -1) {    // robots.txt ?
                                      checkrobots_set(&robots,adr,"");          // ajouter entrée vide
                                      if (checkrobots(&robots,adr,"") == -1) {    // robots.txt ?
                                        // enregistrer robots.txt (MACRO)
                                        liens_record(adr,"/robots.txt","","","");
                                        if (liens[lien_tot]==NULL) {  // erreur, pas de place réservée
                                          printf("PANIC! : Not enough memory [%d]\n",__LINE__);
                                          if (opt.errlog) { 
                                            fprintf(opt.errlog,"Not enough memory, can not re-allocate %d bytes"LF,(int)((add_tab_alloc+1)*sizeof(lien_url)));
                                            test_flush;
                                          }
                                          if ((opt.getmode & 1) && (ptr>0)) { if (fp) { fclose(fp); fp=NULL; } }
                                          XH_uninit;    // désallocation mémoire & buffers
                                          return 0;
                                        }  
                                        liens[lien_tot]->testmode=0;          // pas mode test
                                        liens[lien_tot]->link_import=0;       // pas mode import     
                                        liens[lien_tot]->premier=lien_tot;
                                        liens[lien_tot]->precedent=ptr;
                                        liens[lien_tot]->depth=0;
                                        liens[lien_tot]->pass2=max(0,numero_passe);
                                        liens[lien_tot]->retry=0;
                                        lien_tot++;  // UN LIEN DE PLUS
#if DEBUG_ROBOTS
                                        printf("robots.txt: added file robots.txt for %s\n",adr);
#endif
                                        if ((opt.debug>1) && (opt.log!=NULL)) {
                                          fspc(opt.log,"debug"); fprintf(opt.log,"robots.txt added at %s"LF,adr);
                                          test_flush;
                                        }
                                      } else {
                                        if (opt.errlog) {   
                                          fprintf(opt.errlog,"Unexpected robots.txt error at %d"LF,__LINE__);
                                          test_flush;
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                            }
                            // FIN ROBOTS.TXT AJOUT
                            
                            // enregistrer (MACRO)
                            liens_record(adr,fil,save,former_adr,former_fil);
                            if (liens[lien_tot]==NULL) {  // erreur, pas de place réservée
                              printf("PANIC! : Not enough memory [%d]\n",__LINE__);
                              if (opt.errlog) { 
                                fprintf(opt.errlog,"Not enough memory, can not re-allocate %d bytes"LF,(int)((add_tab_alloc+1)*sizeof(lien_url)));
                                test_flush;
                              }
                              if ((opt.getmode & 1) && (ptr>0)) { if (fp) { fclose(fp); fp=NULL; } }
                              XH_uninit;    // désallocation mémoire & buffers
                              return 0;
                            }  
                            
                            // mode test?
                            if (!just_test_it)
                              liens[lien_tot]->testmode=0;          // pas mode test
                            else
                              liens[lien_tot]->testmode=1;          // mode test
                            if (!import_done)
                              liens[lien_tot]->link_import=0;       // pas mode import
                            else
                              liens[lien_tot]->link_import=1;       // mode import
                            // écrire autres paramètres de la structure-lien
                            if ((meme_adresse) && (!import_done) && (liens[ptr]->premier != 0))
                              liens[lien_tot]->premier=liens[ptr]->premier;
                            else    // sinon l'objet père est le précédent lui même
                              liens[lien_tot]->premier=lien_tot;
                            // liens[lien_tot]->premier=ptr;
                            
                            liens[lien_tot]->precedent=ptr;
                            // noter la priorité
                            if (!set_prio_to)
                              liens[lien_tot]->depth=liens[ptr]->depth - 1;
                            else
                              liens[lien_tot]->depth=max(0,min(liens[ptr]->depth-1,set_prio_to-1));         // PRIORITE NULLE (catch page)
                            // noter pass
                            liens[lien_tot]->pass2=pass_fix;
                            liens[lien_tot]->retry=opt.retry;
                            
                            //strcpy(liens[lien_tot]->adr,adr);
                            //strcpy(liens[lien_tot]->fil,fil);
                            //strcpy(liens[lien_tot]->sav,save); 
                            if ((opt.debug>1) && (opt.log!=NULL)) {
                              if (!just_test_it) {
                                fspc(opt.log,"debug"); fprintf(opt.log,"OK, NOTE: %s%s -> %s"LF,liens[lien_tot]->adr,liens[lien_tot]->fil,liens[lien_tot]->sav);
                              } else {
                                fspc(opt.log,"debug"); fprintf(opt.log,"OK, TEST: %s%s"LF,liens[lien_tot]->adr,liens[lien_tot]->fil);
                              }
                              test_flush;
                            }
                            
                            lien_tot++;  // UN LIEN DE PLUS
                          } else { // if !dejafait
                            if ((opt.debug>1) && (opt.log!=NULL)) {
                              fspc(opt.log,"debug"); fprintf(opt.log,"link has already been recorded, cancelled: %s"LF,save);
                              test_flush;
                            }
                            
                          }
                          
                          
                        }   // si pas trop de liens
                      }   // si adr[0]!='\0'
                      
                      
                    }  // if adr[0]!='\0' 
                    
                  }  // if adr[0]!='\0'
                  
                }    // if strlen(lien)>0
                
              }   // if ok==0      
              
              adr=eadr-1;  // ** sauter
              
            }  // if (p) 
            
          }  // si '<' ou '>'
          
          // plus loin
          adr++;


          /* Otimization: if we are scanning in HTML data (not in tag or script), 
          then jump to the next starting tag */
          if (ptr>0) {
            if ( (!intag)         /* Not in tag */
              && (!inscript)      /* Not in (java)script */
              && (!incomment)     /* Not in comment (<!--) */
              && (!inscript_tag)  /* Not in tag with script inside */
              ) 
            {
              /* Not at the end */
              if (( ((int) (adr - r.adr)) ) < r.size) {
                /* Not on a starting tag yet */
                if (*adr != '<') {
                  char* adr_next = strchr(adr,'<');
                  /* Jump to near end (index hack) */
                  if (!adr_next) {
                    if (
                      ( (int)(adr - r.adr) < (r.size - 4)) 
                      &&
                      (r.size > 4)
                      ) {
                      adr = r.adr + r.size - 2;
                    }
                  } else {
                    adr = adr_next;
                  }
                }
              }
            }
          }
          
          // ----------
          // écrire peu à peu
          if ((opt.getmode & 1) && (ptr>0)) HT_ADD_ADR;
          lastsaved=adr;    // dernier écrit+1
          // ----------
          
          // pour les stats du shell si parsing trop long
#if HTS_ANALYSTE
          if (r.size)
            _hts_in_html_done=(100 * ((int) (adr - r.adr)) ) / (int)(r.size);
          if (_hts_in_html_poll) {
            _hts_in_html_poll=0;
            // temps à attendre, et remplir autant que l'on peut le cache (backing)
            back_wait(back,back_max,&opt,&cache,HTS_STAT.stat_timestart);        
            back_fillmax(back,back_max,&opt,&cache,liens,ptr,numero_passe,lien_tot);

            // Transfer rate
            engine_stats();
            
            // Refresh various stats
            HTS_STAT.stat_nsocket=back_nsoc(back,back_max);
            HTS_STAT.stat_errors=fspc(NULL,"error");
            HTS_STAT.stat_warnings=fspc(NULL,"warning");
            HTS_STAT.stat_infos=fspc(NULL,"info");
            HTS_STAT.nbk=backlinks_done(liens,lien_tot,ptr);
            HTS_STAT.nb=back_transfered(HTS_STAT.stat_bytes,back,back_max);

            if (!hts_htmlcheck_loop(back,back_max,0,ptr,lien_tot,(int) (time_local()-HTS_STAT.stat_timestart),&HTS_STAT)) {
              if (opt.errlog) {
                fspc(opt.errlog,"info"); fprintf(opt.errlog,"Exit requested by shell or user"LF);
                test_flush;
              } 
              exit_xh=1;  // exit requested
              XH_uninit;
              return 0;
              //adr = r.adr + r.size;  // exit
            } else if (_hts_cancel==1) {
              // adr = r.adr + r.size;  // exit
              nofollow=1;               // moins violent
              _hts_cancel=0;
            }
          }

          // refresh the backing system each 2 seconds
          if (engine_stats()) {
            back_wait(back,back_max,&opt,&cache,HTS_STAT.stat_timestart);        
            back_fillmax(back,back_max,&opt,&cache,liens,ptr,numero_passe,lien_tot);
          }
#endif
        } while(( ((int) (adr - r.adr)) ) < r.size);
#if HTS_ANALYSTE
        _hts_in_html_parsing=0;  // flag
        _hts_cancel=0;           // pas de cancel
#endif
        if ((opt.getmode & 1) && (ptr>0)) {
          HT_ADD_END;    // achever
        }
        //
        //
        //
      }  // if !error
      
      
      if (opt.getmode & 1) { if (fp) { fclose(fp); fp=NULL; } }
      // sauver fichier
      //structcheck(savename);
      //filesave(r.adr,r.size,savename);
      
#if HTS_ANALYSTE
    }  // analyse OK
#endif
        
