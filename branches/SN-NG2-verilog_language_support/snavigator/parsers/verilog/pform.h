#ifndef __pform_H
#define __pform_H
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
#ident "$Id: pform.h,v 1.1 2000/12/21 21:57:13 jrandrews Exp $"
#endif

# include  "netlist.h"
# include  "named.h"
# include  "Module.h"
# include  "Statement.h"
# include  "PGate.h"
# include  "PExpr.h"
# include  "PTask.h"
# include  "PUdp.h"
# include  "PWire.h"
# include  "verinum.h"
# include  <iostream.h>
# include  <string>
# include  <list>
# include  <stdio.h>

/*
 * These classes implement the parsed form (P-form for short) of the
 * original verilog source. the parser generates the pform for the
 * convenience of later processing steps.
 */


/*
 * Wire objects represent the named wires (of various flavor) declared
 * in the source.
 *
 * Gate objects are the functional modules that are connected together
 * by wires.
 *
 * Wires and gates, connected by joints, represent a netlist. The
 * netlist is therefore a representation of the desired circuit.
 */
class PGate;
class PExpr;
struct vlltype;

/*
 * The min:typ:max expression s selected at parse time using the
 * enumeration. When the compiler makes a choise, it also prints a
 * warning if min_typ_max_warn > 0.
 */
extern enum MIN_TYP_MAX { MIN, TYP, MAX } min_typ_max_flag;
extern unsigned min_typ_max_warn;
PExpr* pform_select_mtm_expr(PExpr*min, PExpr*typ, PExpr*max);

/*
 * These type are lexical types -- that is, types that are used as
 * lexical values to decorate the parse tree during parsing. They are
 * not in any way preserved once parsing is done.
 */

/* This is information about port name information for named port
   connections. */

typedef struct named<PExpr*> portname_t;

struct parmvalue_t {
      svector<PExpr*>*by_order;
      svector<portname_t*>*by_name;
};

struct str_pair_t { PGate::strength_t str0, str1; };

/* The lgate is gate instantiation information. */
struct lgate {
      lgate(int =0)
      : parms(0), parms_by_name(0), lineno(0)
      { range[0] = 0;
        range[1] = 0;
      }

      string name;
      svector<PExpr*>*parms;
      svector<portname_t*>*parms_by_name;

      PExpr*range[2];

      string file;
      unsigned lineno;
};

/*
 * The parser uses startmodule and endmodule together to build up a
 * module as it parses it. The startmodule tells the pform code that a
 * module has been noticed in the source file and the following events
 * are to apply to the scope of that module. The endmodule causes the
 * pform to close up and finish the named module.
 */
extern void pform_startmodule(const string&, svector<Module::port_t*>*, unsigned ln);
extern void pform_endmodule(const string&);

extern void pform_make_udp(const char*name, list<string>*parms,
			   svector<PWire*>*decl, list<string>*table,
			   Statement*init);

/*
 * Enter/exit name scopes.
 */
extern void pform_push_scope(const string&name);
extern void pform_pop_scope();

/*
 * The makewire functions announce to the pform code new wires. These
 * go into a module that is currently opened.
 */
extern void pform_makewire(const struct vlltype&li, const string&name,
			   NetNet::Type type = NetNet::IMPLICIT);
extern void pform_makewire(const struct vlltype&li, const list<string>*names,
			   NetNet::Type type);
extern void pform_make_reginit(const struct vlltype&li,
			       const string&name, PExpr*expr);
extern void pform_set_port_type(list<string>*names, NetNet::PortType, int li);
extern void pform_set_net_range(list<string>*names, const svector<PExpr*>*);
extern void pform_set_reg_idx(const string&name, PExpr*l, PExpr*r);
extern void pform_set_reg_integer(list<string>*names);
extern void pform_set_task(const string&, PTask*, unsigned ln);
extern void pform_set_function(const string&, svector<PExpr*>*, PFunction*, unsigned ln);
extern void pform_set_attrib(const string&name, const string&key,
			     const string&value);
extern void pform_set_type_attrib(const string&name, const string&key,
				  const string&value);
extern void pform_set_parameter(const string&name, PExpr*expr);
extern void pform_set_localparam(const string&name, PExpr*expr);
extern void pform_set_defparam(const string&name, PExpr*expr);
extern PProcess*  pform_make_behavior(PProcess::Type, Statement*);

extern svector<PWire*>* pform_make_udp_input_ports(list<string>*);

extern bool pform_expression_is_constant(const PExpr*);

extern void pform_make_events(const list<string>*names,
			      const string&file, unsigned lineno);

/*
 * The makegate function creates a new gate (which need not have a
 * name) and connects it to the specified wires.
 */
extern void pform_makegates(PGBuiltin::Type type,
			    struct str_pair_t str,
			    svector<PExpr*>*delay,
			    svector<lgate>*gates);

extern void pform_make_modgates(const string&type,
				struct parmvalue_t*overrides,
				svector<lgate>*gates);

/* Make a continuous assignment node, with optional bit- or part- select. */
extern PGAssign* pform_make_pgassign(PExpr*lval, PExpr*rval,
				     svector<PExpr*>*delays,
				     struct str_pair_t str);
extern void pform_make_pgassign_list(svector<PExpr*>*alist,
				     svector<PExpr*>*del,
				     struct str_pair_t str,
				     const string& text,
				     unsigned lineno);

/* Given a port type and a list of names, make a list of wires that
   can be used as task port information. */
extern svector<PWire*>*pform_make_task_ports(NetNet::PortType pt,
					     const svector<PExpr*>*range,
					     const list<string>*names,
					     const string& file,
					     unsigned lineno);


/*
 * These are functions that the outside-the-parser code uses the do
 * interesting things to the verilog. The parse function reads and
 * parses the source file and places all the modules it finds into the
 * mod list. The dump function dumps a module to the output stream.
 */
extern int  pform_parse(const char*path, map<string,Module*>&mod,
			map<string,PUdp*>&prim);
extern void pform_dump(ostream&out, Module*mod);

/*
 * $Log: pform.h,v $
 * Revision 1.1  2000/12/21 21:57:13  jrandrews
 * initial import
 *
 * Revision 1.41  2000/07/29 17:58:21  steve
 *  Introduce min:typ:max support.
 *
 * Revision 1.40  2000/05/08 05:30:20  steve
 *  Deliver gate output strengths to the netlist.
 *
 * Revision 1.39  2000/05/06 15:41:57  steve
 *  Carry assignment strength to pform.
 *
 * Revision 1.38  2000/04/01 19:31:57  steve
 *  Named events as far as the pform.
 *
 * Revision 1.37  2000/03/12 17:09:41  steve
 *  Support localparam.
 *
 * Revision 1.36  2000/03/08 04:36:54  steve
 *  Redesign the implementation of scopes and parameters.
 *  I now generate the scopes and notice the parameters
 *  in a separate pass over the pform. Once the scopes
 *  are generated, I can process overrides and evalutate
 *  paremeters before elaboration begins.
 *
 * Revision 1.35  2000/02/23 02:56:55  steve
 *  Macintosh compilers do not support ident.
 *
 * Revision 1.34  2000/01/10 22:16:24  steve
 *  minor type syntax fix for stubborn C++ compilers.
 *
 * Revision 1.33  2000/01/09 05:50:49  steve
 *  Support named parameter override lists.
 *
 * Revision 1.32  1999/12/30 19:06:14  steve
 *  Support reg initial assignment syntax.
 *
 * Revision 1.31  1999/09/10 05:02:09  steve
 *  Handle integers at task parameters.
 *
 * Revision 1.30  1999/08/27 15:08:37  steve
 *  continuous assignment lists.
 *
 * Revision 1.29  1999/08/25 22:22:41  steve
 *  elaborate some aspects of functions.
 *
 * Revision 1.28  1999/08/23 16:48:39  steve
 *  Parameter overrides support from Peter Monta
 *  AND and XOR support wide expressions.
 *
 * Revision 1.27  1999/08/03 04:14:49  steve
 *  Parse into pform arbitrarily complex module
 *  port declarations.
 *
 * Revision 1.26  1999/08/01 16:34:50  steve
 *  Parse and elaborate rise/fall/decay times
 *  for gates, and handle the rules for partial
 *  lists of times.
 *
 * Revision 1.25  1999/07/31 19:14:47  steve
 *  Add functions up to elaboration (Ed Carter)
 *
 * Revision 1.24  1999/07/24 02:11:20  steve
 *  Elaborate task input ports.
 *
 * Revision 1.23  1999/07/10 01:03:18  steve
 *  remove string from lexical phase.
 *
 * Revision 1.22  1999/07/03 02:12:52  steve
 *  Elaborate user defined tasks.
 *
 * Revision 1.21  1999/06/24 04:24:18  steve
 *  Handle expression widths for EEE and NEE operators,
 *  add named blocks and scope handling,
 *  add registers declared in named blocks.
 *
 * Revision 1.20  1999/06/15 03:44:53  steve
 *  Get rid of the STL vector template.
 *
 * Revision 1.19  1999/06/12 20:35:27  steve
 *  parse more verilog.
 *
 * Revision 1.18  1999/06/06 20:45:39  steve
 *  Add parse and elaboration of non-blocking assignments,
 *  Replace list<PCase::Item*> with an svector version,
 *  Add integer support.
 *
 * Revision 1.17  1999/06/02 15:38:46  steve
 *  Line information with nets.
 *
 * Revision 1.16  1999/05/29 02:36:17  steve
 *  module parameter bind by name.
 *
 * Revision 1.15  1999/05/20 04:31:45  steve
 *  Much expression parsing work,
 *  mark continuous assigns with source line info,
 *  replace some assertion failures with Sorry messages.
 *
 * Revision 1.14  1999/05/16 05:08:42  steve
 *  Redo constant expression detection to happen
 *  after parsing.
 *
 *  Parse more operators and expressions.
 *
 * Revision 1.13  1999/05/10 00:16:58  steve
 *  Parse and elaborate the concatenate operator
 *  in structural contexts, Replace vector<PExpr*>
 *  and list<PExpr*> with svector<PExpr*>, evaluate
 *  constant expressions with parameters, handle
 *  memories as lvalues.
 *
 *  Parse task declarations, integer types.
 *
 * Revision 1.12  1999/05/07 04:26:49  steve
 *  Parse more complex continuous assign lvalues.
 *
 * Revision 1.11  1999/05/06 04:37:17  steve
 *  Get rid of list<lgate> types.
 *
 * Revision 1.10  1999/05/06 04:09:28  steve
 *  Parse more constant expressions.
 *
 * Revision 1.9  1999/04/19 01:59:37  steve
 *  Add memories to the parse and elaboration phases.
 *
 * Revision 1.8  1999/02/21 17:01:57  steve
 *  Add support for module parameters.
 *
 * Revision 1.7  1999/02/15 02:06:15  steve
 *  Elaborate gate ranges.
 *
 * Revision 1.6  1999/01/25 05:45:56  steve
 *  Add the LineInfo class to carry the source file
 *  location of things. PGate, Statement and PProcess.
 *
 *  elaborate handles module parameter mismatches,
 *  missing or incorrect lvalues for procedural
 *  assignment, and errors are propogated to the
 *  top of the elaboration call tree.
 *
 *  Attach line numbers to processes, gates and
 *  assignment statements.
 *
 * Revision 1.5  1998/12/09 04:02:47  steve
 *  Support the include directive.
 *
 * Revision 1.4  1998/12/01 00:42:14  steve
 *  Elaborate UDP devices,
 *  Support UDP type attributes, and
 *  pass those attributes to nodes that
 *  are instantiated by elaboration,
 *  Put modules into a map instead of
 *  a simple list.
 *
 * Revision 1.3  1998/11/25 02:35:53  steve
 *  Parse UDP primitives all the way to pform.
 *
 * Revision 1.2  1998/11/23 00:20:23  steve
 *  NetAssign handles lvalues as pin links
 *  instead of a signal pointer,
 *  Wire attributes added,
 *  Ability to parse UDP descriptions added,
 *  XNF generates EXT records for signals with
 *  the PAD attribute.
 *
 * Revision 1.1  1998/11/03 23:29:04  steve
 *  Introduce verilog to CVS.
 *
 */
#endif
