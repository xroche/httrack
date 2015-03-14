/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998-2015 Xavier Roche and other contributors

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.

Important notes:

- We hereby ask people using this source NOT to use it in purpose of grabbing
emails addresses, or collecting any other private information on persons.
This would disgrace our work, and spoil the many hours we spent on it.

Please visit our Website: http://www.httrack.com
*/

/* ------------------------------------------------------------ */
/* File: htsindex.c                                             */
/*       keyword indexing system (search index)                 */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

#include "htsindex.h"
#include "htsglobal.h"
#include "htslib.h"

#if HTS_MAKE_KEYWORD_INDEX
#include "htshash.h"
#include "coucal.h"

/* Keyword Indexer Parameters */

// Maximum length for a keyword
#define KEYW_LEN             50
// Minimum length for a keyword - MUST NOT BE NULL!!!
#define KEYW_MIN_LEN         3
// What characters to accept? - MUST NOT BE EMPTY AND MUST NOT CONTAIN THE SPACE (32) CHARACTER!!!
#define KEYW_ACCEPT          "abcdefghijklmnopqrstuvwxyz0123456789-_."
// Convert A to a, and so on.. to avoid case problems in indexing
// This can be a generic table, containing characters that are in fact not accepted by KEYW_ACCEPT
// MUST HAVE SAME SIZES!!
#define KEYW_TRANSCODE_FROM  (\
                               "ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
                               "àâä" \
                               "ÀÂÄ" \
                               "éèêë" \
                               "ÈÈÊË" \
                               "ìîï" \
                               "ÌÎÏ" \
                               "òôö" \
                               "ÒÔÖ" \
                               "ùûü" \
                               "ÙÛÜ" \
                               "ÿ" \
                             )
#define KEYW_TRANSCODE_TO    ( \
                               "abcdefghijklmnopqrstuvwxyz" \
                               "aaa" \
                               "aaa" \
                               "eeee" \
                               "eeee" \
                               "iii" \
                               "iii" \
                               "ooo" \
                               "ooo" \
                               "uuu" \
                               "uuu" \
                               "y" \
                             )
// These (accepted) characters will be ignored at beginning of a keyword
#define KEYW_IGNORE_BEG       "-_."
// These (accepted) characters will be stripped if at the end of a keyword
#define KEYW_STRIP_END       "-_."
// Words beginning with these (accepted) characters will be ignored
#define KEYW_NOT_BEG         "0123456789"
// Treat these characters as space characters - MUST NOT BE EMPTY!!!
#define KEYW_SPACE           " ',;:!?\"\x0d\x0a\x09\x0b\x0c"
// Common words (the,for..) detector
// If a word represents more than KEYW_USELESS1K (%1000) of total words, then ignore it
// 5 (0.5%)
#define KEYW_USELESS1K       5
// If a word is present in more than KEYW_USELESS1KPG (%1000) pages, then ignore it
// 800 (80%)
#define KEYW_USELESS1KPG     800
// This number will be reduced by index hit for sorting purpose
// leave it as it is here if you don't REALLY know what you are doing
// Yes, I may be the only person, maybe
#define KEYW_SORT_MAXCOUNT 999999999

/* End of Keyword Indexer Parameters */

int strcpos(const char *adr, char c);
int mystrcmp(const void *_e1, const void *_e2);

// Global variables
int hts_index_init = 1;
int hts_primindex_size = 0;
FILE *fp_tmpproject = NULL;
int hts_primindex_words = 0;

#endif

/* 
  Init index 
*/
void index_init(const char *indexpath) {
#if HTS_MAKE_KEYWORD_INDEX
  /* remove(concat(indexpath,"index.txt")); */
  hts_index_init = 1;
  hts_primindex_size = 0;
  hts_primindex_words = 0;
  fp_tmpproject = tmpfile();
#endif
}

/* 
   Indexing system
   A little bit dirty, (quick'n dirty, in fact)
   But should be okay on most cases
   Tags and javascript handled (ignored)
*/
/* Note: utf-8 */
int index_keyword(const char *html_data, LLint size, const char *mime,
                  const char *filename, const char *indexpath) {
#if HTS_MAKE_KEYWORD_INDEX
  char catbuff[CATBUFF_SIZE];
  int intag = 0, inscript = 0, incomment = 0;
  char keyword[KEYW_LEN + 32];
  int i = 0;

  //
  //int WordIndexSize = 1024;
  coucal WordIndexHash = NULL;
  FILE *tmpfp = NULL;

  //

  // Check parameters
  if (!html_data)
    return 0;
  if (!size)
    return 0;
  if (!mime)
    return 0;
  if (!filename)
    return 0;

  // Init ?
  if (hts_index_init) {
    UNLINK(concat(catbuff, sizeof(catbuff), indexpath, "index.txt"));
    UNLINK(concat(catbuff, sizeof(catbuff), indexpath, "sindex.html"));
    hts_index_init = 0;
  }
  // Check MIME type
  if (is_html_mime_type(mime)) {
    inscript = 0;
  }
  // FIXME - temporary fix for image/svg+xml (svg)
  // "IN XML" (html like, in fact :) )
  else if ((strfield2(mime, "image/svg+xml"))
           || (strfield2(mime, "image/svg-xml"))) {
    inscript = 0;
  } else if ((strfield2(mime, "application/x-javascript"))
             || (strfield2(mime, "text/css"))
    ) {
    inscript = 1;
    //} else if (strfield2(mime, "text/vnd.wap.wml")) {   // humm won't work in many cases
    //  inscript=0;
  } else
    return 0;

  // Temporary file
  tmpfp = tmpfile();
  if (!tmpfp)
    return 0;

  // Create hash structure
  // Hash tables rulez da world!
  WordIndexHash = coucal_new(0);
  if (!WordIndexHash)
    return 0;

  // Start indexing this page
  keyword[0] = '\0';
  while(i < size) {
    if (strfield(html_data + i, "<script")) {
      inscript = 1;
    } else if (strfield(html_data + i, "<!--")) {
      incomment = 1;
    } else if (strfield(html_data + i, "</script")) {
      if (!incomment)
        inscript = 0;
    } else if (strfield(html_data + i, "-->")) {
      incomment = 0;
    } else if (html_data[i] == '<') {
      if (!inscript)
        intag = 1;
    } else if (html_data[i] == '>') {
      intag = 0;
    } else {
      // Okay, parse keywords
      if ((!inscript) && (!incomment) && (!intag)) {
        char cchar = html_data[i];
        int pos;
        int len = (int) strlen(keyword);

        // Replace (ignore case, and so on..)
        if ((pos = strcpos(KEYW_TRANSCODE_FROM, cchar)) >= 0)
          cchar = KEYW_TRANSCODE_TO[pos];

        if (strchr(KEYW_ACCEPT, cchar)) {
          /* Ignore some characters at beginning */
          if ((len > 0) || (!strchr(KEYW_IGNORE_BEG, cchar))) {
            keyword[len++] = cchar;
            keyword[len] = '\0';
          }
        } else if ((strchr(KEYW_SPACE, cchar)) || (!cchar)) {

          /* Avoid these words */
          if (len > 0) {
            if (strchr(KEYW_NOT_BEG, keyword[0])) {
              keyword[(len = 0)] = '\0';
            }
          }

          /* Strip ending . and so */
          {
            int ok = 0;

            while((len = (int) strlen(keyword)) && (!ok)) {
              if (strchr(KEYW_STRIP_END, keyword[len - 1])) {   /* strip it */
                keyword[len - 1] = '\0';
              } else
                ok = 1;
            }
          }

          /* Store it ? */
          if (len >= KEYW_MIN_LEN) {
            hts_primindex_words++;
            if (coucal_inc(WordIndexHash, keyword)) {  /* added new */
              fprintf(tmpfp, "%s\n", keyword);
            }
          }
          keyword[(len = 0)] = '\0';
        } else                  /* Invalid */
          keyword[(len = 0)] = '\0';

        if (len > KEYW_LEN) {
          keyword[(len = 0)] = '\0';
        }
      }

    }

    i++;
  }

  // Reset temp file
  fseek(tmpfp, 0, SEEK_SET);

  // Process indexing for this page
  {
    //FILE* fp=NULL;
    //fp=fopen(concat(indexpath,"index.txt"),"ab");
    if (fp_tmpproject) {
      while(!feof(tmpfp)) {
        char line[KEYW_LEN + 32];

        linput(tmpfp, line, KEYW_LEN + 2);
        if (strnotempty(line)) {
          intptr_t e = 0;

          if (coucal_read(WordIndexHash, line, &e)) {
            //if (e) {
            char BIGSTK savelst[HTS_URLMAXSIZE * 2];

            e++;                /* 0 means "once" */

            if (strncmp((const char *) fslash(catbuff, sizeof(catbuff), (const char *) indexpath), filename, strlen(indexpath)) == 0)  // couper
              strcpybuff(savelst, filename + strlen(indexpath));
            else
              strcpybuff(savelst, filename);

            // Add entry for this file and word
            fprintf(fp_tmpproject, "%s %d %s\n", line,
                    (int) (KEYW_SORT_MAXCOUNT - e), savelst);
            hts_primindex_size++;
            //}
          }
        }
      }
      //fclose(fp);
    }
  }

  // Delete temp file
  fclose(tmpfp);
  tmpfp = NULL;

  // Clear hash table
  coucal_delete(&WordIndexHash);
#endif
  return 1;
}

/*
  Sort index!
*/
/* Note: NOT utf-8 */
void index_finish(const char *indexpath, int mode) {
#if HTS_MAKE_KEYWORD_INDEX
  char catbuff[CATBUFF_SIZE];
  char **tab;
  char *blk;
  off_t size = fpsize(fp_tmpproject);

  if (size > 0) {
    //FILE* fp=fopen(concat(indexpath,"index.txt"),"rb");
    if (fp_tmpproject) {
      tab = (char **) malloct(sizeof(char *) * (hts_primindex_size + 2));
      if (tab) {
        blk = malloct(size + 4);
        if (blk) {
          fseek(fp_tmpproject, 0, SEEK_SET);
          if ((INTsys) fread(blk, 1, size, fp_tmpproject) == size) {
            char *a = blk, *b;
            int index = 0;
            int i;
            FILE *fp;

            while((b = strchr(a, '\n')) && (index < hts_primindex_size)) {
              tab[index++] = a;
              *b = '\0';
              a = b + 1;
            }

            // Sort it!
            qsort(tab, index, sizeof(char *), mystrcmp);

            // Delete fp_tmpproject
            fclose(fp_tmpproject);
            fp_tmpproject = NULL;

            // Write new file
            if (mode == 1)      // TEXT
              fp = fopen(concat(catbuff, sizeof(catbuff), indexpath, "index.txt"), "wb");
            else                // HTML
              fp = fopen(concat(catbuff, sizeof(catbuff), indexpath, "sindex.html"), "wb");
            if (fp) {
              char current_word[KEYW_LEN + 32];
              char word[KEYW_LEN + 32];
              int hit;
              int total_hit = 0;
              int total_line = 0;
              int last_pos = 0;
              char word0 = '\0';

              current_word[0] = '\0';

              if (mode == 2) {  // HTML
                for(i = 0; i < index; i++) {
                  if (word0 != tab[i][0]) {
                    word0 = tab[i][0];
                    fprintf(fp, " <a href=\"#%c\">%c</a>\r\n", word0, word0);
                  }
                }
                word0 = '\0';
                fprintf(fp, "<br><br>\r\n");
                fprintf(fp,
                        "<table width=\"100%%\" border=\"0\">\r\n<tr>\r\n<td>word</td>\r\n<td>location\r\n");
              }

              for(i = 0; i < index; i++) {
                if (sscanf(tab[i], "%s %d", word, &hit) == 2) {
                  char *a = strchr(tab[i], ' ');

                  if (a)
                    a = strchr(a + 1, ' ');
                  if (a++) {    /* Yes, a++, not ++a :) */
                    hit = KEYW_SORT_MAXCOUNT - hit;
                    if (strcmp(word, current_word)) {   /* New word */
                      if (total_hit) {
                        if (mode == 1)  // TEXT
                          fprintf(fp, "\t=%d\r\n", total_hit);
                        //else                // HTML
                        //  fprintf(fp,"<br>(%d total hits)\r\n",total_hit);
                        if ((((total_hit * 1000) / hts_primindex_words) >=
                             KEYW_USELESS1K)
                            || (((total_line * 1000) / index) >=
                                KEYW_USELESS1KPG)
                          ) {
                          fseek(fp, last_pos, SEEK_SET);
                          if (mode == 1)        // TEXT
                            fprintf(fp, "\tignored (%d)\r\n",
                                    ((total_hit * 1000) / hts_primindex_words));
                          else
                            fprintf(fp, "(ignored) [%d hits]<br>\r\n",
                                    total_hit);
                        } else {
                          if (mode == 1)        // TEXT
                            fprintf(fp, "\t(%d)\r\n",
                                    ((total_hit * 1000) / hts_primindex_words));
                          //else                // HTML
                          //  fprintf(fp,"(%d)\r\n",((total_hit*1000)/hts_primindex_words));
                        }
                      }
                      if (mode == 1)    // TEXT
                        fprintf(fp, "%s\r\n", word);
                      else {    // HTML
                        fprintf(fp, "</td></tr>\r\n");
                        if (word0 != word[0]) {
                          word0 = word[0];
                          fprintf(fp, "<th>%c</th>\r\n", word0);
                          fprintf(fp, "<a name=\"%c\"></a>\r\n", word0);
                        }
                        fprintf(fp, "<tr>\r\n<td>%s</td>\r\n<td>\r\n", word);
                      }
                      fflush(fp);
                      last_pos = ftell(fp);
                      strcpybuff(current_word, word);
                      total_hit = total_line = 0;
                    }
                    total_hit += hit;
                    total_line++;
                    if (mode == 1)      // TEXT
                      fprintf(fp, "\t%d %s\r\n", hit, a);
                    else        // HTML
                      fprintf(fp, "<a href=\"%s\">%s</a> [%d hits]<br>\r\n", a,
                              a, hit);
                  }
                }
              }
              if (mode == 2)    // HTML
                fprintf(fp, "</td></tr>\r\n</table>\r\n");
              fclose(fp);
            }

          }
          freet(blk);
        }
        freet(tab);
      }

    }
    //qsort
  }
  if (fp_tmpproject)
    fclose(fp_tmpproject);
  fp_tmpproject = NULL;
#endif
}

/* Subroutines */

#if HTS_MAKE_KEYWORD_INDEX
int strcpos(const char *adr, char c) {
  const char *apos = strchr(adr, c);

  if (apos)
    return (int) (apos - adr);
  else
    return -1;
}

int mystrcmp(const void *_e1, const void *_e2) {
  const char *const*const e1 = (const char *const*) _e1;
  const char *const*const e2 = (const char *const*) _e2;

  return strcmp(*e1, *e2);
}
#endif
