/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) Xavier Roche and other contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 3
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
/* File: httrack.c subroutines:                                 */
/*       robots.txt (website robot file)                        */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

/* specific definitions */
#include "htscore.h"
#include "htsbase.h"
#include "htslib.h"
/* END specific definitions */

#include "htsrobots.h"

// -- robots --

// fil="" : vérifier si règle déja enregistrée
int checkrobots(robots_wizard* robots,char* adr,char* fil) {
  while(robots) {
    if (strfield2(robots->adr,adr)) {
      if (fil[0]) {
        int ptr=0;
        char line[250];
        if (strnotempty(robots->token)) {
          do {
            ptr+=binput(robots->token+ptr,line,200);
            if (line[0]=='/') {    // absolu
              if (strfield(fil,line)) {                 // commence avec ligne
                return -1;        // interdit
              }
            } else {    // relatif
              if (strstrcase(fil,line)) {
                return -1;
              }
            }
          } while( (strnotempty(line)) && (ptr<(int) strlen(robots->token)) );
        }
      } else {
        return -1;
      }
    }
    robots=robots->next;
  }
  return 0;
}
int checkrobots_set(robots_wizard* robots,char* adr,char* data) {
  if (((int) strlen(adr)) >= sizeof(robots->adr) - 2) return 0;
  if (((int) strlen(data)) >= sizeof(robots->token) - 2) return 0;
  while(robots) {
    if (strfield2(robots->adr,adr)) {    // entrée existe
      strcpybuff(robots->token,data);
#if DEBUG_ROBOTS
        printf("robots.txt: set %s to %s\n",adr,data);
#endif
      return -1;
    }
    else if (!robots->next) {
      robots->next=(robots_wizard*) calloct(1,sizeof(robots_wizard));
      if (robots->next) {
        robots->next->next=NULL;
        strcpybuff(robots->next->adr,adr);
        strcpybuff(robots->next->token,data);
#if DEBUG_ROBOTS
        printf("robots.txt: new set %s to %s\n",adr,data);
#endif
      }
#if DEBUG_ROBOTS
      else
        printf("malloc error!!\n");
#endif
    }
    robots=robots->next;
  }
  return 0;
}
void checkrobots_free(robots_wizard* robots) {
  if (robots->next) {
    checkrobots_free(robots->next);
    freet(robots->next);
    robots->next=NULL;
  }
}

// -- robots --
