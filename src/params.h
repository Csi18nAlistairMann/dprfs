/*
  Copyright (C) 2015 Alistair Mann <al+dprfs@pectw.net>
  Copyright (C) 2012 Joseph J. Pfeiffer, Jr., Ph.D. <pfeiffer@cs.nmsu.edu>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  There are a couple of symbols that need to be #defined before
  #including all the headers.
*/
#ifndef _PARAMS_H_
#define _PARAMS_H_

// 0 = compile for service; 1 = compile for unit tests
#define RUN_AS_UNIT_TESTS 0

// 0 = compile to use rdrive only; 1 = compile for tdrive too
#define USE_TDRIVE 1

/* Add rolling_stats by default, 8 second periods, max 8(+8) history */
#define RS_DELETE_FILE "/dev/shm/dprfs-rolling-stats-delete"
#define RS_DELETE_SUPPORT 1
#define RS_DELETE_HISTORY 16
#define RS_DELETE_PERIOD 8
#define RS_RENAME_FILE "/dev/shm/dprfs-rolling-stats-rename"
#define RS_RENAME_SUPPORT 1
#define RS_RENAME_HISTORY 16
#define RS_RENAME_PERIOD 8

// The FUSE API has been changed a number of times.  So, our code
// needs to define the version of the API that we assume.  As of this
// writing, the most current API version is 26
/* #define FUSE_USE_VERSION 30   */

// need this to get pwrite().  I have to use setvbuf() instead of
// setlinebuf() later in consequence.
/* #define _XOPEN_SOURCE 500 */

// maintain dprfs state in here
#include <dirent.h>
#include <inttypes.h>
#include <limits.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <linux/limits.h>
#include <openssl/sha.h>

#define DPR_DATA ((struct dpr_state *) fuse_get_context()->private_data)

#define LOG_DIVIDER "%%%%%% "

/*
 * Types of object in a DPRFS filesystem
 *
 * NA Not known; DIR Directory; LL Linkedlist; DS datastore
 * Other can be ordinary file, wildcards etc
 */
#define DPRFS_FILETYPE_NA 1
#define DPRFS_FILETYPE_DIR 2
#define DPRFS_FILETYPE_LL 3
#define DPRFS_FILETYPE_DS 4
#define DPRFS_FILETYPE_OTHER 5

#define R_PATH "/var/lib/samba/usershares/rdrive"
#define G_PATH "/var/lib/samba/usershares/gdrive"
#define TMP_PATH "/tmp/dprfs"
#define DATASTORE_PATH "/home/smb/datastores"
#define BEYOND_USE_RELPATH "/Beyond Use"
#define BEYOND_USE_README_FILE "Readme 1st.txt"
#define BEYOND_USE_COUNTERS_FILENAME "statistics"
#define BEYOND_USE_COUNTERS_RELPAF BEYOND_USE_RELPATH "/" BEYOND_USE_COUNTERS_FILENAME

// now I feel dirty
#define BODGE_WD "/home/smb/dprfs_v1/src"
#define ACCESS_DENIED_FILE "access-denied-file"
#define ACCESS_DENIED_DIR "access-denied-dir"
#define LATEST_FILENAME ":latest"
#define DMETADATA_FILENAME ":Dmetadata"
#define FMETADATA_FILENAME ":Fmetadata"
/* #define RESERVED_FILENAME ":Reserved" */
#define TIMESTAMP_SIZE 21	// 20160105190333846925\0
// XWP_DEPTH_MAX how many times dpr_xlateWholePath can recurse. In normal
// operations this limits how many subdirectories there can be, how many
// renames one file can have, or how long a "points-to" chain can be
#define REVISION_NAME_MINIMUM "AA00000"
#define REVISION_NAME_MAXIMUM "ZZ99999"
#define REVISION_NAME_LEN 7
#define XWP_DEPTH_MAX 256
#define DEFAULT_MALLOC_SZ 4
// If the source for the next payload location is fwd (forward)
// then the next payload-loc is the same as the last. If new,
// the code is expected to construct a new relative path during
// processing
#define PAYLOAD_LOC_SRC_NEW 1
#define PAYLOAD_LOC_SRC_FWD 2
/*
 * keys that can be written to a metadata file
 * although note as written, dprfs only checks
 * first unique characters
 */
#define METADATA_KEY_BEYONDUSEON "beyond-use-on = "
#define METADATA_KEY_DELETED "deleted = "
#define METADATA_KEY_LLID "llid = "
#define METADATA_KEY_NOTVIA "not-via = "
#define METADATA_KEY_PAYLOADLOC "payload-loc = "
#define METADATA_KEY_ORIGINALDIR "original-dir = "
#define METADATA_KEY_RENAMEDFROM "renamed-from = "
#define METADATA_KEY_RENAMEDTO "renamed-to = "
#define METADATA_KEY_SHA256 "sha256 = "
#define METADATA_KEY_SUPERCEDES "supercedes = "

#if RUN_AS_UNIT_TESTS
#define R_PATH_STROKE R_PATH "/"
#endif

/*
 * The forensic log array (fl_arr), which tracks whether an opened
 * file is tainted by actions upon that file, includes an array
 * whose elements match those actions. The first element is used
 * to indicate any change: there'll be a log action when it hits
 * as well as log actions when the file is finally closed
 */
#define TAINTED_KEY 0
#define MKNOD_KEY 1
#define MKDIR_KEY 2
#define UNLINK_KEY 3
#define RMDIR_KEY 4
#define RENAME_KEY 5
#define CHMOD_KEY 6
#define CHOWN_KEY 7
#define TRUNCATE_KEY 8
#define UTIME_KEY 9
#define WRITE_KEY 10
#define FLUSH_KEY 11
#define CREAT_KEY 12
#define SYMLINK_KEY 13
#define LINK_KEY 14
#define SETXATTR_KEY 15
#define REMOVEXATTR_KEY 16
#define FALLOCATE_KEY 17
#define RECREATE_KEY 18
#define NUM_FL_KEYS 19

#define XMPDIRP_NORMAL 0
#define XMPDIRP_SHADOW 1
#define XMPDIRP_EXHAUSTED 2

struct rolling_stats {
	unsigned long *data_p;	/* array where one element is hits per period */
	unsigned long history_sz;	/* number of periods tracked + number in history */
	unsigned long change;	/* timestamp shifted right */

	/* Period: number of seconds to fit in one data point */
	unsigned long period;
	unsigned long period_bits;
	/* Halfsize: number of periods before flushing to history */
	unsigned long halfsize;
	unsigned long halfsize_bits;
};

struct xmp_dirp {
	DIR *dp;
	DIR *shadow_dp;
	struct dirent *entry;
	struct dirent *shadow_entry;
	off_t offset;
	off_t shadow_offset;
	int lookit;
};

//
// parsing command line
struct internal_options {
	char *fuse_argv[5];
	int fuse_argc;
	char *rdrive;
	char debuglevel;
	unsigned int rdrive_len;
};

//
// Mallocable
struct longlong_arr_single {
	// used by shadowFile_fd, filetype
	unsigned long key;
	unsigned long value;
};

struct dirxmpdirp_arr_single {
	// used by shadowFile_fd, filetype
	DIR *key;
	struct xmp_dirp *value;
};

struct backup_gpath_arr_single {
	unsigned long fd;
	char backup_gpath[PATH_MAX];
};

/* aka flarrs */
struct fl_arr_single {
	char paf[PATH_MAX];
	unsigned long counts[NUM_FL_KEYS];
};

struct mallocable_string {
	char *string;
	long string_len, string_max;
};

struct extensible_array_ptrs_to_string {
	char **array;
	long array_len, array_max;
};

struct extensible_array_ptrs_to_fl_arr_singles {
	struct fl_arr_single **array;
	long array_len, array_max;
};

struct extensible_array_ptrs_to_longlong_arr_singles {
	struct longlong_arr_single **array;
	long array_len, array_max;
};

struct extensible_array_ptrs_to_longxmpdirp_arr_singles {
	struct dirxmpdirp_arr_single **array;
	long array_len, array_max;
};

struct extensible_array_ptrs_to_backup_gpath_arr_singles {
	struct backup_gpath_arr_single **array;
	long array_len, array_max;
};

/*
 * DPRFS data persisted across calls
 */
#define LOG_DIR "/var/log/dprfs"
#define DEBUG_LOG "dprfs.debug"
#define FORENSIC_LOG "forensic.debug"
#define NO_SHADOWFILE_FD -1
struct dpr_state {
	FILE *debugfile;
	FILE *forensiclogfile;
	char *rootdir;
	char debuglevel;
	unsigned int rootdir_len;

	struct rolling_stats delstats_p;
	struct rolling_stats renstats_p;

	// Array of pointers to files needing reload bc of
	// linkedlist change while head already open
	struct extensible_array_ptrs_to_backup_gpath_arr_singles
	 backup_gpath_arr;
	struct extensible_array_ptrs_to_longlong_arr_singles filetype_arr;
	struct extensible_array_ptrs_to_fl_arr_singles fl_arr;
	struct extensible_array_ptrs_to_string pr_arr;
	struct extensible_array_ptrs_to_longlong_arr_singles shadowFile_arr;
	struct extensible_array_ptrs_to_longxmpdirp_arr_singles shadowDir_arr;
};

/*
 * Data about a particular linkedlist entry
 */
struct dpr_xlate_data {
	// .part: store in /tmp/dprfs or samba share that part of the rdrive
	// below the rootdir, excluding the final filename, directory or the
	// linkedlist name just above timestamp directory
	bool deleted;
	int dprfs_filetype;
	// whatever is after the final "/", or all of the path if no "/" present
	char finalpath[PATH_MAX];
	// OSX can't have directories with the same name as certain files
	// so suffix/remove "_d" where required
	bool is_accdb;
	bool is_osx_bodge;
	bool is_part_file;
	// payload doesn't exist in this link, use this location to find in previous
	char payload[PATH_MAX];
	char payload_root[PATH_MAX];
	char originaldir[PATH_MAX];
	// The relpath is the left-hand side of the user's request up to the
	// final path separator. If there isn't one, the relpath is set to
	// "/". ".part" files will use a sha26 of the relpath string to
	// store files within the temp folder
	char relpath[PATH_MAX];
	char relpath_sha256[SHA256_DIGEST_LENGTH * 2 + 1];
	char revision[REVISION_NAME_LEN + 1];
	char rootdir[PATH_MAX];
	char timestamp[TIMESTAMP_SIZE];
};

//
// For processing :?metadata files
#define MD_NOP 0
#define MD_UPDATE 1
#define MD_DELETE 2
#define MD_ADD 3
#define MD_KEY_DELETED 0
struct metadata_single {
	int operation;		// nop, update, delete, add
	char value[PATH_MAX];
};

struct metadata_multiple {
	int operation;		// nop, update, delete, add
	char value[PATH_MAX];
	struct metadata_multiple *next;
};

struct metadata_array {
	struct metadata_single beyond_use_on, deleted, llid, payload_loc,
	    renamed_from, renamed_to, sha256, supercedes, original_dir;
	struct metadata_multiple not_via;
	// others used to store any other fields
	// without processing implications
	struct mallocable_string others;
};

// makeAndPopulateNewRevisionTSDir() uses to determine what to do
#define LINKEDLIST_CREATE 1
#define LINKEDLIST_EXTEND 2

// constant that auto initialises metadata_arrays
#define MD_ARR_INIT { .beyond_use_on.operation = MD_NOP, .beyond_use_on.value = "", .deleted.operation = MD_NOP, .deleted.value = "", .llid.operation = MD_NOP, .llid.value = "", .not_via.operation = MD_NOP, .not_via.value = "", .not_via.next = NULL, .others.string = NULL, .others.string_len = 0, .others.string_max = 0, .payload_loc.operation = MD_NOP, .payload_loc.value = "", .original_dir.operation = MD_NOP, .original_dir.value = "", .renamed_from.operation = MD_NOP, .renamed_from.value = "", .renamed_to.operation = MD_NOP, .renamed_to.value = "", .sha256.operation = MD_NOP, .sha256.value = "", .supercedes.operation = MD_NOP, .supercedes.value = "" }

#define DXD_INIT { .deleted = false, .dprfs_filetype = DPRFS_FILETYPE_NA, .finalpath = "", .is_accdb = false, .is_osx_bodge = false, .is_part_file = false, .payload = "", .originaldir = "", .relpath = "", .relpath_sha256 = "", .revision = REVISION_NAME_MINIMUM, .rootdir = "", .timestamp = "" }
#endif

#define OPTIONS_INIT { .fuse_argv[0] = NULL, .fuse_argv[0] = NULL, .fuse_argv[0] = NULL, .fuse_argv[0] = NULL, .fuse_argv[0] = NULL, .fuse_argc = 4, .rdrive = NULL, .debuglevel = '0' };

#define STATE_INIT { .debugfile = NULL, .forensiclogfile = NULL, .rootdir = NULL, .debuglevel = '0', .backup_gpath_arr = NULL, .filetype_arr = NULL, .fl_arr = NULL, .pr_arr = NULL, .shadowDir_arr = NULL, .shadowFile_arr = NULL }

/* macros */
#define DEBUGe(level) if (DPR_DATA->debuglevel >= level)
#define DEBUGi(level) if (dpr_data->debuglevel >= level)
#define CKPTe debug_msg(DPR_DATA, " checkpoint: %s %d\n", __FILE__, __LINE__)
#define CKPTi debug_msg(dpr_data, " checkpoint: %s %d\n", __FILE__, __LINE__)

/* How to interact with the original-dir directive? */
#define OBSERVE_ORIGINAL_DIR 1
#define IGNORE_ORIGINAL_DIR 2
#define SERVERSIDE 3
#define USERSIDE 4
