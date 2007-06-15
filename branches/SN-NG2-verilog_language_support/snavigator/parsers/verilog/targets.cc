/*
 * Copyright (c) 1998 Stephen Williams (steve@icarus.com)
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
#ident "$Id: targets.cc,v 1.1 2000/12/21 21:57:13 jrandrews Exp $"
#endif

# include "target.h"

extern const struct target tgt_null;
//extern const struct target tgt_verilog;
extern const struct target tgt_vvm;
extern const struct target tgt_xnf;

const struct target *target_table[] = {
      &tgt_null,
	//&tgt_verilog,
      &tgt_vvm,
      &tgt_xnf,
      0
};

/*
 * $Log: targets.cc,v $
 * Revision 1.1  2000/12/21 21:57:13  jrandrews
 * initial import
 *
 * Revision 1.5  2000/02/23 02:56:56  steve
 *  Macintosh compilers do not support ident.
 *
 * Revision 1.4  1999/05/01 02:57:53  steve
 *  Handle much more complex event expressions.
 *
 * Revision 1.3  1999/01/24 01:35:36  steve
 *  Support null target for generating no output.
 *
 * Revision 1.2  1998/11/16 05:03:53  steve
 *  Add the sigfold function that unlinks excess
 *  signal nodes, and add the XNF target.
 *
 * Revision 1.1  1998/11/03 23:29:07  steve
 *  Introduce verilog to CVS.
 *
 */

