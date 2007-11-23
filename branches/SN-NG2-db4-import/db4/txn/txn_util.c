/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001,2007 Oracle.  All rights reserved.
 *
 * $Id: txn_util.c,v 12.18 2007/06/13 18:21:32 ubell Exp $
 */

#include "db_config.h"

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/lock.h"
#include "dbinc/mp.h"
#include "dbinc/txn.h"
#include "dbinc/log.h"
#include "dbinc/db_am.h"

typedef struct __txn_event TXN_EVENT;
struct __txn_event {
	TXN_EVENT_T op;
	TAILQ_ENTRY(__txn_event) links;
	union {
		struct {
			/* Delayed close. */
			DB *dbp;
		} c;
		struct {
			/* Delayed remove. */
			char *name;
			u_int8_t *fileid;
			int inmem;
		} r;
		struct {
			/* Lock event. */
			DB_LOCK lock;
			DB_LOCKER *locker;
			DB *dbp;
		} t;
	} u;
};

/*
 * __txn_closeevent --
 *
 * Creates a close event that can be added to the [so-called] commit list, so
 * that we can redo a failed DB handle close once we've aborted the transaction.
 *
 * PUBLIC: int __txn_closeevent __P((DB_ENV *, DB_TXN *, DB *));
 */
int
__txn_closeevent(dbenv, txn, dbp)
	DB_ENV *dbenv;
	DB_TXN *txn;
	DB *dbp;
{
	int ret;
	TXN_EVENT *e;

	e = NULL;
	if ((ret = __os_calloc(dbenv, 1, sizeof(TXN_EVENT), &e)) != 0)
		return (ret);

	e->u.c.dbp = dbp;
	e->op = TXN_CLOSE;
	TAILQ_INSERT_TAIL(&txn->events, e, links);

	return (0);
}

/*
 * __txn_remevent --
 *
 * Creates a remove event that can be added to the commit list.
 *
 * PUBLIC: int __txn_remevent __P((DB_ENV *,
 * PUBLIC:       DB_TXN *, const char *, u_int8_t *, int));
 */
int
__txn_remevent(dbenv, txn, name, fileid, inmem)
	DB_ENV *dbenv;
	DB_TXN *txn;
	const char *name;
	u_int8_t *fileid;
	int inmem;
{
	int ret;
	TXN_EVENT *e;

	e = NULL;
	if ((ret = __os_calloc(dbenv, 1, sizeof(TXN_EVENT), &e)) != 0)
		return (ret);

	if ((ret = __os_strdup(dbenv, name, &e->u.r.name)) != 0)
		goto err;

	if (fileid != NULL) {
		if ((ret = __os_calloc(dbenv,
		    1, DB_FILE_ID_LEN, &e->u.r.fileid)) != 0)
			return (ret);
		memcpy(e->u.r.fileid, fileid, DB_FILE_ID_LEN);
	}

	e->u.r.inmem = inmem;
	e->op = TXN_REMOVE;
	TAILQ_INSERT_TAIL(&txn->events, e, links);

	return (0);

err:	if (e != NULL)
		__os_free(dbenv, e);

	return (ret);
}

/*
 * __txn_remrem --
 *	Remove a remove event because the remove has been superceeded,
 * by a create of the same name, for example.
 *
 * PUBLIC: void __txn_remrem __P((DB_ENV *, DB_TXN *, const char *));
 */
void
__txn_remrem(dbenv, txn, name)
	DB_ENV *dbenv;
	DB_TXN *txn;
	const char *name;
{
	TXN_EVENT *e, *next_e;

	for (e = TAILQ_FIRST(&txn->events); e != NULL; e = next_e) {
		next_e = TAILQ_NEXT(e, links);
		if (e->op != TXN_REMOVE || strcmp(name, e->u.r.name) != 0)
			continue;
		TAILQ_REMOVE(&txn->events, e, links);
		__os_free(dbenv, e->u.r.name);
		if (e->u.r.fileid != NULL)
			__os_free(dbenv, e->u.r.fileid);
		__os_free(dbenv, e);
	}

	return;
}

/*
 * __txn_lockevent --
 *
 * Add a lockevent to the commit-queue.  The lock event indicates a locker
 * trade.
 *
 * PUBLIC: int __txn_lockevent __P((DB_ENV *,
 * PUBLIC:     DB_TXN *, DB *, DB_LOCK *, DB_LOCKER *));
 */
int
__txn_lockevent(dbenv, txn, dbp, lock, locker)
	DB_ENV *dbenv;
	DB_TXN *txn;
	DB *dbp;
	DB_LOCK *lock;
	DB_LOCKER *locker;
{
	int ret;
	TXN_EVENT *e;

	if (!LOCKING_ON(dbenv))
		return (0);

	e = NULL;
	if ((ret = __os_calloc(dbenv, 1, sizeof(TXN_EVENT), &e)) != 0)
		return (ret);

	e->u.t.locker = locker;
	e->u.t.lock = *lock;
	e->u.t.dbp = dbp;
	e->op = TXN_TRADE;
	TAILQ_INSERT_TAIL(&txn->events, e, links);
	dbp->cur_txn = txn;

	return (0);
}

/*
 * __txn_remlock --
 *	Remove a lock event because the locker is going away.  We can remove
 * by lock (using offset) or by locker_id (or by both).
 *
 * PUBLIC: void __txn_remlock __P((DB_ENV *, DB_TXN *, DB_LOCK *, DB_LOCKER *));
 */
void
__txn_remlock(dbenv, txn, lock, locker)
	DB_ENV *dbenv;
	DB_TXN *txn;
	DB_LOCK *lock;
	DB_LOCKER *locker;
{
	TXN_EVENT *e, *next_e;

	for (e = TAILQ_FIRST(&txn->events); e != NULL; e = next_e) {
		next_e = TAILQ_NEXT(e, links);
		if ((e->op != TXN_TRADE && e->op != TXN_TRADED) ||
		    (e->u.t.lock.off != lock->off && e->u.t.locker != locker))
			continue;
		TAILQ_REMOVE(&txn->events, e, links);
		__os_free(dbenv, e);
	}

	return;
}

/*
 * __txn_doevents --
 * Process the list of events associated with a transaction.  On commit,
 * apply the events; on abort, just toss the entries.
 *
 * PUBLIC: int __txn_doevents __P((DB_ENV *, DB_TXN *, int, int));
 */
#define	DO_TRADE do {							\
	memset(&req, 0, sizeof(req));					\
	req.lock = e->u.t.lock;						\
	req.op = DB_LOCK_TRADE;						\
	t_ret = __lock_vec(dbenv, e->u.t.locker, 0, &req, 1, NULL);	\
	if (t_ret == 0)	{						\
		e->u.t.dbp->cur_locker = e->u.t.locker;			\
		e->u.t.dbp->cur_txn = NULL;				\
	} else if (t_ret == DB_NOTFOUND)				\
		t_ret = 0;						\
	if (t_ret != 0 && ret == 0)					\
		ret = t_ret;						\
	e->op = TXN_TRADED;						\
} while (0)

int
__txn_doevents(dbenv, txn, opcode, preprocess)
	DB_ENV *dbenv;
	DB_TXN *txn;
	int opcode, preprocess;
{
	DB_LOCKREQ req;
	TXN_EVENT *e;
	int ret, t_ret;

	ret = 0;

	/*
	 * This phase only gets called if we have a phase where we
	 * release read locks.  Since not all paths will call this
	 * phase, we have to check for it below as well.  So, when
	 * we do the trade, we update the opcode of the entry so that
	 * we don't try the trade again.
	 */
	if (preprocess) {
		for (e = TAILQ_FIRST(&txn->events);
		    e != NULL; e = TAILQ_NEXT(e, links)) {
			if (e->op != TXN_TRADE ||
			    IS_WRITELOCK(e->u.t.lock.mode))
				continue;
			DO_TRADE;
		}
		return (ret);
	}

	/*
	 * Prepare should only cause a preprocess, since the transaction
	 * isn't over.
	 */
	DB_ASSERT(dbenv, opcode != TXN_PREPARE);
	while ((e = TAILQ_FIRST(&txn->events)) != NULL) {
		TAILQ_REMOVE(&txn->events, e, links);
		/*
		 * Most deferred events should only happen on
		 * commits, not aborts or prepares.  The one exception
		 * is a close which gets done on commit and abort, but
		 * not prepare. If we're not doing operations, then we
		 * can just go free resources.
		 */
		if (opcode == TXN_ABORT && e->op != TXN_CLOSE)
			goto dofree;
		switch (e->op) {
		case TXN_CLOSE:
			if ((t_ret = __db_close(e->u.c.dbp,
			    NULL, DB_NOSYNC)) != 0 && ret == 0)
				ret = t_ret;
			break;
		case TXN_REMOVE:
			if (e->u.r.fileid != NULL) {
				if ((t_ret = __memp_nameop(dbenv,
				    e->u.r.fileid, NULL, e->u.r.name,
				    NULL, e->u.r.inmem)) != 0 && ret == 0)
					ret = t_ret;
			} else if ((t_ret =
			    __os_unlink(dbenv, e->u.r.name)) != 0 && ret == 0)
				ret = t_ret;
			break;
		case TXN_TRADE:
			DO_TRADE;
			/* Fall through */
		case TXN_TRADED:
			/* Downgrade the lock. */
			if ((t_ret = __lock_downgrade(dbenv,
			    &e->u.t.lock, DB_LOCK_READ, 0)) != 0 && ret == 0)
				ret = t_ret;
			break;
		default:
			/* This had better never happen. */
			DB_ASSERT(dbenv, 0);
		}
dofree:
		/* Free resources here. */
		switch (e->op) {
		case TXN_REMOVE:
			if (e->u.r.fileid != NULL)
				__os_free(dbenv, e->u.r.fileid);
			__os_free(dbenv, e->u.r.name);
			break;
		case TXN_TRADE:
			if (opcode == TXN_ABORT)
				e->u.t.dbp->cur_txn = NULL;
		case TXN_CLOSE:
		case TXN_TRADED:
		default:
			break;
		}
		__os_free(dbenv, e);
	}

	return (ret);
}

/*
 * PUBLIC: int __txn_record_fname __P((DB_ENV *, DB_TXN *, FNAME *));
 */
int
__txn_record_fname(dbenv, txn, fname)
	DB_ENV *dbenv;
	DB_TXN *txn;
	FNAME *fname;
{
	DB_TXNMGR *mgr;
	DB_LOG *dblp;
	TXN_DETAIL *td;
	roff_t fname_off;
	roff_t *np, *ldbs;
	u_int32_t i;
	int ret;

	if ((td = txn->td) == NULL)
		return (0);
	mgr = dbenv->tx_handle;
	dblp = dbenv->lg_handle;
	fname_off = R_OFFSET(&dblp->reginfo, fname);

	/* See if we already have a ref to this DB handle. */
	ldbs = R_ADDR(&mgr->reginfo, td->log_dbs);
	for (i = 0, np = ldbs; i < td->nlog_dbs; i++, np++)
		if (*np == fname_off)
			return (0);

	if (td->nlog_slots <= td->nlog_dbs) {
		TXN_SYSTEM_LOCK(dbenv);
		if ((ret = __env_alloc(&mgr->reginfo,
		    sizeof(roff_t) * (td->nlog_slots << 1), &np)) != 0)
			return (ret);
		memcpy(np, ldbs, td->nlog_dbs * sizeof(roff_t));
		if (td->nlog_slots > TXN_NSLOTS)
			__env_alloc_free(&mgr->reginfo, ldbs);

		TXN_SYSTEM_UNLOCK(dbenv);
		td->log_dbs = R_OFFSET(&mgr->reginfo, np);
		ldbs = np;
		td->nlog_slots = td->nlog_slots << 1;
	}

	ldbs[td->nlog_dbs] = fname_off;
	td->nlog_dbs++;
	fname->txn_ref++;

	return (0);
}

/*
 * __txn_dref_fnam --
 *	Either pass the fname to our parent txn or decrement the refcount
 * and close the fileid if it goes to zero.
 *
 * PUBLIC: int __txn_dref_fname __P((DB_ENV *, DB_TXN *));
 */
int
__txn_dref_fname(dbenv, txn)
	DB_ENV *dbenv;
	DB_TXN *txn;
{
	DB_TXNMGR *mgr;
	DB_LOG *dblp;
	FNAME *fname;
	roff_t *np;
	TXN_DETAIL *ptd, *td;
	u_int32_t i;
	int ret;

	td = txn->td;

	if (td->nlog_dbs == 0)
		return (0);

	mgr = dbenv->tx_handle;
	dblp = dbenv->lg_handle;
	ret = 0;

	ptd = txn->parent != NULL ? txn->parent->td : NULL;

	np = R_ADDR(&mgr->reginfo, td->log_dbs);
	for (i = 0; i < td->nlog_dbs; i++, np++) {
		fname = R_ADDR(&dblp->reginfo, *np);
		MUTEX_LOCK(dbenv, fname->mutex);
		if (ptd != NULL) {
			fname->txn_ref--;
			ret = __txn_record_fname(dbenv, txn->parent, fname);
			MUTEX_UNLOCK(dbenv, fname->mutex);
		} else if (fname->txn_ref == 1) {
			MUTEX_UNLOCK(dbenv, fname->mutex);
			DB_ASSERT(dbenv, fname->txn_ref != 0);
			ret = __dbreg_close_id_int(
			    dbenv, fname, DBREG_CLOSE, 0);
		} else {
			fname->txn_ref--;
			MUTEX_UNLOCK(dbenv, fname->mutex);
		}
		if (ret != 0)
			break;
	}

	return (ret);
}
