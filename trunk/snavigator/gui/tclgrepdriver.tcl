# Copyright (c) 2002, Mo DeJong
# 
# This file is part of Source-Navigator.
# 
# Source-Navigator is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as published
# by the Free Software Foundation; either version 2, or (at your option)
# any later version.
# 
# Source-Navigator is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
# 
# You should have received a copy of the GNU General Public License along
# with Source-Navigator; see the file COPYING.  If not, write to
# the Free Software Foundation, 59 Temple Place - Suite 330, Boston,
# MA 02111-1307, USA.


itcl::class sourcenav::TclGrepDriver {
    inherit sourcenav::GrepDriver

    destructor {}

    public method start { pat files nocase max }

    public method finish {}

    public method cancel {}

    public method getScaleValueVariable {}

    public method setTextWidget { widget }

    # Invoked when ready to process next file
    private method processNextIndex {}

    # Invoked when file data is ready to be read
    private method inputReadyCallback {}

    # Invoked when all file data has been read
    private method doneReadingData {}

    # Variables

    # List of files to search
    private variable fileList
    private variable fileListLength
    private variable fileListIndex

    private variable currentFileName
    private variable currentFileBuffer
    private variable currentFileBytesToRead
    private variable currentFileFD ""

    # after id of next file process callback
    private variable nextIndexId ""

    # set to 1 if we should do case insensitive matching
    private variable ignorecase

    # Max number of matches to insert into text widget
    private variable maxmatches

    # set to 1 if grep was canceled
    private variable grep_canceled
    private variable grep_truncated

    private variable grep_running

    # Set to the pattern we are searching for only while
    # a grep is actually active, otherwise set to ""
    private variable activePattern ""

    # The text widget grep results will be added to
    private variable text ""

    # Variable linked to the progress bar display
    private variable scalevalue 0
}


itcl::body sourcenav::TclGrepDriver::destructor { } {
    ${this} cancel
}

itcl::body sourcenav::TclGrepDriver::start { pat files nocase max } {
    set activePattern $pat
    set ignorecase $nocase
    set maxmatches $max

    set fileList $files
    set fileListLength [llength $files]
    set fileListIndex 0
    set nextIndexId [after idle [itcl::code $this processNextIndex]]

    set grep_canceled 0
    set grep_truncated 0
    set grep_running 1

    sn_log "Start grep :\t\t[clock seconds]"

    # wait until finish is invoked
    vwait [itcl::scope grep_running]
}

itcl::body sourcenav::TclGrepDriver::finish { } {
    if {$grep_canceled} {
        $text insert end [get_indep String GrepCanceledString]
        $text insert end \n
    } elseif {$grep_truncated} {
        $text insert end [get_indep String GrepTruncatedString]
        $text insert end \n
    }
}

itcl::body sourcenav::TclGrepDriver::cancel {} {
    set grep_canceled 1
    after cancel $nextIndexId

    if {![string equal $currentFileFD ""]} {
        if {[catch {close $currentFileFD} err]} {
            sn_log "error during close : \"$err\""
        }
        set currentFileFD ""
    }
    set grep_running 0
}

itcl::body sourcenav::TclGrepDriver::getScaleValueVariable {} {
    return [itcl::scope scalevalue]
}

itcl::body sourcenav::TclGrepDriver::setTextWidget { widget } {
    set text $widget
}

itcl::body sourcenav::TclGrepDriver::processNextIndex { } {
    global sn_options
    cd $sn_options(sys,project-dir)

    set currentFileName [lindex $fileList $fileListIndex]
    incr fileListIndex

    # If a project file no longer exists, insert an error and move on
    if {![file exists $currentFileName]} {
        $text config -state normal
        $text insert end "grep: $currentFileName: "
        $text insert end [get_indep String GrepFileDoesNotExist]
        $text insert end "\n"
        $text config -state disabled

        if {$fileListIndex < $fileListLength} {
            set nextIndexId [after idle [itcl::code $this processNextIndex]]
        }
        return
    }

#    sn_log "TclGrepDriver::processNextIndex $currentFileName"

    set currentFileBuffer ""
    set currentFileBytesToRead [file size $currentFileName]

    set currentFileFD [open $currentFileName r]

    # This could do some strange things with binary files!
    # FIMXE: Could we improve performance with a lerger buffer size?
    fconfigure $currentFileFD -blocking 0 -buffering full \
        -encoding $sn_options(def,system-encoding)
    fileevent $currentFileFD readable [itcl::code $this inputReadyCallback]
}


itcl::body sourcenav::TclGrepDriver::inputReadyCallback {} {
#    sn_log "inputReadyCallback: reading $currentFileBytesToRead bytes"

    set buf [read $currentFileFD $currentFileBytesToRead]
    append currentFileBuffer $buf
    incr currentFileBytesToRead -[string length $buf]

#    sn_log "inputReadyCallback: read [string length $buf] bytes"

    if {$currentFileBytesToRead == 0 || [eof $currentFileFD]} {
        if {[catch {close $currentFileFD} err]} {
            sn_log "error during close : \"$err\""
        }
        set currentFileFD ""
        $this doneReadingData
    }
}

itcl::body sourcenav::TclGrepDriver::doneReadingData {} {
    set pat $activePattern
    set pat_len [string length $pat]

    set scalevalue $fileListIndex

    set results [sourcenav::line_grep $pat $currentFileBuffer $ignorecase]
    #sourcenav::print_line_grep_results $results

    if {[llength $results] > 0} {
        set t $text
        $t config -state normal
        set index_list [list]

        foreach result $results {
            set matchlinenum [lindex $result 0]
            set matchline [lindex $result 1]
            set matchoffs [lindex $result 2]
            set str "${currentFileName}:${matchlinenum}:"
            $t insert end $str
            set cur [$t index {end - 1 char}]
            $t insert end $matchline
            $t insert end \n

            foreach matchoff $matchoffs {
                set matchtext [lindex $matchoff 0]
                set matchlineoff [lindex $matchoff 1]
                lappend index_list [list $cur + $matchlineoff chars]
                lappend index_list [list $cur + $matchlineoff chars \
                    + [string length $matchtext] chars]
            }

            # If we have reached the max number of results, truncate now
            set numlines [expr {int([$t index end]) - 1}]
            if {$numlines >= $maxmatches} {
                sn_log "truncating grep results"
                set grep_truncated 1
                set fileListIndex $fileListLength
                break
            }
        }

        if {[llength $index_list] > 0} {
            #foreach {start end} $index_list {
            #    puts -nonewline "tagging the pair \{$start\} \{$end\},"
            #    puts " resolves to \{[$t index $start] [$t index $end]\}"
            #}
            eval {$t tag add grep} $index_list
        }

        $t see end
        $t configure -state disabled
    }

    if {$fileListIndex < $fileListLength} {
        set nextIndexId [after idle [itcl::code $this processNextIndex]]
    } else {
        sn_log "Done grep :\t\t[clock seconds]"
        set grep_running 0
    }
}


# Helper proc that implements line oriented grep functionality

proc sourcenav::line_grep { pattern buffer {ignorecase 0}} {

    if {[regexp "\n" $pattern]} {
        error "Pattern cannot contain a newline"
    }

    if {[string index $buffer end] != "\n"} {
        append buffer "\n"
    }

    if {$ignorecase} {
        set matches [regexp -all -line -inline -indices -nocase -- $pattern $buffer]
    } else {
        set matches [regexp -all -line -inline -indices -- $pattern $buffer]
    }

    if {[llength $matches] == 0} {
        return
    }

    # To optimize this function we could write a native method that would
    # take the list of matches (filtering out matched/non-matched subranges)
    # and return the line numbers for each matched region.

    # search the range (up to the last found match) for newlines and
    # return the line number for each found match.

    # Get indexes of the newlines in the buffer
    set newline_indexes [list]
    foreach ind [regexp -all -inline -indices -- "\n" $buffer] {
        lappend newline_indexes [lindex $ind 0]
    }
    set newline_length [llength $newline_indexes]
    if {$newline_length == 0} {error "no newlines found in buffer"}

    # Loop over matches and map indexes to line numbers

    set linenum 1
    set lineend 0
    set newline_index 0
    set matches_length [llength $matches]
    set results [list]

    for {set matches_index 0} {$matches_index < $matches_length} \
            {incr matches_index} {
        set match [lindex $matches $matches_index]
        set match_start [lindex $match 0]

        # Increment the newline count until we reach the line this match is in

        for {} {$newline_index < $newline_length} {incr newline_index} {
            set linestart $lineend
            set lineend [lindex $newline_indexes $newline_index]
            if {$lineend > $match_start} {
                break
            }
            incr linenum
        }

        set matches_this_line [list $match]

        # Check for additional matches on the same line

        for {set tmp_index [expr {$matches_index + 1}]} \
                {$tmp_index < $matches_length} {incr tmp_index} {
            set tmp_match [lindex $matches $tmp_index]
            set tmp_match_start [lindex $tmp_match 0]

            if {$tmp_match_start > $lineend} {
                break
            }
            # Filter out subexpressions, their range falls
            # within the range of the previous match
            # or they are returned as {-1 -1}
            if {$tmp_match_start != -1 &&
                    $tmp_match_start > [lindex $match 1]} {
                lappend matches_this_line $tmp_match
            }
            incr matches_index
        }

        # Generate results for this line
        # {MATCHLINENUM MATCHLINE {{MATCHTEXT OFFSET} ...}}

        if {$linestart == 0} {
            set actual_linestart 0
        } else {
            set actual_linestart [expr {$linestart + 1}]
        }

        set match_line [string range $buffer \
            $actual_linestart \
            [expr {$lineend - 1}]]

        set result [list $linenum $match_line]

        set mtoff [list]

        # Generate a list of pairs containing the matching text and
        # the offset from the beginning of the line.

        foreach match $matches_this_line {
            set match_start [lindex $match 0]
            set match_end [lindex $match 1]
            lappend mtoff [list [string range $buffer $match_start $match_end] \
                [expr {$match_start - $actual_linestart}]]
        }

        lappend result $mtoff
        lappend results $result
    }

    return $results
}


# Debugging helper proc to help visualize output of line_grep

proc sourcenav::print_line_grep_results { results } {
    foreach result $results {
        foreach {matchlinenum matchline matchoffs} $result break
        set str "FILENAME:${matchlinenum}:"
        puts -nonewline $str
        puts ${matchline}
        puts -nonewline [string repeat " " [string length $str]]

        set index 0
        foreach matchoff $matchoffs {
            foreach {matchtext matchoff} $matchoff break
            set num [expr {$matchoff - $index}]
            puts -nonewline [string repeat " " $num]
            incr index $num
            set num [string length $matchtext]
            puts -nonewline [string repeat "*" $num]
            incr index $num
        }
        puts ""
    }
}
