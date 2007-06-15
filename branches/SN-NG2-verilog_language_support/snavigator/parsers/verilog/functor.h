#ifndef __functor_H
#define __functor_H
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
#ident "$Id: functor.h,v 1.1 2000/12/21 21:57:13 jrandrews Exp $"
#endif

/*
 * The functor is an object that can be applied to a design to
 * transform it. This is different from the target_t, which can only
 * scan the design but not transform it in any way.
 *
 * When a functor it scanning a process, signal or node, the functor
 * is free to manipulate the list by deleting items, including the
 * node being scanned. The Design class scanner knows how to handle
 * the situation. However, if objects are added to the netlist, there
 * is no guarantee that object will be scanned unless the functor is
 * rerun.
 */

class Design;
class NetNet;
class NetProcTop;

struct functor_t {
      virtual ~functor_t();

	/* Events are scanned here. */
      virtual void event(class Design*des, class NetEvent*);

	/* This is called once for each signal in the design. */
      virtual void signal(class Design*des, class NetNet*);

	/* This method is called for each process in the design. */
      virtual void process(class Design*des, class NetProcTop*);

	/* This method is called for each structural adder. */
      virtual void lpm_add_sub(class Design*des, class NetAddSub*);

	/* This method is called for each structural comparator. */
      virtual void lpm_compare(class Design*des, class NetCompare*);

	/* This method is called for each structural constant. */
      virtual void lpm_const(class Design*des, class NetConst*);

	/* This method is called for each structural constant. */
      virtual void lpm_divide(class Design*des, class NetDivide*);

	/* This method is called for each FF in the design. */
      virtual void lpm_ff(class Design*des, class NetFF*);

	/* Handle LPM combinational logic devices. */
      virtual void lpm_logic(class Design*des, class NetLogic*);

	/* This method is called for each multiplier. */
      virtual void lpm_mult(class Design*des, class NetMult*);

	/* This method is called for each MUX. */
      virtual void lpm_mux(class Design*des, class NetMux*);
};

struct proc_match_t {
      virtual ~proc_match_t();

      virtual int assign(class NetAssign*);
      virtual int assign_mem(class NetAssignMem*);
      virtual int assign_nb(class NetAssignNB*);
      virtual int assign_mem_nb(class NetAssignMemNB*);
      virtual int condit(class NetCondit*);
      virtual int event_wait(class NetEvWait*);
      virtual int block(class NetBlock*);
};


/*
 * $Log: functor.h,v $
 * Revision 1.1  2000/12/21 21:57:13  jrandrews
 * initial import
 *
 * Revision 1.16  2000/08/01 02:48:42  steve
 *  Support <= in synthesis of DFF and ram devices.
 *
 * Revision 1.15  2000/07/16 04:56:07  steve
 *  Handle some edge cases during node scans.
 *
 * Revision 1.14  2000/07/15 05:13:44  steve
 *  Detect muxing Vz as a bufufN.
 *
 * Revision 1.13  2000/04/20 00:28:03  steve
 *  Catch some simple identity compareoptimizations.
 *
 * Revision 1.12  2000/04/18 04:50:19  steve
 *  Clean up unneeded NetEvent objects.
 *
 * Revision 1.11  2000/04/12 20:02:53  steve
 *  Finally remove the NetNEvent and NetPEvent classes,
 *  Get synthesis working with the NetEvWait class,
 *  and get started supporting multiple events in a
 *  wait in vvm.
 *
 * Revision 1.10  2000/04/01 21:40:22  steve
 *  Add support for integer division.
 *
 * Revision 1.9  2000/02/23 02:56:54  steve
 *  Macintosh compilers do not support ident.
 *
 * Revision 1.8  2000/02/13 04:35:43  steve
 *  Include some block matching from Larry.
 *
 * Revision 1.7  2000/01/13 03:35:35  steve
 *  Multiplication all the way to simulation.
 *
 * Revision 1.6  1999/12/30 04:19:12  steve
 *  Propogate constant 0 in low bits of adders.
 *
 * Revision 1.5  1999/12/17 06:18:16  steve
 *  Rewrite the cprop functor to use the functor_t interface.
 */
#endif
