#ifndef __svector_H
#define __svector_H
/*
 * Copyright (c) 1999 Stephen Williams (steve@icarus.com)
 *
 *    This source code is free software; you can redistribute it
 *    and/or modify it in source code form under the terms of the GNU
 *    General Public License as published by the Free Software
 *    Foundation; either version 2 of the License, or (at your option)
 *    any later version. In order to redistribute the software in
 *    binary form, you will need a Picture Elements Binary Software
 *    License.
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
#ident "$Id: svector.h,v 1.1 2000/12/21 21:57:13 jrandrews Exp $"
#endif

# include  <assert.h>

/*
 * This is a way simplified vector class that cannot grow or shrink,
 * and is really only able to handle values. It is intended to be
 * lighter weight then the STL list class.
 */

template <class TYPE> class svector {

    public:
      explicit svector() : nitems_(0), items_(0) { }

      explicit svector(unsigned size) : nitems_(size), items_(new TYPE[size])
	    { for (unsigned idx = 0 ;  idx < size ;  idx += 1)
		  items_[idx] = 0;
	    }

      svector(const svector<TYPE>&that)
            : nitems_(that.nitems_), items_(new TYPE[nitems_])
	    { for (unsigned idx = 0 ;  idx < that.nitems_ ;  idx += 1)
		    items_[idx] = that[idx];
	    }

      svector(const svector<TYPE>&l, const svector<TYPE>&r)
            : nitems_(l.nitems_ + r.nitems_), items_(new TYPE[nitems_])
	    { for (unsigned idx = 0 ;  idx < l.nitems_ ;  idx += 1)
		    items_[idx] = l[idx];

	      for (unsigned idx = 0 ;  idx < r.nitems_ ;  idx += 1)
		    items_[l.nitems_+idx] = r[idx];
	    }

      svector(const svector<TYPE>&l, TYPE r)
            : nitems_(l.nitems_ + 1), items_(new TYPE[nitems_])
	    { for (unsigned idx = 0 ;  idx < l.nitems_ ;  idx += 1)
		    items_[idx] = l[idx];
	      items_[nitems_-1] = r;
	    }

      ~svector() { delete[]items_; }

      svector<TYPE>& operator= (const svector<TYPE>&that)
	    { if (&that == this) return *this;
	      delete[]items_;
	      nitems_ = that.nitems_;
	      items_ = new TYPE[nitems_];
	      for (unsigned idx = 0 ;  idx < nitems_ ;  idx += 1) {
		    items_[idx] = that.items_[idx];
	      }
	      return *this;
	    }

      unsigned count() const { return nitems_; }

      TYPE&operator[] (unsigned idx)
	    { assert(idx < nitems_);
	      return items_[idx];
	    }

      TYPE operator[] (unsigned idx) const
	    { assert(idx < nitems_);
	      return items_[idx];
	    }

    private:
      unsigned nitems_;
      TYPE* items_;

};

/*
 * $Log: svector.h,v $
 * Revision 1.1  2000/12/21 21:57:13  jrandrews
 * initial import
 *
 * Revision 1.5  2000/02/23 02:56:55  steve
 *  Macintosh compilers do not support ident.
 *
 * Revision 1.4  1999/06/15 03:44:53  steve
 *  Get rid of the STL vector template.
 *
 * Revision 1.3  1999/05/06 04:37:17  steve
 *  Get rid of list<lgate> types.
 *
 * Revision 1.2  1999/05/01 02:57:53  steve
 *  Handle much more complex event expressions.
 *
 * Revision 1.1  1999/04/29 02:16:26  steve
 *  Parse OR of event expressions.
 *
 */
#endif
