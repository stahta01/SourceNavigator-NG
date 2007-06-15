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
#ident "$Id: xnfio.cc,v 1.1 2000/12/21 21:57:13 jrandrews Exp $"
#endif

# include  "functor.h"
# include  "netlist.h"
# include  "netmisc.h"
# include  <strstream>

class xnfio_f  : public functor_t {

    public:
      void signal(Design*des, NetNet*sig);
      void lpm_compare(Design*des, NetCompare*dev);

    private:
      bool compare_sideb_const(Design*des, NetCompare*dev);
};

static bool is_a_pad(const NetNet*net)
{
      if (net->attribute("PAD") == "")
	    return false;

      return true;
}

/*
 * The xnfio function looks for the PAD signals in the design, and
 * generates the needed IOB devices to handle being connected to the
 * actual FPGA PAD. This will add items to the netlist if needed.
 *
 * FIXME: If there is a DFF connected to the pad, try to convert it
 *        to an IO DFF instead. This would save a CLB, and it is
 *        really lame to not do the obvious optimization.
 */

static NetLogic* make_obuf(Design*des, NetNet*net)
{
      assert(net->pin_count() == 1);

	/* FIXME: If there is nothing internally driving this PAD, I
	   can connect the PAD to a pullup and disconnect it from the
	   rest of the circuit. This would save routing resources. */
      if (count_outputs(net->pin(0)) <= 0) {
	    cerr << net->get_line() << ":warning: No outputs to OPAD: "
		 << net->name() << endl;
	    return 0;
      }

      assert(count_outputs(net->pin(0)) > 0);

	/* Look for an existing OBUF connected to this signal. If it
	   is there, then no need to add one. */
      Nexus*nex = net->pin(0).nexus();
      for (Link*idx = nex->first_nlink()
		 ; idx  ; idx = idx->next_nlink()) {
	    NetLogic*tmp;
	    if ((tmp = dynamic_cast<NetLogic*>(idx->get_obj())) == 0)
		  continue;

	      // Try to use an existing BUF as an OBUF. This moves the
	      // BUF into the IOB.
	    if ((tmp->type() == NetLogic::BUF)
		&& (count_inputs(tmp->pin(0)) == 0)
		&& (count_outputs(tmp->pin(0)) == 1)
		&& (idx->get_pin() == 0)  ) {
		  tmp->attribute("XNF-LCA", "OBUF:O,I");
		  return tmp;
	    }

	      // Try to use an existing INV as an OBUF. Certain
	      // technologies support inverting the input of an OBUF,
	      // which looks just like an inverter. This uses the
	      // available resources of an IOB to optimize away an
	      // otherwise expensive inverter.
	    if ((tmp->type() == NetLogic::NOT)
		&& (count_inputs(tmp->pin(0)) == 0)
		&& (count_outputs(tmp->pin(0)) == 1)
		&& (idx->get_pin() == 0)  ) {
		  tmp->attribute("XNF-LCA", "OBUF:O,~I");
		  return tmp;
	    }

	      // Try to use an existing bufif1 as an OBUFT. Of course
	      // this will only work if the output of the bufif1 is
	      // connected only to the pad. Handle bufif0 the same
	      // way, but the T input is inverted.
	    if ((tmp->type() == NetLogic::BUFIF1)
		&& (count_inputs(tmp->pin(0)) == 0)
		&& (count_outputs(tmp->pin(0)) == 1)
		&& (idx->get_pin() == 0)  ) {
		  tmp->attribute("XNF-LCA", "OBUFT:O,I,~T");
		  return tmp;
	    }

	    if ((tmp->type() == NetLogic::BUFIF0)
		&& (count_inputs(tmp->pin(0)) == 0)
		&& (count_outputs(tmp->pin(0)) == 1)
		&& (idx->get_pin() == 0)  ) {
		  tmp->attribute("XNF-LCA", "OBUFT:O,I,T");
		  return tmp;
	    }
      }

	// Can't seem to find a way to rearrange the existing netlist,
	// so I am stuck creating a new buffer, the OBUF.
      NetLogic*buf = new NetLogic(des->local_symbol("$"), 2, NetLogic::BUF);
      des->add_node(buf);

      map<string,string>attr;
      attr["XNF-LCA"] = "OBUF:O,I";
      buf->set_attributes(attr);

	// Put the buffer between this signal and the rest of the
	// netlist.
      connect(net->pin(0), buf->pin(1));
      net->pin(0).unlink();
      connect(net->pin(0), buf->pin(0));

	// It is possible, in putting an OBUF between net and the rest
	// of the netlist, to create a ring without a signal. Detect
	// this case and create a new signal.
      if (count_signals(buf->pin(1)) == 0) {
	    NetNet*tmp = new NetNet(net->scope(),
				    des->local_symbol("$"),
				    NetNet::WIRE);
	    tmp->local_flag(true);
	    connect(buf->pin(1), tmp->pin(0));
      }

      return buf;
}

static void absorb_OFF(Design*des, NetLogic*buf)
{
	/* If the nexus connects is not a simple point-to-point link,
	   then I can't drag it into the IOB. Give up. */
      if (count_outputs(buf->pin(1)) != 1)
	    return;
      if (count_inputs(buf->pin(1)) != 1)
	    return;
	/* For now, only support OUTFF. */
      if (buf->type() != NetLogic::BUF)
	    return;

      Link*drv = find_next_output(&buf->pin(1));
      assert(drv);

	/* Make sure the device is a FF with width 1. */
      NetFF*ff = dynamic_cast<NetFF*>(drv->get_obj());
      if (ff == 0)
	    return;
      if (ff->width() != 1)
	    return;
      if (ff->attribute("LPM_FFType") != "DFF")
	    return;

	/* Connect the flip-flop output to the buffer output and
	   delete the buffer. The XNF OUTFF can buffer the pin. */
      connect(ff->pin_Q(0), buf->pin(0));
      delete buf;

	/* Finally, build up an XNF-LCA value that defines this
	   devices as an OUTFF and gives each pin an XNF name. */
      char**names = new char*[ff->pin_count()];
      for (unsigned idx = 0 ;  idx < ff->pin_count() ;  idx += 1)
	    names[idx] = "";

      if (ff->attribute("Clock:LPM_Polarity") == "INVERT")
	    names[ff->pin_Clock().get_pin()] = "~C";
      else
	    names[ff->pin_Clock().get_pin()] = "C";

      names[ff->pin_Data(0).get_pin()] = "D";
      names[ff->pin_Q(0).get_pin()] = "Q";

      string lname = string("OUTFF:") + names[0];
      for (unsigned idx = 1 ;  idx < ff->pin_count() ;  idx += 1)
	    lname = lname + "," + names[idx];
      delete[]names;

      ff->attribute("XNF-LCA", lname);
}

static void make_ibuf(Design*des, NetNet*net)
{
      NetScope*scope = net->scope();
      assert(scope);

      assert(net->pin_count() == 1);
	// XXXX For now, require at least one input.
      assert(count_inputs(net->pin(0)) > 0);

	/* Look for an existing BUF connected to this signal and
	   suitably connected that I can use it as an IBUF. */

      Nexus*nex = net->pin(0).nexus();
      for (Link*idx = nex->first_nlink()
		 ; idx ; idx = idx->next_nlink()) {
	    NetLogic*tmp;
	    if ((tmp = dynamic_cast<NetLogic*>(idx->get_obj())) == 0)
		  continue;

	    if (tmp->attribute("XNF-LCA") != "")
		  continue;

	      // Found a BUF, it is only useable if the only input is
	      // the signal and there are no other inputs.
	    if ((tmp->type() == NetLogic::BUF) &&
		(count_inputs(tmp->pin(1)) == 1) &&
		(count_outputs(tmp->pin(1)) == 0)) {
		  tmp->attribute("XNF-LCA", "IBUF:O,I");
		  return;
	    }

      }

	// I give up, create an IBUF.
      NetLogic*buf = new NetLogic(des->local_symbol("$"), 2, NetLogic::BUF);
      des->add_node(buf);

      map<string,string>attr;
      attr["XNF-LCA"] = "IBUF:O,I";
      buf->set_attributes(attr);

	// Put the buffer between this signal and the rest of the
	// netlist.
      connect(net->pin(0), buf->pin(0));
      net->pin(0).unlink();
      connect(net->pin(0), buf->pin(1));

	// It is possible, in putting an OBUF between net and the rest
	// of the netlist, to create a ring without a signal. Detect
	// this case and create a new signal.
      if (count_signals(buf->pin(0)) == 0) {
	    NetNet*tmp = new NetNet(scope,
				    des->local_symbol(scope->name()),
				    NetNet::WIRE);
	    connect(buf->pin(0), tmp->pin(0));
      }
}

void xnfio_f::signal(Design*des, NetNet*net)
{
      NetNode*buf;
      if (! is_a_pad(net))
	    return;

      assert(net->pin_count() == 1);
      string pattr = net->attribute("PAD");

      switch (pattr[0]) {
	  case 'i':
	  case 'I':
	    make_ibuf(des, net);
	    break;
	  case 'o':
	  case 'O': {
		NetLogic*buf = make_obuf(des, net);
		if (buf == 0) break;
		absorb_OFF(des, buf);
		break;
	  }

	      // FIXME: Only IPAD and OPAD supported. Need to
	      // add support for IOPAD.
	  default:
	    assert(0);
	    break;
      }
}

/*
 * Attempt some XNF specific optimizations on comparators.
 */
void xnfio_f::lpm_compare(Design*des, NetCompare*dev)
{
      if (compare_sideb_const(des, dev))
	    return;

      return;
}

bool xnfio_f::compare_sideb_const(Design*des, NetCompare*dev)
{
	/* Even if side B is all constant, if there are more then 4
	   signals on side A we will not be able to fit the operation
	   into a function unit, so we might as well accept a
	   comparator. Give up. */
      if (dev->width() > 4)
	    return false;

      verinum side (verinum::V0, dev->width());

	/* Is the B side all constant? */
      for (unsigned idx = 0 ;  idx < dev->width() ;  idx += 1) {
	    NetConst*cobj;
	    unsigned cidx;
	    cobj = link_const_value(dev->pin_DataB(idx), cidx);
	    if (cobj == 0)
		  return false;

	    side.set(idx, cobj->value(cidx));
      }

	/* Handle the special case of comparing A to 0. Use an N-input
	   NOR gate to return 0 if any of the bits is not 0. */
      if ((side.as_ulong() == 0) && (count_inputs(dev->pin_AEB()) > 0)) {
	    NetLogic*sub = new NetLogic(dev->name(), dev->width()+1,
					NetLogic::NOR);
	    connect(sub->pin(0), dev->pin_AEB());
	    for (unsigned idx = 0 ;  idx < dev->width() ;  idx += 1)
		  connect(sub->pin(idx+1), dev->pin_DataA(idx));
	    delete dev;
	    des->add_node(sub);
	    return true;
      }

	/* Handle the special case of comparing A to 0. Use an N-input
	   NOR gate to return 0 if any of the bits is not 0. */
      if ((side.as_ulong() == 0) && (count_inputs(dev->pin_ANEB()) > 0)) {
	    NetLogic*sub = new NetLogic(dev->name(), dev->width()+1,
					NetLogic::OR);
	    connect(sub->pin(0), dev->pin_ANEB());
	    for (unsigned idx = 0 ;  idx < dev->width() ;  idx += 1)
		  connect(sub->pin(idx+1), dev->pin_DataA(idx));
	    delete dev;
	    des->add_node(sub);
	    return true;
      }

      return false;
}

void xnfio(Design*des)
{
      xnfio_f xnfio_obj;
      des->functor(&xnfio_obj);
}

/*
 * $Log: xnfio.cc,v $
 * Revision 1.1  2000/12/21 21:57:13  jrandrews
 * initial import
 *
 * Revision 1.15  2000/06/25 19:59:42  steve
 *  Redesign Links to include the Nexus class that
 *  carries properties of the connected set of links.
 *
 * Revision 1.14  2000/05/07 04:37:56  steve
 *  Carry strength values from Verilog source to the
 *  pform and netlist for gates.
 *
 *  Change vvm constants to use the driver_t to drive
 *  a constant value. This works better if there are
 *  multiple drivers on a signal.
 *
 * Revision 1.13  2000/05/02 00:58:12  steve
 *  Move signal tables to the NetScope class.
 *
 * Revision 1.12  2000/04/20 00:28:03  steve
 *  Catch some simple identity compareoptimizations.
 *
 * Revision 1.11  2000/02/23 02:56:56  steve
 *  Macintosh compilers do not support ident.
 *
 * Revision 1.10  1999/12/11 05:45:41  steve
 *  Fix support for attaching attributes to primitive gates.
 *
 * Revision 1.9  1999/11/27 19:07:58  steve
 *  Support the creation of scopes.
 *
 * Revision 1.8  1999/11/19 05:02:15  steve
 *  Handle inverted clock into OUTFF.
 *
 * Revision 1.7  1999/11/19 03:02:25  steve
 *  Detect flip-flops connected to opads and turn
 *  them into OUTFF devices. Inprove support for
 *  the XNF-LCA attribute in the process.
 *
 * Revision 1.6  1999/11/18 02:58:37  steve
 *  Handle (with a warning) unconnected opads.
 *
 * Revision 1.5  1999/11/02 04:55:01  steve
 *  repair the sense of T from bufif01
 *
 * Revision 1.4  1999/11/02 01:43:55  steve
 *  Fix iobuf and iobufif handling.
 *
 * Revision 1.3  1999/10/09 17:52:27  steve
 *  support XNF OBUFT devices.
 *
 * Revision 1.2  1999/07/17 22:01:14  steve
 *  Add the functor interface for functor transforms.
 *
 * Revision 1.1  1998/12/07 04:53:17  steve
 *  Generate OBUF or IBUF attributes (and the gates
 *  to garry them) where a wire is a pad. This involved
 *  figuring out enough of the netlist to know when such
 *  was needed, and to generate new gates and signales
 *  to handle what's missing.
 *
 */

