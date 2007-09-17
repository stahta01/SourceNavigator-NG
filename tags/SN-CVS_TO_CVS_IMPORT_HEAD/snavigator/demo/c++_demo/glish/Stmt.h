// $Header$


#ifndef stmt_h
#define stmt_h

#include "Glish/List.h"

#include "Func.h"
#include "Event.h"


class Stmt;
class EventDesignator;
class Agent;
class Task;
class Sequencer;

declare(PList,Stmt);
typedef PList(Stmt) stmt_list;

declare(PDict,stmt_list);
typedef PDict(stmt_list) stmt_list_dict;


typedef enum {
	FLOW_NEXT,		// continue on to next statement
	FLOW_LOOP,		// go to top of loop
	FLOW_BREAK,		// break out of loop
	FLOW_RETURN		// return from function
	} stmt_flow_type;

class Stmt : public GlishObject {
    public:
	Stmt()
		{ index = 0; }

	// Exec() tells a statement to go ahead and execute.  We use
	// it as a wrapper around the actual execute member function
	// so we can reset the current line number and perform any
	// other "global" statement execution (such as setting the
	// control flow to default to FLOW_NEXT).
	//
	// The "value_needed" argument, indicates whether any value
	// produced by this statement is of interest (because the
	// statement is possibly the last one in a function, so the
	// value will become the function value).  Note that "value_needed"
	// is advisory; some statements (like "return") will always
	// return a value regardless of "value_needed"'s setting.
	//
	// Exec() returns a value associated with the statement or 0
	// if there is none, and in "flow" returns a stmt_flow_type
	// indicating control flow information.
	virtual Value* Exec( int value_needed, stmt_flow_type& flow );

	// Called when an event we've expressed interest in has arrived.
	// The argument specifies the Agent associated with the
	// event.
	virtual void Notify( Agent* agent );

	// Returns true if we're currently active for the given event
	// generated by the given agent, with the given value; false
	// otherwise.  Only actually used by "whenever" statements.
	virtual int IsActiveFor( Agent* agent, const char* field,
					Value* value ) const;

	// Sets the statement's activity, either to true (if "activate" is
	// true) or to false.
	virtual void SetActivity( int activate );

	// Return the index of this statement.  Might be 0, indicating
	// that the statement is not intended to be indexed (presently,
	// only "whenever" statements are meant to be indexed).
	int Index() const	{ return index; }

    protected:
	// DoExec() does the real work of executing the statement.
	virtual Value* DoExec( int value_needed, stmt_flow_type& flow ) = 0;

	int index;
	};


class SeqStmt : public Stmt {
    public:
	SeqStmt( Stmt* arg_lhs, Stmt* arg_rhs );

	Value* DoExec( int value_needed, stmt_flow_type& flow );
	void Describe( ostream& s ) const;

    protected:
	Stmt* lhs;
	Stmt* rhs;
	};


class WheneverStmt : public Stmt {
    public:
	WheneverStmt( event_list* arg_trigger, Stmt* arg_stmt,
			Sequencer* arg_sequencer );

	virtual ~WheneverStmt();

	Value* DoExec( int value_needed, stmt_flow_type& flow );
	void Notify( Agent* agent );

	int IsActiveFor( Agent* agent, const char* field, Value* value ) const;
	void SetActivity( int activate );

	void Describe( ostream& s ) const;

    protected:
	event_list* trigger;
	Stmt* stmt;
	Sequencer* sequencer;
	int active;
	};


class LinkStmt : public Stmt {
    public:
	LinkStmt( event_list* source, event_list* sink, Sequencer* sequencer );

	Value* DoExec( int value_needed, stmt_flow_type& flow );
	void Describe( ostream& s ) const;

    protected:
	void MakeLink( Task* src, const char* source_event,
			Task* snk, const char* sink_event );

	virtual void LinkAction( Task* src, Value* v );

	event_list* source;
	event_list* sink;
	Sequencer* sequencer;
	};


class UnLinkStmt : public LinkStmt {
    public:
	UnLinkStmt( event_list* source, event_list* sink,
			Sequencer* sequencer );

    protected:
	void LinkAction( Task* src, Value* v );
	};


class AwaitStmt : public Stmt {
    public:
	AwaitStmt( event_list* arg_await_list, int arg_only_flag,
		   event_list* arg_except_list,
		   Sequencer* arg_sequencer );

	Value* DoExec( int value_needed, stmt_flow_type& flow );
	void Describe( ostream& s ) const;

    protected:
	event_list* await_list;
	int only_flag;
	event_list* except_list;
	Sequencer* sequencer;
	Stmt* except_stmt;
	};


class ActivateStmt : public Stmt {
    public:
	ActivateStmt( int activate, Expr* e, Sequencer* sequencer );

	Value* DoExec( int value_needed, stmt_flow_type& flow );
	void Describe( ostream& s ) const;

    protected:
	int activate;
	Expr* expr;
	Sequencer* sequencer;
	};


class IfStmt : public Stmt {
    public:
	IfStmt( Expr* arg_expr,
		Stmt* arg_true_branch,
		Stmt* arg_false_branch );

	Value* DoExec( int value_needed, stmt_flow_type& flow );
	void Describe( ostream& s ) const;

    protected:
	Expr* expr;
	Stmt* true_branch;
	Stmt* false_branch;
	};


class ForStmt : public Stmt {
    public:
	ForStmt( Expr* index_expr, Expr* range_expr,
		 Stmt* body_stmt );

	Value* DoExec( int value_needed, stmt_flow_type& flow );
	void Describe( ostream& s ) const;

    protected:
	Expr* index;
	Expr* range;
	Stmt* body;
	};


class WhileStmt : public Stmt {
    public:
	WhileStmt( Expr* test_expr, Stmt* body_stmt );

	Value* DoExec( int value_needed, stmt_flow_type& flow );
	void Describe( ostream& s ) const;

    protected:
	Expr* test;
	Stmt* body;
	};


class PrintStmt : public Stmt {
    public:
	PrintStmt( parameter_list* arg_args )
		{
		args = arg_args;
		description = "print";
		}

	Value* DoExec( int value_needed, stmt_flow_type& flow );
	void Describe( ostream& s ) const;

    protected:
	parameter_list* args;
	};


class ExprStmt : public Stmt {
    public:
	ExprStmt( Expr* arg_expr )
		{ expr = arg_expr; description = "expression"; }

	Value* DoExec( int value_needed, stmt_flow_type& flow );
	void Describe( ostream& s ) const;
	void DescribeSelf( ostream& s ) const;

    protected:
	Expr* expr;
	};


class ExitStmt : public Stmt {
    public:
	ExitStmt( Expr* arg_status, Sequencer* arg_sequencer )
		{
		description = "exit";
		status = arg_status;
		sequencer = arg_sequencer;
		}

	Value* DoExec( int value_needed, stmt_flow_type& flow );
	void Describe( ostream& s ) const;

    protected:
	Expr* status;
	Sequencer* sequencer;
	};


class LoopStmt : public Stmt {
    public:
	LoopStmt()	{ description = "next"; }

	Value* DoExec( int value_needed, stmt_flow_type& flow );
	};


class BreakStmt : public Stmt {
    public:
	BreakStmt()	{ description = "break"; }

	Value* DoExec( int value_needed, stmt_flow_type& flow );
	};


class ReturnStmt : public Stmt {
    public:
	ReturnStmt( Expr* arg_retval )
		{ description = "return"; retval = arg_retval; }

	Value* DoExec( int value_needed, stmt_flow_type& flow );
	void Describe( ostream& s ) const;

    protected:
	Expr* retval;
	};


class NullStmt : public Stmt {
    public:
	NullStmt()	{ description = ";"; }

	Value* DoExec( int value_needed, stmt_flow_type& flow );
	};


extern Stmt* null_stmt;
extern Stmt* merge_stmts( Stmt* stmt1, Stmt* stmt2 );

#endif /* stmt_h */