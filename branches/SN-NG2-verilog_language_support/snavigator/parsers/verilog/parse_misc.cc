/*
 * Copyright (c) 1998-1999 Stephen Williams (steve@icarus.com)
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
#ident "$Id: parse_misc.cc,v 1.1 2000/12/21 21:57:13 jrandrews Exp $"
#endif

# include  "parse_misc.h"
# include  <iostream.h>
#include "snptools.h"

extern const char*vl_file;
unsigned error_count = 0;
unsigned warn_count = 0;

void VLerror(const char*msg)
{
      error_count += 1;
      cerr << sn_current_file() << yylloc.text << ":" << yylloc.first_line << ": " << msg << endl;
}

void VLerror(const YYLTYPE&loc, const char*msg)
{
      error_count += 1;
      if (loc.text)
	    cerr << loc.text << ":";

      cerr << loc.first_line << ": " << msg << endl;
}

void yywarn(const YYLTYPE&loc, const char*msg)
{
      warn_count += 1;
      if (loc.text)
	    cerr << loc.text << ":";

      cerr << loc.first_line << ": warning: " << msg << endl;
}

int VLwrap()
{
      return -1;
}

/*
 * $Log: parse_misc.cc,v $
 * Revision 1.1  2000/12/21 21:57:13  jrandrews
 * initial import
 *
 * Revision 1.4  2000/02/23 02:56:55  steve
 *  Macintosh compilers do not support ident.
 *
 * Revision 1.3  1999/09/29 21:15:31  steve
 *  Standardize formatting of warning messages.
 *
 * Revision 1.2  1998/11/07 17:05:05  steve
 *  Handle procedural conditional, and some
 *  of the conditional expressions.
 *
 *  Elaborate signals and identifiers differently,
 *  allowing the netlist to hold signal information.
 *
 * Revision 1.1  1998/11/03 23:29:02  steve
 *  Introduce verilog to CVS.
 *
 */

