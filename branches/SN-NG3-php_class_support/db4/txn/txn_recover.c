/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001,2007 Oracle.  All rights reserved.
 *
 * $Id: txn_recover.c,v 12.29 2007/06/29 00:25:02 margo Exp $
 */

#include "db_config.h"

#include "db_int.h"
#include "dbinc/txn.h"
#include "dbinc/db_page.h"
#include "dbinc/db_dispatch.h"
#include "dbinc/log.h"
#include "dbinc_auto/db_auto.h"
#include "dbinc_auto/crdel_auto.h"
#include "dbinc_auto/db_ext.h"

/*
 * __txn_map_gid
 *	Return the txn that corresponds to this global ID.
 *
 * PUBLIC: int __txn_map_gid __P((DB_ENV *,
 * PUBLIC:     u_int8_t *, TXN_DETAIL **, roff_t *));
 */
int
__txn_map_gid(dbenv, gid, tdp, offp)
	DB_ENV *dbenv;
	u_int8_t *gid;
	TXN_DETAIL **tdp;
	roff_t *offp;
{
	DB_TXNMGR *mgr;
	DB_TXNREGION *region;

	mgr = dbenv->tx_handle;
	region = mgr->reginfo.primary;

	/*
	 * Search the internal active transaction table to find the
	 * matching xid.  If this is a performance hit, then we
	 * can create a hash table, but I doubt it's worth it.
	 */
	TXN_SYSTEM_LOCK(dbenv);
	SH_TAILQ_FOREACH(*tdp, &region->active_txn, links, __txn_detail)
		if (memcmp(gid, (*tdp)->xid, sizeof((*tdp)->xid)) == 0)
			break;
	TXN_SYSTEM_UNLOCK(dbenv);

	if (*tdp == NULL)
		return (EINVAL);

	*offp = R_OFFSET(&mgr->reginfo, *tdp);
	return (0);
}

/*
 * __txn_recover_pp --
 *	DB_ENV->txn_recover pre/post processing.
 *
 * PUBLIC: int __txn_recover_pp
 * PUBLIC:     __P((DB_ENV *, DB_PREPLIST *, long, long *, u_int32_t));
 */
int
__txn_recover_pp(dbenv, preplist, count, retp, flags)
	DB_ENV *dbenv;
	DB_PREPLIST *preplist;
	long count, *retp;
	u_int32_t flags;
{
	DB_THREAD_INFO *ip;
	int ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(
	    dbenv, dbenv->tx_handle, "txn_recover", DB_INIT_TXN);

	if (F_ISSET((DB_TXNREGION *)dbenv->tx_handle->reginfo.primary,
	    TXN_IN_RECOVERY)) {
		__db_errx(dbenv, "operation not permitted while in recovery");
		return (EINVAL);
	}

	if (flags != DB_FIRST && flags != DB_NEXT)
		return (__db_ferr(dbenv, "DB_ENV->txn_recover", 0));

	ENV_ENTER(dbenv, ip);
	REPLICATION_WRAP(dbenv,
	    (__txn_recover(dbenv, preplist, count, retp, flags)), ret);
	ENV_LEAVE(dbenv, ip);
	return (ret);
}

/*
 * __txn_recover --
 *	DB_ENV->txn_recover.
 *
 * PUBLIC: int __txn_recover
 * PUBLIC:         __P((DB_ENV *, DB_PREPLIST *, long, long *, u_int32_t));
 */
int
__txn_recover(dbenv, preplist, count, retp, flags)
	DB_ENV *dbenv;
	DB_PREPLIST *preplist;
	long count, *retp;
	u_int32_t flags;
{
	/*
	 * Public API to retrieve the list of prepared, but not yet committed
	 * transactions.  See __txn_get_prepared for details.  This function
	 * and __db_xa_recover both wrap that one.
	 */
	return (__txn_get_prepared(dbenv, NULL, preplist, count, retp, flags));
}

/*
 * __txn_get_prepared --
 *	Returns a list of prepared (and for XA, heuristically completed)
 *	transactions (less than or equal to the count parameter).  One of
 *	xids or txns must be set to point to an array of the appropriate type.
 *	The count parameter indicates the number of entries in the xids and/or
 *	txns array. The retp parameter will be set to indicate the number of
 *	entries	returned in the xids/txns array.  Flags indicates the operation,
 *	one of DB_FIRST or DB_NEXT.
 *
 * PUBLIC: int __txn_get_prepared __P((DB_ENV *,
 * PUBLIC:     XID *, DB_PREPLIST *, long, long *, u_int32_t));
 */
int
__txn_get_prepared(dbenv, xids, txns, count, retp, flags)
	DB_ENV *dbenv;
	XID *xids;
	DB_PREPLIST *txns;
	long count;		/* This is long for XA compatibility. */
	long *retp;
	u_int32_t flags;
{
	DB_LSN min;
	DB_PREPLIST *prepp;
	DB_TXNMGR *mgr;
	DB_TXNREGION *region;
	TXN_DETAIL *td;
	XID *xidp;
	long i;
	int restored, ret;

	*retp = 0;

	MAX_LSN(min);
	prepp = txns;
	xidp = xids;
	restored = ret = 0;

	/*
	 * If we are starting a scan, then we traverse the active transaction
	 * list once making sure that all transactions are marked as not having
	 * been collected.  Then on each pass, we mark the ones we collected
	 * so that if we cannot collect them all at once, we can finish up
	 * next time with a continue.
	 */

	mgr = dbenv->tx_handle;
	region = mgr->reginfo.primary;

	/*
	 * During this pass we need to figure out if we are going to need
	 * to open files.  We need to open files if we've never collected
	 * before (in which case, none of the COLLECTED bits will be set)
	 * and the ones that we are collecting are restored (if they aren't
	 * restored, then we never crashed; just the main server did).
	 */
	TXN_SYSTEM_LOCK(dbenv);

	/* Now begin collecting active transactions. */
	for (td = SH_TAILQ_FIRST(&region->active_txn, __txn_detail);
	    td != NULL && *retp < count;
	    td = SH_TAILQ_NEXT(td, links, __txn_detail)) {
		if (td->status != TXN_PREPARED ||
		    (flags != DB_FIRST && F_ISSET(td, TXN_DTL_COLLECTED)))
			continue;

		if (F_ISSET(td, TXN_DTL_RESTORED))
			restored = 1;

		if (xids != NULL) {
			xidp->formatID = td->format;
			/*
			 * XID structure uses longs; we use u_int32_t's as we
			 * log them to disk.  Cast them to make the conversion
			 * explicit.
			 */
			xidp->gtrid_length = (long)td->gtrid;
			xidp->bqual_length = (long)td->bqual;
			memcpy(xidp->data, td->xid, sizeof(td->xid));
			xidp++;
		}

		if (txns != NULL) {
			if ((ret = __os_calloc(dbenv,
			    1, sizeof(DB_TXN), &prepp->txn)) != 0) {
				TXN_SYSTEM_UNLOCK(dbenv);
				goto err;
			}
			if ((ret = __txn_continue(dbenv, prepp->txn, td)) != 0)
				goto err;
			F_SET(prepp->txn, TXN_MALLOC);
			memcpy(prepp->gid, td->xid, sizeof(td->xid));
			prepp++;
		}

		if (!IS_ZERO_LSN(td->begin_lsn) &&
		    LOG_COMPARE(&td->begin_lsn, &min) < 0)
			min = td->begin_lsn;

		(*retp)++;
		F_SET(td, TXN_DTL_COLLECTED);
	}
	if (flags == DB_FIRST)
		for (; td != NULL; td = SH_TAILQ_NEXT(td, links, __txn_detail))
			F_CLR(td, TXN_DTL_COLLECTED);
	TXN_SYSTEM_UNLOCK(dbenv);

	/*
	 * Now link all the transactions into the transaction manager's list.
	 */
	if (txns != NULL && *retp != 0) {
		MUTEX_LOCK(dbenv, mgr->mutex);
		for (i = 0; i < *retp; i++)
			TAILQ_INSERT_TAIL(&mgr->txn_chain, txns[i].txn, links);
		MUTEX_UNLOCK(dbenv, mgr->mutex);

		/*
		 * If we are restoring, update our count of outstanding
		 * transactions.
		 */
		if (REP_ON(dbenv)) {
			REP_SYSTEM_LOCK(dbenv);
			dbenv->rep_handle->region->op_cnt += (u_long)*retp;
			REP_SYSTEM_UNLOCK(dbenv);
		}

	}
	/*
	 * If recovery already opened the files for us, don't
	 * do it here.
	 */
	if (restored != 0 && flags == DB_FIRST &&
	    !F_ISSET(dbenv->lg_handle, DBLOG_OPENFILES))
		ret = __txn_openfiles(dbenv, &min, 0);

	if (0) {
err:		TXN_SYSTEM_UNLOCK(dbenv);
	}
	return (ret);
}

/*
 * __txn_openfiles --
 *	Call env_openfiles.
 *
 * PUBLIC: int __txn_openfiles __P((DB_ENV *, DB_LSN *, int));
 */
int
__txn_openfiles(dbenv, min, force)
	DB_ENV *dbenv;
	DB_LSN *min;
	int force;
{
	DBT data;
	DB_LOGC *logc;
	DB_LSN open_lsn;
	DB_TXNHEAD *txninfo;
	__txn_ckp_args *ckp_args;
	int ret, t_ret;

	/*
	 * Figure out the last checkpoint before the smallest
	 * start_lsn in the region.
	 */
	logc = NULL;
	if ((ret = __log_cursor(dbenv, &logc)) != 0)
		goto err;

	memset(&data, 0, sizeof(data));
	if ((ret = __txn_getckp(dbenv, &open_lsn)) == 0)
		while (!IS_ZERO_LSN(open_lsn) && (ret =
		    __logc_get(logc, &open_lsn, &data, DB_SET)) == 0 &&
		    (force ||
		    (min != NULL && LOG_COMPARE(min, &open_lsn) < 0))) {
			/* Format the log record. */
			if ((ret = __txn_ckp_read(dbenv,
			    data.data, &ckp_args)) != 0) {
				__db_errx(dbenv,
			    "Invalid checkpoint record at [%lu][%lu]",
				    (u_long)open_lsn.file,
				    (u_long)open_lsn.offset);
				goto err;
			}
			/*
			 * If force is set, then we're forcing ourselves
			 * to go back far enough to open files.
			 * Use ckp_lsn and then break out of the loop.
			 */
			open_lsn = force ? ckp_args->ckp_lsn :
			    ckp_args->last_ckp;
			__os_free(dbenv, ckp_args);
			if (force) {
				if ((ret = __logc_get(logc, &open_lsn,
				    &data, DB_SET)) != 0)
					goto err;
				break;
			}
		}

	/*
	 * There are several ways by which we may have gotten here.
	 * - We got a DB_NOTFOUND -- we need to read the first
	 *	log record.
	 * - We found a checkpoint before min.  We're done.
	 * - We found a checkpoint after min who's last_ckp is 0.  We
	 *	need to start at the beginning of the log.
	 * - We are forcing an openfiles and we have our ckp_lsn.
	 */
	if ((ret == DB_NOTFOUND || IS_ZERO_LSN(open_lsn)) && (ret =
	    __logc_get(logc, &open_lsn, &data, DB_FIRST)) != 0) {
		__db_errx(dbenv, "No log records");
		goto err;
	}

	if ((ret = __db_txnlist_init(dbenv, 0, 0, NULL, &txninfo)) != 0)
		goto err;
	ret = __env_openfiles(dbenv, logc,
	    txninfo, &data, &open_lsn, NULL, 0, 0);
	if (txninfo != NULL)
		__db_txnlist_end(dbenv, txninfo);

err:
	if (logc != NULL && (t_ret = __logc_close(logc)) != 0 && ret == 0)
		ret = t_ret;
	return (ret);
}
