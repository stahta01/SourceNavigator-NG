#ifndef __PTask_H
#define __PTask_H
/*
 * Copyright (c) 1999-2000 Stephen Williams (steve@icarus.com)
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
#ident "$Id: PTask.h,v 1.1 2000/12/21 21:57:12 jrandrews Exp $"
#endif

# include  "LineInfo.h"
# include  "svector.h"
# include  <string>
class Design;
class NetScope;
class PWire;
class Statement;

/*
 * The PTask holds the parsed definitions of a task.
 */
class PTask  : public LineInfo {

    public:
      explicit PTask(svector<PWire*>*p, Statement*s);
      ~PTask();

	// Tasks introduce scope, to need to be handled during the
	// scope elaboration pass. The scope passed is my scope,
	// created by the containing scope. I fill it in with stuff if
	// I need to.
      void elaborate_scope(Design*des, NetScope*scope) const;

	// Bind the ports to the regs that are the ports.
      void elaborate_sig(Design*des, NetScope*scope) const;

	// Elaborate the statement to finish off the task definition.
      void elaborate(Design*des, const string&path) const;

      void dump(ostream&, unsigned) const;

    private:
      svector<PWire*>*ports_;
      Statement*statement_;

    private: // Not implemented
      PTask(const PTask&);
      PTask& operator=(const PTask&);
};

/*
 * The function is similar to a task (in this context) but there is a
 * single output port and a set of input ports. The output port is the
 * function return value.
 */
class PFunction : public LineInfo {

    public:
      explicit PFunction(svector<PWire *>*p, Statement *s);
      ~PFunction();

      void set_output(PWire*);

      void elaborate_scope(Design*des, NetScope*scope) const;

	/* elaborate the ports and return value. */
      void elaborate_sig(Design *des, NetScope*) const;

	/* Elaborate the behavioral statement. */
      void elaborate(Design *des, NetScope*) const;

      void dump(ostream&, unsigned) const;

    private:
      PWire*out_;
      svector<PWire *> *ports_;
      Statement *statement_;
};

/*
 * $Log: PTask.h,v $
 * Revision 1.1  2000/12/21 21:57:12  jrandrews
 * initial import
 *
 * Revision 1.9  2000/07/30 18:25:43  steve
 *  Rearrange task and function elaboration so that the
 *  NetTaskDef and NetFuncDef functions are created during
 *  signal enaboration, and carry these objects in the
 *  NetScope class instead of the extra, useless map in
 *  the Design class.
 *
 * Revision 1.8  2000/03/08 04:36:53  steve
 *  Redesign the implementation of scopes and parameters.
 *  I now generate the scopes and notice the parameters
 *  in a separate pass over the pform. Once the scopes
 *  are generated, I can process overrides and evalutate
 *  paremeters before elaboration begins.
 *
 * Revision 1.7  2000/02/23 02:56:53  steve
 *  Macintosh compilers do not support ident.
 *
 * Revision 1.6  1999/09/30 21:28:34  steve
 *  Handle mutual reference of tasks by elaborating
 *  task definitions in two passes, like functions.
 *
 * Revision 1.5  1999/09/01 20:46:19  steve
 *  Handle recursive functions and arbitrary function
 *  references to other functions, properly pass
 *  function parameters and save function results.
 *
 * Revision 1.4  1999/08/25 22:22:41  steve
 *  elaborate some aspects of functions.
 *
 * Revision 1.3  1999/07/31 19:14:47  steve
 *  Add functions up to elaboration (Ed Carter)
 *
 * Revision 1.2  1999/07/24 02:11:19  steve
 *  Elaborate task input ports.
 *
 * Revision 1.1  1999/07/03 02:12:51  steve
 *  Elaborate user defined tasks.
 *
 */
#endif
