#ifndef __PUdp_H
#define __PUdp_H
/*
 * Copyright (c) 1998-2000 Stephen Williams (steve@picturel.com)
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
#ident "$Id: PUdp.h,v 1.1 2000/12/21 21:57:12 jrandrews Exp $"
#endif

# include  <map>
# include  "svector.h"
# include  <string>
# include  "verinum.h"

svector<string>::svector<string>(unsigned size)
: nitems_(size), items_(new string[size])
{
}

/*
 * This class represents a parsed UDP. This is a much simpler object
 * then a module or macromodule.
 *
 *   - all ports are scaler,
 *   - pin 0 (the first port) is always output,
 *     and the remaining pins are input.
 *
 * Thus, the ports can be represented as an ordered list of pin names.
 * If the output port is declared as a register in the Verilog source,
 * then this is a sequential UDP and the sequential flag is set to true.
 *
 * STATE TABLE
 * Each entry in the state table is given as a string with the same
 * number of characters as inputs. If the UDP is sequential, a
 * character is also included at the end of the string to represent
 * the current output.
 *
 * If the UDP is sequential, the "initial" member is taken to be the
 * intial value assigned in the source, or 'x' if none is given.
 */
class PUdp {

    public:
      explicit PUdp(const string&n, unsigned nports)
      : ports(nports), sequential(false), initial(verinum::Vx), name_(n) { }

      svector<string>ports;
      bool sequential;

      svector<string>tinput;
      svector<char>  tcurrent;
      svector<char>  toutput;

      verinum::V initial;

      map<string,string> attributes;

      void dump(ostream&out) const;

    private:
      const string name_;

    private: // Not implemented
      PUdp(const PUdp&);
      PUdp& operator= (const PUdp&);
};

/*
 * $Log: PUdp.h,v $
 * Revision 1.1  2000/12/21 21:57:12  jrandrews
 * initial import
 *
 * Revision 1.4  2000/02/23 02:56:53  steve
 *  Macintosh compilers do not support ident.
 *
 * Revision 1.3  1999/06/15 03:44:53  steve
 *  Get rid of the STL vector template.
 *
 * Revision 1.2  1998/12/01 00:42:13  steve
 *  Elaborate UDP devices,
 *  Support UDP type attributes, and
 *  pass those attributes to nodes that
 *  are instantiated by elaboration,
 *  Put modules into a map instead of
 *  a simple list.
 *
 * Revision 1.1  1998/11/25 02:35:53  steve
 *  Parse UDP primitives all the way to pform.
 *
 */
#endif
