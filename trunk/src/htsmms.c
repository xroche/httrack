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
/* File: basic mms protocol manager                             */
/* Author: Xavier Roche                                         */
/*                                                              */
/* The mms routines were written by Nicolas BENOIT,             */
/* based on the work of SDP Multimedia and Major MMS		        */
/* Thanks to all of them!                                       */
/* ------------------------------------------------------------ */

/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

// Gestion protocole mms

#include "htsglobal.h"

#if HTS_USEMMS

#include "htscore.h"

#include "htsmms.h"
#include "mmsrip/mms.h"

#define FTP_STATUS_READY 1001

static int run_launch_mms(MMSDownloadStruct* back);
static void back_launch_mms( void* pP ) {
	MMSDownloadStruct *pStruct = (MMSDownloadStruct*)pP;
  if (pStruct == NULL)
    return ;

	/* Initialize */ 
	hts_init();

	/* Run */
	run_launch_mms(pStruct);

	/* Set as ready */
	{
		lien_back* back = pStruct->pBack;
		back->status=FTP_STATUS_READY;
	}

	/* Delete structure */
	free(pP);

	/* Uninitialize */
  hts_uninit();
  return ;
}

/* download cancelled */
static int stop_mms(lien_back* back) {
	if (back->stop_ftp) {
		strcpybuff(back->r.msg, "Cancelled by User");
		back->r.statuscode = STATUSCODE_INVALID;
		return 1;
	}
	return 0;
}

/* Background launch */
void launch_mms(const MMSDownloadStruct* pStruct) {
	MMSDownloadStruct *pCopy = calloc(sizeof(MMSDownloadStruct), 1);
	memcpy(pCopy, pStruct, sizeof(*pCopy));
  hts_newthread(back_launch_mms, (void*) pCopy);
}

/* Code mainly written by Nicolas BENOIT */
static int run_launch_mms(MMSDownloadStruct* pStruct) {
	lien_back* back = pStruct->pBack;
	httrackp* opt = pStruct->pOpt;
	/* */
	char url[HTS_URLMAXSIZE*2];
	char catbuff[CATBUFF_SIZE];
	char catbuff2[CATBUFF_SIZE];
  MMS *mms;
  FILE *f;
  ssize_t len_written;
	uint64_t total_len_written;
	int delay = opt->mms_maxtime;
	time_t end = time(NULL) + delay;
	short checkPending = 0;
	ssize_t existingSize = fsize(back->url_sav);

	// effacer
	strcpybuff(back->r.msg,"");
	back->status=STATUS_FTP_TRANSFER;
	back->r.statuscode=HTTP_OK;
	back->r.size=0;

	/* Create file */
	if (existingSize > 0) {
		/* back->r.out = fileappend(back->url_sav);
		*/
		(void) unlink(fconcat(catbuff,back->url_sav, ".old"));
		if (rename(fconcat(catbuff,back->url_sav, ""), fconcat(catbuff2,back->url_sav, ".old")) == 0) {
			checkPending = 1;
		}
		back->r.out = filecreate(&pStruct->pOpt->state.strc, back->url_sav);
	} else {
		back->r.out = filecreate(&pStruct->pOpt->state.strc, back->url_sav);
	}
	if ((f = back->r.out) != NULL) {
		// create mms resource
		strcpybuff(url, back->url_adr);				/* mms:// */
		strcatbuff(url, back->url_fil);
		if ( ( mms = mms_create( url, f, NULL, 0, 1 ) ) != NULL ) {
			if ( mms_connect ( mms ) == 0 ) {
				if ( mms_handshake ( mms ) == 0 ) {
					if ( ( len_written = mms_write_stream_header ( mms ) ) != -1 ) {
						total_len_written = len_written;
						HTS_STAT.HTS_TOTAL_RECV += len_written;

						/* not modified */
						if (checkPending) {
							if (mms->is_live != MMS_LIVE
								&& mms->expected_file_size == existingSize + 50	/* Why 50 additional bytes declared ?? */
								)				// abort download
							{
								fclose(back->r.out);
								f = back->r.out = NULL;
								if (unlink(fconcat(catbuff, back->url_sav, "")) == 0 
									&& rename(fconcat(catbuff, back->url_sav, ".old"), fconcat(catbuff2, back->url_sav, "")) == 0) 
								{
									back->r.notmodified = 1;
									back->r.statuscode = HTTP_OK;
									strcpybuff(back->r.msg, "Not modified");
								} else {
									back->r.statuscode = HTTP_INTERNAL_SERVER_ERROR;
									strcpybuff(back->r.msg, "Unable to rename previous file (not updated)");
								}
							} else {
								(void) unlink(fconcat(catbuff, back->url_sav, ".old"));
							}
						}

						/* begin rip */
						if ( f != NULL && mms_begin_rip ( mms ) == 0 ) {
							if ( mms->is_live != MMS_LIVE ) {
								back->r.totalsize = mms->expected_file_size;
								back->r.totalsize -= 50;	/* Why 50 additional bytes declared ?? */
							} else
								back->r.totalsize = -1;

							/* Start download */
							while ( !stop_mms(back) ) {
								len_written = mms_write_stream_data ( mms );
								if ( len_written == 0 ) {
									break;
								} else if ( len_written == -1 ) {
									back->r.statuscode = HTTP_INTERNAL_SERVER_ERROR;
									strcpybuff(back->r.msg, "Unable to write stream data");
									break;
								}

								total_len_written += len_written;
								back->r.size = total_len_written;
								HTS_STAT.HTS_TOTAL_RECV += len_written;

								fflush ( f );

								if ( delay != 0 && end <= time(NULL) ) {
									delay = -1;
									back->r.statuscode = HTTP_OK;
									strcpybuff(back->r.msg, "Download interrupted");
									break;
								}
							}		// while

							back->r.statuscode = HTTP_OK;			/* Finished */
						} else if (f != NULL) {
							back->r.statuscode = HTTP_INTERNAL_SERVER_ERROR;
							strcpybuff(back->r.msg, "Can not begin ripping");
						}
					} else {
						back->r.statuscode = HTTP_INTERNAL_SERVER_ERROR;
						strcpybuff(back->r.msg, "Can not write stream header");
					}
				} else {
					back->r.statuscode = HTTP_INTERNAL_SERVER_ERROR;
					strcpybuff(back->r.msg, "Can not handshake");
				}
				mms_disconnect ( mms );
			} else {
				back->r.statuscode = HTTP_INTERNAL_SERVER_ERROR;
				strcpybuff(back->r.msg, "Can not connect");
			}
			mms_destroy ( mms );
		} else {
			back->r.statuscode = HTTP_INTERNAL_SERVER_ERROR;
			strcpybuff(back->r.msg, "Can not create mms resource");
		}
	} else {
		back->r.statuscode = HTTP_INTERNAL_SERVER_ERROR;
		strcpybuff(back->r.msg, "Unable to open local output file");
	}
	return 0;
}

#endif
