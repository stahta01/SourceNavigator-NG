/*

Copyright (c) 2000, Red Hat, Inc.

This file is part of Source-Navigator.

Source-Navigator is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as published
by the Free Software Foundation; either version 2, or (at your option)
any later version.

Source-Navigator is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License along
with Source-Navigator; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330, Boston,
MA 02111-1307, USA.



*/

/*
 * skeleton.c
 *
 * Copyright (C) 1998 Cygnus Solutions
 *
 * Description:
 * Startup code for the C and C++ parser.
 */

#include <stdio.h>
#include <stdlib.h>
// #include <tcl.h>
//#include "dbutils.h"
#include "mxdefine.h"
#include "sn.h"
//#include "tcl.h"

#include "parser.h"

#ifdef WIN32
#define  OPEN_MODE   O_RDONLY|O_BINARY
#else
#define  OPEN_MODE   O_RDONLY
#endif /* WIN32 */

/* Tcl encoding to translate from. The default (when equal to NULL) is to
   do no translation. */
//Tcl_Encoding encoding = NULL;

extern   int yyfd;

void start_parser(char *fname,int parse_cplpl,FILE *highl_fp,int highlight);
void MacroReadFile(const char *Filename);
void MacroFreeTable();
void free_lex_buffers();
void free_token_buffers();

static int parse_cplpl = TRUE;

extern   int   comment_database;
extern int report_local_vars;
extern FILE *cross_ref_fp;
FILE *hig_fp;
static FILE *out_fp;
static int  highlight;

static int
log_symbol_filename(FILE *fp,char *fname)
{
   char  *outfile = NULL;
   char  *group;

   if (fname == NULL) {
       fprintf(stderr, "log_symbol_filename called with NULL filename\n");
       exit(1);
   }

   if (yyfd != -1)
       close(yyfd);

   yyfd = open(fname,OPEN_MODE);
   if (yyfd == -1)
   {
       fprintf(stderr, "Error: unable to open file \"%s\",errno: %d\n",
               fname,errno);
       return 1;
   }

//    if (highlight)
//    {
//        if (hig_fp)
//        {
//            fclose(hig_fp);
//        }
// 
//        outfile = Paf_tempnam(NULL,"hc");
//        if (fp)
//        {
//            fprintf(fp,"%s\n",outfile);
//        }
// 
//        hig_fp = fopen(outfile,"w+");
//   }
  put_status_parsing_file(fname);

  if (parse_cplpl)
  {
      group = "c++";
  }
  else
  {
      group = "c";
  }
  put_file(fname,group,outfile);

  return 0;
}

int main(int argc, char *argv[])
{
   extern int optind;
   extern char *optarg;
   int   opt, cont;
   char  tmp[MAXPATHLEN];
   char  *fname;
   char  *db_prefix = NULL;
   char  *incl_to_pipe = NULL;
   char  *list_file = NULL;
   char  *include_files = NULL;
   char  *cross_ref_file = NULL;


	/* This part is called when the project is beeing created. */
	for (cont=1;cont<argc;cont++) {
		strcpy(tmp,argv[cont]);
		if (log_symbol_filename(out_fp,tmp) == 0) {
			start_parser(tmp,parse_cplpl,NULL,0);
		}
	}

   if (yyfd != -1)
      close(yyfd);

   if (out_fp)
      fclose(out_fp);

   if (hig_fp && hig_fp != out_fp)
      fclose(hig_fp);


   if (cross_ref_fp)
      fclose(cross_ref_fp);

   free_lex_buffers();
   free_token_buffers();

   return 0;
}

