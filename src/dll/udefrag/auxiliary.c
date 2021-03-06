/*
 *  UltraDefrag - a powerful defragmentation tool for Windows NT.
 *  Copyright (c) 2007-2017 Dmitri Arkhangelski (dmitriar@gmail.com).
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/**
 * @file auxiliary.c
 * @brief Auxiliary routines.
 * @addtogroup Auxiliary
 * @{
 */

/*
* Ideas by Dmitri Arkhangelski <dmitriar@gmail.com>
* and Stefan Pendl <stefanpe@users.sourceforge.net>.
*/

#include "udefrag-internals.h"

/**
 * @internal
 * @brief Displays generic information
 * about the program and the operating
 * system.
 */
void dbg_print_header(udefrag_job_parameters *jp)
{
    int os_version;
    int mj, mn;
    winx_time t;

    /* print driver version */
    winx_dbg_print_header(0,0,I"*");
    winx_dbg_print_header(0x20,0,I"%s",VERSIONINTITLE);

    /* print windows version */
    //decide whether to call it again just in case someone
    //  did not pre-load the version before trying to print the header.
    os_version = jp->win_version ? jp->win_version : winx_get_os_version();
    mj = os_version / 10;
    mn = os_version % 10;
    winx_dbg_print_header(0x20,0,I"Windows NT %u.%u",mj,mn);
    
    /* print date and time */
    memset(&t,0,sizeof(winx_time));
    (void)winx_get_local_time(&t);
    winx_dbg_print_header(0x20,0,I"[%02i.%02i.%04i at %02i:%02i]",
        (int)t.day,(int)t.month,(int)t.year,(int)t.hour,(int)t.minute);
    winx_dbg_print_header(0,0,I"*");
}

/**
 * @internal
 * @brief Displays a message like
 * <b>analysis of c: started</b>
 * and returns the current time
 * (needed for stop_timing).
 */
ULONGLONG start_timing(char *operation_name,udefrag_job_parameters *jp)
{
    winx_dbg_print_header(0,0,I"%s of %c: started",operation_name,jp->volume_letter);
    jp->progress_trigger = 0;
    return winx_xtime();
}

/**
 * @internal
 * @brief Displays how much time
 * the specified operation took.
 * @note The start_time parameter
 * must be obtained from the
 * start_timing procedure.
 */
ULONGLONG stop_timing(char *operation_name,ULONGLONG start_time,udefrag_job_parameters *jp)
{
    ULONGLONG time, seconds,returntime;
    char buffer[32];
    
    returntime = time = winx_xtime() - start_time;
    seconds = time / 1000;
    winx_time2str(seconds,buffer,sizeof buffer);
    time -= seconds * 1000;
    winx_dbg_print_header(0,0,I"%s of %c: completed in %s %I64ums",
        operation_name,jp->volume_letter,buffer,time);
    jp->progress_trigger = 0;
    return returntime;
}

/**
 * @internal
 * @brief Displays a single
 * performance counter.
 */
static void dbg_print_single_counter(udefrag_job_parameters *jp,ULONGLONG counter,char *name)
{
    ULONGLONG time, seconds;
    double p;
    unsigned int ip;
    char buffer[32];
    char *s;

    time = counter;
    seconds = time / 1000;
    winx_time2str(seconds,buffer,sizeof(buffer));
    time -= seconds * 1000;
    
    p = calc_percentage(counter,jp->p_counters.overall_time);
    ip = (unsigned int)(p * 100);
    s = winx_sprintf("%s %I64ums",buffer,time);
    if(s == NULL){
        itrace(" - %s %-18s %6I64ums  %3u.%02u %%",name,buffer,time,ip / 100,ip % 100);
    } else {
        itrace(" - %s %-25s  %3u.%02u %%",name,s,ip / 100,ip % 100);
        winx_free(s);
    }
}

/**
 * @internal
 * @brief Displays all the
 * performance counters.
 */
void dbg_print_performance_counters(udefrag_job_parameters *jp)
{
    ULONGLONG time, seconds;
    char buffer[32];
    
    time = jp->p_counters.overall_time;
    seconds = time / 1000;
    winx_time2str(seconds,buffer,sizeof(buffer));
    time -= seconds * 1000;
    winx_dbg_print_header(0,0,I"*");
    itrace("volume processing completed in %s %I64ums:",buffer,time);
    dbg_print_single_counter(jp,jp->p_counters.analysis_time,             "analysis ...............");
    dbg_print_single_counter(jp,jp->p_counters.searching_time,            "searching ..............");
    dbg_print_single_counter(jp,jp->p_counters.moving_time,               "moving .................");
    dbg_print_single_counter(jp,jp->p_counters.temp_space_releasing_time, "releasing temp space ...");
}

/**
 * @internal
 * @brief Displays how much time
 * the entire disk processing job
 * took.
 */
void dbg_print_footer(udefrag_job_parameters *jp)
{
    winx_dbg_print_header(0,0,I"*");
    winx_dbg_print_header(0,0,I"Processing of %c: %s",
        jp->volume_letter, (jp->pi.completion_status > 0) ? "succeeded" : "failed");
    winx_dbg_print_header(0,0,I"*");
}

double calc_percentage(ULONGLONG x,ULONGLONG y)
{
    return (y == 0) ? 0.00 : (double)x / (double)y * 100.00;
}

/** @} */
