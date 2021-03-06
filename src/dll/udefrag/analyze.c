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
 * @file analyze.c
 * @brief Volume analysis.
 * @addtogroup Analysis
 * @{
 */

/*
* Ideas by Dmitri Arkhangelski <dmitriar@gmail.com>
* and Stefan Pendl <stefanpe@users.sourceforge.net>.
*/

#include "udefrag-internals.h"

static void update_progress_counters(winx_file_info *f,udefrag_job_parameters *jp);

/* This is how we distinguish FAT/NTFS */
fs_type_struct fs_types[8] = {
    { "NTFS",  FS_NTFS,  0,1 },
    { "FAT12", FS_FAT12, 1,0 },
    { "FAT",   FS_FAT16, 1,0 },
    { "FAT16", FS_FAT16, 1,0 },
    { "FAT32", FS_FAT32, 1,0 },
    { "EXFAT", FS_EXFAT, 1,0 },
    { "UDF",   FS_UDF,   0,0 },
    { NULL,    FS_UNKNOWN,0,0 }
};

/**
 * @internal
 * @brief Constant definitions for
 * the adjust_move_at_once_parameter
 * routine.
 */
#define _256K                           (256LL * 1024LL)
#define _4M                      (4LL * 1024LL * 1024LL)
#define _8M                      (8LL * 1024LL * 1024LL)
#define _16M                    (16LL * 1024LL * 1024LL)
#define _32M                    (32LL * 1024LL * 1024LL)
#define _64M                    (64LL * 1024LL * 1024LL)
#define _20G           (20LL * 1024LL * 1024LL * 1024LL)
#define _100G         (100LL * 1024LL * 1024LL * 1024LL)
#define _250G         (250LL * 1024LL * 1024LL * 1024LL)
#define _1T          (1024LL * 1024LL * 1024LL * 1024LL)
#define _2T    (2LL * 1024LL * 1024LL * 1024LL * 1024LL)

/**
 * @internal
 * @brief Defines how many clusters to move at once in the move_file routine.
 * @details This algorithm has been suggested by Joachim Otahal:
 * http://sourceforge.net/projects/ultradefrag/forums/forum/709672/topic/4779581
 */
static void adjust_move_at_once_parameter(udefrag_job_parameters *jp)
{
    ULONGLONG bytes_at_once;
    char buffer[32];
    
    /* comply with "one half second to stop defragmentation" rule */
    if(jp->v_info.device_capacity < _20G){
        bytes_at_once = _256K;
    } else if(jp->v_info.device_capacity < _100G){
        bytes_at_once = _4M;
    } else if(jp->v_info.device_capacity < _250G){
        bytes_at_once = _8M;
    } else if(jp->v_info.device_capacity < _1T){
        bytes_at_once = _16M;
    } else if(jp->v_info.device_capacity < _2T){
        bytes_at_once = _32M;
    } else {
        bytes_at_once = _64M;
    }
    jp->clusters_at_once = bytes_at_once / jp->v_info.bytes_per_cluster;
    if(jp->clusters_at_once == 0)
        jp->clusters_at_once ++;
    winx_bytes_to_hr(bytes_at_once,0,buffer,sizeof(buffer));
    itrace("the program will move %s (%I64u clusters) at once",
        buffer, jp->clusters_at_once);
}

/**
 * @internal
 * @brief Retrieves complete information about the disk.
 * @return Zero for success, negative value otherwise.
 * @note Resets statistics and cluster map.
 */
static int get_volume_information(udefrag_job_parameters *jp)
{
    char fs_name[MAX_FS_NAME_LENGTH + 1];
    int i;
    
    /* reset mft zone disposition */
    memset(&jp->mft_zone,0,sizeof(struct _mft_zone));

    /* reset v_info structure */
    memset(&jp->v_info,0,sizeof(winx_volume_information));
    
    /* reset statistics */
    jp->pi.files = 0;
    jp->pi.directories = 0;
    jp->pi.compressed = 0;
    jp->pi.fragmented = 0;
    jp->pi.fragments = 0;
    jp->pi.total_space = 0;
    jp->pi.free_space = 0;
    jp->pi.mft_size = 0;
    jp->pi.clusters_to_process = 0;
    jp->pi.processed_clusters = 0;
    
    jp->fs_type = FS_UNKNOWN;
    jp->is_fat = 0;
    jp->is_ntfs = 0;
    
    /* reset file lists */
    destroy_lists(jp);
    
    /* update global variables holding drive geometry */
    if(winx_get_volume_information(jp->volume_letter,&jp->v_info) < 0)
        return (-1);
    
    /* don't touch dirty volumes */
    if(jp->v_info.is_dirty)
        return UDEFRAG_DIRTY_VOLUME;

    jp->pi.total_space = jp->v_info.total_bytes;
    jp->pi.free_space = jp->v_info.free_bytes;
    itrace("total clusters: %I64u",jp->v_info.total_clusters);
    jp->pi.used_clusters = jp->v_info.total_clusters - (jp->v_info.free_bytes / jp->v_info.bytes_per_cluster);
    itrace("used clusters : %I64u",jp->pi.used_clusters);
    itrace("cluster size: %I64u",jp->v_info.bytes_per_cluster);
    /* validate geometry */
    if(!jp->v_info.total_clusters || !jp->v_info.bytes_per_cluster){
        etrace("wrong volume geometry detected");
        return (-1);
    }
    adjust_move_at_once_parameter(jp);
    /* check partition type */
    itrace("%s partition detected",jp->v_info.fs_name);
    strncpy(fs_name,jp->v_info.fs_name,MAX_FS_NAME_LENGTH);
    fs_name[MAX_FS_NAME_LENGTH] = 0;
    _strupr(fs_name);
    for(i = 0; fs_types[i].name; i++){
        if(!strcmp(fs_name,fs_types[i].name)){
            jp->fs_type = fs_types[i].type;
            jp->is_fat = fs_types[i].is_fat;
            jp->is_ntfs = fs_types[i].is_ntfs;
            break;
        }
    }
    if(jp->fs_type == FS_UNKNOWN){
        etrace("file system type is not recognized");
        etrace("type independent routines will be used to defragment it");
    }
    
    jp->pi.clusters_to_process = jp->v_info.total_clusters;
    jp->pi.processed_clusters = 0;
    
    if(jp->udo.fragment_size_threshold){
        if(jp->udo.fragment_size_threshold <= jp->v_info.bytes_per_cluster){
            itrace("fragment size threshold is below the cluster size, so it will be ignored");
            jp->udo.fragment_size_threshold = 0;
        }
    }

    /* reset cluster map */
    reset_cluster_map(jp);
    return 0;
}

/**
 * @internal
 * @brief get_free_space_layout helper.
 */
static int process_free_region(winx_volume_region *rgn,void *user_defined_data)
{
    udefrag_job_parameters *jp = (udefrag_job_parameters *)user_defined_data;
    
    if(jp->udo.dbgprint_level >= DBG_PARANOID)
        itrace("Free block start: %I64u len: %I64u",rgn->lcn,rgn->length);
    colorize_map_region(jp,rgn->lcn,rgn->length,FREE_SPACE,DEFAULT_COLOR);  //genBTC,-was using wrong alias.
    jp->pi.processed_clusters += rgn->length;
    jp->free_regions_count ++;
    return 0; /* continue scan */
}

/**
 * @internal
 * @brief Retrieves free space layout.
 * @return Zero for success, negative value otherwise.
 */
static int get_free_space_layout(udefrag_job_parameters *jp)
{
    char buffer[32];

    jp->free_regions = winx_get_free_volume_regions(jp->volume_letter,
        WINX_GVR_ALLOW_PARTIAL_SCAN,process_free_region,(void *)jp);
    
    winx_bytes_to_hr(jp->v_info.free_bytes,2,buffer,sizeof(buffer));
    itrace("free space amount : %s",buffer);
    itrace("free regions count: %u",jp->free_regions_count);
    
    /* let full disks to pass the analysis successfully */
    if(jp->free_regions == NULL || jp->free_regions_count == 0)
        etrace("disk is full or some error has been encountered");
    return 0;
}

/**
 * @internal
 * @brief Checks whether the specified 
 * region is inside of the volume.
 */
int check_region(udefrag_job_parameters *jp,ULONGLONG lcn,ULONGLONG length)
{
    if(lcn < jp->v_info.total_clusters \
      && (lcn + length) <= jp->v_info.total_clusters)
        return 1;
    
    return 0;
}

/**
 * @internal
 * @brief Retrieves mft zones layout.
 * @note Since we have MFT optimization routine, 
 * let's use MFT zone for files placement on XP
 * and more recent Windows editions.
 */
static void get_mft_zones_layout(udefrag_job_parameters *jp)
{
    ULONGLONG start,length,mirror_size;

    if(jp->fs_type != FS_NTFS) return;
    
    /* 
    * Don't increment progress counters,
    * because mft zones are partially inside
    * of the already counted free space pool.
    */    

    /* $MFT */
    start = jp->v_info.ntfs_data.MftStartLcn.QuadPart;
    if(jp->v_info.ntfs_data.BytesPerCluster)
        length = jp->v_info.ntfs_data.MftValidDataLength.QuadPart / jp->v_info.ntfs_data.BytesPerCluster;
    else
        length = 0;
    itrace("%-12s: %-20s: %-20s", "mft section", "start", "length");
    jp->pi.mft_size = length * jp->v_info.bytes_per_cluster;
    itrace("mft size = %I64u bytes", jp->pi.mft_size);
    itrace("%-12s: %-20I64u: %-20I64u", "mft", start, length);

    /* MFT Zone */
    start = jp->v_info.ntfs_data.MftZoneStart.QuadPart;
    length = jp->v_info.ntfs_data.MftZoneEnd.QuadPart - jp->v_info.ntfs_data.MftZoneStart.QuadPart + 1;
    itrace("%-12s: %-20I64u: %-20I64u", "mft zone", start, length);
    if(check_region(jp,start,length)){
        /* remark space as MFT Zone */
        colorize_map_region(jp,start,length,MFT_ZONE_SPACE,0);
        if(jp->win_version < WINDOWS_XP)
            jp->free_regions = winx_sub_volume_region(jp->free_regions,start,length);
        jp->mft_zone.start = start; jp->mft_zone.length = length;
    }

    /* $MFT Mirror */
    start = jp->v_info.ntfs_data.Mft2StartLcn.QuadPart;
    length = 1;
    mirror_size = jp->v_info.ntfs_data.BytesPerFileRecordSegment * 4;
    if(jp->v_info.ntfs_data.BytesPerCluster && mirror_size > jp->v_info.ntfs_data.BytesPerCluster){
        length = mirror_size / jp->v_info.ntfs_data.BytesPerCluster;
        if(mirror_size - length * jp->v_info.ntfs_data.BytesPerCluster)
            length ++;
    }
    itrace("%-12s: %-20I64u: %-20I64u", "mft mirror", start, length);
}

/**
 * @internal
 * @brief Excludes files according to UD_FRAGMENT_SIZE_THRESHOLD filter.
 */
int exclude_by_fragment_size(winx_file_info *f,udefrag_job_parameters *jp)
{
    winx_blockmap *block;
    ULONGLONG fragment_size = 0;
    
    if(jp->udo.fragment_size_threshold == DEFAULT_FRAGMENT_SIZE_THRESHOLD) return 0;
    /* don't filter out files if threshold is set by algorithm */
    if(jp->udo.algorithm_defined_fst) return 0;
    
    if(f->disp.blockmap == NULL) return 0;
    
    for(block = f->disp.blockmap; block; block = block->next){
        if(block == f->disp.blockmap){
            fragment_size += block->length;
        } else if(block->lcn == block->prev->lcn + block->prev->length){
            fragment_size += block->length;
        } else {
            if(fragment_size){
                if(fragment_size * jp->v_info.bytes_per_cluster < jp->udo.fragment_size_threshold)
                    return 0; /* file contains little fragments */
            }
            fragment_size = block->length;
        }
        if(block->next == f->disp.blockmap) break;
    }
    
    if(fragment_size){
        if(fragment_size * jp->v_info.bytes_per_cluster < jp->udo.fragment_size_threshold)
            return 0; /* file contains little fragments */
    }

    return 1;
}

/**
* @internal
* @brief Excludes files according to startLCN and endLCN parameters.
*/
int exclude_by_region(winx_file_info *f, ULONGLONG startLCN, ULONGLONG endLCN)
{
    winx_blockmap *block;
    int exclude = 0;
    //not needed. already checked before.
    //if (f->disp.blockmap == NULL) return 0;

    //iterate the entire blockmap for each file, checking LCNs along the way.
    for (block = f->disp.blockmap; block; block = block->next) {
        //first file
        if (block == f->disp.blockmap) {
            if (block->lcn > startLCN && block->lcn < endLCN)
                exclude = 1;
        }
        //confirmation for nextfiles. (partials?)
        else if (block->lcn == block->prev->lcn + block->prev->length) {
            if (block->lcn > startLCN && block->lcn < endLCN)
                exclude = 1;
        }
        if (exclude)
            return exclude;
        if (block->next == f->disp.blockmap) break;
    }
    return 0;
}
/**
 * @internal
 * @brief Excludes files according to UD_FRAGMENTS_THRESHOLD filter.
 */
int exclude_by_fragments(winx_file_info *f,udefrag_job_parameters *jp)
{
    if(jp->udo.fragments_limit == 0) return 0;
    return (f->disp.fragments < jp->udo.fragments_limit) ? 1 : 0;
}

/**
 * @internal
 * @brief Excludes files according to UD_FILE_SIZE_THRESHOLD filter.
 */
int exclude_by_size(winx_file_info *f,udefrag_job_parameters *jp)
{
    ULONGLONG filesize;
    
    f->user_defined_flags &= ~UD_FILE_OVER_LIMIT;
    filesize = f->disp.clusters * jp->v_info.bytes_per_cluster;
    if(filesize > jp->udo.size_limit){
        f->user_defined_flags |= UD_FILE_OVER_LIMIT;
        return 1;
    }
    return 0;
}

/**
 * @internal
 * @brief Excludes files according to UD_IN_FILTER and UD_EX_FILTER filters.
 */
int exclude_by_path(winx_file_info *f,udefrag_job_parameters *jp)
{
    /* note that paths have the \??\ internal prefix while patterns haven't */
    if(wcslen(f->path) < 0x4)
        return 1; /* the path is invalid */
    
    if(jp->udo.ex_filter.count){
        if(winx_patcmp(f->path + 0x4,&jp->udo.ex_filter))
            return 1;
    }
    
    if(jp->udo.cut_filter.count){
        if(!winx_patcmp(f->path + 0x4,&jp->udo.cut_filter))
            return 1;
    }

    if(jp->udo.in_filter.count == 0) return 0;
    return !winx_patcmp(f->path + 0x4,&jp->udo.in_filter);
}

/**
 * @internal
 * @brief find_files helper.
 * @note Optimized for speed.
 */
static int filter(winx_file_info *f,void *user_defined_data)
{
    udefrag_job_parameters *jp = (udefrag_job_parameters *)user_defined_data;
    int length;
    
    /* START OF AUX CODE */
    
    /* skip entries with empty path, as well as their children */
    if(f->path == NULL) goto skip_file_and_children;
    if(f->path[0] == 0) goto skip_file_and_children;
    
    /*
    * Remove trailing dot from the root
    * directory path, otherwise we'll not
    * be able to defragment it.
    */
    length = (int)wcslen(f->path);
    if(length >= 2){
        if(f->path[length - 1] == '.' && f->path[length - 2] == '\\'){
            itrace("root directory detected, its trailing dot will be removed");
            f->path[length - 1] = 0;
        }
    }
    
    /* skip resident streams */
    if(f->disp.fragments == 0)
        goto skip_file;
    
    /* show debugging information about interesting cases */
    /* comment it out after testing to speed things up */
    /*
    if(is_sparse(f))
        dtrace("sparse file found: %ws",f->path);
    if(is_reparse_point(f))
        dtrace("reparse point found: %ws",f->path);
    if(winx_wcsistr(f->path,L"$BITMAP"))
        dtrace("bitmap found: %ws",f->path);
    if(winx_wcsistr(f->path,L"$ATTRIBUTE_LIST"))
        dtrace("attribute list found: %ws",f->path);
    */
    
    /* START OF FILTERING */
    
    /* skip files with invalid map */
    if(f->disp.blockmap == NULL)
        goto skip_file;

    /* skip temporary files */
    if(is_temporary(f))
        goto skip_file;

    /* filter files by their sizes */
    if(exclude_by_size(f,jp))
        goto skip_file;

    /* filter files by their number of fragments */
    if(exclude_by_fragments(f,jp))
        goto skip_file;

    /* filter files by their fragment sizes */
    if(exclude_by_fragment_size(f,jp))
        goto skip_file;
    
    /* filter files by their paths */
    if(exclude_by_path(f,jp)){
        /*
        * Don't skip children however since 
        * their paths may match patterns.
        */
        goto skip_file;
    }
    
    goto accept_file;

skip_file:
    f->user_defined_flags |= UD_FILE_EXCLUDED;
    
accept_file:
    /* count everything in context menu handler to avoid ambiguity */
    if(jp->udo.job_flags & UD_JOB_CONTEXT_MENU_HANDLER){
        if(jp->udo.cut_filter.count){
            if(winx_patcmp(f->path + 0x4,&jp->udo.cut_filter))
                update_progress_counters(f,jp);
        } else {
            update_progress_counters(f,jp);
        }
    }
    return 0;

skip_file_and_children:
    f->user_defined_flags |= UD_FILE_EXCLUDED;
    return 1;
}

/**
 * @internal
 */
static void update_progress_counters(winx_file_info *f,udefrag_job_parameters *jp)
{
    ULONGLONG filesize;

    jp->pi.files ++;
    if(is_directory(f)) jp->pi.directories ++;
    if(is_compressed(f)) jp->pi.compressed ++;
    jp->pi.processed_clusters += f->disp.clusters;

    filesize = f->disp.clusters * jp->v_info.bytes_per_cluster;
    if(filesize >= GIANT_FILE_SIZE)
        jp->f_counters.giant_files ++;
    else if(filesize >= HUGE_FILE_SIZE)
        jp->f_counters.huge_files ++;
    else if(filesize >= BIG_FILE_SIZE)
        jp->f_counters.big_files ++;
    else if(filesize >= AVERAGE_FILE_SIZE)
        jp->f_counters.average_files ++;
    else if(filesize >= SMALL_FILE_SIZE)
        jp->f_counters.small_files ++;
    else
        jp->f_counters.tiny_files ++;
}

/**
 * @internal
 * @brief find_files helper.
 */
static void progress_callback(winx_file_info *f,void *user_defined_data)
{
    udefrag_job_parameters *jp = (udefrag_job_parameters *)user_defined_data;
    
    /* don't count excluded files in the context menu handler */
    if(!(jp->udo.job_flags & UD_JOB_CONTEXT_MENU_HANDLER))
        update_progress_counters(f,jp);
}

/**
 * @internal
 * @brief find_files helper.
 */
static int terminator(void *user_defined_data)
{
    udefrag_job_parameters *jp = (udefrag_job_parameters *)user_defined_data;

    return jp->termination_router((void *)jp);
}

/**
 * @internal
 * @brief Displays file counters.
 */
void dbg_print_file_counters(udefrag_job_parameters *jp)
{
    itrace("folders total:    %u",jp->pi.directories);
    itrace("files total:      %u",jp->pi.files);
    itrace("fragmented files: %u",jp->pi.fragmented);
    itrace("compressed files: %u",jp->pi.compressed);
    itrace("tiny ...... <  10 KB: %u",jp->f_counters.tiny_files);
    itrace("small ..... < 100 KB: %u",jp->f_counters.small_files);
    itrace("average ... <   1 MB: %u",jp->f_counters.average_files);
    itrace("big ....... <  16 MB: %u",jp->f_counters.big_files);
    itrace("huge ...... < 128 MB: %u",jp->f_counters.huge_files);
    itrace("giant ..............: %u",jp->f_counters.giant_files);
}

/**
 * @internal
 * @brief Searches for all files on the disk.
 * @return Zero for success, negative value otherwise.
 */
static int find_files(udefrag_job_parameters *jp)
{
    int context_menu_handler = 0;
    wchar_t parent_directory[MAX_PATH + 1];
    wchar_t *p;
    wchar_t c;
    int flags = 0;
    winx_file_info *f;
    winx_blockmap *block;
    
    /* check for context menu handler (single files/directories)*/
    if(jp->udo.job_flags & UD_JOB_CONTEXT_MENU_HANDLER){
        if(jp->udo.cut_filter.count > 0){
            if(wcslen(jp->udo.cut_filter.array[0]) >= wcslen(L"C:\\"))
                context_menu_handler = 1;
        }
    }

    /* speed up the context menu handler for (single files/directories) & non-NTFS */
    if(jp->fs_type != FS_NTFS && context_menu_handler){
        /* in case of c:\* or c:\ scan the entire disk */
        c = jp->udo.cut_filter.array[0][3];
        if(c == 0 || c == '*')
            goto scan_entire_disk;
        /* in case of c:\test;c:\test\* scan the parent directory recursively */
        if(jp->udo.cut_filter.count > 1)
            flags = WINX_FTW_RECURSIVE;
        /* in case of c:\test scan the parent directory, not recursively */
        _snwprintf(parent_directory, MAX_PATH, L"\\??\\%ws", jp->udo.cut_filter.array[0]);
        parent_directory[MAX_PATH] = 0;
        p = wcsrchr(parent_directory,'\\');
        if(p) *p = 0;
        if(wcslen(parent_directory) <= wcslen(L"\\??\\C:\\"))
            goto scan_entire_disk;
        jp->filelist = winx_ftw(parent_directory,
            flags | WINX_FTW_DUMP_FILES | \
            WINX_FTW_ALLOW_PARTIAL_SCAN | WINX_FTW_SKIP_RESIDENT_STREAMS,
            filter,progress_callback,terminator,(void *)jp);
    } else {
    scan_entire_disk:
        jp->filelist = winx_scan_disk(jp->volume_letter,
            WINX_FTW_DUMP_FILES | WINX_FTW_ALLOW_PARTIAL_SCAN | \
            WINX_FTW_SKIP_RESIDENT_STREAMS,
            filter,progress_callback,terminator,(void *)jp);
    }
    if(jp->filelist == NULL && !jp->termination_router((void *)jp))
        return (-1);
    
    /* calculate number of fragmented files; redraw the map */
    for(f = jp->filelist; f; f = f->next){
        /* skip excluded files. if excluded, count as 1 fragment.
            obviously if not fragmented, it counts as 1. */
        if(!is_fragmented(f) || is_excluded(f)){
            jp->pi.fragments ++;
        } else {
            jp->pi.fragmented ++;
            jp->pi.fragments += f->disp.fragments;
        }

        /* redraw cluster map */
        colorize_file(jp,f,DEFAULT_COLOR);  //genBTC,-was using wrong alias.
        
        /* add file blocks to a binary tree - after winx_scan_disk! */
        for(block = f->disp.blockmap; block; block = block->next){
            if(add_block_to_file_blocks_tree(jp,f,block) < 0) break;
            if(block->next == f->disp.blockmap) break;
        }

        if(f->next == jp->filelist) break;
    }

    dbg_print_file_counters(jp);
    return 0;
}

/**
 * @internal
 * @brief Defines whether a file
 * is locked by system or not.
 * @return Nonzero value indicates
 * that the file is locked.
 */
int is_file_locked(winx_file_info *f,udefrag_job_parameters *jp)
{
    NTSTATUS status;
    HANDLE hFile;
    int old_color;
    
    /* check whether the file has been passed the check already */
    if(f->user_defined_flags & UD_FILE_NOT_LOCKED)
        return 0;
    if(f->user_defined_flags & UD_FILE_LOCKED)
        return 1;

    /* file status is undefined, so let's try to open it */
    status = winx_defrag_fopen(f,WINX_OPEN_FOR_MOVE,&hFile);
    if(status == STATUS_SUCCESS){
        winx_defrag_fclose(hFile);
        f->user_defined_flags |= UD_FILE_NOT_LOCKED;
        return 0;
    }

    /*strace(status,"cannot open %ws",f->path);*/
    /* redraw space */
    old_color = get_file_color(jp,f);
    f->user_defined_flags |= UD_FILE_LOCKED;
    colorize_file(jp,f,old_color);
    return 1;
}

/**
 * @internal
 * @brief Defines whether a file is from the
 * list of well known locked files or not.
 * @note Optimized for speed.
 */
static int is_well_known_locked_file(winx_file_info *f,udefrag_job_parameters *jp)
{
    /* these files are usually locked on Windows XP */
    wchar_t *locked_files[] = {
        L"$Bitmap",
        L"$Extend\\$ObjId",
        L"$Extend\\$UsnJrnl",
        L"$Extend\\$UsnJrnl:$J",
        L"$LogFile",
        L"$MFT::$BITMAP",
        L"$Secure",
        NULL
    };
    int i, length = (int)wcslen(f->path);
    
    /* search for well known locked NTFS meta files */
    if(length >= 9){ /* ensure that we have at least \??\X:\$x */
        if(f->path[7] == '$'){
            for(i = 0; locked_files[i]; i++){
                if(winx_wcsistr(f->path,locked_files[i]))
                    return 1;
            }
        }
    }

    /* check for paging and hibernation files */
    if(winx_wcsistr(f->name,L"pagefile.sys"))
        return 1;
    if(winx_wcsistr(f->name,L"hiberfil.sys"))
        return 1;
    // Win10 has created a swapfile.sys
    if(winx_wcsistr(f->name,L"swapfile.sys"))
        return 1;
    return 0;
}

/**
 * @internal
 * @brief Searches for well known locked files
 * and applies their dispositions to the map.
 * @details Resets f->disp structure of locked files.
 */
static void redraw_well_known_locked_files(udefrag_job_parameters *jp)
{
    winx_file_info *f;
    ULONGLONG time;
    ULONGLONG n = 0;

    winx_dbg_print_header(0,0,I"searching for well known locked files...");
    time = winx_xtime();
    
    for(f = jp->filelist; f; f = f->next){
        if(f->disp.blockmap){ /* otherwise nothing to redraw */
            if(is_well_known_locked_file(f,jp)){
                if(!is_file_locked(f,jp)){
                    /* possibility of this case should be reduced */
                    dtrace("file wasn't locked: %ws",f->path);
                } else {
                    itrace("locked file DETECTED:  %ws",f->path);
                    ++n;
                }
            }
        }
        if(f->next == jp->filelist) break;
    }

    itrace("%I64u locked files found",n);
    winx_dbg_print_header(0,0,I"well known locked files search completed in %I64u ms",
        winx_xtime() - time);
}

/**
 * @internal
 * @brief Defines rules for the fragmented files list sorting.
 */
static int fragmented_files_compare(const void *prb_a, const void *prb_b, void *prb_param)
{
    winx_file_info *a, *b;
    //udefrag_job_parameters *jp;
    
    a = (winx_file_info *)prb_a;
    b = (winx_file_info *)prb_b;
    //jp = (udefrag_job_parameters *)prb_param;

    /* sort files in descending order by number of fragments */
    if(a->disp.fragments != b->disp.fragments)
        return (a->disp.fragments < b->disp.fragments) ? 1 : (-1);

    /* if files have equal number of fragments, sort 'em by path */
    return winx_wcsicmp(a->path, b->path);
}

/**
 * @internal
 * @brief Adds a file to the list of fragmented files.
 * @note Ignores files excluded from the disk processing.
 */
int expand_fragmented_files_list(winx_file_info *f,udefrag_job_parameters *jp)
{
    void **p;
    
    /* don't include filtered out files, for better performance */
    //genBTC asks:
    // Wouldnt this already be filtered out, since this is only called from:
    // Analyze.C, produce_list_of_fragmented_files()
    //   and
    // Move.C, move_file() near the very end
    // and in both functions, they are called from inside this if block:
    //         if(is_fragmented(f) && !is_excluded(f)){
    // so they should already be verified by !is_exluded(f)
    //I guess this was put in for future expansion?

    //if(!is_excluded(f)){
        p = prb_probe(jp->fragmented_files,(void *)f);
        if(p && *p != f) etrace("a duplicate found for %ws",f->path);
    //}
    return 0;
}

/**
 * @internal
 * @brief Removes a file from the list of fragmented files.
 */
void truncate_fragmented_files_list(winx_file_info *f,udefrag_job_parameters *jp)
{
    if(!prb_delete(jp->fragmented_files,(void *)f))
        etrace("%ws is not found in the tree",f->path);
}

/**
 * @internal
 * @brief Produces the list of fragmented files.
 */
static void produce_list_of_fragmented_files(udefrag_job_parameters *jp)
{
    winx_file_info *f;
    ULONGLONG bad_fragments = 0;
    ULONGLONG bad_clusters = 0;

    itrace("started creation of fragmented files list");
    jp->fragmented_files = prb_create(fragmented_files_compare,(void *)jp,NULL);
    for(f = jp->filelist; f; f = f->next){
        if(is_fragmented(f) && !is_excluded(f)){
            expand_fragmented_files_list(f,jp);
            //Old way counts number of fragments and calculates percentage on
            // how many TOTAL fragments exist. Seems very inaccurate...
            bad_fragments += f->disp.fragments;
            bad_clusters += f->disp.clusters;   //use clusters instead.
        }
        if(f->next == jp->filelist) break;
    }
    jp->pi.bad_fragments = bad_fragments;
    jp->pi.bad_clusters = bad_clusters;
    jp->pi.fragmented_files_prb = jp->fragmented_files; //pointer for fileslist.cpp to access
    jp->pi.isfragfileslist = 1;
    itrace("finished creation of fragmented files list");
    itrace("fragments total: %u",jp->pi.fragments);
    itrace("bad_clusters   : %u",jp->pi.bad_clusters);
}

/**
 * @internal
 * @brief Checks whether the requested
 * action is allowed or not.
 * @return Zero indicates that it's allowed,
 * negative value indicates the contrary.
 */
static int check_requested_action(udefrag_job_parameters *jp)
{
    if(jp->job_type != ANALYSIS_JOB && jp->fs_type == FS_UDF){
        etrace("cannot defragment/optimize UDF volumes,");
        etrace("because the file system driver does not support FSCTL_MOVE_FILE");
        return UDEFRAG_UDF_DEFRAG;
    }

    if(jp->is_fat) itrace("FAT directories cannot be moved entirely");
    return 0;
}

/**
 * @internal
 * @brief Defines whether the fragmentation level
 * is above the fragmentation threshold or not.
 */
int check_fragmentation_level(udefrag_job_parameters *jp)
{
    unsigned int ifr, it;
    double fragmentation;
    
    //fragmentation = calc_percentage(jp->pi.bad_fragments,jp->pi.fragments);
    fragmentation = calc_percentage(jp->pi.bad_clusters,jp->pi.used_clusters);
    
    ifr = (unsigned int)(fragmentation * 100.00);
    it = (unsigned int)(jp->udo.fragmentation_threshold * 100.00);
    if(fragmentation < jp->udo.fragmentation_threshold){
        itrace("fragmentation is below the threshold: %u.%02u%% < %u.%02u%%",
            ifr / 100, ifr % 100, it / 100, it % 100);
        return 0;
    }
    itrace("fragmentation is above the threshold: %u.%02u%% >= %u.%02u%%",
        ifr / 100, ifr % 100, it / 100, it % 100);
    return 1;
}

/**
 * @internal
 * @brief Analyzes the disk.
 * @return Zero for success,
 * negative value otherwise.
 */
int analyze(udefrag_job_parameters *jp)
{
    ULONGLONG time;
    int result;
    
    time = start_timing("analysis",jp);
    jp->pi.current_operation = VOLUME_ANALYSIS;
    
    /* update volume information */
    result = get_volume_information(jp);
    if(result < 0)
        return result;
    
    /* search for free space areas */
    if(get_free_space_layout(jp) < 0)
        return (-1);
    
    /* redraw mft zone in light magenta */
    get_mft_zones_layout(jp);
    
    /* search for files */
    if(find_files(jp) < 0)
        return (-1);
    
    /* redraw well known locked files in green */
    redraw_well_known_locked_files(jp);

    /* produce a list of fragmented files */
    produce_list_of_fragmented_files(jp);
    (void)check_fragmentation_level(jp); /* for debugging */

    result = check_requested_action(jp);
    if(result < 0)
        return result;
    
    jp->p_counters.analysis_time = winx_xtime() - time;
    stop_timing("analysis",time,jp);
    return 0;
}

/** @} */
