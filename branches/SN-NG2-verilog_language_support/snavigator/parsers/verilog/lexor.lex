
%{
/*
 * Copyright (c) 1998-2000 Stephen Williams (steve@icarus.com)
 *
 *    This source code is free software; you can redistribute it
 *    and/or modify it in source code form under the terms of the GNU
 *    General Public License as published by the Free Software
 *    Foundation; either version 2 of the License, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */
#if !defined(WINNT) && !defined(macintosh)
#ident "$Id: lexor.lex,v 1.1 2000/12/21 21:57:13 jrandrews Exp $"
#endif

      //# define YYSTYPE lexval

# include  <iostream.h>
# include  "compiler.h"
# include  "parse_misc.h"
# include  "parse.h"
# include  <ctype.h>
# include  <string.h>
# include  "lexor_keyword.h"

#include "snptools.h"

#define MAX_INCLUDE_DEPTH 10
struct include_stack_t {
       YY_BUFFER_STATE include_state;
       int  line_number;
       char file_name[512];
};


static struct include_stack_t  include_stack[MAX_INCLUDE_DEPTH];
int include_stack_ptr = 0;

FILE  *include_fptr = (FILE *) NULL;

extern FILE*vl_input;
extern string vl_file;

# define YY_USER_INIT reset_lexor();
# define yylval VLlval
extern YYLTYPE yylloc;

extern void pform_set_timescale(int, int);

void reset_lexor();
static void line_directive();
static void line_directive2();

static verinum*make_sized_binary(const char*txt);
static verinum*make_sized_dec(const char*txt);
static verinum*make_unsized_dec(const char*txt);
static verinum*make_sized_octal(const char*txt);
static verinum*make_sized_hex(const char*txt);
static verinum*make_unsized_binary(const char*txt);
static verinum*make_unsized_dec(const char*txt);
static verinum*make_unsized_octal(const char*txt);
static verinum*make_unsized_hex(const char*txt);

static void process_timescale(const char*txt);

static int comment_enter;
static int ifdef_enter;
static char inc_filename[512];

#define MAX_INCLUDE_DEPTH 10



%}

%x CCOMMENT
%x LCOMMENT
%x CSTRING
%s UDPTABLE
%x PPTIMESCALE
%x IFDEF


W [ \t\b\f\r]+

%%

^"#line"[ ]+\"[^\"]*\"[ ]+[0-9]+.* { line_directive(); }
^"`line"[ ]+[0-9]+[ ]+\"[^\"]*\".* { line_directive2(); }

[ \t\b\f\r] { ; }
\n { yylloc.first_line += 1; }

"//".* { comment_enter = YY_START; BEGIN(LCOMMENT); }
<LCOMMENT>.    { yymore(); }
<LCOMMENT>\n   { yylloc.first_line += 1; BEGIN(comment_enter); }

"/*" { comment_enter = YY_START; BEGIN(CCOMMENT); }
<CCOMMENT>.    { yymore(); }
<CCOMMENT>\n   { yylloc.first_line += 1; yymore(); }
<CCOMMENT>"*/" { BEGIN(comment_enter); }

"<<" { return K_LS; }
">>" { return K_RS; }
"<=" { return K_LE; }
">=" { return K_GE; }
"=>" { return K_EG; }
"*>" { return K_SG; }
"==" { return K_EQ; }
"!=" { return K_NE; }
"===" { return K_CEQ; }
"!==" { return K_CNE; }
"||" { return K_LOR; }
"&&" { return K_LAND; }
"~|" { return K_NOR; }
"~^" { return K_NXOR; }
"^~" { return K_NXOR; }
"~&" { return K_NAND; }
"->" { return K_TRIGGER; }


[}{;:\[\],()#=.@&!?<>%|^~+*/-] { return yytext[0]; }

\"            { BEGIN(CSTRING); }
<CSTRING>\\\" { yymore(); }
<CSTRING>\n   { BEGIN(0);
                yylval.text = strdup(yytext);
		VLerror(yylloc, "Missing close quote of string.");
		yylloc.first_line += 1;
		return STRING; }
<CSTRING>\"   { BEGIN(0);
                yylval.text = strdup(yytext);
		yylval.text[strlen(yytext)-1] = 0;
		return STRING; }
<CSTRING>.    { yymore(); }

<UDPTABLE>\(\?0\)    { return '_'; }
<UDPTABLE>\(\?1\)    { return '+'; }
<UDPTABLE>\(\?[xX]\) { return '%'; }
<UDPTABLE>\(\?\?\)  { return '*'; }
<UDPTABLE>\(01\)    { return 'r'; }
<UDPTABLE>\(0[xX]\) { return 'P'; }
<UDPTABLE>\(10\)    { return 'f'; }
<UDPTABLE>\(1[xX]\) { return 'N'; }
<UDPTABLE>\([xX]0\) { return 'F'; }
<UDPTABLE>\([xX]1\) { return 'R'; }
<UDPTABLE>[bB]     { return 'b'; }
<UDPTABLE>[fF]     { return 'f'; }
<UDPTABLE>[rR]     { return 'r'; }
<UDPTABLE>[xX]     { return 'x'; }
<UDPTABLE>[pPnN01\?\*\-] { return yytext[0]; }

[a-zA-Z_][a-zA-Z0-9$_]* {
      int rc = lexor_keyword_code(yytext, yyleng);
      if (rc == IDENTIFIER)
	    yylval.text = strdup(yytext);
      else
	    yylval.text = 0;

      return rc;
 }


[a-zA-Z_][a-zA-Z0-9$_]*(\.[`a-zA-Z_][a-zA-Z0-9$_]*)+ {
      yylval.text = strdup(yytext);
      return HIDENTIFIER; }

\\[^ \t\b\f\r]+         {
      yylval.text = strdup(yytext);
      FILE *t;
      return IDENTIFIER; }

\$([a-zA-Z0-9$_]+)        {
      if (strcmp(yytext,"$attribute") == 0)
	    return KK_attribute;
      yylval.text = strdup(yytext);
      return SYSTEM_IDENTIFIER; }

\.{W}?[a-zA-Z_][a-zA-Z0-9$_]* {
      char*cp = yytext+1;
      while (! (isalpha(*cp) || (*cp == '_')))
	    cp += 1;
      yylval.text = strdup(cp);
      return PORTNAME; }

[0-9][0-9_]*[ \t]*\'[sS]?[dD][ \t]*[0-9][0-9_]* {
      yylval.number = make_sized_dec(yytext);
      return NUMBER; }
[0-9][0-9_]*[ \t]*\'[sS]?[bB][ \t]*[0-1xzXZ_\?]+ {
      yylval.number = make_sized_binary(yytext);
      return NUMBER; }
[0-9][0-9_]*[ \t]*\'[sS]?[oO][ \t]*[0-7xzXZ_\?]+ {
      yylval.number = make_sized_octal(yytext);
      return NUMBER; }
[0-9][0-9_]*[ \t]*\'[sS]?[hH][ \t]*[0-9a-fA-FxzXZ_\?]+ {
      yylval.number = make_sized_hex(yytext);
      return NUMBER; }

\'[sS]?[dD][ \t]*[0-9][0-9_]*  { yylval.number = make_unsized_dec(yytext);
                            return NUMBER; }
\'[sS]?[bB][ \t]*[0-1xzXZ_\?]+ { yylval.number = make_unsized_binary(yytext);
                        return NUMBER; }
\'[sS]?[oO][ \t]*[0-7xzXZ_\?]+ { yylval.number = make_unsized_octal(yytext);
                        return NUMBER; }
\'[sS]?[hH][ \t]*[0-9a-fA-FxzXZ_\?]+ { yylval.number = make_unsized_hex(yytext);
                              return NUMBER; }

[0-9][0-9_]*		 {
	/* Handle the special case of the unsized decimal number. */
      unsigned long value = 0;
      for (const char*cp = yytext ;  *cp ;  cp += 1) {
	    if (*cp != '_')
		  value = 10 * value + (*cp - '0');
      }

      assert(INTEGER_WIDTH <= (8*sizeof(value)));
      unsigned nbits = INTEGER_WIDTH;
      verinum::V*bits = new verinum::V[8 * sizeof value];

      for (unsigned idx = 0 ;  idx < nbits ;  idx += 1, value >>= 1) {
	    bits[idx] = (value&1) ? verinum::V1 : verinum::V0;
      }

      yylval.number = new verinum(bits, nbits, false);
      yylval.number->has_sign(true);
      delete[]bits;
      return NUMBER; }

[0-9][0-9_]*\.[0-9][0-9_]*([Ee][+-]?[0-9][0-9_]*)? {
      yylval.realtime = new verireal(yytext);
      return REALTIME; }

[0-9][0-9_]*[Ee][+-]?[0-9][0-9_]* {
      yylval.realtime = new verireal(yytext);
      return REALTIME; }


  /* Notice and handle the timescale directive. */

`timescale { BEGIN(PPTIMESCALE); }
<PPTIMESCALE>.* { process_timescale(yytext); }
<PPTIMESCALE>\n {
      yylloc.first_line += 1;
      BEGIN(0); }


  /* These are directives that I do not yet support. I think that IVL
     should handle these, not an external preprocessor. */

^`celldefine{W}?.*           {  }
^`default_nettype{W}?.*      {  }
^`delay_mode_distributed{W}?.*  {  }
^`delay_mode_unit{W}?.*      {  }
^`delay_mode_path{W}?.*      {  }
^`disable_portfaults{W}?.*   {  }
^`enable_portfaults{W}?.*    {  }
^`endcelldefine{W}?.*        {  }
^`endprotect{W}?.*           {  }
^`nosuppress_faults{W}?.*    {  }
^`nounconnected_drive{W}?.*  {  }
^`protect{W}?.*              {  }
^`resetall{W}?.*             {  }
^`suppress_faults{W}?.*      {  }
^`unconnected_drive{W}?.*    {  }
^`uselib{W}?.*               {  }


  /* These are directives that are not supported by me and should have
     been handled by an external preprocessor such as ivlpp. */

`define{W}?.* { 
      static char def_name[256];
      char *ws;
      
      for (ws = yytext; isspace(*ws); ws++);
      sscanf(ws, "`define %s", def_name);

      sn_insert_symbol(SN_MACRO_DEF, NULL, def_name, sn_current_file(),
              yylloc.first_line, 0, yylloc.first_line, 0,
              0, NULL, NULL, NULL, NULL,
              yylloc.first_line, 0, yylloc.first_line, 0);


      /*cerr << yylloc.text << ":" << yylloc.first_line <<
	    ": `define not supported. Use an external preprocessor."
	   << endl;*/
  }


^{W}?`else{W}?.* {  ifdef_enter = YY_START; BEGIN(IFDEF); }
<IFDEF>. { ; }
<IFDEF>\n	{ yylloc.first_line += 1; }
<IFDEF>^{W}?`endif{W}?.* { BEGIN(ifdef_enter); }

^{W}?`endif{W}?.* {
      /*cerr << yylloc.text << ":" << yylloc.first_line <<
	    ": `endif not supported. Use an external preprocessor."
	   << endl;*/
  }


^{W}?`ifdef{W}?.* {
      /*cerr << yylloc.text << ":" << yylloc.first_line <<
	    ": `ifdef not supported. Use an external preprocessor."
	   << endl;*/
  }

^{W}?`include{W}\".*\"  { 

        char inc_name[256];
        char *p, *q, *ws;
        FILE *bogus;
      
        for (ws = yytext; isspace(*ws); ws++);
        sscanf(ws, "`include %s", inc_name);
        p = inc_name;
        p = p++;
        q = strtok(p,"\"");
        strcpy(inc_filename,q);

      	sn_insert_symbol(SN_INCLUDE_DEF, NULL, inc_filename, sn_current_file(),
                  yylloc.first_line, 0, yylloc.first_line, 0,
                  0, NULL, NULL, NULL, NULL,
                  yylloc.first_line, 0, yylloc.first_line, 0); 
      
 /*      
        if ( include_stack_ptr >= MAX_INCLUDE_DEPTH )
        {
            fprintf( stderr, "Includes nested too deeply" );
            exit(1);
        }
 
        include_stack[include_stack_ptr].line_number = yylloc.first_line;
	strcpy(include_stack[include_stack_ptr].file_name,sn_current_file());
	yylloc.first_line = 1;
        include_stack[include_stack_ptr++].include_state = YY_CURRENT_BUFFER;

        include_fptr = fopen(inc_filename,"r");

        if (!include_fptr)
	{
	    fprintf(stderr, "Cannot open include file %s\n",inc_filename);
	    exit(1);
	}

        yy_switch_to_buffer(yy_create_buffer( include_fptr, YY_BUF_SIZE ) );
 
        bogus = fopen("/dev/null","r");
        sn_register_filename(&bogus, inc_filename);
 */
       
 }

^`undef{W}?.* {
      /*cerr << yylloc.text << ":" << yylloc.first_line <<
	    ": `undef not supported. Use an external preprocessor."
	   << endl;*/
  }

` { ; }   // Eat the ` for macros

 /*
 <<EOF>> {
    FILE *bogus;
    if ( --include_stack_ptr < 0 )
    {
        yyterminate();
    }
    else
    {
        yylloc.first_line = include_stack[include_stack_ptr].line_number;
	bogus = fopen("/dev/null","r");
	sn_register_filename(&bogus, include_stack[include_stack_ptr].file_name);
        yy_delete_buffer( YY_CURRENT_BUFFER );
        yy_switch_to_buffer(include_stack[include_stack_ptr].include_state );

	sn_insert_symbol(SN_INCLUDE_DEF, NULL, inc_filename, sn_current_file(),
                  yylloc.first_line, 0, yylloc.first_line, 0,
                  0, NULL, NULL, NULL, NULL,
                  yylloc.first_line, 0, yylloc.first_line, 0); 
    }
  }
  */
  
  /* Final catchall. something got lost or mishandled. */

. {   
  
      FILE *t;
      t = fopen("snvpp.err","a");
      fprintf(t,"Unmatched character in lexor: %s  in %s at line %d\n",yytext, 
		        sn_current_file(), yylloc.first_line);
      fflush(t);

  }

%%

/*
 * The UDP state table needs some slightly different treatment by the
 * lexor. The level characters are normally accepted as other things,
 * so the parser needs to switch my mode when it believes in needs to.
 */
void lex_start_table()
{
      BEGIN(UDPTABLE);
}

void lex_end_table()
{
      BEGIN(INITIAL);
}

static verinum*make_binary_with_size(unsigned size, bool fixed, const char*ptr)
{
      bool sign_flag = false;
      verinum::V*bits = new verinum::V[size];

      if (tolower(ptr[0]) == 's') {
	    ptr += 1;
	    sign_flag = true;
      }
      assert(tolower(*ptr) == 'b');
      ptr += 1;

      while (*ptr && ((*ptr == ' ') || (*ptr == '\t')))
	    ptr += 1;

      unsigned idx = 0;
      const char*eptr = ptr + strlen(ptr) - 1;
      while ((eptr >= ptr) && (idx < size)) {

	    switch (*eptr) {
		case '_':
		  break;
		case '0':
		  bits[idx++] = verinum::V0;
		  break;
		case '1':
		  bits[idx++] = verinum::V1;
		  break;
		case 'z': case 'Z': case '?':
		  bits[idx++] = verinum::Vz;
		  break;
		case 'x': case 'X':
		  bits[idx++] = verinum::Vx;
		  break;
		default:
		  assert(0);
	    }

	    eptr -= 1;
      }
	// Zero-extend binary number, except that z or x is extended
	// if it is the highest supplied digit.
      while (idx < size) {
	    switch (ptr[0]) {
		case '0':
		case '1':
		  bits[idx++] = verinum::V0;
		  break;
		case 'z': case 'Z': case '?':
		  bits[idx++] = verinum::Vz;
		  break;
		case 'x': case 'X':
		  bits[idx++] = verinum::Vx;
		  break;
		default:
		  assert(0);
	    }
      }

      verinum*out = new verinum(bits, size, fixed);
      delete[]bits;
      out->has_sign(sign_flag);
      return out;
}

static verinum*make_sized_binary(const char*txt)
{
      char*ptr;
      unsigned size = strtoul(txt,&ptr,10);
      while (*ptr && ((*ptr == ' ') || (*ptr == '\t')))
	    ptr += 1;
      assert(*ptr == '\'');
      ptr += 1;

      return make_binary_with_size(size, true, ptr);
}

static verinum*make_unsized_binary(const char*txt)
{
      bool sign_flag = false;
      const char*ptr = txt;
      assert(*ptr == '\'');
      ptr += 1;

      if (tolower(*ptr) == 's') {
	    sign_flag = true;
	    ptr += 1;
      }
      assert(tolower(*ptr) == 'b');
      while (*ptr && ((*ptr == 'b') || (*ptr == ' ') || (*ptr == '\t')))
	    ptr += 1;

      unsigned size = 0;
      for (const char*idx = ptr ;  *idx ;  idx += 1)
	    if (*idx != '_') size += 1;

      verinum::V*bits = new verinum::V[size];

      unsigned idx = size;
      while (*ptr) {
	    switch (ptr[0]) {
		case '0':
		  bits[--idx] = verinum::V0;
		  break;
		case '1':
		  bits[--idx] = verinum::V1;
		  break;
		case 'z': case 'Z': case '?':
		  bits[--idx] = verinum::Vz;
		  break;
		case 'x': case 'X':
		  bits[--idx] = verinum::Vx;
		  break;
		  case '_':
		  break;
		default:
		  fprintf(stderr, "%c\n", ptr[0]);
		  assert(0);
	    }
	    ptr += 1;
      }

      verinum*out = new verinum(bits, size);
      out->has_sign(sign_flag);
      delete[]bits;
      return out;
}

static verinum*make_sized_octal(const char*txt)
{
      bool sign_flag = false;
      char*ptr;

      unsigned size = strtoul(txt,&ptr,10);
      while (*ptr && ((*ptr == ' ') || (*ptr == '\t')))
	    ptr += 1;
      assert(*ptr == '\'');
      ptr += 1;

      if (tolower(*ptr) == 's') {
	    sign_flag = true;
	    ptr += 1;
      }
      assert(tolower(*ptr) == 'o');

	/* We know from the size number how bit to make the verinom
	   array, so make it now. */
      verinum::V*bits = new verinum::V[size];

	/* skip white space between size and the base token. */
      while (*ptr && ((*ptr == ' ') || (*ptr == '\t')))
	    ptr += 1;

	/* Find the end of the digits. ptr already points to the start. */
      char*eptr = ptr + strlen(ptr);

	/* From the last digit and forward, build up the number, least
	   significant bit first. This loop will not get the last few
	   bits if the size is not a multiple of 3. */
      unsigned idx = 0;
      while ((eptr > ptr) && ((idx/3) < (size/3))) switch (*--eptr) {

		case 'x': case 'X':
		  bits[idx++] = verinum::Vx;
		  bits[idx++] = verinum::Vx;
		  bits[idx++] = verinum::Vx;
		  break;

		case 'z': case 'Z': case '?':
		  bits[idx++] = verinum::Vz;
		  bits[idx++] = verinum::Vz;
		  bits[idx++] = verinum::Vz;
		  break;

		default: {
			unsigned val = *eptr - '0';
			bits[idx++] = (val&1)? verinum::V1 : verinum::V0;
			bits[idx++] = (val&2)? verinum::V1 : verinum::V0;
			bits[idx++] = (val&4)? verinum::V1 : verinum::V0;
		  }
      }

      if ((eptr > ptr) && (idx < size)) switch (*--eptr) {

	  case 'x': case 'X':
	    for ( ;  idx < size ;  idx += 1)
		  bits[idx] = verinum::Vx;
	    break;

	  case 'z': case 'Z': case '?':
	    for ( ;  idx < size ;  idx += 1)
		  bits[idx] = verinum::Vz;
	    break;

	  default: {
		  unsigned val = *eptr - '0';
		  for ( ;  idx < size ;  idx += 1) {
			bits[idx] = (val&1)? verinum::V1 : verinum::V0;
			val >>= 1;
		  }
		  break;
	    }

      } else {

	      // zero extend octal numbers
	    while (idx < size) switch (ptr[1]) {
		case 'x': case 'X':
		  bits[idx++] = verinum::Vx;
		  break;
		case 'z': case 'Z': case '?':
		  bits[idx++] = verinum::Vz;
		  break;
		default:
		  bits[idx++] = verinum::V0;
	    }
      }

      verinum*out = new verinum(bits, size, true);
      delete[]bits;
      out->has_sign(sign_flag);
      return out;
}

static verinum*make_unsized_octal(const char*txt)
{
      bool sign_flag = false;
      const char*ptr = txt;
      assert(*ptr == '\'');
      ptr += 1;

      if (tolower(*ptr) == 's') {
	    sign_flag = true;
	    ptr += 1;
      }

      assert(tolower(*ptr) == 'o');
      ptr += 1;

      while (*ptr && ((*ptr == ' ') || (*ptr == '\t')))
	    ptr += 1;

      unsigned size = 0;
      for (const char*idx = ptr ;  *idx ;  idx += 1)
	    if (*idx != '_') size += 3;

      verinum::V*bits = new verinum::V[size];

      unsigned idx = size;
      while (*ptr) {
	    unsigned val;
	    switch (ptr[0]) {
		case '0': case '1': case '2': case '3':
		case '4': case '5': case '6': case '7':
		  val = *ptr - '0';
		  bits[--idx] = (val&4) ? verinum::V1 : verinum::V0;
		  bits[--idx] = (val&2) ? verinum::V1 : verinum::V0;
		  bits[--idx] = (val&1) ? verinum::V1 : verinum::V0;
		  break;
		case 'x': case 'X':
		  bits[--idx] = verinum::Vx;
		  bits[--idx] = verinum::Vx;
		  bits[--idx] = verinum::Vx;
		  break;
		case 'z': case 'Z': case '?':
		  bits[--idx] = verinum::Vz;
		  bits[--idx] = verinum::Vz;
		  bits[--idx] = verinum::Vz;
		  break;
		case '_':
		  break;
		default:
		  assert(0);
	    }
	    ptr += 1;
      }

      verinum*out = new verinum(bits, size);
      out->has_sign(sign_flag);
      delete[]bits;
      return out;
}

static verinum*make_sized_hex(const char*txt)
{
      bool sign_flag = false;
      char*ptr;
      unsigned size = strtoul(txt,&ptr,10);

      while (*ptr && ((*ptr == ' ') || (*ptr == '\t')))
	    ptr += 1;

      assert(*ptr == '\'');
      ptr += 1;

      if (tolower(*ptr) == 's') {
	    sign_flag = true;
	    ptr += 1;
      }

      assert(tolower(*ptr) == 'h');

      ptr += 1;
      while (*ptr && ((*ptr == ' ') || (*ptr == '\t')))
	    ptr += 1;

      verinum::V*bits = new verinum::V[(size+3)&~3];

      unsigned idx = 0;
      char*eptr = ptr + strlen(ptr) - 1;

      while ((eptr >= ptr) && (idx < size)) {
	    switch (*eptr) {
		case 'x': case 'X':
		  bits[idx++] = verinum::Vx;
		  bits[idx++] = verinum::Vx;
		  bits[idx++] = verinum::Vx;
		  bits[idx++] = verinum::Vx;
		  break;
		case 'z': case 'Z': case '?':
		  bits[idx++] = verinum::Vz;
		  bits[idx++] = verinum::Vz;
		  bits[idx++] = verinum::Vz;
		  bits[idx++] = verinum::Vz;
		  break;
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
		    {
			  unsigned val = tolower(*eptr) - 'a' + 10;
			  bits[idx++] = (val&1)? verinum::V1 : verinum::V0;
			  bits[idx++] = (val&2)? verinum::V1 : verinum::V0;
			  bits[idx++] = (val&4)? verinum::V1 : verinum::V0;
			  bits[idx++] = (val&8)? verinum::V1 : verinum::V0;
			  break;
		    }
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
		    {
			  unsigned val = *eptr - '0';
			  bits[idx++] = (val&1)? verinum::V1 : verinum::V0;
			  bits[idx++] = (val&2)? verinum::V1 : verinum::V0;
			  bits[idx++] = (val&4)? verinum::V1 : verinum::V0;
			  bits[idx++] = (val&8)? verinum::V1 : verinum::V0;
			  break;
		    }
		case '_':
		  break;
		default:
		  assert(0);
	    }

	    eptr -= 1;
      }

	// zero extend octal numbers
      while (idx < size) switch (ptr[1]) {
	  case 'x': case 'X':
	    bits[idx++] = verinum::Vx;
	    break;
	  case 'z': case 'Z': case '?':
	    bits[idx++] = verinum::Vz;
	    break;
	  default:
	    bits[idx++] = verinum::V0;
      }

      verinum*out = new verinum(bits, size, true);
      out->has_sign(sign_flag);
      delete[]bits;
      return out;
}

static verinum*make_unsized_hex(const char*txt)
{
      bool sign_flag = false;
      const char*ptr = txt;
      assert(*ptr == '\'');
      ptr += 1;

      if (tolower(*ptr) == 's') {
	    sign_flag = true;
	    ptr += 1;
      }
      assert(tolower(*ptr) == 'h');

      ptr += 1;
      while (*ptr && ((*ptr == ' ') || (*ptr == '\t')))
	    ptr += 1;

      unsigned size = 0;
      for (const char*idx = ptr ;  *idx ;  idx += 1)
	    if (*idx != '_') size += 4;

      verinum::V*bits = new verinum::V[size];

      unsigned idx = size;
      while (*ptr) {
	    unsigned val;
	    switch (ptr[0]) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
		  val = *ptr - '0';
		  bits[--idx] = (val&8) ? verinum::V1 : verinum::V0;
		  bits[--idx] = (val&4) ? verinum::V1 : verinum::V0;
		  bits[--idx] = (val&2) ? verinum::V1 : verinum::V0;
		  bits[--idx] = (val&1) ? verinum::V1 : verinum::V0;
		  break;
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
		  val = tolower(*ptr) - 'a' + 10;
		  bits[--idx] = (val&8) ? verinum::V1 : verinum::V0;
		  bits[--idx] = (val&4) ? verinum::V1 : verinum::V0;
		  bits[--idx] = (val&2) ? verinum::V1 : verinum::V0;
		  bits[--idx] = (val&1) ? verinum::V1 : verinum::V0;
		  break;
		case 'x': case 'X':
		  bits[--idx] = verinum::Vx;
		  bits[--idx] = verinum::Vx;
		  bits[--idx] = verinum::Vx;
		  bits[--idx] = verinum::Vx;
		  break;
		case 'z': case 'Z': case '?':
		  bits[--idx] = verinum::Vz;
		  bits[--idx] = verinum::Vz;
		  bits[--idx] = verinum::Vz;
		  bits[--idx] = verinum::Vz;
		  break;
		case '_':
		  break;
		default:
		  assert(0);
	    }
	    ptr += 1;
      }

      verinum*out = new verinum(bits, size);
      out->has_sign(sign_flag);
      delete[]bits;
      return out;
}

/*
 * Making a decimal number is much easier then the other base numbers
 * because there are no z or x values to worry about.
 */
static verinum*make_dec_with_size(unsigned size, bool fixed, const char*ptr)
{
      bool signed_flag = false;
      if (tolower(*ptr) == 's') {
	    signed_flag = true;
	    ptr += 1;
      }
      assert(tolower(*ptr) == 'd');

      ptr += 1;
      while (*ptr && ((*ptr == ' ') || (*ptr == '\t')))
	    ptr += 1;
      
      unsigned long value = 0;
      for ( ; *ptr ; ptr += 1)
	    if (isdigit(*ptr)) {
		  value *= 10;
		  value += *ptr - '0';
	    } else  {
		  assert(*ptr == '_');
	    }


      verinum::V*bits = new verinum::V[size];

      for (unsigned idx = 0 ;  idx < size ;  idx += 1) {
	    bits[idx] = (value&1)? verinum::V1 : verinum::V0;
	    value /= 2;
      }

      verinum*out = new verinum(bits, size, fixed);
      out->has_sign(signed_flag);
      delete[]bits;
      return out;
}

static verinum*make_sized_dec(const char*txt)
{
      char*ptr;
      unsigned size = strtoul(txt,&ptr,10);

      while (*ptr && ((*ptr == ' ') || (*ptr == '\t')))
	    ptr += 1;

      assert(*ptr == '\'');
      ptr += 1;

      return make_dec_with_size(size, true, ptr);
}

static verinum*make_unsized_dec(const char*txt)
{
      return make_dec_with_size(INTEGER_WIDTH, false, txt+1);
}

/*
 * The timescale parameter has the form:
 *      " <num> xs / <num> xs"
 */
static void process_timescale(const char*txt)
{
      unsigned num;
      const char*cp = txt + strspn(txt, " \t");
      char*tmp;
      const char*ctmp;

      int unit = 0;
      int prec = 0;

      num = strtoul(cp, &tmp, 10);
      if (num == 0) {
	    VLerror(yylloc, "Invalid timescale string.");
	    return;
      }

      while (num >= 10) {
	    unit += 1;
	    num  /= 10;
      }
      if (num != 1) {
	    VLerror(yylloc, "Invalid timescale unit number.");
	    return;
      }

      cp = tmp;
      cp += strspn(cp, " \t");
      ctmp = cp + strcspn(cp, " \t/");

      if (strncmp("s", cp, ctmp-cp) == 0) {
	    unit -= 0;

      } else if (strncmp("ms", cp, ctmp-cp) == 0) {
	    unit -= 3;

      } else if (strncmp("us", cp, ctmp-cp) == 0) {
	    unit -= 6;

      } else if (strncmp("ns", cp, ctmp-cp) == 0) {
	    unit -= 9;

      } else if (strncmp("ps", cp, ctmp-cp) == 0) {
	    unit -= 12;

      } else if (strncmp("fs", cp, ctmp-cp) == 0) {
	    unit -= 15;

      } else {
	    VLerror(yylloc, "Invalid timescale unit of measurement");
	    return;
      }

      cp = ctmp;
      cp += strspn(cp, " \t/");

      num = strtoul(cp, &tmp, 10);
      if (num == 0) {
	    VLerror(yylloc, "Invalid timescale string.");
	    return;
      }
      assert(num);
      while (num >= 10) {
	    prec += 1;
	    num  /= 10;
      }
      if (num != 1) {
	    VLerror(yylloc, "Invalid timescale precision number.");
	    return;
      }

      cp = tmp;
      cp += strspn(cp, " \t");
      ctmp = cp + strcspn(cp, " \t\r");

      if (strncmp("s", cp, ctmp-cp) == 0) {
	    prec -= 0;

      } else if (strncmp("ms", cp, ctmp-cp) == 0) {
	    prec -= 3;

      } else if (strncmp("us", cp, ctmp-cp) == 0) {
	    prec -= 6;

      } else if (strncmp("ns", cp, ctmp-cp) == 0) {
	    prec -= 9;

      } else if (strncmp("ps", cp, ctmp-cp) == 0) {
	    prec -= 12;

      } else if (strncmp("fs", cp, ctmp-cp) == 0) {
	    prec -= 15;

      } else {
	    VLerror(yylloc, "Invalid timescale precision units of measurement");
	    return;
      }

      pform_set_timescale(unit, prec);
}

static int yywrap()
{
      return 1;
}

/*
 * The line directive matches lines of the form #line "foo" N and
 * calls this function. Here I parse out the file name and line
 * number, and change the yylloc to suite.
 */
static void line_directive()
{
      char*qt1 = strchr(yytext, '"');
      assert(qt1);
      qt1 += 1;

      char*qt2 = strchr(qt1, '"');
      assert(qt2);

      char*buf = new char[qt2-qt1+1];
      strncpy(buf, qt1, qt2-qt1);
      buf[qt2-qt1] = 0;

      delete[]yylloc.text;
      yylloc.text = buf;

      qt2 += 1;
      yylloc.first_line = strtoul(qt2,0,0);
}

static void line_directive2()
{
      assert(strncmp(yytext,"`line",5) == 0);
      char*cp = yytext + strlen("`line");
      cp += strspn(cp, " ");
      yylloc.first_line = strtoul(cp,&cp,10);

      cp += strspn(cp, " ");
      if (*cp == 0) return;

      char*qt1 = strchr(yytext, '"');
      assert(qt1);
      qt1 += 1;

      char*qt2 = strchr(qt1, '"');
      assert(qt2);

      char*buf = new char[qt2-qt1+1];
      strncpy(buf, qt1, qt2-qt1);
      buf[qt2-qt1] = 0;

      delete[]yylloc.text;
      yylloc.text = buf;
}

void reset_lexor()
{
     int  i;
     char *ws;
     


      sn_reset_line();
      sn_reset_column();

      yylloc.first_line = 1;
      yylloc.text = strdup(vl_file.c_str());
}
