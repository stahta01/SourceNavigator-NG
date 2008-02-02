# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996,2007 Oracle.  All rights reserved.
#
# $Id: lock004.tcl,v 12.5 2007/05/17 15:15:55 bostic Exp $
#
# TEST	lock004
# TEST	Test locker ids wraping around.

proc lock004 {} {
	source ./include.tcl
	global lock_curid
	global lock_maxid

	set save_curid $lock_curid
	set save_maxid $lock_maxid

	set lock_curid [expr $lock_maxid - 1]
	puts "Lock004: Locker id wraparound test"
	puts "\tLock004.a: repeat lock001-lock003 with wraparound lockids"

	lock001
	lock002
	lock003

	set lock_curid $save_curid
	set lock_maxid $save_maxid
}
