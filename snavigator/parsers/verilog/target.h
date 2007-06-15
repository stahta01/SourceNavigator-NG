#ifndef __target_H
#define __target_H
/*
 * Copyright (c) 1998-1999 Stephen Williams (steve@icarus.com)
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
#ident "$Id: target.h,v 1.1 2000/12/21 21:57:13 jrandrews Exp $"
#endif

# include  "netlist.h"
class ostream;

/*
 * This header file describes the types and constants used to describe
 * the possible target output types of the compiler. The backends
 * provide one of these in order to tell the previous steps what the
 * backend is able to do.
 */

/*
 * The backend driver is hooked into the compiler, and given a name,
 * by creating an instance of the target structure. The structure has
 * the name that the compiler will use to locate the driver, and a
 * pointer to a target_t object that is the actual driver.
 */
struct target {
      string name;
      struct target_t* meth;
};

/*
 * The emit process uses a target_t driver to send the completed
 * design to a file. It is up to the driver object to follow along in
 * the iteration through the design, generating output as it can.
 */

struct target_t {
      virtual ~target_t();

	/* Start the design. */
      virtual void start_design(ostream&os, const Design*);

	/* This is called once for each scope in the design, before
	   anything else is called. */
      virtual void scope(ostream&os, const NetScope*);

	/* Output an event object. Called for each named event in the scope. */
      virtual void event(ostream&os, const NetEvent*);

	/* Output a signal (called for each signal) */
      virtual void signal(ostream&os, const NetNet*);

	/* Output a memory (called for each memory object) */
      virtual void memory(ostream&os, const NetMemory*);

	/* Output a defined task. */
      virtual void task_def(ostream&, const NetTaskDef*);
      virtual void func_def(ostream&, const NetFuncDef*);

	/* LPM style components are handled here. */
      virtual void lpm_add_sub(ostream&os, const NetAddSub*);
      virtual void lpm_clshift(ostream&os, const NetCLShift*);
      virtual void lpm_compare(ostream&os, const NetCompare*);
      virtual void lpm_divide(ostream&os, const NetDivide*);
      virtual void lpm_ff(ostream&os, const NetFF*);
      virtual void lpm_mult(ostream&os, const NetMult*);
      virtual void lpm_mux(ostream&os, const NetMux*);
      virtual void lpm_ram_dq(ostream&os, const NetRamDq*);

	/* Output a gate (called for each gate) */
      virtual void logic(ostream&os, const NetLogic*);
      virtual void bufz(ostream&os, const NetBUFZ*);
      virtual void udp(ostream&os,  const NetUDP*);
      virtual void udp_comb(ostream&os,  const NetUDP_COMB*);
      virtual void net_assign(ostream&os, const NetAssign*);
      virtual void net_assign_nb(ostream&os, const NetAssignNB*);
      virtual void net_case_cmp(ostream&os, const NetCaseCmp*);
      virtual bool net_cassign(ostream&os, const NetCAssign*);
      virtual void net_const(ostream&os, const NetConst*);
      virtual bool net_force(ostream&os, const NetForce*);
      virtual void net_probe(ostream&os, const NetEvProbe*);

	/* Output a process (called for each process). It is up to the
	   target to recurse if desired. */
      virtual bool process(ostream&os, const NetProcTop*);

	/* Various kinds of process nodes are dispatched through these. */
      virtual void proc_assign(ostream&os, const NetAssign*);
      virtual void proc_assign_mem(ostream&os, const NetAssignMem*);
      virtual void proc_assign_nb(ostream&os, const NetAssignNB*);
      virtual void proc_assign_mem_nb(ostream&os, const NetAssignMemNB*);
      virtual bool proc_block(ostream&os, const NetBlock*);
      virtual void proc_case(ostream&os,  const NetCase*);
      virtual bool proc_cassign(ostream&os, const NetCAssign*);
      virtual void proc_condit(ostream&os, const NetCondit*);
      virtual bool proc_deassign(ostream&os, const NetDeassign*);
      virtual bool proc_disable(ostream&os, const NetDisable*);
      virtual bool proc_force(ostream&os, const NetForce*);
      virtual void proc_forever(ostream&os, const NetForever*);
      virtual bool proc_release(ostream&os, const NetRelease*);
      virtual void proc_repeat(ostream&os, const NetRepeat*);
      virtual bool proc_trigger(ostream&os, const NetEvTrig*);
      virtual void proc_stask(ostream&os, const NetSTask*);
      virtual void proc_utask(ostream&os, const NetUTask*);
      virtual bool proc_wait(ostream&os,  const NetEvWait*);
      virtual void proc_while(ostream&os, const NetWhile*);
      virtual bool proc_delay(ostream&os, const NetPDelay*);

	/* Done with the design. */
      virtual void end_design(ostream&os, const Design*);
};

/* This class is used by the NetExpr class to help with the scanning
   of expressions. */
struct expr_scan_t {
      virtual ~expr_scan_t();
      virtual void expr_const(const NetEConst*);
      virtual void expr_concat(const NetEConcat*);
      virtual void expr_ident(const NetEIdent*);
      virtual void expr_memory(const NetEMemory*);
      virtual void expr_scope(const NetEScope*);
      virtual void expr_sfunc(const NetESFunc*);
      virtual void expr_signal(const NetESignal*);
      virtual void expr_subsignal(const NetESubSignal*);
      virtual void expr_ternary(const NetETernary*);
      virtual void expr_ufunc(const NetEUFunc*);
      virtual void expr_unary(const NetEUnary*);
      virtual void expr_binary(const NetEBinary*);
};


/* The emit functions take a design and emit it to the output stream
   using the specified target. If the target is given by name, it is
   located in the target_table and used. */
extern bool emit(ostream&o, const Design*des, const char*type);

/* This function takes a fully qualified verilog name (which may have,
   for example, dots in it) and produces a mangled version that can be
   used by most any language. */
extern string mangle(const string&str);

/* This is the table of supported output targets. It is a null
   terminated array of pointers to targets. */
extern const struct target *target_table[];

/*
 * $Log: target.h,v $
 * Revision 1.1  2000/12/21 21:57:13  jrandrews
 * initial import
 *
 * Revision 1.40  2000/07/29 16:21:08  steve
 *  Report code generation errors through proc_delay.
 *
 * Revision 1.39  2000/07/27 05:13:44  steve
 *  Support elaboration of disable statements.
 *
 * Revision 1.38  2000/05/11 23:37:27  steve
 *  Add support for procedural continuous assignment.
 *
 * Revision 1.37  2000/05/04 03:37:59  steve
 *  Add infrastructure for system functions, move
 *  $time to that structure and add $random.
 *
 * Revision 1.36  2000/04/23 03:45:25  steve
 *  Add support for the procedural release statement.
 *
 * Revision 1.35  2000/04/22 04:20:20  steve
 *  Add support for force assignment.
 *
 * Revision 1.34  2000/04/12 04:23:58  steve
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
 * Revision 1.33  2000/04/10 05:26:06  steve
 *  All events now use the NetEvent class.
 *
 * Revision 1.32  2000/04/04 03:20:15  steve
 *  Simulate named event trigger and waits.
 *
 * Revision 1.31  2000/04/01 21:40:23  steve
 *  Add support for integer division.
 *
 * Revision 1.30  2000/03/29 04:37:11  steve
 *  New and improved combinational primitives.
 */
#endif
