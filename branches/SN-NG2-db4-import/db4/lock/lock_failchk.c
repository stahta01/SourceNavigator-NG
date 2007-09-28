/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2005,2007 Oracle.  All rights reserved.
 *
 * $Id: lock_failchk.c,v 12.13 2007/05/17 15:15:43 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"
#include "dbinc/lock.h"
#include "dbinc/txn.h"

/*
 * __lock_failchk --
 *	Check for locks held by dead threads of control.
 *
 * PUBLIC: int __lock_failchk __P((DB_ENV *));
 */
int
__lock_failchk(dbenv)
	DB_ENV *dbenv;
{
	DB_LOCKER *lip;
	DB_LOCKREGION *lrp;
	DB_LOCKREQ request;
	DB_LOCKTAB *lt;
	u_int32_t i;
	int ret;
	char buf[DB_THREADID_STRLEN];

	lt = dbenv->lk_handle;
	lrp = lt->reginfo.primary;

retry:	LOCK_SYSTEM_LOCK(dbenv);
	LOCK_LOCKERS(dbenv, lrp);

	ret = 0;
	for (i = 0; i < lrp->locker_t_size; i++)
		SH_TAILQ_FOREACH(lip, &lt->locker_tab[i], links, __db_locker) {
			/*
			 * If the locker is transactional, we can ignore it;
			 * __txn_failchk aborts any transactions the locker
			 * is involved in.
			 */
			if (lip->id >= TXN_MINIMUM)
				continue;

			/* If the locker is still alive, it's not a problem. */
			if (dbenv->is_alive(dbenv, lip->pid, lip->tid, 0))
				continue;

			/*
			 * We can only deal with read locks.  If the locker
			 * holds write locks we have to assume a Berkeley DB
			 * operation was interrupted with only 1-of-N pages
			 * modified.
			 */
			if (lip->nwrites != 0) {
				ret = __db_failed(dbenv,
				     "locker has write locks",
				     lip->pid, lip->tid);
				break;
			}

			/*
			 * Discard the locker and its read locks.
			 */
			__db_msg(dbenv, "Freeing locks for locker %#lx: %s",
			    (u_long)lip->id, dbenv->thread_id_string(
			    dbenv, lip->pid, lip->tid, buf));
			LOCK_SYSTEM_UNLOCK(dbenv);
			UNLOCK_LOCKERS(dbenv, lrp);
			memset(&request, 0, sizeof(request));
			request.op = DB_LOCK_PUT_ALL;
			if ((ret = __lock_vec(dbenv,
			    lip, 0, &request, 1, NULL)) != 0)
				return (ret);

			/*
			 * This locker is most likely referenced by a cursor
			 * which is owned by a dead thread.  Normally the
			 * cursor would be available for other threads
			 * but we assume the dead thread will never release
			 * it.
			 */
			if ((ret = __lock_freefamilylocker(lt, lip)) != 0)
				return (ret);
			goto retry;
		}

	LOCK_SYSTEM_UNLOCK(dbenv);
	UNLOCK_LOCKERS(dbenv, lrp);

	return (ret);
}
