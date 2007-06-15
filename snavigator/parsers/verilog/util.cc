/*
 * Copyright (c) 2000 Stephen Williams (steve@icarus.com)
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
#ident "$Id: util.cc,v 1.1 2000/12/21 21:57:13 jrandrews Exp $"
#endif

# include  "util.h"

string parse_first_name(string&path)
{
      unsigned pos = path.find('.');
      if (pos > path.length()) {
	    string res = path;
	    path = "";
	    return res;
      }

      string res = path.substr(0, pos);
      path = path.substr(pos+1, path.length());
      return res;
}

/*
 * $Log: util.cc,v $
 * Revision 1.1  2000/12/21 21:57:13  jrandrews
 * initial import
 *
 * Revision 1.1  2000/04/28 16:50:53  steve
 *  Catch memory word parameters to tasks.
 *
 */

