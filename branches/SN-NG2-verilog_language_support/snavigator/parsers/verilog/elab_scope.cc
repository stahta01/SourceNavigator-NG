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
#ident "$Id: elab_scope.cc,v 1.1 2000/12/21 21:57:13 jrandrews Exp $"
#endif

/*
 * Elaboration happens in two passes, generally. The first scans the
 * pform to generate the NetScope tree and attach it to the Design
 * object. The methods in this source file implement the elaboration
 * of the scopes.
 */

# include  "Module.h"
# include  "PEvent.h"
# include  "PExpr.h"
# include  "PGate.h"
# include  "PTask.h"
# include  "PWire.h"
# include  "Statement.h"
# include  "netlist.h"
# include  <typeinfo>

bool Module::elaborate_scope(Design*des, NetScope*scope) const
{
	// Generate all the parameters that this instance of this
	// module introduces to the design. This loop elaborates the
	// parameters, but doesn't evaluate references to
	// parameters. This scan practically locates all the
	// parameters and puts them in the parameter table in the
	// design.

	// No expressions are evaluated, yet. For now, leave them in
	// the pform and just place a NetEParam placeholder in the
	// place of the elaborated expression.

      typedef map<string,PExpr*>::const_iterator mparm_it_t;


	// This loop scans the parameters in the module, and creates
	// stub parameter entries in the scope for the parameter name.

      for (mparm_it_t cur = parameters.begin()
		 ; cur != parameters.end() ;  cur ++) {

	    scope->set_parameter((*cur).first, new NetEParam);
      }

      for (mparm_it_t cur = localparams.begin()
		 ; cur != localparams.end() ;  cur ++) {

	    scope->set_parameter((*cur).first, new NetEParam);
      }


	// Now scan the parameters again, this time elaborating them
	// for use as parameter values. This is after the previous
	// scan so that local parameter names can be used in the
	// r-value expressions.

      for (mparm_it_t cur = parameters.begin()
		 ; cur != parameters.end() ;  cur ++) {

	    PExpr*ex = (*cur).second;
	    assert(ex);

	    NetExpr*val = ex->elaborate_pexpr(des, scope);
	    val = scope->set_parameter((*cur).first, val);
	    assert(val);
	    delete val;
      }

      for (mparm_it_t cur = localparams.begin()
		 ; cur != localparams.end() ;  cur ++) {

	    PExpr*ex = (*cur).second;
	    assert(ex);

	    NetExpr*val = ex->elaborate_pexpr(des, scope);
	    val = scope->set_parameter((*cur).first, val);
	    assert(val);
	    delete val;
      }

	// Run through the defparams for this module, elaborate the
	// expressions in this context and save the result is a table
	// for later final override.

	// It is OK to elaborate the expressions of the defparam here
	// because Verilog requires that the expressions only use
	// local parameter names. It is *not* OK to do the override
	// here becuase the parameter receiving the assignment may be
	// in a scope not discovered by this pass.

      for (mparm_it_t cur = defparms.begin()
		 ; cur != defparms.end() ;  cur ++ ) {

	    PExpr*ex = (*cur).second;
	    assert(ex);

	    NetExpr*val = ex->elaborate_pexpr(des, scope);
	    if (val == 0) continue;
	    scope->defparams[(*cur).first] = val;
      }


	// Tasks introduce new scopes, so scan the tasks in this
	// module. Create a scope for the task and pass that to the
	// elaborate_scope method of the PTask for detailed
	// processing.

      typedef map<string,PTask*>::const_iterator tasks_it_t;

      for (tasks_it_t cur = tasks_.begin()
		 ; cur != tasks_.end() ;  cur ++ ) {

	    NetScope*task_scope = new NetScope(scope, (*cur).first,
					       NetScope::TASK);
	    (*cur).second->elaborate_scope(des, task_scope);
      }


	// Functions are very similar to tasks, at least from the
	// perspective of scopes. So handle them exactly the same
	// way.

      typedef map<string,PFunction*>::const_iterator funcs_it_t;

      for (funcs_it_t cur = funcs_.begin()
		 ; cur != funcs_.end() ;  cur ++ ) {

	    NetScope*func_scope = new NetScope(scope, (*cur).first,
					       NetScope::FUNC);
	    (*cur).second->elaborate_scope(des, func_scope);
      }


	// Gates include modules, which might introduce new scopes, so
	// scan all of them to create those scopes.

      typedef list<PGate*>::const_iterator gates_it_t;

      for (gates_it_t cur = gates_.begin()
		 ; cur != gates_.end() ;  cur ++ ) {

	    (*cur) -> elaborate_scope(des, scope);
      }


	// initial and always blocks may contain begin-end and
	// fork-join blocks that can introduce scopes. Therefore, I
	// get to scan processes here.

      typedef list<PProcess*>::const_iterator proc_it_t;

      for (proc_it_t cur = behaviors_.begin()
		 ; cur != behaviors_.end() ;  cur ++ ) {

	    (*cur) -> statement() -> elaborate_scope(des, scope);
      }

	// Scan through all the named events in this scope. We do not
	// need anything more then the current scope to do this
	// elaboration, so do it now. This allows for normal
	// elaboration to reference these events.

      for (map<string,PEvent*>::const_iterator et = events.begin()
		 ; et != events.end() ;  et ++ ) {

	    (*et).second->elaborate_scope(des, scope);
      }

      return des->errors == 0;
}

void PGModule::elaborate_scope_mod_(Design*des, Module*mod, NetScope*sc) const
{
	// Missing module instance names have already been rejected.
      assert(get_name() != "");

      string path = sc->name();

	// Check for duplicate scopes. Simply look up the scope I'm
	// about to create, and if I find it then somebody beat me to
	// it.

      if (NetScope*tmp = des->find_scope(path + "." + get_name())) {
	    cerr << get_line() << ": error: Instance/Scope name " <<
		  get_name() << " already used in this context." <<
		  endl;
	    des->errors += 1;
	    return;
      }

	// Create the new scope as a MODULE with my name.
      NetScope*my_scope = new NetScope(sc, get_name(), NetScope::MODULE);
	// Set time units and precision.
      my_scope->time_unit(mod->time_unit);
      my_scope->time_precision(mod->time_precision);
      des->set_precision(mod->time_precision);

	// This call actually arranges for the description of the
	// module type to process this instance and handle parameters
	// and sub-scopes that might occur. Parameters are also
	// created in that scope, as they exist. (I'll override them
	// later.)
      mod->elaborate_scope(des, my_scope);

	// Look for module parameter replacements. This map receives
	// those replacements.

      typedef map<string,PExpr*>::const_iterator mparm_it_t;
      map<string,PExpr*> replace;


	// Positional parameter overrides are matched to parameter
	// names by using the param_names list of parameter
	// names. This is an ordered list of names so the first name
	// is parameter 0, the second parameter 1, and so on.

      if (overrides_) {
	    assert(parms_ == 0);
	    list<string>::const_iterator cur = mod->param_names.begin();
	    for (unsigned idx = 0
		       ;  idx < overrides_->count()
		       ; idx += 1, cur++) {
		  replace[*cur] = (*overrides_)[idx];
	    }

      }

	// Named parameter overrides carry a name with each override
	// so the mapping into the replace list is much easier.
      if (parms_) {
	    assert(overrides_ == 0);
	    for (unsigned idx = 0 ;  idx < nparms_ ;  idx += 1)
		  replace[parms_[idx].name] = parms_[idx].parm;

      }


	// And here we scan the replacements we collected. Elaborate
	// the expression in my context, then replace the sub-scope
	// parameter value with the new expression.

      for (mparm_it_t cur = replace.begin()
		 ; cur != replace.end() ;  cur ++ ) {

	    PExpr*tmp = (*cur).second;
	    NetExpr*val = tmp->elaborate_pexpr(des, sc);
	    val = my_scope->set_parameter((*cur).first, val);
	    assert(val);
	    delete val;
      }
}

/*
 * The isn't really able to create new scopes, but it does create the
 * event name in the current scope, so can be done during the
 * elaborate_scope scan.
 */
void PEvent::elaborate_scope(Design*des, NetScope*scope) const
{
      NetEvent*ev = new NetEvent(name_);
      ev->set_line(*this);
      scope->add_event(ev);
}

void PFunction::elaborate_scope(Design*des, NetScope*scope) const
{
}

void PTask::elaborate_scope(Design*des, NetScope*scope) const
{
      assert(scope->type() == NetScope::TASK);
}


/*
 * The base statement does not have sub-statements and does not
 * introduce any scope, so this is a no-op.
 */
void Statement::elaborate_scope(Design*, NetScope*) const
{
}

/*
 * When I get a behavioral block, check to see if it has a name. If it
 * does, then create a new scope for the statements within it,
 * otherwise use the current scope. Use the selected scope to scan the
 * statements that I contain.
 */
void PBlock::elaborate_scope(Design*des, NetScope*scope) const
{
      NetScope*my_scope = scope;

      if (name_ != "") {
	    my_scope = new NetScope(scope, name_, bl_type_==BL_PAR
				    ? NetScope::FORK_JOIN
				    : NetScope::BEGIN_END);
      }

      for (unsigned idx = 0 ;  idx < list_.count() ;  idx += 1)
	    list_[idx] -> elaborate_scope(des, my_scope);

}

/*
 * The case statement itseof does not introduce scope, but contains
 * other statements that may be named blocks. So scan the case items
 * with the elaborate_scope method.
 */
void PCase::elaborate_scope(Design*des, NetScope*scope) const
{
      assert(items_);
      for (unsigned idx = 0 ;  idx < (*items_).count() ;  idx += 1) {
	    assert( (*items_)[idx] );

	    if (Statement*sp = (*items_)[idx]->stat)
		  sp -> elaborate_scope(des, scope);
      }
}

/*
 * The conditional statement (if-else) does not introduce scope, but
 * the statements of the clauses may, so elaborate_scope the contained
 * statements.
 */
void PCondit::elaborate_scope(Design*des, NetScope*scope) const
{
      if (if_)
	    if_ -> elaborate_scope(des, scope);

      if (else_)
	    else_ -> elaborate_scope(des, scope);
}

/*
 * Statements that contain a further statement but do not
 * intrinsically add a scope need to elaborate_scope the contained
 * statement.
 */
void PDelayStatement::elaborate_scope(Design*des, NetScope*scope) const
{
      if (statement_)
	    statement_ -> elaborate_scope(des, scope);
}

/*
 * Statements that contain a further statement but do not
 * intrinsically add a scope need to elaborate_scope the contained
 * statement.
 */
void PEventStatement::elaborate_scope(Design*des, NetScope*scope) const
{
      if (statement_)
	    statement_ -> elaborate_scope(des, scope);
}

/*
 * Statements that contain a further statement but do not
 * intrinsically add a scope need to elaborate_scope the contained
 * statement.
 */
void PForever::elaborate_scope(Design*des, NetScope*scope) const
{
      if (statement_)
	    statement_ -> elaborate_scope(des, scope);
}

/*
 * Statements that contain a further statement but do not
 * intrinsically add a scope need to elaborate_scope the contained
 * statement.
 */
void PForStatement::elaborate_scope(Design*des, NetScope*scope) const
{
      if (statement_)
	    statement_ -> elaborate_scope(des, scope);
}

/*
 * Statements that contain a further statement but do not
 * intrinsically add a scope need to elaborate_scope the contained
 * statement.
 */
void PRepeat::elaborate_scope(Design*des, NetScope*scope) const
{
      if (statement_)
	    statement_ -> elaborate_scope(des, scope);
}

/*
 * Statements that contain a further statement but do not
 * intrinsically add a scope need to elaborate_scope the contained
 * statement.
 */
void PWhile::elaborate_scope(Design*des, NetScope*scope) const
{
      if (statement_)
	    statement_ -> elaborate_scope(des, scope);
}


/*
 * $Log: elab_scope.cc,v $
 * Revision 1.1  2000/12/21 21:57:13  jrandrews
 * initial import
 *
 * Revision 1.6  2000/07/30 18:25:43  steve
 *  Rearrange task and function elaboration so that the
 *  NetTaskDef and NetFuncDef functions are created during
 *  signal enaboration, and carry these objects in the
 *  NetScope class instead of the extra, useless map in
 *  the Design class.
 *
 * Revision 1.5  2000/07/22 22:09:03  steve
 *  Parse and elaborate timescale to scopes.
 *
 * Revision 1.4  2000/04/09 17:44:30  steve
 *  Catch event declarations during scope elaborate.
 *
 * Revision 1.3  2000/03/12 17:09:41  steve
 *  Support localparam.
 *
 * Revision 1.2  2000/03/11 03:25:52  steve
 *  Locate scopes in statements.
 *
 * Revision 1.1  2000/03/08 04:36:53  steve
 *  Redesign the implementation of scopes and parameters.
 *  I now generate the scopes and notice the parameters
 *  in a separate pass over the pform. Once the scopes
 *  are generated, I can process overrides and evalutate
 *  paremeters before elaboration begins.
 *
 */

