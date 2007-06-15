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
#ident "$Id: elaborate.cc,v 1.1 2000/12/21 21:57:13 jrandrews Exp $"
#endif

/*
 * Elaboration takes as input a complete parse tree and the name of a
 * root module, and generates as output the elaborated design. This
 * elaborated design is presented as a Module, which does not
 * reference any other modules. It is entirely self contained.
 */

# include  <typeinfo>
# include  <strstream>
# include  "pform.h"
# include  "PEvent.h"
# include  "netlist.h"
# include  "netmisc.h"
# include  "util.h"

  // Urff, I don't like this global variable. I *will* figure out a
  // way to get rid of it. But, for now the PGModule::elaborate method
  // needs it to find the module definition.
static const map<string,Module*>* modlist = 0;
static const map<string,PUdp*>*   udplist = 0;

static Link::strength_t drive_type(PGate::strength_t drv)
{
      switch (drv) {
	  case PGate::HIGHZ:
	    return Link::HIGHZ;
	  case PGate::WEAK:
	    return Link::WEAK;
	  case PGate::PULL:
	    return Link::PULL;
	  case PGate::STRONG:
	    return Link::STRONG;
	  case PGate::SUPPLY:
	    return Link::SUPPLY;
	  default:
	    assert(0);
      }
      return Link::STRONG;
}


void PGate::elaborate(Design*des, const string&path) const
{
      cerr << "internal error: what kind of gate? " <<
	    typeid(*this).name() << endl;
}

/*
 * Elaborate the continuous assign. (This is *not* the procedural
 * assign.) Elaborate the lvalue and rvalue, and do the assignment.
 */
void PGAssign::elaborate(Design*des, const string&path) const
{
      unsigned long rise_time, fall_time, decay_time;
      eval_delays(des, path, rise_time, fall_time, decay_time);

      Link::strength_t drive0 = drive_type(strength0());
      Link::strength_t drive1 = drive_type(strength1());

      assert(pin(0));
      assert(pin(1));

	/* Elaborate the l-value. */
      NetNet*lval = pin(0)->elaborate_lnet(des, path);
      if (lval == 0) {
	    des->errors += 1;
	    return;
      }


	/* Handle the special case that the rval is simply an
	   identifier. Get the rval as a NetNet, then use NetBUFZ
	   objects to connect it to the l-value. This is necessary to
	   direct drivers. This is how I attach strengths to the
	   assignment operation. */
      if (const PEIdent*id = dynamic_cast<const PEIdent*>(pin(1))) {
	    NetNet*rid = id->elaborate_net(des, path, lval->pin_count(),
					   0, 0, 0, Link::STRONG,
					   Link::STRONG);
	    assert(rid);


	      /* If the right hand net is the same type as the left
		 side net (i.e. WIRE/WIRE) then it is enough to just
		 connect them together. Otherwise, put a bufz between
		 them to carry strengths from the rval */

	    if (rid->type() == lval->type())
		  for (unsigned idx = 0 ;  idx < lval->pin_count(); idx += 1) {
			connect(lval->pin(idx), rid->pin(idx));
		  }

	    else
		  for (unsigned idx = 0 ; idx < lval->pin_count();  idx += 1) {
			NetBUFZ*dev = new NetBUFZ(des->local_symbol(path));
			connect(lval->pin(idx), dev->pin(0));
			connect(rid->pin(idx), dev->pin(1));
			dev->pin(0).drive0(drive0);
			dev->pin(0).drive1(drive1);
			des->add_node(dev);
		  }

	    return;
      }

	/* Elaborate the r-value. Account for the initial decays,
	   which are going to be attached to the last gate before the
	   generated NetNet. */
      NetNet*rval = pin(1)->elaborate_net(des, path,
					  lval->pin_count(),
					  rise_time, fall_time, decay_time,
					  drive0, drive1);
      if (rval == 0) {
	    cerr << get_line() << ": error: Unable to elaborate r-value: "
		 << *pin(1) << endl;
	    des->errors += 1;
	    return;
      }

      assert(lval && rval);

      if (lval->pin_count() > rval->pin_count()) {
	    cerr << get_line() << ": sorry: lval width (" <<
		  lval->pin_count() << ") > rval width (" <<
		  rval->pin_count() << ")." << endl;
	    delete lval;
	    delete rval;
	    des->errors += 1;
	    return;
      }

      for (unsigned idx = 0 ;  idx < lval->pin_count() ;  idx += 1)
	    connect(lval->pin(idx), rval->pin(idx));

      if (lval->local_flag())
	    delete lval;

}

/*
 * Elaborate a Builtin gate. These normally get translated into
 * NetLogic nodes that reflect the particular logic function.
 */
void PGBuiltin::elaborate(Design*des, const string&path) const
{
      unsigned count = 1;
      unsigned low = 0, high = 0;
      string name = get_name();
      if (name == "")
	    name = des->local_symbol(path);
      else
	    name = path+"."+name;

	/* If the verilog source has a range specification for the
	   gates, then I am expected to make more then one
	   gate. Figure out how many are desired. */
      if (msb_) {
	    verinum*msb = msb_->eval_const(des, path);
	    verinum*lsb = lsb_->eval_const(des, path);

	    if (msb == 0) {
		  cerr << get_line() << ": error: Unable to evaluate "
			"expression " << *msb_ << endl;
		  des->errors += 1;
		  return;
	    }

	    if (lsb == 0) {
		  cerr << get_line() << ": error: Unable to evaluate "
			"expression " << *lsb_ << endl;
		  des->errors += 1;
		  return;
	    }

	    if (msb->as_long() > lsb->as_long())
		  count = msb->as_long() - lsb->as_long() + 1;
	    else
		  count = lsb->as_long() - msb->as_long() + 1;

	    low = lsb->as_long();
	    high = msb->as_long();
      }


	/* Allocate all the getlist nodes for the gates. */
      NetLogic**cur = new NetLogic*[count];
      assert(cur);

	/* Calculate the gate delays from the delay expressions
	   given in the source. For logic gates, the decay time
	   is meaningless because it can never go to high
	   impedence. However, the bufif devices can generate
	   'bz output, so we will pretend that anything can.

	   If only one delay value expression is given (i.e. #5
	   nand(foo,...)) then rise, fall and decay times are
	   all the same value. If two values are given, rise and
	   fall times are use, and the decay time is the minimum
	   of the rise and fall times. Finally, if all three
	   values are given, they are taken as specified. */

      unsigned long rise_time, fall_time, decay_time;
      eval_delays(des, path, rise_time, fall_time, decay_time);

	/* Now make as many gates as the bit count dictates. Give each
	   a unique name, and set the delay times. */

      for (unsigned idx = 0 ;  idx < count ;  idx += 1) {
	    strstream tmp;
	    unsigned index;
	    if (low < high)
		  index = low + idx;
	    else
		  index = low - idx;

	    tmp << name << "<" << index << ">" << ends;
	    const string inm = tmp.str();

	    switch (type()) {
		case AND:
		  cur[idx] = new NetLogic(inm, pin_count(), NetLogic::AND);
		  break;
		case BUF:
		  cur[idx] = new NetLogic(inm, pin_count(), NetLogic::BUF);
		  break;
		case BUFIF0:
		  cur[idx] = new NetLogic(inm, pin_count(), NetLogic::BUFIF0);
		  break;
		case BUFIF1:
		  cur[idx] = new NetLogic(inm, pin_count(), NetLogic::BUFIF1);
		  break;
		case NAND:
		  cur[idx] = new NetLogic(inm, pin_count(), NetLogic::NAND);
		  break;
		case NOR:
		  cur[idx] = new NetLogic(inm, pin_count(), NetLogic::NOR);
		  break;
		case NOT:
		  cur[idx] = new NetLogic(inm, pin_count(), NetLogic::NOT);
		  break;
		case OR:
		  cur[idx] = new NetLogic(inm, pin_count(), NetLogic::OR);
		  break;
		case XNOR:
		  cur[idx] = new NetLogic(inm, pin_count(), NetLogic::XNOR);
		  break;
		case XOR:
		  cur[idx] = new NetLogic(inm, pin_count(), NetLogic::XOR);
		  break;
		default:
		  cerr << get_line() << ": internal error: unhandled "
			"gate type." << endl;
		  des->errors += 1;
		  return;
	    }

	    cur[idx]->set_attributes(attributes);
	    cur[idx]->rise_time(rise_time);
	    cur[idx]->fall_time(fall_time);
	    cur[idx]->decay_time(decay_time);

	    cur[idx]->pin(0).drive0(drive_type(strength0()));
	    cur[idx]->pin(0).drive1(drive_type(strength1()));

	    des->add_node(cur[idx]);
      }

	/* The gates have all been allocated, this loop runs through
	   the parameters and attaches the ports of the objects. */

      for (unsigned idx = 0 ;  idx < pin_count() ;  idx += 1) {
	    const PExpr*ex = pin(idx);
	    NetNet*sig = ex->elaborate_net(des, path, 0, 0, 0, 0);
	    if (sig == 0)
		  continue;

	    assert(sig);

	    if (sig->pin_count() == 1)
		  for (unsigned gdx = 0 ;  gdx < count ;  gdx += 1)
			connect(cur[gdx]->pin(idx), sig->pin(0));

	    else if (sig->pin_count() == count)
		  for (unsigned gdx = 0 ;  gdx < count ;  gdx += 1)
			connect(cur[gdx]->pin(idx), sig->pin(gdx));

	    else {
		  cerr << get_line() << ": error: Gate count of " <<
			count << " does not match net width of " <<
			sig->pin_count() << " at pin " << idx << "."
		       << endl;
		  des->errors += 1;
	    }

	    if (NetTmp*tmp = dynamic_cast<NetTmp*>(sig))
		  delete tmp;
      }
}

/*
 * Instantiate a module by recursively elaborating it. Set the path of
 * the recursive elaboration so that signal names get properly
 * set. Connect the ports of the instantiated module to the signals of
 * the parameters. This is done with BUFZ gates so that they look just
 * like continuous assignment connections.
 */
void PGModule::elaborate_mod_(Design*des, Module*rmod, const string&path) const
{
	// Missing module instance names have already been rejected.
      assert(get_name() != "");

      if (msb_) {
	    cerr << get_line() << ": sorry: Module instantiation arrays "
		  "are not yet supported." << endl;
	    des->errors += 1;
	    return;
      }

      NetScope*scope = des->find_scope(path);
      assert(scope);

	// I know a priori that the elaborate_scope created the scope
	// already, so just look it up as a child of the current scope.
      NetScope*my_scope = scope->child(get_name());
      assert(my_scope);

      const svector<PExpr*>*pins;

	// Detect binding by name. If I am binding by name, then make
	// up a pins array that reflects the positions of the named
	// ports. If this is simply positional binding in the first
	// place, then get the binding from the base class.
      if (pins_) {
	    unsigned nexp = rmod->port_count();
	    svector<PExpr*>*exp = new svector<PExpr*>(nexp);

	      // Scan the bindings, matching them with port names.
	    for (unsigned idx = 0 ;  idx < npins_ ;  idx += 1) {

		    // Given a binding, look at the module port names
		    // for the position that matches the binding name.
		  unsigned pidx = rmod->find_port(pins_[idx].name);

		    // If the port name doesn't exist, the find_port
		    // method will return the port count. Detect that
		    // as an error.
		  if (pidx == nexp) {
			cerr << get_line() << ": error: port ``" <<
			      pins_[idx].name << "'' is not a port of "
			     << get_name() << "." << endl;
			des->errors += 1;
			continue;
		  }

		    // If I already bound something to this port, then
		    // the (*exp) array will already have a pointer
		    // value where I want to place this expression.
		  if ((*exp)[pidx]) {
			cerr << get_line() << ": error: port ``" <<
			      pins_[idx].name << "'' already bound." <<
			      endl;
			des->errors += 1;
			continue;
		  }

		    // OK, do the binding by placing the expression in
		    // the right place.
		  (*exp)[pidx] = pins_[idx].parm;
	    }

	    pins = exp;

      } else {

	    if (pin_count() != rmod->port_count()) {
		  cerr << get_line() << ": error: Wrong number "
			"of parameters. Expecting " << rmod->port_count() <<
			", got " << pin_count() << "."
		       << endl;
		  des->errors += 1;
		  return;
	    }

	      // No named bindings, just use the positional list I
	      // already have.
	    assert(pin_count() == rmod->port_count());
	    pins = get_pins();
      }

	// Elaborate this instance of the module. The recursive
	// elaboration causes the module to generate a netlist with
	// the ports represented by NetNet objects. I will find them
	// later.
      rmod->elaborate(des, my_scope);

	// Now connect the ports of the newly elaborated designs to
	// the expressions that are the instantiation parameters. Scan
	// the pins, elaborate the expressions attached to them, and
	// bind them to the port of the elaborated module.

      for (unsigned idx = 0 ;  idx < pins->count() ;  idx += 1) {
	      // Skip unconnected module ports.
	    if ((*pins)[idx] == 0)
		  continue;

	      // Inside the module, the port is one or more signals,
	      // that were already elaborated. List all those signals,
	      // and I will connect them up later.
	    svector<PEIdent*> mport = rmod->get_port(idx);
	    svector<NetNet*>prts (mport.count());

	    unsigned prts_pin_count = 0;
	    for (unsigned ldx = 0 ;  ldx < mport.count() ;  ldx += 1) {
		  PEIdent*pport = mport[ldx];
		  prts[ldx] = pport->elaborate_port(des, my_scope);
		  if (prts[ldx] == 0) {
			cerr << pport->get_line() << ": internal error: "
			     << "Failed to elaborate port expr: "
			     << *pport << endl;
			des->errors += 1;
			continue;
		  }
		  assert(prts[ldx]);
		  prts_pin_count += prts[ldx]->pin_count();
	    }

	    NetNet*sig = (*pins)[idx]->elaborate_net(des, path,
						     prts_pin_count,
						     0, 0, 0);
	    if (sig == 0) {
		  cerr << "internal error: Expression too complicated "
			"for elaboration." << endl;
		  continue;
	    }

	    assert(sig);

	      // Check that the parts have matching pin counts. If
	      // not, they are different widths.
	    if (prts_pin_count != sig->pin_count()) {
		  cerr << get_line() << ": error: Port " << idx << " of "
		       << type_ << " expects " << prts_pin_count <<
			" pins, got " << sig->pin_count() << " from "
		       << sig->name() << endl;
		  des->errors += 1;
		  continue;
	    }

	      // Connect the sig expression that is the context of the
	      // module instance to the ports of the elaborated
	      // module.

	    assert(prts_pin_count == sig->pin_count());
	    for (unsigned ldx = 0 ;  ldx < prts.count() ;  ldx += 1) {
		  for (unsigned p = 0 ;  p < prts[ldx]->pin_count() ; p += 1) {
			prts_pin_count -= 1;
			connect(sig->pin(prts_pin_count),
				prts[ldx]->pin(prts[ldx]->pin_count()-p-1));
		  }
	    }

	    if (NetTmp*tmp = dynamic_cast<NetTmp*>(sig))
		  delete tmp;
      }
}

void PGModule::elaborate_cudp_(Design*des, PUdp*udp, const string&path) const
{
      const string my_name = path+"."+get_name();
      NetUDP_COMB*net = new NetUDP_COMB(my_name, udp->ports.count());
      net->set_attributes(udp->attributes);

	/* Run through the pins, making netlists for the pin
	   expressions and connecting them to the pin in question. All
	   of this is independent of the nature of the UDP. */
      for (unsigned idx = 0 ;  idx < net->pin_count() ;  idx += 1) {
	    if (pin(idx) == 0)
		  continue;

	    NetNet*sig = pin(idx)->elaborate_net(des, path, 1, 0, 0, 0);
	    if (sig == 0) {
		  cerr << "internal error: Expression too complicated "
			"for elaboration:" << *pin(idx) << endl;
		  continue;
	    }

	    connect(sig->pin(0), net->pin(idx));

	      // Delete excess holding signal.
	    if (NetTmp*tmp = dynamic_cast<NetTmp*>(sig))
		  delete tmp;
      }

	/* Build up the truth table for the netlist from the input
	   strings. */
      for (unsigned idx = 0 ;  idx < udp->tinput.count() ;  idx += 1) {
	    string input = udp->tinput[idx];

	    net->set_table(input, udp->toutput[idx]);
      }

      net->cleanup_table();

	// All done. Add the object to the design.
      des->add_node(net);
}

/*
 * From a UDP definition in the source, make a NetUDP
 * object. Elaborate the pin expressions as netlists, then connect
 * those networks to the pins.
 */
void PGModule::elaborate_sudp_(Design*des, PUdp*udp, const string&path) const
{
      const string my_name = path+"."+get_name();
      NetUDP*net = new NetUDP(my_name, udp->ports.count(), udp->sequential);
      net->set_attributes(udp->attributes);

	/* Run through the pins, making netlists for the pin
	   expressions and connecting them to the pin in question. All
	   of this is independent of the nature of the UDP. */
      for (unsigned idx = 0 ;  idx < net->pin_count() ;  idx += 1) {
	    if (pin(idx) == 0)
		  continue;

	    NetNet*sig = pin(idx)->elaborate_net(des, path, 1, 0, 0, 0);
	    if (sig == 0) {
		  cerr << "internal error: Expression too complicated "
			"for elaboration:" << *pin(idx) << endl;
		  continue;
	    }

	    connect(sig->pin(0), net->pin(idx));

	      // Delete excess holding signal.
	    if (NetTmp*tmp = dynamic_cast<NetTmp*>(sig))
		  delete tmp;
      }

	/* Build up the truth table for the netlist from the input
	   strings. */
      for (unsigned idx = 0 ;  idx < udp->tinput.count() ;  idx += 1) {
	    string input = string("") + udp->tcurrent[idx] + udp->tinput[idx];
	    net->set_table(input, udp->toutput[idx]);
      }

      net->cleanup_table();

      switch (udp->initial) {
	  case verinum::V0:
	    net->set_initial('0');
	    break;
	  case verinum::V1:
	    net->set_initial('1');
	    break;
	  case verinum::Vx:
	  case verinum::Vz:
	    net->set_initial('x');
	    break;
      }

	// All done. Add the object to the design.
      des->add_node(net);
}


bool PGModule::elaborate_sig(Design*des, NetScope*scope) const
{
	// Look for the module type
      map<string,Module*>::const_iterator mod = modlist->find(type_);
      if (mod != modlist->end())
	    return elaborate_sig_mod_(des, scope, (*mod).second);

      return true;
}


void PGModule::elaborate(Design*des, const string&path) const
{
	// Look for the module type
      map<string,Module*>::const_iterator mod = modlist->find(type_);
      if (mod != modlist->end()) {
	    elaborate_mod_(des, (*mod).second, path);
	    return;
      }

	// Try a primitive type
      map<string,PUdp*>::const_iterator udp = udplist->find(type_);
      if (udp != udplist->end()) {
	    if ((*udp).second->sequential)
		  elaborate_sudp_(des, (*udp).second, path);
	    else
		  elaborate_cudp_(des, (*udp).second, path);
	    return;
      }

      cerr << get_line() << ": internal error: Unknown module type: " <<
	    type_ << endl;
}

void PGModule::elaborate_scope(Design*des, NetScope*sc) const
{
	// Look for the module type
      map<string,Module*>::const_iterator mod = modlist->find(type_);
      if (mod != modlist->end()) {
	    elaborate_scope_mod_(des, (*mod).second, sc);
	    return;
      }

	// Try a primitive type
      map<string,PUdp*>::const_iterator udp = udplist->find(type_);
      if (udp != udplist->end())
	    return;


      cerr << get_line() << ": error: Unknown module type: " << type_ << endl;
      des->errors += 1;
}

/*
 * The concatenation is also OK an an l-value. This method elaborates
 * it as a structural l-value.
 */
NetNet* PEConcat::elaborate_lnet(Design*des, const string&path) const
{
      NetScope*scope = des->find_scope(path);
      assert(scope);

      svector<NetNet*>nets (parms_.count());
      unsigned pins = 0;
      unsigned errors = 0;

      if (repeat_) {
	    cerr << get_line() << ": sorry: I do not know how to"
		  " elaborate repeat concatenation nets." << endl;
	    return 0;
      }

	/* Elaborate the operands of the concatenation. */
      for (unsigned idx = 0 ;  idx < nets.count() ;  idx += 1) {
	    nets[idx] = parms_[idx]->elaborate_lnet(des, path);
	    if (nets[idx] == 0)
		  errors += 1;
	    else
		  pins += nets[idx]->pin_count();
      }

	/* If any of the sub expressions failed to elaborate, then
	   delete all those that did and abort myself. */
      if (errors) {
	    for (unsigned idx = 0 ;  idx < nets.count() ;  idx += 1) {
		  if (nets[idx]) delete nets[idx];
	    }
	    des->errors += 1;
	    return 0;
      }

	/* Make the temporary signal that connects to all the
	   operands, and connect it up. Scan the operands of the
	   concat operator from least significant to most significant,
	   which is opposite from how they are given in the list. */
      NetNet*osig = new NetNet(scope, des->local_symbol(path),
			       NetNet::IMPLICIT, pins);
      pins = 0;
      for (unsigned idx = nets.count() ;  idx > 0 ;  idx -= 1) {
	    NetNet*cur = nets[idx-1];
	    for (unsigned pin = 0 ;  pin < cur->pin_count() ;  pin += 1) {
		  connect(osig->pin(pins), cur->pin(pin));
		  pins += 1;
	    }
      }

      osig->local_flag(true);
      return osig;
}

NetProc* Statement::elaborate(Design*des, const string&path) const
{
      cerr << get_line() << ": internal error: elaborate: What kind of statement? " <<
	    typeid(*this).name() << endl;
      NetProc*cur = new NetProc;
      des->errors += 1;
      return cur;
}

NetProc* PAssign::assign_to_memory_(NetMemory*mem, PExpr*ix,
				    Design*des, const string&path) const
{
      NetScope*scope = des->find_scope(path);
      assert(scope);
      NetExpr*rv = rval()->elaborate_expr(des, scope);
      if (rv == 0)
	    return 0;

      assert(rv);

      rv->set_width(mem->width());
      if (ix == 0) {
	    cerr << get_line() << ": internal error: No index in lval "
		 << "of assignment to memory?" << endl;
	    return 0;
      }

      assert(ix);
      NetExpr*idx = ix->elaborate_expr(des, scope);
      assert(idx);

      if (rv->expr_width() < mem->width())
	    rv = pad_to_width(rv, mem->width());

      NetAssignMem*am = new NetAssignMem(mem, idx, rv);
      am->set_line(*this);
      return am;
}

/*
 * Elaborate an l-value as a NetNet (it may already exist) and make up
 * the part select stuff for where the assignment is going to be made.
 */
NetNet* PAssign_::elaborate_lval(Design*des, const string&path,
				 unsigned&msb, unsigned&lsb,
				 NetExpr*&mux) const
{
      NetScope*scope = des->find_scope(path);
      assert(scope);

	/* Get the l-value, and assume that it is an identifier. */
      const PEIdent*id = dynamic_cast<const PEIdent*>(lval());

	/* If the l-value is not a register, then make a structural
	   elaboration. Make a synthetic register that connects to the
	   generated circuit and return that as the l-value. */
      if (id == 0) {
	    NetNet*ll = lval_->elaborate_net(des, path, 0, 0, 0, 0);
	    if (ll == 0) {
		  cerr << get_line() << ": Assignment l-value too complex."
		       << endl;
		  return 0;
	    }

	    lsb = 0;
	    msb = ll->pin_count()-1;
	    mux = 0;
	    return ll;
      }

      assert(id);

	/* Get the signal referenced by the identifier, and make sure
	   it is a register. (Wires are not allows in this context. */
      NetNet*reg = des->find_signal(scope, id->name());

      if (reg == 0) {
	    cerr << get_line() << ": error: Could not match signal ``" <<
		  id->name() << "'' in ``" << path << "''" << endl;
	    des->errors += 1;
	    return 0;
      }
      assert(reg);

      if ((reg->type() != NetNet::REG) && (reg->type() != NetNet::INTEGER)) {
	    cerr << get_line() << ": error: " << *lval() <<
		  " is not a register." << endl;
	    des->errors += 1;
	    return 0;
      }

      if (id->msb_ && id->lsb_) {
	      /* This handles part selects. In this case, there are
		 two bit select expressions, and both must be
		 constant. Evaluate them and pass the results back to
		 the caller. */
	    verinum*vl = id->lsb_->eval_const(des, path);
	    if (vl == 0) {
		  cerr << id->lsb_->get_line() << ": error: "
			"Expression must be constant in this context: "
		       << *id->lsb_;
		  des->errors += 1;
		  return 0;
	    }
	    verinum*vm = id->msb_->eval_const(des, path);
	    if (vl == 0) {
		  cerr << id->msb_->get_line() << ": error: "
			"Expression must be constant in this context: "
		       << *id->msb_;
		  des->errors += 1;
		  return 0;
	    }

	    msb = vm->as_ulong();
	    lsb = vl->as_ulong();
	    mux = 0;

      } else if (id->msb_) {

	      /* If there is only a single select expression, it is a
		 bit select. Evaluate the constant value and treat it
		 as a part select with a bit width of 1. If the
		 expression it not constant, then return the
		 expression as a mux. */
	    assert(id->lsb_ == 0);
	    verinum*v = id->msb_->eval_const(des, path);
	    if (v == 0) {
		  NetExpr*m = id->msb_->elaborate_expr(des, scope);
		  assert(m);
		  msb = 0;
		  lsb = 0;
		  mux = m;

	    } else {

		  msb = v->as_ulong();
		  lsb = v->as_ulong();
		  mux = 0;
	    }

      } else {

	      /* No select expressions, so presume a part select the
		 width of the register. */

	    assert(id->msb_ == 0);
	    assert(id->lsb_ == 0);
	    msb = reg->msb();
	    lsb = reg->lsb();
	    mux = 0;
      }

      return reg;
}

NetProc* PAssign::elaborate(Design*des, const string&path) const
{
      NetScope*scope = des->find_scope(path);
      assert(scope);

	/* Catch the case where the lvalue is a reference to a memory
	   item. These are handled differently. */
      do {
	    const PEIdent*id = dynamic_cast<const PEIdent*>(lval());
	    if (id == 0) break;

	    NetNet*net = des->find_signal(scope, id->name());
	    if (net && (net->scope() == scope))
		  break;

	    if (NetMemory*mem = des->find_memory(scope, id->name()))
		  return assign_to_memory_(mem, id->msb_, des, path);

      } while(0);


	/* elaborate the lval. This detects any part selects and mux
	   expressions that might exist. */
      unsigned lsb, msb;
      NetExpr*mux;
      NetNet*reg = elaborate_lval(des, path, msb, lsb, mux);
      if (reg == 0) return 0;

	/* If there is a delay expression, elaborate it. */
      unsigned long rise_time, fall_time, decay_time;
      delay_.eval_delays(des, path, rise_time, fall_time, decay_time);


	/* Elaborate the r-value expression. */
      assert(rval());

      NetExpr*rv;

      if (verinum*val = rval()->eval_const(des,path)) {
	    rv = new NetEConst(*val);
	    delete val;

      } else if (rv = rval()->elaborate_expr(des, scope)) {

	      /* OK, go on. */

      } else {
	      /* Unable to elaborate expression. Retreat. */
	    return 0;
      }

      assert(rv);

	/* Try to evaluate the expression, at least as far as possible. */
      if (NetExpr*tmp = rv->eval_tree()) {
	    delete rv;
	    rv = tmp;
      }

      NetAssign*cur;

	/* Rewrite delayed assignments as assignments that are
	   delayed. For example, a = #<d> b; becomes:

	     begin
	        tmp = b;
		#<d> a = tmp;
	     end

	   If the delay is an event delay, then the transform is
	   similar, with the event delay replacing the time delay. It
	   is an event delay if the event_ member has a value.

	   This rewriting of the expression allows me to not bother to
	   actually and literally represent the delayed assign in the
	   netlist. The compound statement is exactly equivalent. */

      if (rise_time || event_) {
	    string n = des->local_symbol(path);
	    unsigned wid = reg->pin_count();

	    rv->set_width(reg->pin_count());
	    rv = pad_to_width(rv, reg->pin_count());

	    if (! rv->set_width(reg->pin_count())) {
		  cerr << get_line() << ": error: Unable to match "
			"expression width of " << rv->expr_width() <<
			" to l-value width of " << wid << "." << endl;
		    //XXXX delete rv;
		  return 0;
	    }

	    NetNet*tmp = new NetNet(scope, n, NetNet::REG, wid);
	    tmp->set_line(*this);

	      /* Generate an assignment of the l-value to the temporary... */
	    n = des->local_symbol(path);
	    NetAssign*a1 = new NetAssign(n, des, wid, rv);
	    a1->set_line(*this);
	    des->add_node(a1);

	    for (unsigned idx = 0 ;  idx < wid ;  idx += 1)
		  connect(a1->pin(idx), tmp->pin(idx));

	      /* Generate an assignment of the temporary to the r-value... */
	    n = des->local_symbol(path);
	    NetESignal*sig = new NetESignal(tmp);
	    NetAssign*a2 = new NetAssign(n, des, wid, sig);
	    a2->set_line(*this);
	    des->add_node(a2);

	    for (unsigned idx = 0 ;  idx < wid ;  idx += 1)
		  connect(a2->pin(idx), reg->pin(idx));

	      /* Generate the delay statement with the final
		 assignment attached to it. If this is an event delay,
		 elaborate the PEventStatement. Otherwise, create the
		 right NetPDelay object. */
	    NetProc*st;
	    if (event_) {
		  st = event_->elaborate_st(des, path, a2);
		  if (st == 0) {
			cerr << event_->get_line() << ": error: "
			      "unable to elaborate event expression."
			     << endl;
			des->errors += 1;
			return 0;
		  }
		  assert(st);

	    } else {
		  NetPDelay*de = new NetPDelay(rise_time, a2);
		  st = de;
	    }

	      /* And build up the complex statement. */
	    NetBlock*bl = new NetBlock(NetBlock::SEQU);
	    bl->append(a1);
	    bl->append(st);

	    return bl;
      }

      if (mux == 0) {
	      /* This is a simple assign to a register. There may be a
		 part select, so take care that the width is of the
		 part, and using the lsb, make sure the correct range
		 of bits is assigned. */
	    unsigned wid = (msb >= lsb)? (msb-lsb+1) : (lsb-msb+1);
	    assert(wid <= reg->pin_count());

	    rv->set_width(wid);
	    rv = pad_to_width(rv, wid);
	    assert(rv->expr_width() >= wid);

	    cur = new NetAssign(des->local_symbol(path), des, wid, rv);
	    unsigned off = reg->sb_to_idx(lsb);
	    assert((off+wid) <= reg->pin_count());
	    for (unsigned idx = 0 ;  idx < wid ;  idx += 1)
		  connect(cur->pin(idx), reg->pin(idx+off));

      } else {

	    assert(msb == lsb);
	    cur = new NetAssign(des->local_symbol(path), des,
				reg->pin_count(), mux, rv);
	    for (unsigned idx = 0 ;  idx < reg->pin_count() ;  idx += 1)
		  connect(cur->pin(idx), reg->pin(idx));
      }


      cur->set_line(*this);
      des->add_node(cur);

      return cur;
}

/*
 * I do not really know how to elaborate mem[x] <= expr, so this
 * method pretends it is a blocking assign and elaborates
 * that. However, I report an error so that the design isn't actually
 * executed by anyone.
 */
NetProc* PAssignNB::assign_to_memory_(NetMemory*mem, PExpr*ix,
				      Design*des, const string&path) const
{
      NetScope*scope = des->find_scope(path);
      assert(scope);

	/* Elaborate the r-value expression, ... */
      NetExpr*rv = rval()->elaborate_expr(des, scope);
      if (rv == 0)
	    return 0;

      assert(rv);
      rv->set_width(mem->width());

	/* Elaborate the expression to calculate the index, ... */
      NetExpr*idx = ix->elaborate_expr(des, scope);
      assert(idx);

	/* And connect them together in an assignment NetProc. */
      NetAssignMemNB*am = new NetAssignMemNB(mem, idx, rv);
      am->set_line(*this);

      return am;
}

/*
 * The l-value of a procedural assignment is a very much constrained
 * expression. To wit, only identifiers, bit selects and part selects
 * are allowed. I therefore can elaborate the l-value by hand, without
 * the help of recursive elaboration.
 *
 * (For now, this does not yet support concatenation in the l-value.)
 */
NetProc* PAssignNB::elaborate(Design*des, const string&path) const
{
      NetScope*scope = des->find_scope(path);
      assert(scope);

	/* Catch the case where the lvalue is a reference to a memory
	   item. These are handled differently. */
      do {
	    const PEIdent*id = dynamic_cast<const PEIdent*>(lval());
	    if (id == 0) break;

	    if (NetMemory*mem = des->find_memory(scope, id->name()))
		  return assign_to_memory_(mem, id->msb_, des, path);

      } while(0);


      unsigned lsb, msb;
      NetExpr*mux;
      NetNet*reg = elaborate_lval(des, path, msb, lsb, mux);
      if (reg == 0) return 0;

      assert(rval());

	/* Elaborate the r-value expression. This generates a
	   procedural expression that I attach to the assignment. */
      NetExpr*rv = rval()->elaborate_expr(des, scope);
      if (rv == 0)
	    return 0;

      assert(rv);

      NetAssignNB*cur;
      if (mux == 0) {
	    unsigned wid = msb - lsb + 1;

	    rv->set_width(wid);
	    rv = pad_to_width(rv, wid);
	    assert(wid <= rv->expr_width());

	    cur = new NetAssignNB(des->local_symbol(path), des, wid, rv);
	    for (unsigned idx = 0 ;  idx < wid ;  idx += 1)
		  connect(cur->pin(idx), reg->pin(reg->sb_to_idx(idx+lsb)));

      } else {

	      /* In this case, there is a mux expression (detected by
		 the elaborate_lval method) that selects where the bit
		 value goes. Create a NetAssignNB object that carries
		 that mux expression, and connect it to the entire
		 width of the lval. */
	    cur = new NetAssignNB(des->local_symbol(path), des,
				  reg->pin_count(), mux, rv);
	    for (unsigned idx = 0 ;  idx < reg->pin_count() ;  idx += 1)
		  connect(cur->pin(idx), reg->pin(idx));
      }


      unsigned long rise_time, fall_time, decay_time;
      delay_.eval_delays(des, path, rise_time, fall_time, decay_time);
      cur->rise_time(rise_time);
      cur->fall_time(fall_time);
      cur->decay_time(decay_time);


	/* All done with this node. mark its line number and check it in. */
      cur->set_line(*this);
      des->add_node(cur);
      return cur;
}


/*
 * This is the elaboration method for a begin-end block. Try to
 * elaborate the entire block, even if it fails somewhere. This way I
 * get all the error messages out of it. Then, if I detected a failure
 * then pass the failure up.
 */
NetProc* PBlock::elaborate(Design*des, const string&path) const
{
      NetScope*scope = des->find_scope(path);
      assert(scope);

      NetBlock::Type type = (bl_type_==PBlock::BL_PAR)
	    ? NetBlock::PARA
	    : NetBlock::SEQU;
      NetBlock*cur = new NetBlock(type);
      bool fail_flag = false;

      string npath;
      NetScope*nscope;
      if (name_.length()) {
	    nscope = scope->child(name_);
	    if (nscope == 0) {
		  cerr << get_line() << ": internal error: "
			"unable to find block scope " << scope->name()
		       << "<" << name_ << ">" << endl;
		  des->errors += 1;
		  delete cur;
		  return 0;
	    }

	    assert(nscope);
	    npath = nscope->name();

      } else {
	    nscope = scope;
	    npath = path;
      }

	// Handle the special case that the block contains only one
	// statement. There is no need to keep the block node.
      if (list_.count() == 1) {
	    NetProc*tmp = list_[0]->elaborate(des, npath);
	    return tmp;
      }

      for (unsigned idx = 0 ;  idx < list_.count() ;  idx += 1) {
	    NetProc*tmp = list_[idx]->elaborate(des, npath);
	    if (tmp == 0) {
		  fail_flag = true;
		  continue;
	    }
	    cur->append(tmp);
      }

      if (fail_flag) {
	    delete cur;
	    cur = 0;
      }

      return cur;
}

/*
 * Elaborate a case statement.
 */
NetProc* PCase::elaborate(Design*des, const string&path) const
{
      NetScope*scope = des->find_scope(path);
      assert(scope);

      NetExpr*expr = expr_->elaborate_expr(des, scope);
      if (expr == 0) {
	    cerr << get_line() << ": error: Unable to elaborate this case"
		  " expression." << endl;
	    return 0;
      }

      unsigned icount = 0;
      for (unsigned idx = 0 ;  idx < items_->count() ;  idx += 1) {
	    PCase::Item*cur = (*items_)[idx];

	    if (cur->expr.count() == 0)
		  icount += 1;
	    else
		  icount += cur->expr.count();
      }

      NetCase*res = new NetCase(type_, expr, icount);
      res->set_line(*this);

      unsigned inum = 0;
      for (unsigned idx = 0 ;  idx < items_->count() ;  idx += 1) {

	    assert(inum < icount);
	    PCase::Item*cur = (*items_)[idx];

	    if (cur->expr.count() == 0) {
		    /* If there are no expressions, then this is the
		       default case. */
		  NetProc*st = 0;
		  if (cur->stat)
			st = cur->stat->elaborate(des, path);

		  res->set_case(inum, 0, st);
		  inum += 1;

	    } else for (unsigned e = 0; e < cur->expr.count(); e += 1) {

		    /* If there are one or more expressions, then
		       iterate over the guard expressions, elaborating
		       a separate case for each. (Yes, the statement
		       will be elaborated again for each.) */
		  NetExpr*gu = 0;
		  NetProc*st = 0;
		  assert(cur->expr[e]);
		  gu = cur->expr[e]->elaborate_expr(des, scope);

		  if (cur->stat)
			st = cur->stat->elaborate(des, path);

		  res->set_case(inum, gu, st);
		  inum += 1;
	    }
      }

      return res;
}

NetProc* PCondit::elaborate(Design*des, const string&path) const
{
      NetScope*scope = des->find_scope(path);
      assert(scope);

	// Elaborate and try to evaluate the conditional expression.
      NetExpr*expr = expr_->elaborate_expr(des, scope);
      if (expr == 0) {
	    cerr << get_line() << ": error: Unable to elaborate"
		  " condition expression." << endl;
	    des->errors += 1;
	    return 0;
      }
      NetExpr*tmp = expr->eval_tree();
      if (tmp) {
	    delete expr;
	    expr = tmp;
      }

	// If the condition of the conditional statement is constant,
	// then look at the value and elaborate either the if statement
	// or the else statement. I don't need both. If there is no
	// else_ statement, the use an empty block as a noop.
      if (NetEConst*ce = dynamic_cast<NetEConst*>(expr)) {
	    verinum val = ce->value();
	    delete expr;
	    if (val[0] == verinum::V1)
		  return if_->elaborate(des, path);
	    else if (else_)
		  return else_->elaborate(des, path);
	    else
		  return new NetBlock(NetBlock::SEQU);
      }

	// If the condition expression is more then 1 bits, then
	// generate a comparison operator to get the result down to
	// one bit. Turn <e> into <e> != 0;

      if (expr->expr_width() < 1) {
	    cerr << get_line() << ": internal error: "
		  "incomprehensible expression width (0)." << endl;
	    return 0;
      }

      if (! expr->set_width(1)) {
	    assert(expr->expr_width() > 1);
	    verinum zero (verinum::V0, expr->expr_width());
	    NetEConst*ezero = new NetEConst(zero);
	    ezero->set_width(expr->expr_width());
	    NetEBComp*cmp = new NetEBComp('n', expr, ezero);
	    expr = cmp;
      }

	// Well, I actually need to generate code to handle the
	// conditional, so elaborate.
      NetProc*i = if_? if_->elaborate(des, path) : 0;
      NetProc*e = else_? else_->elaborate(des, path) : 0;

      NetCondit*res = new NetCondit(expr, i, e);
      res->set_line(*this);
      return res;
}

NetProc* PCallTask::elaborate(Design*des, const string&path) const
{
      if (name_[0] == '$')
	    return elaborate_sys(des, path);
      else
	    return elaborate_usr(des, path);
}

/*
 * A call to a system task involves elaborating all the parameters,
 * then passing the list to the NetSTask object.
 *XXXX
 * There is a single special in the call to a system task. Normally,
 * an expression cannot take an unindexed memory. However, it is
 * possible to take a system task parameter a memory if the expression
 * is trivial.
 */
NetProc* PCallTask::elaborate_sys(Design*des, const string&path) const
{
      NetScope*scope = des->find_scope(path);
      assert(scope);

      svector<NetExpr*>eparms (nparms());

      for (unsigned idx = 0 ;  idx < nparms() ;  idx += 1) {
	    PExpr*ex = parm(idx);

	    eparms[idx] = ex? ex->elaborate_expr(des, scope) : 0;
      }

      NetSTask*cur = new NetSTask(name(), eparms);
      return cur;
}

/*
 * A call to a user defined task is different from a call to a system
 * task because a user task in a netlist has no parameters: the
 * assignments are done by the calling thread. For example:
 *
 *  task foo;
 *    input a;
 *    output b;
 *    [...]
 *  endtask;
 *
 *  [...] foo(x, y);
 *
 * is really:
 *
 *  task foo;
 *    reg a;
 *    reg b;
 *    [...]
 *  endtask;
 *
 *  [...]
 *  begin
 *    a = x;
 *    foo;
 *    y = b;
 *  end
 */
NetProc* PCallTask::elaborate_usr(Design*des, const string&path) const
{
      NetScope*scope = des->find_scope(path);
      assert(scope);

      NetTaskDef*def = des->find_task(path, name_);
      if (def == 0) {
	    cerr << get_line() << ": error: Enable of unknown task ``" <<
		  path << "." << name_ << "''." << endl;
	    des->errors += 1;
	    return 0;
      }

      if (nparms() != def->port_count()) {
	    cerr << get_line() << ": error: Port count mismatch in call to ``"
		 << name_ << "''." << endl;
	    des->errors += 1;
	    return 0;
      }

      NetUTask*cur;

	/* Handle tasks with no parameters specially. There is no need
	   to make a sequential block to hold the generated code. */
      if (nparms() == 0) {
	    cur = new NetUTask(def);
	    return cur;
      }

      NetBlock*block = new NetBlock(NetBlock::SEQU);


	/* Detect the case where the definition of the task is known
	   empty. In this case, we need not bother with calls to the
	   task, all the assignments, etc. Just return a no-op. */

      if (const NetBlock*tp = dynamic_cast<const NetBlock*>(def->proc())) {
	    if (tp->proc_first() == 0)
		  return block;
      }

	/* Generate assignment statement statements for the input and
	   INOUT ports of the task. These are managed by writing
	   assignments with the task port the l-value and the passed
	   expression the r-value. We know by definition that the port
	   is a reg type, so this elaboration is pretty obvious. */

      for (unsigned idx = 0 ;  idx < nparms() ;  idx += 1) {

	    NetNet*port = def->port(idx);
	    assert(port->port_type() != NetNet::NOT_A_PORT);
	    if (port->port_type() == NetNet::POUTPUT)
		  continue;

	    NetExpr*rv = parms_[idx]->elaborate_expr(des, scope);
	    NetAssign*pr = new NetAssign("@", des, port->pin_count(), rv);
	    for (unsigned pi = 0 ;  pi < port->pin_count() ;  pi += 1)
		  connect(port->pin(pi), pr->pin(pi));
	    des->add_node(pr);
	    block->append(pr);
      }

	/* Generate the task call proper... */
      cur = new NetUTask(def);
      block->append(cur);


	/* Generate assignment statements for the output and INOUT
	   ports of the task. The l-value in this case is the
	   expression passed as a parameter, and the r-value is the
	   port to be copied out.

	   We know by definition that the r-value of this copy-out is
	   the port, which is a reg. The l-value, however, may be any
	   expression that can be a target to a procedural
	   assignment, including a memory word. */

      for (unsigned idx = 0 ;  idx < nparms() ;  idx += 1) {

	    NetNet*port = def->port(idx);

	      /* Skip input ports. */
	    assert(port->port_type() != NetNet::NOT_A_PORT);
	    if (port->port_type() == NetNet::PINPUT)
		  continue;

	    const PEIdent*id;
	    NetNet*val = 0;
	    NetMemory*mem = 0;
	    if ( (id = dynamic_cast<const PEIdent*>(parms_[idx])) )
		  des->find_symbol(scope, id->name(), val, mem);

	      /* Catch the case of memory words passed as an out
		 parameter. Generate an assignment to memory instead
		 of a normal assignment. */
	    if (mem != 0) {
		  assert(id->msb_);
		  NetExpr*ix = id->msb_->elaborate_expr(des, scope);
		  assert(ix);

		  NetExpr*rv = new NetESignal(port);
		  if (rv->expr_width() < mem->width())
			rv = pad_to_width(rv, mem->width());

		  NetAssignMem*am = new NetAssignMem(mem, ix, rv);
		  am->set_line(*this);
		  block->append(am);
		  continue;
	    }


	      /* Elaborate the parameter expression as a net so that
		 it can be used as an l-value. Then check that the
		 parameter width match up.

		 XXXX FIXME XXXX This goes nuts if the parameter is a
		 memory word. that must be handled by generating
		 NetAssignMem objects instead. */
	    if (val == 0)
		  val = parms_[idx]->elaborate_net(des, path,
						   0, 0, 0, 0);
	    assert(val);


	      /* Make an expression out of the actual task port. If
		 the port is smaller then the expression to receive
		 the result, then expand the port by padding with
		 zeros. */
	    NetESignal*sig = new NetESignal(port);
	    NetExpr*pexp = sig;
	    if (sig->expr_width() < val->pin_count()) {
		  unsigned cwid = val->pin_count()-sig->expr_width();
		  verinum pad (verinum::V0, cwid);
		  NetEConst*cp = new NetEConst(pad);
		  cp->set_width(cwid);

		  NetEConcat*con = new NetEConcat(2);
		  con->set(0, cp);
		  con->set(1, sig);
		  con->set_width(val->pin_count());
		  pexp = con;
	    }


	      /* Generate the assignment statement. */
	    NetAssign*ass = new NetAssign("@", des, val->pin_count(), pexp);
	    for (unsigned pi = 0 ; pi < val->pin_count() ;  pi += 1)
		  connect(val->pin(pi), ass->pin(pi));

	    des->add_node(ass);
	    block->append(ass);
      }

      return block;
}

NetCAssign* PCAssign::elaborate(Design*des, const string&path) const
{
      NetScope*scope = des->find_scope(path);
      assert(scope);

      NetNet*lval = lval_->elaborate_net(des, path, 0, 0, 0, 0);
      if (lval == 0)
	    return 0;

      NetNet*rval = expr_->elaborate_net(des, path, lval->pin_count(),
					 0, 0, 0);
      if (rval == 0)
	    return 0;

      if (rval->pin_count() < lval->pin_count())
	    rval = pad_to_width(des, path, rval, lval->pin_count());

      NetCAssign* dev = new NetCAssign(des->local_symbol(path), lval);
      des->add_node(dev);

      for (unsigned idx = 0 ;  idx < dev->pin_count() ;  idx += 1)
	    connect(dev->pin(idx), rval->pin(idx));

      return dev;
}

NetDeassign* PDeassign::elaborate(Design*des, const string&path) const
{
      NetScope*scope = des->find_scope(path);
      assert(scope);

      NetNet*lval = lval_->elaborate_net(des, path, 0, 0, 0, 0);
      if (lval == 0)
	    return 0;

      NetDeassign*dev = new NetDeassign(lval);
      dev->set_line( *this );
      return dev;
}

/*
 * Elaborate the delay statment (of the form #<expr> <statement>) as a
 * NetPDelay object. If the expression is constant, evaluate it now
 * and make a constant delay. If not, then pass an elaborated
 * expression to the constructor of NetPDelay so that the code
 * generator knows to evaluate the expression at run time.
 */
NetProc* PDelayStatement::elaborate(Design*des, const string&path) const
{
      NetScope*scope = des->find_scope(path);
      assert(scope);

      verinum*num = delay_->eval_const(des, path);
      if (num == 0) {
	      /* Ah, the delay is not constant. OK, elaborate the
		 expression and let the run-time handle it. */
	    NetExpr*dex = delay_->elaborate_expr(des, scope);
	    if (statement_)
		  return new NetPDelay(dex, statement_->elaborate(des, path));
	    else
		  return new NetPDelay(dex, 0);
      }
      assert(num);

	/* Convert the delay in the units of the scope to the
	   precision of the design as a whole. */
      unsigned long val = des->scale_to_precision(num->as_ulong(), scope);

	/* If there is a statement, then elaborate it and create a
	   NetPDelay statement to contain it. Note that we create a
	   NetPDelay statement even if the value is 0 because #0 does
	   in fact have a well defined meaning in Verilog. */

      if (statement_) {
	    NetProc*stmt = statement_->elaborate(des, path);
	    return new NetPDelay(val, stmt);

      }  else {
	    return new NetPDelay(val, 0);
      }
}

/*
 * The disable statement is not yet supported.
 */
NetProc* PDisable::elaborate(Design*des, const string&path) const
{
      NetScope*scope = des->find_scope(path);
      assert(scope);

      NetScope*target = des->find_scope(scope, scope_);
      if (target == 0) {
	    cerr << get_line() << ": error: Cannot find scope "
		 << scope_ << " in " << scope->name() << endl;
	    des->errors += 1;
	    return 0;
      }

      switch (target->type()) {
	  case NetScope::FUNC:
	    cerr << get_line() << ": error: Cannot disable functions." << endl;
	    des->errors += 1;
	    return 0;

	  case NetScope::MODULE:
	    cerr << get_line() << ": error: Cannot disable modules." << endl;
	    des->errors += 1;
	    return 0;

	  default:
	    break;
      }

      NetDisable*obj = new NetDisable(target);
      obj->set_line(*this);
      return obj;
}

/*
 * An event statement is an event delay of some sort, attached to a
 * statement. Some Verilog examples are:
 *
 *      @(posedge CLK) $display("clock rise");
 *      @event_1 $display("event triggered.");
 *      @(data or negedge clk) $display("data or clock fall.");
 *
 * The elaborated netlist uses the NetEvent, NetEvWait and NetEvProbe
 * classes. The NetEvWait class represents the part of the netlist
 * that is executed by behavioral code. The process starts waiting on
 * the NetEvent when it executes the NetEvWait step. Net NetEvProbe
 * and NetEvTrig are structural and behavioral equivilents that
 * trigger the event, and awakens any processes blocking in the
 * associated wait.
 *
 * The basic data structure is:
 *
 *       NetEvWait ---/--->  NetEvent  <----\---- NetEvProbe
 *        ...         |                     |         ...
 *       NetEvWait ---+                     +---- NetEvProbe
 *                                          |         ...
 *                                          +---- NetEvTrig
 *
 * That is, many NetEvWait statements may wait on a single NetEvent
 * object, and Many NetEvProbe objects may trigger the NetEvent
 * object. The many NetEvWait objects pointing to the NetEvent object
 * reflects the possibility of different places in the code blocking
 * on the same named event, like so:
 *
 *         event foo;
 *           [...]
 *         always begin @foo <statement1>; @foo <statement2> end
 *
 * This tends to not happen with signal edges. The multiple probes
 * pointing to the same event reflect the possibility of many
 * expressions in the same blocking statement, like so:
 *
 *         wire reset, clk;
 *           [...]
 *         always @(reset or posedge clk) <stmt>;
 *
 * Conjunctions like this cause a NetEvent object be created to
 * represent the overall conjuction, and NetEvProbe objects for each
 * event expression.
 *
 * If the NetEvent object represents a named event from the source,
 * then there are NetEvTrig objects that represent the trigger
 * statements instead of the NetEvProbe objects representing signals.
 * For example:
 *
 *         event foo;
 *         always @foo <stmt>;
 *         initial begin
 *                [...]
 *            -> foo;
 *                [...]
 *            -> foo;
 *                [...]
 *         end
 *
 * Each trigger statement in the source generates a separate NetEvTrig
 * object in the netlist. Those trigger objects are elaborated
 * elsewhere.
 *
 * Additional complications arise when named events show up in
 * conjunctions. An example of such a case is:
 *
 *         event foo;
 *         wire bar;
 *         always @(foo or posedge bar) <stmt>;
 *
 * Since there is by definition a NetEvent object for the foo object,
 * this is handled by allowing the NetEvWait object to point to
 * multiple NetEvent objects. All the NetEvProbe based objects are
 * collected and pointed as the synthetic NetEvent object, and all the
 * named events are added into the list of NetEvent object that the
 * NetEvWait object can refer to.
 */

NetProc* PEventStatement::elaborate_st(Design*des, const string&path,
				    NetProc*enet) const
{
      NetScope*scope = des->find_scope(path);
      assert(scope);


	/* The Verilog wait (<expr>) <statement> statement is a level
	   sensitive wait. Handle this special case by elaborating
	   something like this:

	       begin
	         if (! <expr>) @(posedge <expr>)
	         <statement>
	       end

	   This is equivilent, and uses the existing capapilities of
	   the netlist format. The resulting netlist should look like
	   this:

	       NetBlock ---+---> NetCondit --+--> <expr>
	                   |                 |
			   |     	     +--> NetEvWait--> NetEvent
			   |
			   +---> <statement>

	   This is quite a mouthful. Should I not move wait handling
	   to specialized objects? */


      if ((expr_.count() == 1) && (expr_[0]->type() == PEEvent::POSITIVE)) {

	    NetBlock*bl = new NetBlock(NetBlock::SEQU);

	    NetNet*ex = expr_[0]->expr()->elaborate_net(des, path,
							1, 0, 0, 0);
	    if (ex == 0) {
		  expr_[0]->dump(cerr);
		  cerr << endl;
		  des->errors += 1;
		  return 0;
	    }

	    NetEvent*ev = new NetEvent(scope->local_symbol());
	    scope->add_event(ev);

	    NetEvWait*we = new NetEvWait(0);
	    we->add_event(ev);

	    NetEvProbe*po = new NetEvProbe(path+"."+scope->local_symbol(),
					   ev, NetEvProbe::POSEDGE, 1);
	    connect(po->pin(0), ex->pin(0));

	    des->add_node(po);

	    NetESignal*ce = new NetESignal(ex);
	    NetCondit*co = new NetCondit(new NetEUnary('!', ce), we, 0);
	    bl->append(co);
	    bl->append(enet);

	    ev->set_line(*this);
	    bl->set_line(*this);
	    we->set_line(*this);
	    co->set_line(*this);

	    return bl;
      }


	/* Handle the special case of an event name as an identifier
	   in an expression. Make a named event reference. */

      if (expr_.count() == 1) {
	    assert(expr_[0]->expr());
	    PEIdent*id = dynamic_cast<PEIdent*>(expr_[0]->expr());
	    NetEvent*ev;
	    if (id && (ev = scope->find_event(id->name()))) {
		  NetEvWait*pr = new NetEvWait(enet);
		  pr->add_event(ev);
		  pr->set_line(*this);
		  return pr;
	    }
      }

	/* Create A single NetEvent and NetEvWait. Then, create a
	   NetEvProbe for each conjunctive event in the event
	   list. The NetEvProbe object al refer back to the NetEvent
	   object. */

      NetEvent*ev = new NetEvent(scope->local_symbol());
      ev->set_line(*this);
      unsigned expr_count = 0;

      NetEvWait*wa = new NetEvWait(enet);
      wa->set_line(*this);

      for (unsigned idx = 0 ;  idx < expr_.count() ;  idx += 1) {

	    assert(expr_[idx]->expr());

	      /* If the expression is an identifier that matches a
		 named event, then handle this case all at once at
		 skip the rest of the expression handling. */

	    if (PEIdent*id = dynamic_cast<PEIdent*>(expr_[idx]->expr())) {
		  NetEvent*tmp = scope->find_event(id->name());
		  if (tmp) {
			wa->add_event(tmp);
			continue;
		  }
	    }

	      /* So now we have a normal event expression. Elaborate
		 the sub-expression as a net and decide how to handle
		 the edge. */

	    NetNet*expr = expr_[idx]->expr()->elaborate_net(des, path,
							    0, 0, 0, 0);
	    if (expr == 0) {
		  expr_[idx]->dump(cerr);
		  cerr << endl;
		  des->errors += 1;
		  continue;
	    }
	    assert(expr);

	    unsigned pins = (expr_[idx]->type() == PEEvent::ANYEDGE)
		  ? expr->pin_count() : 1;

	    NetEvProbe*pr;
	    switch (expr_[idx]->type()) {
		case PEEvent::POSEDGE:
		  pr = new NetEvProbe(des->local_symbol(path), ev,
				      NetEvProbe::POSEDGE, pins);
		  break;

		case PEEvent::NEGEDGE:
		  pr = new NetEvProbe(des->local_symbol(path), ev,
				      NetEvProbe::NEGEDGE, pins);
		  break;

		case PEEvent::ANYEDGE:
		  pr = new NetEvProbe(des->local_symbol(path), ev,
				      NetEvProbe::ANYEDGE, pins);
		  break;

		default:
		  assert(0);
	    }

	    for (unsigned p = 0 ;  p < pr->pin_count() ; p += 1)
		  connect(pr->pin(p), expr->pin(p));

	    des->add_node(pr);
	    expr_count += 1;
      }

	/* If there was at least one conjunction that was an
	   expression (and not a named event) then add this
	   event. Otherwise, we didn't use it so delete it. */
      if (expr_count > 0) {
	    if (NetEvent*match = ev->find_similar_event()) {
		  delete ev;
		  wa->add_event(match);

	    } else {

		  scope->add_event(ev);
		  wa->add_event(ev);
	    }
      } else {
	    delete ev;
      }

      return wa;
}

NetProc* PEventStatement::elaborate(Design*des, const string&path) const
{
      NetProc*enet = 0;
      if (statement_) {
	    enet = statement_->elaborate(des, path);
	    if (enet == 0)
		  return 0;
      }

      return elaborate_st(des, path, enet);
}

/*
 * Forever statements are represented directly in the netlist. It is
 * theoretically possible to use a while structure with a constant
 * expression to represent the loop, but why complicate the code
 * generators so?
 */
NetProc* PForever::elaborate(Design*des, const string&path) const
{
      NetProc*stat = statement_->elaborate(des, path);
      if (stat == 0) return 0;

      NetForever*proc = new NetForever(stat);
      return proc;
}

NetProc* PForce::elaborate(Design*des, const string&path) const
{
      NetScope*scope = des->find_scope(path);
      assert(scope);

      NetNet*lval = lval_->elaborate_net(des, path, 0, 0, 0, 0);
      if (lval == 0)
	    return 0;

      NetNet*rval = expr_->elaborate_net(des, path, lval->pin_count(),
					 0, 0, 0);
      if (rval == 0)
	    return 0;

      if (rval->pin_count() < lval->pin_count())
	    rval = pad_to_width(des, path, rval, lval->pin_count());

      NetForce* dev = new NetForce(des->local_symbol(path), lval);
      des->add_node(dev);

      for (unsigned idx = 0 ;  idx < dev->pin_count() ;  idx += 1)
	    connect(dev->pin(idx), rval->pin(idx));

      return dev;
}

/*
 * elaborate the for loop as the equivalent while loop. This eases the
 * task for the target code generator. The structure is:
 *
 *     begin : top
 *       name1_ = expr1_;
 *       while (cond_) begin : body
 *          statement_;
 *          name2_ = expr2_;
 *       end
 *     end
 */
NetProc* PForStatement::elaborate(Design*des, const string&path) const
{
      NetScope*scope = des->find_scope(path);
      assert(scope);

      const PEIdent*id1 = dynamic_cast<const PEIdent*>(name1_);
      assert(id1);
      const PEIdent*id2 = dynamic_cast<const PEIdent*>(name2_);
      assert(id2);

      NetBlock*top = new NetBlock(NetBlock::SEQU);

	/* make the expression, and later the initial assignment to
	   the condition variable. The statement in the for loop is
	   very specifically an assignment. */
      NetNet*sig = des->find_signal(scope, id1->name());
      if (sig == 0) {
	    cerr << id1->get_line() << ": register ``" << id1->name()
		 << "'' unknown in this context." << endl;
	    des->errors += 1;
	    return 0;
      }
      assert(sig);
      NetAssign*init = new NetAssign("@for-assign", des, sig->pin_count(),
				     expr1_->elaborate_expr(des, scope));
      for (unsigned idx = 0 ;  idx < init->pin_count() ;  idx += 1)
	    connect(init->pin(idx), sig->pin(idx));

      top->append(init);

      NetBlock*body = new NetBlock(NetBlock::SEQU);

	/* Elaborate the statement that is contained in the for
	   loop. If there is an error, this will return 0 and I should
	   skip the append. No need to worry, the error has been
	   reported so it's OK that the netlist is bogus. */
      NetProc*tmp = statement_->elaborate(des, path);
      if (tmp)
	    body->append(tmp);


	/* Elaborate the increment assignment statement at the end of
	   the for loop. This is also a very specific assignment
	   statement. Put this into the "body" block. */
      sig = des->find_signal(scope, id2->name());
      assert(sig);
      NetAssign*step = new NetAssign("@for-assign", des, sig->pin_count(),
				     expr2_->elaborate_expr(des, scope));
      for (unsigned idx = 0 ;  idx < step->pin_count() ;  idx += 1)
	    connect(step->pin(idx), sig->pin(idx));

      body->append(step);


	/* Elaborate the condition expression. Try to evaluate it too,
	   in case it is a constant. This is an interesting case
	   worthy of a warning. */
      NetExpr*ce = cond_->elaborate_expr(des, scope);
      if (ce == 0) {
	    delete top;
	    return 0;
      }

      if (NetExpr*tmp = ce->eval_tree()) {
	    if (dynamic_cast<NetEConst*>(tmp))
		  cerr << get_line() << ": warning: condition expression "
			"is constant." << endl;

	    ce = tmp;
      }


	/* All done, build up the loop. */

      NetWhile*loop = new NetWhile(ce, body);
      top->append(loop);
      return top;
}

/*
 * (See the PTask::elaborate methods for basic common stuff.)
 *
 * The return value of a function is represented as a reg variable
 * within the scope of the function that has the name of the
 * function. So for example with the function:
 *
 *    function [7:0] incr;
 *      input [7:0] in1;
 *      incr = in1 + 1;
 *    endfunction
 *
 * The scope of the function is <parent>.incr and there is a reg
 * variable <parent>.incr.incr. The elaborate_1 method is called with
 * the scope of the function, so the return reg is easily located.
 *
 * The function parameters are all inputs, except for the synthetic
 * output parameter that is the return value. The return value goes
 * into port 0, and the parameters are all the remaining ports.
 */

void PFunction::elaborate(Design*des, NetScope*scope) const
{
      NetFuncDef*def = des->find_function(scope->name());
      assert(def);

      NetProc*st = statement_->elaborate(des, scope->name());
      if (st == 0) {
	    cerr << statement_->get_line() << ": error: Unable to elaborate "
		  "statement in function " << def->name() << "." << endl;
	    des->errors += 1;
	    return;
      }

      def->set_proc(st);
}

NetProc* PRelease::elaborate(Design*des, const string&path) const
{
      NetScope*scope = des->find_scope(path);
      assert(scope);

      NetNet*lval = lval_->elaborate_net(des, path, 0, 0, 0, 0);
      if (lval == 0)
	    return 0;

      NetRelease*dev = new NetRelease(lval);
      dev->set_line( *this );
      return dev;
}

NetProc* PRepeat::elaborate(Design*des, const string&path) const
{
      NetScope*scope = des->find_scope(path);
      assert(scope);

      NetExpr*expr = expr_->elaborate_expr(des, scope);
      if (expr == 0) {
	    cerr << get_line() << ": Unable to elaborate"
		  " repeat expression." << endl;
	    des->errors += 1;
	    return 0;
      }
      NetExpr*tmp = expr->eval_tree();
      if (tmp) {
	    delete expr;
	    expr = tmp;
      }

      NetProc*stat = statement_->elaborate(des, path);
      if (stat == 0) return 0;

	// If the expression is a constant, handle certain special
	// iteration counts.
      if (NetEConst*ce = dynamic_cast<NetEConst*>(expr)) {
	    verinum val = ce->value();
	    switch (val.as_ulong()) {
		case 0:
		  delete expr;
		  delete stat;
		  return new NetBlock(NetBlock::SEQU);
		case 1:
		  delete expr;
		  return stat;
		default:
		  break;
	    }
      }

      NetRepeat*proc = new NetRepeat(expr, stat);
      return proc;
}

/*
 * A task definition is elaborated by elaborating the statement that
 * it contains, and connecting its ports to NetNet objects. The
 * netlist doesn't really need the array of parameters once elaboration
 * is complete, but this is the best place to store them.
 *
 * The first elaboration pass finds the reg objects that match the
 * port names, and creates the NetTaskDef object. The port names are
 * in the form task.port.
 *
 *      task foo;
 *        output blah;
 *        begin <body> end
 *      endtask
 *
 * So in the foo example, the PWire objects that represent the ports
 * of the task will include a foo.blah for the blah port. This port is
 * bound to a NetNet object by looking up the name. All of this is
 * handled by the PTask::elaborate_sig method and the results stashed
 * in the created NetDaskDef attached to the scope.
 *
 * Elaboration pass 2 for the task definition causes the statement of
 * the task to be elaborated and attached to the NetTaskDef object
 * created in pass 1.
 *
 * NOTE: I am not sure why I bothered to prepend the task name to the
 * port name when making the port list. It is not really useful, but
 * that is what I did in pform_make_task_ports, so there it is.
 */

void PTask::elaborate(Design*des, const string&path) const
{
      NetTaskDef*def = des->find_task(path);
      assert(def);

      NetProc*st;
      if (statement_ == 0) {
	    cerr << get_line() << ": warning: task has no statement." << endl;
	    st = new NetBlock(NetBlock::SEQU);

      } else {

	    st = statement_->elaborate(des, path);
	    if (st == 0) {
		  cerr << statement_->get_line() << ": Unable to elaborate "
			"statement in task " << path << " at " << get_line()
		       << "." << endl;
		  return;
	    }
      }

      def->set_proc(st);
}

NetProc* PTrigger::elaborate(Design*des, const string&path) const
{
      NetScope*scope = des->find_scope(path);
      assert(scope);

      NetEvent*ev = scope->find_event(event_);
      if (ev == 0) {
	    cerr << get_line() << ": error: event <" << event_ << ">"
		 << " not found." << endl;
	    des->errors += 1;
	    return 0;
      }

      NetEvTrig*trig = new NetEvTrig(ev);
      trig->set_line(*this);
      return trig;
}

/*
 * The while loop is fairly directly represented in the netlist.
 */
NetProc* PWhile::elaborate(Design*des, const string&path) const
{
      NetScope*scope = des->find_scope(path);
      assert(scope);

      NetWhile*loop = new NetWhile(cond_->elaborate_expr(des, scope),
				   statement_->elaborate(des, path));
      return loop;
}

/*
 * When a module is instantiated, it creates the scope then uses this
 * method to elaborate the contents of the module.
 */
bool Module::elaborate(Design*des, NetScope*scope) const
{
      const string path = scope->name();
      bool result_flag = true;


	// Elaborate functions.
      typedef map<string,PFunction*>::const_iterator mfunc_it_t;
      for (mfunc_it_t cur = funcs_.begin()
		 ; cur != funcs_.end() ;  cur ++) {

	    NetScope*fscope = scope->child((*cur).first);
	    assert(fscope);
	    (*cur).second->elaborate(des, fscope);
      }

	// Elaborate the task definitions. This is done before the
	// behaviors so that task calls may reference these, and after
	// the signals so that the tasks can reference them.
      typedef map<string,PTask*>::const_iterator mtask_it_t;
      for (mtask_it_t cur = tasks_.begin()
		 ; cur != tasks_.end() ;  cur ++) {
	    string pname = path + "." + (*cur).first;
	    (*cur).second->elaborate(des, pname);
      }

	// Get all the gates of the module and elaborate them by
	// connecting them to the signals. The gate may be simple or
	// complex.
      const list<PGate*>&gl = get_gates();

      for (list<PGate*>::const_iterator gt = gl.begin()
		 ; gt != gl.end()
		 ; gt ++ ) {

	    (*gt)->elaborate(des, path);
      }

	// Elaborate the behaviors, making processes out of them.
      const list<PProcess*>&sl = get_behaviors();

      for (list<PProcess*>::const_iterator st = sl.begin()
		 ; st != sl.end()
		 ; st ++ ) {

	    NetProc*cur = (*st)->statement()->elaborate(des, path);
	    if (cur == 0) {
		  cerr << (*st)->get_line() << ": error: Elaboration "
			"failed for this process." << endl;
		  result_flag = false;
		  continue;
	    }

	    NetProcTop*top;
	    switch ((*st)->type()) {
		case PProcess::PR_INITIAL:
		  top = new NetProcTop(NetProcTop::KINITIAL, cur);
		  break;
		case PProcess::PR_ALWAYS:
		  top = new NetProcTop(NetProcTop::KALWAYS, cur);
		  break;
	    }

	    top->set_line(*(*st));
	    des->add_process(top);
      }

      return result_flag;
}

Design* elaborate(const map<string,Module*>&modules,
		  const map<string,PUdp*>&primitives,
		  const string&root)
{
	// Look for the root module in the list.
      map<string,Module*>::const_iterator mod = modules.find(root);
      if (mod == modules.end())
	    return 0;

      Module*rmod = (*mod).second;

	// This is the output design. I fill it in as I scan the root
	// module and elaborate what I find.
      Design*des = new Design;

      modlist = &modules;
      udplist = &primitives;

	// Make the root scope, then scan the pform looking for scopes
	// and parameters. 
      NetScope*scope = des->make_root_scope(root);
      scope->time_unit(rmod->time_unit);
      scope->time_precision(rmod->time_precision);
      des->set_precision(rmod->time_precision);
      if (! rmod->elaborate_scope(des, scope)) {
	    delete des;
	    return 0;
      }

	// This method recurses through the scopes, looking for
	// defparam assignments to apply to the parameters in the
	// various scopes. This needs to be done after all the scopes
	// and basic parameters are taken care of because the defparam
	// can assign to a paramter declared *after* it.
      des->run_defparams();


	// At this point, all parameter overrides are done. Scane the
	// scopes and evaluate the parameters all the way down to
	// constants.
      des->evaluate_parameters();


	// With the parameters evaluated down to constants, we have
	// what we need to elaborate signals and memories. This pass
	// creates all the NetNet and NetMemory objects for declared
	// objects.
      if (! rmod->elaborate_sig(des, scope)) {
	    delete des;
	    return 0;
      }

	// Now that the structure and parameters are taken care of,
	// run through the pform again and generate the full netlist.
      bool rc = rmod->elaborate(des, scope);


      modlist = 0;
      udplist = 0;

      if (rc == false) {
	    delete des;
	    des = 0;
      }

      return des;
}


/*
 * $Log: elaborate.cc,v $
 * Revision 1.1  2000/12/21 21:57:13  jrandrews
 * initial import
 *
 * Revision 1.182  2000/07/30 18:25:43  steve
 *  Rearrange task and function elaboration so that the
 *  NetTaskDef and NetFuncDef functions are created during
 *  signal enaboration, and carry these objects in the
 *  NetScope class instead of the extra, useless map in
 *  the Design class.
 *
 * Revision 1.181  2000/07/27 05:13:44  steve
 *  Support elaboration of disable statements.
 *
 * Revision 1.180  2000/07/26 05:08:07  steve
 *  Parse disable statements to pform.
 *
 * Revision 1.179  2000/07/22 22:09:03  steve
 *  Parse and elaborate timescale to scopes.
 *
 * Revision 1.178  2000/07/14 06:12:57  steve
 *  Move inital value handling from NetNet to Nexus
 *  objects. This allows better propogation of inital
 *  values.
 *
 *  Clean up constant propagation  a bit to account
 *  for regs that are not really values.
 *
 * Revision 1.177  2000/07/07 04:53:54  steve
 *  Add support for non-constant delays in delay statements,
 *  Support evaluating ! in constant expressions, and
 *  move some code from netlist.cc to net_proc.cc.
 *
 * Revision 1.176  2000/06/13 03:24:48  steve
 *  Index in memory assign should be a NetExpr.
 *
 * Revision 1.175  2000/05/31 02:26:49  steve
 *  Globally merge redundant event objects.
 *
 * Revision 1.174  2000/05/27 19:33:23  steve
 *  Merge similar probes within a module.
 *
 * Revision 1.173  2000/05/16 04:05:16  steve
 *  Module ports are really special PEIdent
 *  expressions, because a name can be used
 *  many places in the port list.
 *
 * Revision 1.172  2000/05/11 23:37:27  steve
 *  Add support for procedural continuous assignment.
 *
 * Revision 1.171  2000/05/08 05:28:29  steve
 *  Use bufz to make assignments directional.
 *
 * Revision 1.170  2000/05/07 21:17:21  steve
 *  non-blocking assignment to a bit select.
 *
 * Revision 1.169  2000/05/07 04:37:56  steve
 *  Carry strength values from Verilog source to the
 *  pform and netlist for gates.
 *
 *  Change vvm constants to use the driver_t to drive
 *  a constant value. This works better if there are
 *  multiple drivers on a signal.
 *
 * Revision 1.168  2000/05/02 16:27:38  steve
 *  Move signal elaboration to a seperate pass.
 *
 * Revision 1.167  2000/05/02 03:13:31  steve
 *  Move memories to the NetScope object.
 *
 * Revision 1.166  2000/05/02 00:58:11  steve
 *  Move signal tables to the NetScope class.
 *
 * Revision 1.165  2000/04/28 23:12:12  steve
 *  Overly aggressive eliding of task calls.
 *
 * Revision 1.164  2000/04/28 22:17:47  steve
 *  Skip empty tasks.
 *
 * Revision 1.163  2000/04/28 16:50:53  steve
 *  Catch memory word parameters to tasks.
 *
 * Revision 1.162  2000/04/23 03:45:24  steve
 *  Add support for the procedural release statement.
 *
 * Revision 1.161  2000/04/22 04:20:19  steve
 *  Add support for force assignment.
 *
 * Revision 1.160  2000/04/21 04:38:15  steve
 *  Bit padding in assignment to memory.
 *
 * Revision 1.159  2000/04/18 01:02:53  steve
 *  Minor cleanup of NetTaskDef.
 *
 * Revision 1.158  2000/04/12 04:23:58  steve
 *  Named events really should be expressed with PEIdent
 *  objects in the pform,
 *
 *  Handle named events within the mix of net events
 *  and edges. As a unified lot they get caught together.
 *  wait statements are broken into more complex statements
 *  that include a conditional.
 *
 *  Do not generate NetPEvent or NetNEvent objects in
 *  elaboration. NetEvent, NetEvWait and NetEvProbe
 *  take over those functions in the netlist.
 *
 * Revision 1.157  2000/04/10 05:26:06  steve
 *  All events now use the NetEvent class.
 */

