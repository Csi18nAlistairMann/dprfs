/*
  Data-Poisoning Resistant File System
  Copyright (C) 2015-7 Alistair Mann <al+dprfs@pectw.net>

  Large parts
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>
  Copyright (C) 2012       Joseph J. Pfeiffer, Jr., Ph.D. <pfeiffer@cs.nmsu.edu>
  Smaller parts credited in comments.

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

/**
 * gcc -fstack-check -Wall -std=gnu90 dprfs.c debug.c forensiclog.c -O2 -D_FILE_OFFSET_BITS=64 -DHAVE_CONFIG_H `pkg-config fuse3 --cflags` -g -O2 -lcrypto -lbsd `pkg-config fuse3 --libs` -DHAVE_CONFIG_H -o dprfs
 * fusermount -u /var/lib/samba/usershares/gdrive; ./dprfs /var/lib/samba/usershares/gdrive -o allow_root -o modules=subdir -o subdir=/var/lib/samba/usershares/rdrive -D 1
 * indent -linux *.h *.c
 *
 * Valgrind use
 * Adapted from and using fusermount bodge at https://sourceforge.net/p/fuse/mailman/message/11633802/
 * gcc -g -fstack-check -Wall -std=gnu90 dprfs.c debug.c forensiclog.c -O2 -D_FILE_OFFSET_BITS=64 -DHAVE_CONFIG_H `pkg-config fuse3 --cflags` -g -O2 -lcrypto -lbsd `pkg-config fuse3 --libs` -DHAVE_CONFIG_H -o dprfs
 * cd ~/dprfs_v1/src/ && fusermount -u /var/lib/samba/usershares/gdrive; valgrind --log-file=/tmp/valgrin --tool=memcheck --trace-children=no --leak-check=full --show-reachable=yes --max-stackframe=3000000 -v ./dprfs /var/lib/samba/usershares/gdrive -o allow_root -o modules=subdir -o subdir=/var/lib/samba/usershares/rdrive -D 1
 */

#define FUSE_USE_VERSION 31

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif				/* #ifdef HAVE_CONFIG_H */

#define _GNU_SOURCE

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <fuse.h>
#include <inttypes.h>
#include <libgen.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <bsd/stdlib.h>
#include <openssl/sha.h>
#include <sys/file.h>		/* flock(2) */
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif				/* #ifdef HAVE_SETXATTR */

#include "params.h"
#include "debug.h"
#include "dprfs.h"
#include "forensiclog.h"

/* Globals only used because SIGUSR1 needs access */
static struct dpr_state *dpr_data;
static struct sigaction siga;

/* inlines */
inline struct xmp_dirp *get_dirp(struct fuse_file_info *fi)
{
	return (struct xmp_dirp *)(uintptr_t) fi->fh;
}

/* Report errors to logfile and give -errno to caller */
static int dpr_error(char *str)
{
	int ret = -errno;

	DEBUGe('2') debug_msg(DPR_DATA, "    ERROR %s: %s\n", str,
			      strerror(errno));

	return ret;
}

/* Report errors to logfile and give -errno to caller */
static int dpr_level_error(const int level, const char *str)
{
	int ret = -errno;

	DEBUGe(level) debug_msg(DPR_DATA, "    ERROR %s: %s\n", str,
				strerror(errno));

	return ret;
}

/*
 * Create a SHA256 of the given cleartext, the concat it to the given dest
 *
 * SHA256 used for forming llids (linkedlist id), checksums, also storing
 * in the temporary directory
 *
 * Based off Adam Rosenfield via
 * http://stackoverflow.com/questions/9284420/how-to-use-sha1-hashing-in-c-programming
 */
static void catSha256ToStr(char *dest, const struct dpr_state *dpr_data,
			   const char *cleartext)
{
	size_t len = strlen((char *)cleartext);
	char *p;
	int a;
	unsigned char hash[SHA256_DIGEST_LENGTH + 1] = "";
	unsigned char u, l;

	SHA256((unsigned char *)cleartext, len, hash);
	// hash now contains the 20-byte SHA-1 hash
	//
	// ... and into hex
	p = dest + strlen(dest);
	DEBUGi('2') debug_msg(dpr_data,
			      "%s() poffset=\"%d\")\n", __func__, p - dest);
	for (a = 0; a < SHA256_DIGEST_LENGTH; a++) {
		u = hash[a];
		u >>= 4;
		u &= 15;
		u += '0';
		if (u > '9')
			u += 'a' - '0' - 10;

		l = hash[a];
		l &= 15;
		l += '0';
		if (l > '9')
			l += 'a' - '0' - 10;

		*p++ = u;
		*p++ = l;
	}
	*p = '\0';
	DEBUGi('3') debug_msg(dpr_data, "%s() completed\n", __func__);
}

/* dump out current state of given dxd to log */
static void
misc_debugDxd(const struct dpr_state *dpr_data, char debuglevel,
	      struct dpr_xlate_data *dxd, const char *prepend,
	      const char *function_name)
{
	DEBUGi(debuglevel) debug_msg
	    (dpr_data,
	     "%s(): %s deleted=\"%d\" dprfs_filetype=\"%d\" finalpath=\"%s\" is_osx_bodge=\"%d\" is_part_file=\"%d\" payload=\"%s\" payload_root=\"%s\" original-dir=\"%s\" relpath=\"%s\" relpath_sha256=\"%s\" revision=\"%s\" rootdir=\"%s\" timestamp=\"%s\")\n",
	     function_name, prepend, dxd->deleted, dxd->dprfs_filetype,
	     dxd->finalpath, dxd->is_osx_bodge, dxd->is_part_file, dxd->payload,
	     dxd->payload_root, dxd->originaldir, dxd->relpath,
	     dxd->relpath_sha256, dxd->revision, dxd->rootdir, dxd->timestamp);
}

/*
 * Recursive delete a directory
 *
 * http://stackoverflow.com/questions/5467725/how-to-delete-a-directory-and-its-contents-in-posix-c
 * Thank you again, caf
 */
static int
unlink_cb(const char *fpath, const struct stat *sb, int typeflag,
	  struct FTW *ftwbuf)
{
	int rv = remove(fpath);

	if (rv)
		perror(fpath);

	return rv;
}

/* tree walk a directory unlink as we go */
/* Could do with path being checked too pls */
/* Do not use outside of linkedlists as /tmp/dprfs file associated */
/* with that directory will get missed */
static int rmrf(char *path)
{
	return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
}

/* Append _d if bodge and not already present */
static void addOsxBodgeExtIfNecc(char *paf)
{
	int len = strlen(paf);
	if (strcmp(paf + len - 2, "_d") != 0)
		strcat(paf, "_d");
}

/*-------------------------------------------------------
  Extensible Arrays.
  dprfs makes quite a bit of use of these: in essence they are an array
  that gets realloc()ed if it gets too small, and stores pointers to data
  associated with a particular file handle. Another pointer to all this
  data is persisted by fuse, and so can we persist state over multiple
  calls.
  Each array holds key and value where the key is the file descriptor
  about which we're storing data. As that key could be anything up to
  302,866 on this machine, it makes little sense to use it as in index,
  and so we use it as a key. At the time of writing we obtain resources
  in groups of 128. Each of them is a pointer to malloc()ed space, or
  NULL otherwise.
  Standard functions
  ea_*_initialise() obtain and clean initial resources
  ea_*_extend() obtain and clean further resources
  ea_*_release() free resources
  ea_*_addElement() store a key/value pair
  ea_*_removeElementByIndex() Remove the Nth key/value pair
  ea_*_removeElementByValue() Remove any pair where value is X
  Each also has particular functions.
  -------------------------------------------------------*/
/*-------------------------------------------------------
 * ea_shadowFile_* functions  (extensible array shadow (char[PATH_MAX], long[17])
 * used for: tracking shadow file descriptors.
 * We open() a file normally but if we write() to it, we actually open a copy
 * (the shadow) and write to that instead. We need to keep both files open
 * in order to avoid the obscure race condition mentioned in
 * http://man7.org/linux/man-pages/man2/close.2.html and which we experience
 * with rsync when the master thread reassigns a file descriptor and
 * fuse doesn't realise.
 */
static void ea_shadowFile_initialise(struct dpr_state *dpr_data)
{
	int a;
	dpr_data->shadowFile_arr.array = NULL;
	dpr_data->shadowFile_arr.array_len = 0;
	dpr_data->shadowFile_arr.array_max = 0;

	if (dpr_data->shadowFile_arr.array_len >=
	    dpr_data->shadowFile_arr.array_max) {
		ea_shadowFile_extend(&dpr_data->shadowFile_arr);
		printf("  shadowFile_arr attempting realloc to %li bytes\n",
		       dpr_data->shadowFile_arr.array_max);
	}

	for (a = 0; a < dpr_data->shadowFile_arr.array_max; a++) {
		dpr_data->shadowFile_arr.array[a] = NULL;
	}
}

static int ea_shadowFile_extend(struct extensible_array_ptrs_to_longlong_arr_singles
				*ea)
{
	int a;

	do
		ea->array_max += DEFAULT_MALLOC_SZ;
	while (ea->array_len >= ea->array_max);

	ea->array = realloc(ea->array, ea->array_max * sizeof(*ea->array));

	for (a = ea->array_len; a < ea->array_max; a++)
		ea->array[a] = NULL;
	return 0;
}

static void ea_shadowFile_release(struct dpr_state *dpr_data)
{
	long a;
	fprintf(stderr, "ea_shadowFile_release called\n");

	for (a = 0; a < dpr_data->shadowFile_arr.array_max; a++)
		if (dpr_data->shadowFile_arr.array[a] != NULL)
			free(dpr_data->shadowFile_arr.array[a]);
	free(dpr_data->shadowFile_arr.array);

	fprintf(stderr, "ea_shadowFile_release completed\n");
}

static signed int ea_shadowFile_findNextFreeSlot(struct dpr_state *dpr_data)
{
	unsigned int num_extensions = 1;
	int a;
	DEBUGe('2') debug_msg(dpr_data, "%s() called\n", __func__);

	a = 0;
	do {
		do
			if (dpr_data->shadowFile_arr.array[a] == NULL)
				return a;
		while (++a < dpr_data->shadowFile_arr.array_max) ;

		ea_shadowFile_extend(&dpr_data->shadowFile_arr);
		DEBUGe('2') debug_msg(dpr_data,
				      "  ea_shadow attempting realloc to %d bytes\n",
				      dpr_data->shadowFile_arr.array_max);
	} while (num_extensions--);

	/* WCPGW that I get here? */
	DEBUGe('2') debug_msg
	    (dpr_data, "[WARNING] %s() can't find next free space\n", __func__);
	return -1;
}

static void ea_shadowFile_addElement(struct dpr_state *dpr_data,
				     const unsigned long fd,
				     const unsigned long shadowFile_fd)
{
	struct longlong_arr_single *ptr;
	int idx;
	int a;

	a = 0;
	do {
		if (dpr_data->shadowFile_arr.array[a] == NULL)
			continue;

		if (dpr_data->shadowFile_arr.array[a]->key != fd)
			continue;

		if (dpr_data->shadowFile_arr.array[a]->value != shadowFile_fd)
			continue;
		// we're tracking this pair already
		return;
	} while (++a < dpr_data->shadowFile_arr.array_max);

	idx = ea_shadowFile_findNextFreeSlot(dpr_data);
	if (idx == -1)
		return;		// no free room, altho it should have malloced enuf

	ptr = malloc(sizeof(*ptr));
	if (ptr == NULL)
		DEBUGe('2') debug_msg(dpr_data,
				      "%s(): insufficient memory\n", __func__);
	ptr->key = fd;
	ptr->value = shadowFile_fd;
	dpr_data->shadowFile_arr.array[idx] = ptr;

	DEBUGe('2') debug_msg(dpr_data,
			      "%s(): index %d sees fd %d and shadowFile_fd %d\n",
			      __func__, idx, fd, shadowFile_fd);
}

static void
ea_shadowFile_removeElementByIndex(struct dpr_state *dpr_data, int index)
{
	DEBUGe('2') debug_msg
	    (dpr_data, "%s() called to remove \"%d\"\n", __func__, index);

	free(dpr_data->shadowFile_arr.array[index]);
	dpr_data->shadowFile_arr.array[index] = NULL;

	DEBUGe('2') debug_msg(dpr_data,
			      "%s() removed \"%d\"\n", __func__, index);
}

static void
ea_shadowFile_removeElementByValue(struct dpr_state *dpr_data,
				   const unsigned long fd,
				   const unsigned long shadowFile_fd)
{
	int a;

	a = 0;
	do {
		if (dpr_data->shadowFile_arr.array[a] == NULL)
			continue;

		if (dpr_data->shadowFile_arr.array[a]->key != fd)
			continue;

		if (dpr_data->shadowFile_arr.array[a]->value != shadowFile_fd)
			continue;

		ea_shadowFile_removeElementByIndex(dpr_data, a);
	} while (++a < dpr_data->shadowFile_arr.array_max);
}

/* Particular: return(shadowFile_fd != NULL) ? shadowFile_fd : fd */
static unsigned long
ea_shadowFile_getValueOrKey(struct dpr_state *dpr_data,
			    struct fuse_file_info *fi)
{
	int a = 0;

	do {
		if (dpr_data->shadowFile_arr.array[a] == NULL)
			continue;

		if (dpr_data->shadowFile_arr.array[a]->key != fi->fh)
			continue;

		DEBUGe('1') debug_msg(DPR_DATA,
				      "%s() found val=\"%d\"\n", __func__,
				      dpr_data->shadowFile_arr.array[a]->value);
		return dpr_data->shadowFile_arr.array[a]->value;
	} while (++a < dpr_data->shadowFile_arr.array_max);

	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s() not found so returning fi->fh(\"%d\")\n",
			      __func__, fi->fh);
	return fi->fh;
}

/*-------------------------------------------------------
 * ea_filetype_* functions  (extensible array filetype (long, long)
 * used for: tracking nature of an opened object: dir, file, linkedlist etc
 */
static void ea_filetype_initialise(struct dpr_state *dpr_data)
{
	int a;
	dpr_data->filetype_arr.array = NULL;
	dpr_data->filetype_arr.array_len = 0;
	dpr_data->filetype_arr.array_max = 0;

	if (dpr_data->filetype_arr.array_len >=
	    dpr_data->filetype_arr.array_max) {
		ea_filetype_extend(&dpr_data->filetype_arr);
		printf("  filetype_arr attempting realloc to %li bytes\n",
		       dpr_data->filetype_arr.array_max);
	}

	for (a = 0; a < dpr_data->filetype_arr.array_max; a++)
		dpr_data->filetype_arr.array[a] = NULL;
}

static int ea_filetype_extend(struct extensible_array_ptrs_to_longlong_arr_singles
			      *ea)
{
	int a;

	do
		ea->array_max += DEFAULT_MALLOC_SZ;
	while (ea->array_len >= ea->array_max);

	ea->array = realloc(ea->array, ea->array_max * sizeof(*ea->array));

	for (a = ea->array_len; a < ea->array_max; a++)
		ea->array[a] = NULL;
	return 0;
}

static void ea_filetype_release(struct dpr_state *dpr_data)
{
	long a;
	fprintf(stderr, "ea_filetype_release called\n");

	for (a = 0; a < dpr_data->filetype_arr.array_max; a++)
		if (dpr_data->filetype_arr.array[a] != NULL)
			free(dpr_data->filetype_arr.array[a]);
	free(dpr_data->filetype_arr.array);

	fprintf(stderr, "ea_filetype_release completed\n");
}

static signed int ea_filetype_findNextFreeSlot(struct dpr_state *dpr_data)
{
	unsigned int num_extensions = 1;
	int a;
	DEBUGe('2') debug_msg(dpr_data, "%s() called\n", __func__);

	a = 0;
	do {
		do
			if (dpr_data->filetype_arr.array[a] == NULL)
				return a;
		while (++a < dpr_data->filetype_arr.array_max) ;

		ea_filetype_extend(&dpr_data->filetype_arr);
		DEBUGe('2') debug_msg(dpr_data,
				      "  ea_filetype attempting realloc to %d bytes\n",
				      dpr_data->filetype_arr.array_max);
	} while (num_extensions--);

	/* WCPGW that I get here? */
	DEBUGe('2') debug_msg
	    (dpr_data, "[WARNING] %s() can't find next free space\n", __func__);
	return -1;
}

static void ea_filetype_addElement(struct dpr_state *dpr_data,
				   const unsigned long fd,
				   const unsigned long filetype)
{
	struct longlong_arr_single *ptr;
	int idx;
	int a;

	a = 0;
	do {
		if (dpr_data->filetype_arr.array[a] == NULL)
			continue;

		if (dpr_data->filetype_arr.array[a]->key != fd)
			continue;

		if (dpr_data->filetype_arr.array[a]->value != filetype)
			continue;
		// tracking this pair already
		return;
	} while (++a < dpr_data->filetype_arr.array_max);

	idx = ea_filetype_findNextFreeSlot(dpr_data);
	if (idx == -1)
		return;		// no free room, altho it should have malloced enuf

	ptr = malloc(sizeof(*ptr));
	if (ptr == NULL)
		DEBUGe('2') debug_msg(dpr_data,
				      "%s(): insufficient memory\n", __func__);
	ptr->key = fd;
	ptr->value = filetype;
	dpr_data->filetype_arr.array[idx] = ptr;

	DEBUGe('2') debug_msg(dpr_data,
			      "%s(): index:%d gets fd %d with filetype %d\n",
			      __func__, idx, fd, filetype);
}

static void
ea_filetype_removeElementByIndex(struct dpr_state *dpr_data, int index)
{
	int key;
	int val;

	DEBUGe('2') debug_msg
	    (dpr_data, "%s() called to remove filetype for fd=\"%d\"\n",
	     __func__, index);

	key = dpr_data->filetype_arr.array[index]->key;
	val = dpr_data->filetype_arr.array[index]->value;

	free(dpr_data->filetype_arr.array[index]);
	dpr_data->filetype_arr.array[index] = NULL;

	DEBUGe('2') debug_msg(dpr_data,
			      "%s() removed index=\"%d\" (key=\"%d\" value=\"%d\")\n",
			      __func__, index, key, val);
}

static void
ea_filetype_removeElementByKey(struct dpr_state *dpr_data,
			       const unsigned long fd)
{
	int a;

	a = 0;
	do {
		DEBUGe('2') debug_msg(dpr_data,
				      "%s(): indx=\"%d\" with max=\"%d\"\n",
				      __func__, a,
				      dpr_data->filetype_arr.array_max);

		if (dpr_data->filetype_arr.array[a] == NULL)
			continue;

		if (dpr_data->filetype_arr.array[a]->key != fd)
			continue;

		ea_filetype_removeElementByIndex(dpr_data, a);
	} while (++a < dpr_data->filetype_arr.array_max);
}

/* Particular: Given key, return value, otherwise return 0 */
static uint64_t
ea_filetype_getValueForKey(struct dpr_state *dpr_data,
			   struct fuse_file_info *fi)
{
	int a = 0;

	do {
		if (dpr_data->filetype_arr.array[a] == NULL)
			continue;

		if (dpr_data->filetype_arr.array[a]->key != fi->fh)
			continue;

		return dpr_data->filetype_arr.array[a]->value;
	} while (++a < dpr_data->filetype_arr.array_max);

	return 0;
}

/*-------------------------------------------------------
 * ea_backup_gpath_* functions  (extensible array backup_gpath (long, char[PATH_MAX])
 * used for: tracking the gpath
 * In certain situations, and here I'm aware only of flush() doing this,
 * fuse will supply a working file descriptor, but with an empty gpath.
 * If gpath is apparently empty (NULL), dprfs can with this code use a
 * copy saved earlier.
 */
static void ea_backup_gpath_initialise(struct dpr_state *dpr_data)
{
	int a;
	dpr_data->backup_gpath_arr.array = NULL;
	dpr_data->backup_gpath_arr.array_len = 0;
	dpr_data->backup_gpath_arr.array_max = 0;

	if (dpr_data->backup_gpath_arr.array_len >=
	    dpr_data->backup_gpath_arr.array_max) {
		ea_backup_gpath_extend(&dpr_data->backup_gpath_arr);
		printf("  backup_gpath_arr attempting realloc to %li bytes\n",
		       dpr_data->backup_gpath_arr.array_max);
	}

	for (a = 0; a < dpr_data->backup_gpath_arr.array_max; a++)
		dpr_data->backup_gpath_arr.array[a] = NULL;
}

static int ea_backup_gpath_extend(struct
				  extensible_array_ptrs_to_backup_gpath_arr_singles
				  *ea)
{
	int a;

	do
		ea->array_max += DEFAULT_MALLOC_SZ;
	while (ea->array_len >= ea->array_max);

	ea->array = realloc(ea->array, ea->array_max * sizeof(*ea->array));

	for (a = ea->array_len; a < ea->array_max; a++)
		ea->array[a] = NULL;
	return 0;
}

static void ea_backup_gpath_release(struct dpr_state *dpr_data)
{
	long a;
	fprintf(stderr, "ea_backup_gpath_release called\n");

	for (a = 0; a < dpr_data->backup_gpath_arr.array_max; a++) {
		if (dpr_data->backup_gpath_arr.array[a] != NULL) {
			free(dpr_data->backup_gpath_arr.array[a]);
		}
	}
	free(dpr_data->backup_gpath_arr.array);

	fprintf(stderr, "ea_backup_gpath_release completed\n");
}

static signed int ea_backup_gpath_findNextFreeSlot(struct dpr_state *dpr_data)
{
	unsigned int num_extensions = 1;
	int a;
	DEBUGe('2') debug_msg(dpr_data, "%s() called\n", __func__);

	a = 0;
	do {
		do
			if (dpr_data->backup_gpath_arr.array[a] == NULL)
				return a;
		while (++a < dpr_data->backup_gpath_arr.array_max) ;

		ea_backup_gpath_extend(&dpr_data->backup_gpath_arr);
		DEBUGe('2') debug_msg(dpr_data,
				      "  ea_backup_gpath attempting realloc to %d bytes\n",
				      dpr_data->backup_gpath_arr.array_max);
	} while (num_extensions--);

	/* WCPGW that I get here? */
	DEBUGe('2') debug_msg
	    (dpr_data, "[WARNING] %s() can't find next free space\n", __func__);
	return -1;
}

static void ea_backup_gpath_addElement(struct dpr_state *dpr_data,
				       const unsigned long fd,
				       const char *backup_gpath)
{
	struct backup_gpath_arr_single *ptr;
	int idx;
	int a;

	a = 0;
	do {
		if (dpr_data->backup_gpath_arr.array[a] == NULL)
			continue;

		if (dpr_data->backup_gpath_arr.array[a]->fd != fd)
			continue;
		// we're tracking the gpath already
		return;
	} while (++a < dpr_data->backup_gpath_arr.array_max);

	idx = ea_backup_gpath_findNextFreeSlot(dpr_data);
	if (idx == -1)
		return;		// no free room, altho it should have malloced enuf

	ptr = malloc(sizeof(*ptr));
	if (ptr == NULL)
		DEBUGe('2') debug_msg(dpr_data,
				      "%s(): insufficient memory\n", __func__);
	ptr->fd = fd;
	strcpy(ptr->backup_gpath, backup_gpath);
	dpr_data->backup_gpath_arr.array[idx] = ptr;

	DEBUGe('2') debug_msg(dpr_data,
			      "%s(): index %d sees fd %d and backup_gpath %s\n",
			      __func__, idx, fd, backup_gpath);
}

/* Particular: Given key, return pointer to backup_gpath string, */
/* or NULL otherwise */
static char *ea_backup_gpath_getValueForKey(struct dpr_state *dpr_data,
					    struct fuse_file_info *fi)
{
	int a = 0;
	do {
		if (dpr_data->backup_gpath_arr.array[a] == NULL)
			continue;

		if (dpr_data->backup_gpath_arr.array[a]->fd != fi->fh)
			continue;

		return dpr_data->backup_gpath_arr.array[a]->backup_gpath;
	} while (++a < dpr_data->backup_gpath_arr.array_max);

	DEBUGe('2') debug_msg
	    (dpr_data, "[WARNING] %s() can't find path for reload\n", __func__);
	return NULL;
}

static void
ea_backup_gpath_removeElementByIndex(struct dpr_state *dpr_data, int index)
{
	DEBUGe('2') debug_msg
	    (dpr_data, "%s() called to remove \"%d\"\n", __func__, index);

	free(dpr_data->backup_gpath_arr.array[index]);
	dpr_data->backup_gpath_arr.array[index] = NULL;

	DEBUGe('2') debug_msg(dpr_data,
			      "%s() removed \"%d\"\n", __func__, index);
}

static void
ea_backup_gpath_removeElementByValue(struct dpr_state *dpr_data,
				     const char *gpath)
{
	int a;
	DEBUGe('2') debug_msg(dpr_data,
			      "%s() asked to remove \"%s\"\n", __func__, gpath);

	a = 0;
	do {
		if (dpr_data->backup_gpath_arr.array[a] == NULL)
			continue;

		if (strcmp
		    (dpr_data->backup_gpath_arr.array[a]->backup_gpath,
		     gpath) != 0)
			continue;

		ea_backup_gpath_removeElementByIndex(dpr_data, a);
	} while (++a < dpr_data->backup_gpath_arr.array_max);
}

/*-------------------------------------------------------
 * ea_flarrs_* functions  (extensible array forensic log array (char[PATH_MAX], long[17])
 * used for: tracking forensic log changes
 *  Actions that change data (write, truncate, utime etc) go into the forensiclog but
 *  the code may be called multiple times for each action.
 *  We output to forensic log twice: on the first loggable action (in
 *  case of crash) and then smushes further actions on the same file
 *  into a count per action. On release of the file, those counts
 *  form the second log entry.
 */
static void ea_flarrs_initialise(struct dpr_state *dpr_data)
{
	int a;
	dpr_data->fl_arr.array = NULL;
	dpr_data->fl_arr.array_len = 0;
	dpr_data->fl_arr.array_max = 0;

	if (dpr_data->fl_arr.array_len >= dpr_data->fl_arr.array_max) {
		ea_flarrs_extend(&dpr_data->fl_arr);
		printf("  fl_arr attempting realloc to %li bytes\n",
		       dpr_data->fl_arr.array_max);
	}

	for (a = 0; a < dpr_data->fl_arr.array_max; a++)
		dpr_data->fl_arr.array[a] = NULL;
}

static int ea_flarrs_extend(struct extensible_array_ptrs_to_fl_arr_singles *ea)
{
	int a;

	do
		ea->array_max += DEFAULT_MALLOC_SZ;
	while (ea->array_len >= ea->array_max);

	ea->array = realloc(ea->array, ea->array_max * sizeof(*ea->array));

	for (a = ea->array_len; a < ea->array_max; a++)
		ea->array[a] = NULL;
	return 0;
}

static void ea_flarrs_release(struct dpr_state *dpr_data)
{
	long a;
	fprintf(stderr, "ea_flarrs_release called\n");

	for (a = 0; a < dpr_data->fl_arr.array_max; a++)
		if (dpr_data->fl_arr.array[a] != NULL)
			free(dpr_data->fl_arr.array[a]);
	free(dpr_data->fl_arr.array);

	fprintf(stderr, "ea_flarrs_release completed\n");
}

static signed int ea_flarrs_findNextFreeSlot(struct dpr_state *dpr_data)
{
	unsigned int num_extensions = 1;
	int a;
	DEBUGe('2') debug_msg(dpr_data, "%s() called\n", __func__);

	if (dpr_data->fl_arr.array_len >= dpr_data->fl_arr.array_max) {
	}

	a = 0;
	do {
		do
			if (dpr_data->fl_arr.array[a] == NULL)
				return a;
		while (++a < dpr_data->fl_arr.array_max) ;

		ea_flarrs_extend(&dpr_data->fl_arr);
		DEBUGe('2') debug_msg(dpr_data,
				      "  ea_flarrs attempting realloc to %d bytes\n",
				      dpr_data->fl_arr.array_max);
	} while (num_extensions--);

	/* WCPGW that I get here? */
	DEBUGe('2') debug_msg
	    (dpr_data, "[WARNING] %s() can't find next free space\n", __func__);
	return -1;
}

static void ea_flarrs_addElement(struct dpr_state *dpr_data, const char *paf)
{
	struct fl_arr_single *ptr;
	int idx;
	int a;

	DEBUGe('2') debug_msg(dpr_data,
			      "%s() called to add \"%s\"\n", __func__, paf);

	a = 0;
	do {
		if (dpr_data->fl_arr.array[a] == NULL)
			continue;

		if (strcmp(dpr_data->fl_arr.array[a]->paf, paf) != 0)
			continue;
		// we tracking paf already
		return;
	} while (++a < dpr_data->fl_arr.array_max);

	idx = ea_flarrs_findNextFreeSlot(dpr_data);
	if (idx == -1)
		return;		// no free room, altho it should have malloced enuf

	ptr = malloc(sizeof(*ptr));
	if (ptr == NULL)
		DEBUGe('2') debug_msg(dpr_data,
				      "%s(): insufficient memory\n", __func__);

	strcpy(ptr->paf, paf);
	ptr->counts[TAINTED_KEY] = 0;
	ptr->counts[MKNOD_KEY] = 0;
	ptr->counts[MKDIR_KEY] = 0;
	ptr->counts[UNLINK_KEY] = 0;
	ptr->counts[RMDIR_KEY] = 0;
	ptr->counts[RENAME_KEY] = 0;
	ptr->counts[CHMOD_KEY] = 0;
	ptr->counts[CHOWN_KEY] = 0;
	ptr->counts[TRUNCATE_KEY] = 0;
	ptr->counts[UTIME_KEY] = 0;
	ptr->counts[WRITE_KEY] = 0;
	ptr->counts[FLUSH_KEY] = 0;
	ptr->counts[CREAT_KEY] = 0;
	ptr->counts[SYMLINK_KEY] = 0;
	ptr->counts[LINK_KEY] = 0;
	ptr->counts[SETXATTR_KEY] = 0;
	ptr->counts[REMOVEXATTR_KEY] = 0;
	ptr->counts[FALLOCATE_KEY] = 0;
	ptr->counts[RECREATE_KEY] = 0;
	dpr_data->fl_arr.array[idx] = ptr;

	DEBUGe('2') debug_msg(dpr_data,
			      "%s(): index %d sees paf %s\n",
			      __func__, idx, paf);
}

static void
ea_flarrs_removeElementByIndex(struct dpr_state *dpr_data, int index)
{
	DEBUGe('2') debug_msg
	    (dpr_data, "%s() called to remove \"%d\"\n", __func__, index);

	free(dpr_data->fl_arr.array[index]);
	dpr_data->fl_arr.array[index] = NULL;

	DEBUGe('2') debug_msg(dpr_data,
			      "%s() removed \"%d\"\n", __func__, index);
}

static void
ea_flarrs_removeElementByValue(struct dpr_state *dpr_data, const char *paf)
{
	int a;
	DEBUGe('2') debug_msg(dpr_data,
			      "%s() asked to remove \"%s\"\n", __func__, paf);

	a = 0;
	do {
		if (dpr_data->fl_arr.array[a] == NULL)
			continue;

		if (strcmp(dpr_data->fl_arr.array[a]->paf, paf) != 0)
			continue;

		ea_flarrs_removeElementByIndex(dpr_data, a);
	} while (++a < dpr_data->fl_arr.array_max);
}

/*
 * Particular:
 * A routine above has just realised a request will change data on the disk, so
 * calls this to log that fact ahead of making them. By logging before the change,
 * a change that exploits the system gives us at least a starting point for
 * analysis.
 */
static void
forensicLogChangesComing(struct dpr_state *dpr_data, unsigned long index,
			 const char *gpath)
{
	bool found;
	int a;
	DEBUGe('2') debug_msg(dpr_data,
			      "%s() file:\"%s\" index:%d\n",
			      __func__, gpath, index);
	a = 0;
	found = false;
	do {
		if (dpr_data->fl_arr.array[a] == NULL)
			continue;

		if (strcmp(dpr_data->fl_arr.array[a]->paf, gpath) != 0)
			continue;

		/* Handle if the fl_arr already references this file (eg: ftruncate()) */
		if (dpr_data->fl_arr.array[a]->counts[TAINTED_KEY] == 0) {
			if (index == CREAT_KEY)
				forensiclog_msg("created: %s\n", gpath);
			else if (index != FLUSH_KEY)
				forensiclog_msg("open:  %s\n", gpath);
			dpr_data->fl_arr.array[a]->counts[TAINTED_KEY] = 1;
		}
		found = true;
		dpr_data->fl_arr.array[a]->counts[index]++;
	} while (++a < dpr_data->fl_arr.array_max);

	/* And handle if the fl_arr doesnt. (eg, truncate()) */
	if (found == false) {
		if (index == MKNOD_KEY)
			forensiclog_msg("mknod: %s\n", gpath);
		else if (index == MKDIR_KEY)
			forensiclog_msg("mkdir: %s\n", gpath);
		else if (index == UNLINK_KEY)
			forensiclog_msg("unlink: %s\n", gpath);
		else if (index == RMDIR_KEY)
			forensiclog_msg("rmdir: %s\n", gpath);
		else if (index == RENAME_KEY)
			forensiclog_msg("rename: %s\n", gpath);
		else if (index == CHMOD_KEY)
			forensiclog_msg("chmod: %s\n", gpath);
		else if (index == CHOWN_KEY)
			forensiclog_msg("chown: %s\n", gpath);
		else if (index == TRUNCATE_KEY)
			forensiclog_msg("truncate: %s\n", gpath);
		else if (index == UTIME_KEY)
			forensiclog_msg("utime: %s\n", gpath);
		else if (index == WRITE_KEY)
			forensiclog_msg("write: %s\n", gpath);
		/* We cannot tell whether a flush() committed changed data */
		/* else if (index == FLUSH_KEY) */
		/*      forensiclog_msg("FLUSH: %s\n", gpath); */
		else if (index == CREAT_KEY)
			forensiclog_msg("creat: %s\n", gpath);
		else if (index == SYMLINK_KEY)
			forensiclog_msg("symlink: %s\n", gpath);
		else if (index == LINK_KEY)
			forensiclog_msg("link: %s\n", gpath);
		else if (index == SETXATTR_KEY)
			forensiclog_msg("setxattr: %s\n", gpath);
		else if (index == REMOVEXATTR_KEY)
			forensiclog_msg("removexattr: %s\n", gpath);
		else if (index == FALLOCATE_KEY)
			forensiclog_msg("fallocate: %s\n", gpath);
		else if (index == RECREATE_KEY)
			forensiclog_msg("recreate: %s\n", gpath);
	}
}

/*
 * Particular:
 * Caller above realises a release() has happened. If the forensic log array was
 * keeping watch, now's the time to dump what it knows out as a second log entry.
 */
static void
forensicLogChangesApplied(struct dpr_state *dpr_data, const char *gpath)
{
	int a;

	DEBUGe('2') debug_msg(dpr_data, "%s() file:\"%s\"\n", __func__, gpath);

	a = 0;
	do {
		if (dpr_data->fl_arr.array[a] == NULL)
			continue;

		if (strcmp(dpr_data->fl_arr.array[a]->paf, gpath) != 0)
			continue;

		if (dpr_data->fl_arr.array[a]->counts[TAINTED_KEY] == 0)
			continue;

		char result[300] = "";
		char logline[1000] = "";
		if (dpr_data->fl_arr.array[a]->counts[MKNOD_KEY] != 0) {
			sprintf(result, "mknod: %lu ",
				dpr_data->fl_arr.array[a]->counts[MKNOD_KEY]);
			strcat(logline, result);
		}
		if (dpr_data->fl_arr.array[a]->counts[MKDIR_KEY] != 0) {
			sprintf(result, "mkdir: %lu ",
				dpr_data->fl_arr.array[a]->counts[MKDIR_KEY]);
			strcat(logline, result);
		}
		if (dpr_data->fl_arr.array[a]->counts[UNLINK_KEY] != 0) {
			sprintf(result, "unlink: %lu ",
				dpr_data->fl_arr.array[a]->counts[UNLINK_KEY]);
			strcat(logline, result);
		}
		if (dpr_data->fl_arr.array[a]->counts[RMDIR_KEY] != 0) {
			sprintf(result, "rmdir: %lu ",
				dpr_data->fl_arr.array[a]->counts[RMDIR_KEY]);
			strcat(logline, result);
		}
		if (dpr_data->fl_arr.array[a]->counts[RENAME_KEY] != 0) {
			sprintf(result, "rename: %lu ",
				dpr_data->fl_arr.array[a]->counts[RENAME_KEY]);
			strcat(logline, result);
		}
		if (dpr_data->fl_arr.array[a]->counts[CHMOD_KEY] != 0) {
			sprintf(result, "chmod: %lu ",
				dpr_data->fl_arr.array[a]->counts[CHMOD_KEY]);
			strcat(logline, result);
		}
		if (dpr_data->fl_arr.array[a]->counts[CHOWN_KEY] != 0) {
			sprintf(result, "chown: %lu ",
				dpr_data->fl_arr.array[a]->counts[CHOWN_KEY]);
			strcat(logline, result);
		}
		if (dpr_data->fl_arr.array[a]->counts[TRUNCATE_KEY] != 0) {
			sprintf(result, "truncate: %lu ",
				dpr_data->fl_arr.array[a]->
				counts[TRUNCATE_KEY]);
			strcat(logline, result);
		}
		if (dpr_data->fl_arr.array[a]->counts[UTIME_KEY] != 0) {
			sprintf(result, "utime: %lu ",
				dpr_data->fl_arr.array[a]->counts[UTIME_KEY]);
			strcat(logline, result);
		}
		if (dpr_data->fl_arr.array[a]->counts[WRITE_KEY] != 0) {
			sprintf(result, "write: %lu ",
				dpr_data->fl_arr.array[a]->counts[WRITE_KEY]);
			strcat(logline, result);
		}
		if (dpr_data->fl_arr.array[a]->counts[CREAT_KEY] != 0) {
			sprintf(result, "creat: %lu ",
				dpr_data->fl_arr.array[a]->counts[CREAT_KEY]);
			strcat(logline, result);
		}
		if (dpr_data->fl_arr.array[a]->counts[SYMLINK_KEY] != 0) {
			sprintf(result, "symlink: %lu ",
				dpr_data->fl_arr.array[a]->counts[SYMLINK_KEY]);
			strcat(logline, result);
		}
		if (dpr_data->fl_arr.array[a]->counts[LINK_KEY] != 0) {
			sprintf(result, "link: %lu ",
				dpr_data->fl_arr.array[a]->counts[LINK_KEY]);
			strcat(logline, result);
		}
		if (dpr_data->fl_arr.array[a]->counts[SETXATTR_KEY] != 0) {
			sprintf(result, "setxattr: %lu ",
				dpr_data->fl_arr.array[a]->
				counts[SETXATTR_KEY]);
			strcat(logline, result);
		}
		if (dpr_data->fl_arr.array[a]->counts[REMOVEXATTR_KEY] != 0) {
			sprintf(result, "removexattr: %lu ",
				dpr_data->fl_arr.array[a]->
				counts[REMOVEXATTR_KEY]);
			strcat(logline, result);
		}
		if (dpr_data->fl_arr.array[a]->counts[FALLOCATE_KEY] != 0) {
			sprintf(result, "fallocate: %lu ",
				dpr_data->fl_arr.array[a]->
				counts[FALLOCATE_KEY]);
			strcat(logline, result);
		}
		if (dpr_data->fl_arr.array[a]->counts[RECREATE_KEY] != 0) {
			sprintf(result, "recreate: %lu ",
				dpr_data->fl_arr.array[a]->
				counts[RECREATE_KEY]);
			strcat(logline, result);
		}
		/*
		 * flush() may or may not change anything: log it
		 * anyway in case file changed by something else
		 */
		if (logline[0] != '\0') {
			if (dpr_data->fl_arr.array[a]->counts[FLUSH_KEY] != 0) {
				sprintf(result, "flush: %lu ",
					dpr_data->fl_arr.array[a]->
					counts[FLUSH_KEY]);
				strcat(logline, result);
			}
		}

		ea_flarrs_removeElementByValue(dpr_data, gpath);
		/* free(dpr_data->fl_arr.array[a]); */
		/* dpr_data->fl_arr.array[a] = NULL; */

		if (logline[0] != '\0')
			forensiclog_msg("close: %s %s\n", gpath, logline);

	} while (++a < dpr_data->fl_arr.array_max);
}

/*-------------------------------------------------------
 * ea_str_* functions (extensible array string)
 * Used for: tracking part file reloads
 * These implement an internal list of ".part" files that
 * require the main code to reload related fuse_file_info
 * data.
 * This is necessary as the linkedlist paradigm means that
 * a file write actually goes to a copy of the original
 * file, so that copy must be reopened if a write occurs.
 *
 * Current implementation would break with the uploading
 * of too many .part files that didn't complete
 *
 * Note no key/value pairs, only values: we won't have
 * nor need a file descriptor
 */
static void ea_str_initialise(struct dpr_state *dpr_data)
{
	int a;
	dpr_data->pr_arr.array = NULL;
	dpr_data->pr_arr.array_len = 0;
	dpr_data->pr_arr.array_max = 0;

	if (dpr_data->pr_arr.array_len >= dpr_data->pr_arr.array_max) {
		ea_str_extend(&dpr_data->pr_arr);
		printf("  pr_arr attempting realloc to %li bytes\n",
		       dpr_data->pr_arr.array_max);
	}

	for (a = 0; a < dpr_data->pr_arr.array_max; a++)
		dpr_data->pr_arr.array[a] = NULL;
}

static int ea_str_extend(struct extensible_array_ptrs_to_string *ea)
{
	int a;

	do
		ea->array_max += DEFAULT_MALLOC_SZ;
	while (ea->array_len >= ea->array_max);

	ea->array = realloc(ea->array, ea->array_max * sizeof(*ea->array));

	for (a = ea->array_len; a < ea->array_max; a++)
		ea->array[a] = NULL;
	return 0;
}

static void ea_str_release(struct dpr_state *dpr_data)
{
	long a;
	fprintf(stderr, "ea_str_release called\n");

	for (a = 0; a < dpr_data->pr_arr.array_max; a++)
		if (dpr_data->pr_arr.array[a] != NULL)
			free(dpr_data->pr_arr.array[a]);
	free(dpr_data->pr_arr.array);
	dpr_data->pr_arr.array = NULL;

	fprintf(stderr, "ea_str_release completed\n");
}

static signed int ea_str_findNextFreeSlot(struct dpr_state *dpr_data)
{
	unsigned int num_extensions = 1;
	unsigned int a;
	DEBUGe('2') debug_msg(dpr_data, "%s() called\n", __func__);

	a = 0;
	do {
		do
			if (dpr_data->pr_arr.array[a] == NULL)
				return a;
		while (++a < dpr_data->pr_arr.array_max) ;

		ea_str_extend(&dpr_data->pr_arr);
		DEBUGe('2') debug_msg(dpr_data,
				      "  ea_str attempting realloc to %d bytes\n",
				      dpr_data->pr_arr.array_max);
	} while (num_extensions--);

	/* WCPGW that I get here? */
	DEBUGe('2') debug_msg
	    (dpr_data, "[WARNING] %s() can't find next free space\n", __func__);
	return -1;
}

static void ea_str_addElement(struct dpr_state *dpr_data, const char *paf)
{
	char *ptr;
	int idx;
	DEBUGe('2') debug_msg(dpr_data,
			      "%s() called to add \"%s\"\n", __func__, paf);

	idx = ea_str_findNextFreeSlot(dpr_data);
	if (idx == -1)
		return;		// no free room, altho it should have malloced enuf

	ptr = malloc(PATH_MAX);
	if (ptr == NULL)
		DEBUGe('2') debug_msg(dpr_data,
				      "%s(): insufficient memory\n", __func__);
	strcpy(ptr, paf);
	dpr_data->pr_arr.array[idx] = ptr;

	DEBUGe('2') debug_msg(dpr_data,
			      "%s(): index %d sees paf %s\n",
			      __func__, idx, paf);
}

static void ea_str_removeElementByIndex(struct dpr_state *dpr_data, int index)
{
	DEBUGe('2') debug_msg
	    (dpr_data, "%s() called to remove \"%d\"\n", __func__, index);

	free(dpr_data->pr_arr.array[index]);
	dpr_data->pr_arr.array[index] = NULL;

	DEBUGe('2') debug_msg(dpr_data,
			      "%s() removed \"%d\"\n", __func__, index);
}

static void ea_str_removeElementByValue(struct dpr_state *dpr_data,
					const char *paf)
{
	int a;
	DEBUGe('2') debug_msg(DPR_DATA,
			      "%s() asked to remove \"%s\"\n", __func__, paf);

	a = 0;
	do {
		if (dpr_data->pr_arr.array[a] == NULL)
			continue;

		if (strcmp(dpr_data->pr_arr.array[a], paf) != 0)
			continue;

		ea_str_removeElementByIndex(dpr_data, a);
	} while (++a < dpr_data->pr_arr.array_max);
}

/* Particular: Return address of paf if present, NULL otherwise */
static char *ea_str_getValueForKey(struct dpr_state *dpr_data, const char *paf)
{
	int a;

	DEBUGe('2') debug_msg(DPR_DATA,
			      "%s() looking for paf=\"%s\"\n", __func__, paf);

	a = 0;
	do {
		if (dpr_data->pr_arr.array[a] == NULL)
			continue;

		if (strcmp(dpr_data->pr_arr.array[a], paf) != 0)
			continue;

		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s() found it at \"%p\"\n",
				      __func__, dpr_data->pr_arr.array[a]);
		return dpr_data->pr_arr.array[a];
	} while (++a < dpr_data->pr_arr.array_max);

	DEBUGe('2') debug_msg(DPR_DATA, "%s() didnt found it\n", __func__);
	return NULL;
}

/*-------------------------------------------------------*/

/*
 * AM: Files are saved in a directory with a timestamp
 * including microseconds.
 *
 * http://stackoverflow.com/questions/5141960/get-the-current-time-in-c
 * http://stackoverflow.com/questions/21905655/take-milliseconds-from-localtime-without-boost-in-c
 */
static void getCondensedSystemUTime(char *output)
{
	struct timeval time_now;
	struct tm *time_str_tm;

	gettimeofday(&time_now, NULL);
	time_str_tm = gmtime(&time_now.tv_sec);

	sprintf(output, "%04i%02i%02i%02i%02i%02i%06i",
		time_str_tm->tm_year + 1900, time_str_tm->tm_mon + 1,
		time_str_tm->tm_mday, time_str_tm->tm_hour, time_str_tm->tm_min,
		time_str_tm->tm_sec, (int)time_now.tv_usec);
}

static void dxd_initialiseTimestamp(struct dpr_xlate_data *dxd)
{
	struct timeval time_now;
	struct tm *time_str_tm;

	gettimeofday(&time_now, NULL);
	time_str_tm = gmtime(&time_now.tv_sec);

	sprintf(dxd->timestamp, "%04i%02i%02i%02i%02i%02i%06i",
		time_str_tm->tm_year + 1900, time_str_tm->tm_mon + 1,
		time_str_tm->tm_mday, time_str_tm->tm_hour, time_str_tm->tm_min,
		time_str_tm->tm_sec, (int)time_now.tv_usec);
}

/*-------------------------------------------------------*/

static void dxd_copy(struct dpr_xlate_data *to, struct dpr_xlate_data *from)
{
	to->deleted = from->deleted;
	to->dprfs_filetype = from->dprfs_filetype;
	strcpy(to->finalpath, from->finalpath);
	to->is_osx_bodge = from->is_osx_bodge;
	to->is_part_file = from->is_part_file;
	strcpy(to->payload, from->payload);
	strcpy(to->payload_root, from->payload_root);
	strcpy(to->originaldir, from->originaldir);
	strcpy(to->relpath, from->relpath);
	strcpy(to->relpath_sha256, from->relpath_sha256);
	strcpy(to->revision, from->revision);
	strcpy(to->rootdir, from->rootdir);
	strcpy(to->timestamp, from->timestamp);
}

// Mode bodges
// http://stackoverflow.com/questions/737673/how-to-read-the-mode-field-of-git-ls-trees-output
// 0100xxx refers to a regular file
// Fake the samba permissions
static mode_t getModeBodge()
{
	// not when I switch into MAXIMUM OVER-BODGE
	return 0100744;
}

static mode_t getDefaultDirMode()
{
	// not when I switch into MAXIMUM OVER-BODGE
	return 0100770;
}

/*
 * The temp directory is used to store *.part files
 * before being copied into the main part of the
 * filesystem
 */
static int makeDirectory(const char *paf)
{
	int rv = mkdir(paf, getModeBodge());
	if (rv == 0) {
		fprintf(stderr, "Created %s\n", paf);

	} else if (rv == -1 && errno == EEXIST) {
		fprintf(stderr, "Already got %s\n", paf);

	} else {
		fprintf(stderr, "Couldn't create %s\n", paf);
	}
	return 0;
}

/*
 * The "Beyond Use" directory and files are intended to give the
 * user information about what "beyond use" means, what they can
 * do about it if they need material that's beyond use, and the
 * extent of any use made of that facility.
 */
static int
makeBeyondUseFiles(const char *rootdir, const char *relpath,
		   const char *filename, const char *i18n)
{
	// Create path to the beyond use directory
	struct stat fileStat;
	char target_readme_paf[PATH_MAX] = "";
	char source_readme_paf[PATH_MAX] = "";
	char target_bustats_paf[PATH_MAX] = "";
	char source_bustats_paf[PATH_MAX] = "";
	int rv;

	strcpy(target_readme_paf, rootdir);
	strcat(target_readme_paf, relpath);

	fprintf(stderr, "mkdir %s\n", target_readme_paf);
	rv = mkdir(target_readme_paf, getModeBodge());
	if (rv == 0) {
		fprintf(stderr, "Created %s\n", target_readme_paf);

	} else if (rv == -1 && errno == EEXIST) {
		// be silent if we already have the path
	}
	// (re)create the readme mode r--r-----
	strcat(target_readme_paf, "/");
	strcat(target_readme_paf, filename);

	strcpy(source_readme_paf, BODGE_WD);
	strcat(source_readme_paf, "/i18n/readme_");
	strcat(source_readme_paf, i18n);

	fprintf(stderr, "from %s\n", source_readme_paf);
	fprintf(stderr, "to %s\n", target_readme_paf);

	unlink(target_readme_paf);

	cp(target_readme_paf, source_readme_paf);

	rv = chmod(target_readme_paf, 0660);
	if (rv == -1)
		fprintf
		    (stderr,
		     "[WARNING] makeBeyondUseFiles() can't chmod \"%s\" saying \"%s\"\n",
		     target_readme_paf, strerror(errno));

	// create the statistics file mode r--r----- but only
	// if it doesn't exist - reboots persist the previous
	// statistics
	strcpy(target_bustats_paf, rootdir);
	strcat(target_bustats_paf, BEYOND_USE_COUNTERS_RELPAF);

	if (stat(target_bustats_paf, &fileStat) == -1) {
		strcpy(source_bustats_paf, BODGE_WD);
		strcat(source_bustats_paf, "/i18n/beyond-use-statistics_");
		strcat(source_bustats_paf, i18n);

		fprintf(stderr, "from %s\n", source_bustats_paf);
		fprintf(stderr, "to %s\n", target_bustats_paf);

		cp(target_bustats_paf, source_bustats_paf);

		rv = chmod(target_bustats_paf, 0660);
		if (rv == -1)
			fprintf
			    (stderr,
			     "[WARNING] makeBeyondUseFiles() can't chmod \"%s\" saying \"%s\"\n",
			     target_bustats_paf, strerror(errno));
	}
	return 0;
}

/*
 * The Access Denied file is used when trying to access verboten
 * files or directories. A regular with all privs removed.
 */
static int makeAccessDeniedConstructs(const char *paf, const char *dir)
{
	// make file first
	FILE *fp;
	int rv;

	fp = fopen(paf, "w");
	if (fp == NULL)
		fprintf(stderr, "  Unable to open \"%s\"\n", paf);
	else
		fclose(fp);

	rv = chmod(paf, 0);
	if (rv == -1)
		fprintf
		    (stderr,
		     "[WARNING] makeAccessDeniedConstructs can't chmod \"%s\" saying \"%s\"\n",
		     paf, strerror(errno));

	// dir second
	rv = mkdir(dir, 0);
	if (rv == -1)
		fprintf
		    (stderr,
		     "[WARNING] makeAccessDeniedConstructs can't mkdir \"%s\" saying \"%s\"\n",
		     dir, strerror(errno));

	rv = chmod(dir, 0);
	if (rv == -1)
		fprintf
		    (stderr,
		     "[WARNING] makeAccessDeniedConstructs can't chmod \"%s\" saying \"%s\"\n",
		     dir, strerror(errno));
	return rv;
}

/*
 * copy a file
 *
 * http://stackoverflow.com/questions/2180079/how-can-i-copy-a-file-on-unix-using-c
 * "The same method that worked back in the 70s still works now:"
 * Bless your heart, caf
 */
static int cp(const char *to, const char *from)
{
	ssize_t nread;
	ssize_t nwritten;
	char *out_ptr;
	char buf[4096] = "";
	int to_fp, from_fp;
	int saved_errno;

	from_fp = open(from, O_RDONLY);
	if (from_fp == -1) {
		DEBUGe('2') debug_msg(DPR_DATA, "%s(): from %s not found\n",
				      __func__, from);
		return from_fp;
	}

	to_fp = open(to, O_WRONLY | O_CREAT | O_EXCL, getModeBodge());
	if (to_fp == -1) {
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s(): to %s exists or can't be opened\n",
				      __func__, to);
		goto out_error;
	}

	while (nread = read(from_fp, buf, sizeof buf), nread > 0) {
		out_ptr = buf;
		do {
			nwritten = write(to_fp, out_ptr, nread);

			if (nwritten >= 0) {
				nread -= nwritten;
				out_ptr += nwritten;

			} else if (errno != EINTR) {
				goto out_error;
			}
		} while (nread > 0);
	}

	if (nread == 0) {
		if (close(to_fp) == -1) {
			to_fp = -1;
			goto out_error;
		}
		close(from_fp);
		/* Success! */
		return 0;
	}

 out_error:
	saved_errno = errno;
	dpr_error("cp err sees");

	close(from_fp);
	if (to_fp >= 0)
		close(to_fp);

	errno = saved_errno;
	return -1;
}

/*
 * Given /path/to/link, read in what it points to
 */
static int
getLinkTarget(char *target, const struct dpr_state *dpr_data, const char *paf)
{
	struct stat sb;
	ssize_t r;

	if (lstat(paf, &sb) == -1) {
		DEBUGi('2') debug_msg(dpr_data,
				      "%s(): %s not found\n", __func__, paf);
		return -1;
	}
	r = readlink(paf, target, sb.st_size + 1);
	if (r == -1) {
		perror("lstat");
		exit(EXIT_FAILURE);
	}
	if (r > sb.st_size) {
		DEBUGi('2') debug_msg(dpr_data,
				      "%s(): symlink raced\n", __func__);
		exit(EXIT_FAILURE);
	}
	target[sb.st_size] = '\0';
	DEBUGi('2') debug_msg(dpr_data,
			      "%s(): target=\"%s\"\n", __func__, target);

	return 0;
}

/* ------------------------------------------------
 * The getPafFor* functions use dxd to construct a
 * valid path and filename for individual purposes
 */

// .part files are rewritten from /path/to/file to
// sha256("/path/to") . ":" . file. This assists in
// allowing different relpaths to inhabit the same
// tmp directory.
static void getPafForFinalPath(char *paf, struct dpr_xlate_data dxd)
{
	if (dxd.is_part_file == true) {
		strcat(paf, dxd.relpath_sha256);
		strcat(paf, ":");
		strcat(paf, dxd.finalpath);

	} else {
		strcat(paf, dxd.finalpath);
	}
}

static void getPafForFinalPathWithOsxBodge(char *paf, struct dpr_xlate_data dxd)
{
	if (dxd.is_part_file == true) {
		strcat(paf, dxd.relpath_sha256);
		strcat(paf, ":");
		strcat(paf, dxd.finalpath);

	} else {
		strcat(paf, dxd.finalpath);
	}
	if (dxd.is_osx_bodge)
		addOsxBodgeExtIfNecc(paf);
}

// During rename involving temporary files we ignore the sha256
// that would otherwise be added, otherwise we'd see that sha256
// used twice, and so the sha256 would also appear in the GDrive
static void getPafForFinalPathWithOsxBodgeIgnoreTemp(char *paf,
						     struct dpr_xlate_data dxd)
{
	strcpy(paf, "");
	getPafForRelPath(paf, dxd);
	strcat(paf, dxd.finalpath);
	if (dxd.is_osx_bodge)
		addOsxBodgeExtIfNecc(paf);
}

// also see getPafForFinalPath(). When rewriting the
// path as noted, the relpath is replaced with "/"
static void getPafForRelPath(char *paf, struct dpr_xlate_data dxd)
{
	if (dxd.is_part_file == true)
		strcat(paf, "/");
	else
		strcat(paf, dxd.relpath);
}

static void getRevisionTSDir(char *paf, struct dpr_xlate_data dxd)
{
	strcpy(paf, dxd.revision);
	strcat(paf, "-");
	strcat(paf, dxd.timestamp);
}

static void getPafForRootDir(char *paf, struct dpr_xlate_data dxd)
{
	strcpy(paf, dxd.rootdir);
}

#define getPafForOrdinaryFile getLinkedlistName
static void getLinkedlistName(char *paf, struct dpr_xlate_data dxd)
{
	strcpy(paf, dxd.rootdir);
	getPafForRelPath(paf, dxd);
	getPafForFinalPathWithOsxBodge(paf, dxd);
}

static void getRelLinkedlistName(char *paf, struct dpr_xlate_data dxd)
{
	strcpy(paf, "");
	getPafForRelPath(paf, dxd);
	getPafForFinalPathWithOsxBodge(paf, dxd);
}

static void getRelLinkedlistDir(char *paf, struct dpr_xlate_data dxd)
{
	strcpy(paf, "");
	getPafForRelPath(paf, dxd);
	getPafForFinalPathWithOsxBodge(paf, dxd);
	strcat(paf, "/");
}

static void getLinkedlistRevisionTSDir(char *paf, struct dpr_xlate_data dxd)
{
	strcpy(paf, dxd.rootdir);
	getPafForRelPath(paf, dxd);
	getPafForFinalPathWithOsxBodge(paf, dxd);
	strcat(paf, "/");
	strcat(paf, dxd.revision);
	strcat(paf, "-");
	strcat(paf, dxd.timestamp);
}

static void getLinkedlistRevisionTSFile(char *paf, struct dpr_xlate_data dxd)
{
	strcpy(paf, dxd.rootdir);
	getPafForRelPath(paf, dxd);
	getPafForFinalPathWithOsxBodge(paf, dxd);
	strcat(paf, "/");
	strcat(paf, dxd.revision);
	strcat(paf, "-");
	strcat(paf, dxd.timestamp);
	strcat(paf, "/");
	getPafForFinalPath(paf, dxd);
}

static void getRelLinkedlistRevisionTSFile(char *paf, struct dpr_xlate_data dxd)
{
	strcpy(paf, "");
	getPafForRelPath(paf, dxd);
	getPafForFinalPathWithOsxBodge(paf, dxd);
	strcat(paf, "/");
	strcat(paf, dxd.revision);
	strcat(paf, "-");
	strcat(paf, dxd.timestamp);
	strcat(paf, "/");
	getPafForFinalPath(paf, dxd);
}

static void getLinkedlistLatestLnk(char *paf, struct dpr_xlate_data dxd)
{
	strcpy(paf, dxd.rootdir);
	getPafForRelPath(paf, dxd);
	getPafForFinalPathWithOsxBodge(paf, dxd);
	strcat(paf, "/");
	strcat(paf, LATEST_FILENAME);
}

static void
getLinkedlistLatestFMetadataLnk(char *paf, struct dpr_xlate_data dxd)
{
	strcpy(paf, dxd.rootdir);
	getPafForRelPath(paf, dxd);
	getPafForFinalPathWithOsxBodge(paf, dxd);
	strcat(paf, "/");
	strcat(paf, LATEST_FILENAME);
	strcat(paf, "/");
	strcat(paf, FMETADATA_FILENAME);
}

static void
getLinkedlistLatestFMetadataLnkTS(char *paf, struct dpr_xlate_data dxd)
{
	strcpy(paf, dxd.rootdir);
	getPafForRelPath(paf, dxd);
	getPafForFinalPathWithOsxBodge(paf, dxd);
	strcat(paf, "/");
	strcat(paf, LATEST_FILENAME);
	strcat(paf, "/");
	strcat(paf, FMETADATA_FILENAME);
	strcat(paf, "-");
	strcat(paf, dxd.timestamp);
}

static void getFMetadataTSFile(char *paf, struct dpr_xlate_data dxd)
{
	strcpy(paf, FMETADATA_FILENAME);
	strcat(paf, "-");
	strcat(paf, dxd.timestamp);
}

static void getLinkedlistDMetadataLnk(char *paf, struct dpr_xlate_data dxd)
{
	strcpy(paf, dxd.rootdir);
	getPafForRelPath(paf, dxd);
	getPafForFinalPathWithOsxBodge(paf, dxd);
	strcat(paf, "/");
	strcat(paf, DMETADATA_FILENAME);
}

/* static void getLinkedlistReservedFile(char *paf, struct dpr_xlate_data dxd) */
/* { */
/*	strcpy(paf, dxd.rootdir); */
/*	getPafForRelPath(paf, dxd); */
/*	getPafForFinalPathWithOsxBodge(paf, dxd); */
/*	strcat(paf, "/"); */
/*	strcat(paf, RESERVED_FILENAME); */
/* } */

static void getDMetadataTSFile(char *paf, struct dpr_xlate_data dxd)
{
	strcpy(paf, DMETADATA_FILENAME);
	strcat(paf, "-");
	strcat(paf, dxd.timestamp);
}

static void getLinkedlistDMetadataTSFile(char *paf, struct dpr_xlate_data dxd)
{
	strcpy(paf, dxd.rootdir);
	getPafForRelPath(paf, dxd);
	getPafForFinalPathWithOsxBodge(paf, dxd);
	strcat(paf, "/");
	strcat(paf, DMETADATA_FILENAME);
	strcat(paf, "-");
	strcat(paf, dxd.timestamp);
}

static void
getLinkedlistLatestLinkedlistFile(char *paf, struct dpr_xlate_data dxd)
{
	if (dxd.payload[0] == '\0') {
		strcpy(paf, dxd.rootdir);
		getPafForRelPath(paf, dxd);
		getPafForFinalPathWithOsxBodge(paf, dxd);
		strcat(paf, "/");
		strcat(paf, LATEST_FILENAME);
		strcat(paf, "/");
		getPafForFinalPath(paf, dxd);

	} else {
		strcpy(paf, dxd.payload_root);
		strcat(paf, dxd.payload);
	}
}

/*
 * Given /path/to/file/:latest, resolve :latest and
 * so return paf /path/to/file/revision-timestamp/file
 */
static int
getLinkedlistRevtsLinkedlistFile(char *paf, const struct dpr_state *dpr_data,
				 struct dpr_xlate_data dxd)
{
	if (dxd.payload[0] == '\0') {
		char revts[PATH_MAX] = "";

		strcpy(paf, dxd.rootdir);
		getPafForRelPath(paf, dxd);
		getPafForFinalPathWithOsxBodge(paf, dxd);
		strcat(paf, "/");
		strcat(paf, LATEST_FILENAME);

		getLinkTarget(revts, dpr_data, paf);

		strcpy(paf, dxd.rootdir);
		getPafForRelPath(paf, dxd);
		getPafForFinalPathWithOsxBodge(paf, dxd);
		strcat(paf, "/");
		strcat(paf, revts);
		strcat(paf, "/");
		getPafForFinalPath(paf, dxd);

	} else {
		strcpy(paf, dxd.payload_root);
		strcat(paf, dxd.payload);
	}
	return 0;
}

static void resetDxd(struct dpr_xlate_data *dxd)
{
	dxd->finalpath[0] = dxd->payload[0] = dxd->payload_root[0] =
	    dxd->originaldir[0] = dxd->relpath[0] = dxd->relpath_sha256[0] =
	    dxd->rootdir[0] = dxd->timestamp[0] = '\0';

	dxd->deleted = dxd->is_osx_bodge = dxd->is_part_file = false;
	dxd->dprfs_filetype = DPRFS_FILETYPE_NA;
	dxd_initialiseTimestamp(dxd);
}

static void
accessDeniedDxdFile(struct dpr_state *dpr_data, struct dpr_xlate_data *dxd)
{
	dxd->timestamp[0] = dxd->relpath_sha256[0] = dxd->payload[0] =
	    dxd->payload_root[0] = dxd->originaldir[0] = '\0';
	strcpy(dxd->rootdir, TMP_PATH);
	strcpy(dxd->relpath, "/");
	catSha256ToStr(dxd->relpath_sha256, dpr_data, dxd->relpath);
	strcpy(dxd->finalpath, ACCESS_DENIED_FILE);
	dxd->is_osx_bodge = false;
	dxd->is_part_file = false;
	dxd->dprfs_filetype = DPRFS_FILETYPE_OTHER;
	dxd_initialiseTimestamp(dxd);
}

static void
accessDeniedDxdDir(struct dpr_state *dpr_data, struct dpr_xlate_data *dxd)
{
	dxd->timestamp[0] = dxd->relpath_sha256[0] = dxd->payload[0] =
	    dxd->payload_root[0] = dxd->originaldir[0] = '\0';
	strcpy(dxd->rootdir, TMP_PATH);
	strcpy(dxd->relpath, "/");
	catSha256ToStr(dxd->relpath_sha256, dpr_data, dxd->relpath);
	strcpy(dxd->finalpath, ACCESS_DENIED_DIR);
	dxd->is_osx_bodge = false;
	dxd->is_part_file = false;
	dxd->dprfs_filetype = DPRFS_FILETYPE_DIR;
	dxd_initialiseTimestamp(dxd);
}

/*------------------------------------------------*/
// Mallocable functionality

/* Mallocable string */
/* We're done: Release resources */
static void mstrfree(struct mallocable_string *mstr)
{
	if (mstr->string != NULL)
		free(mstr->string);
	mstr->string = NULL;
	mstr->string_len = 0;
	mstr->string_max = 0;
}

/* We're not done: get to default */
static void mstrreset(struct mallocable_string *mstr)
{
	mstr->string_max = DEFAULT_MALLOC_SZ;
	mstr->string = realloc(mstr->string,
			       mstr->string_max * (sizeof(*mstr->string)));
	*mstr->string = '\0';
	mstr->string_len = 0;
}

/* Concatenate to a mallocable string */
static void
mstrcat(struct mallocable_string *mstr, const struct dpr_state *dpr_data,
	const char *str)
{
	size_t newlen = strlen(str);

	if (newlen == 0)
		return;

	DEBUGi('2') debug_msg
	    (dpr_data, "%s() hex\"%x\" dec\"%d\" len\"%d\"\n",
	     __func__, str[0], str[0], strlen(str));
	DEBUGi('2') debug_msg(dpr_data, "%s() string in \"%s\"\n", __func__,
			      str);
	DEBUGi('2') debug_msg(dpr_data, "%s string dest \"%s\"\n", __func__,
			      mstr->string);

	newlen += mstr->string_len;
	if (newlen >= mstr->string_max) {
		do {
			mstr->string_max += DEFAULT_MALLOC_SZ;
		} while (newlen >= mstr->string_max);
		DEBUGi('2') debug_msg(dpr_data,
				      "%s() attempting realloc to %d bytes\n",
				      __func__,
				      mstr->string_max * sizeof(*mstr->string));
		mstr->string =
		    realloc(mstr->string,
			    mstr->string_max * sizeof(*mstr->string));
		mstr->string[mstr->string_len] = '\0';
	}
	DEBUGi('2') debug_msg
	    (dpr_data, "%s() string dest \"%s\"\n", __func__, mstr->string);
	strcat(mstr->string, str);
	DEBUGi('2') debug_msg
	    (dpr_data, "%s() string dest \"%s\"\n", __func__, mstr->string);
	mstr->string_len = newlen;
}

/*------------------------------------------------*/
// Metadata functionality

static void md_reset(struct metadata_array *md_arr)
{
	md_arr->beyond_use_on.operation = MD_NOP;
	md_arr->beyond_use_on.value[0] = '\0';
	md_arr->deleted.operation = MD_NOP;
	md_arr->deleted.value[0] = '\0';
	md_arr->llid.operation = MD_NOP;
	md_arr->llid.value[0] = '\0';
	md_arr->not_via.operation = MD_NOP;
	md_arr->not_via.value[0] = '\0';
	md_arr->not_via.next = NULL;
	mstrreset(&md_arr->others);
	md_arr->payload_loc.operation = MD_NOP;
	md_arr->payload_loc.value[0] = '\0';
	md_arr->original_dir.operation = MD_NOP;
	md_arr->original_dir.value[0] = '\0';
	md_arr->renamed_from.operation = MD_NOP;
	md_arr->renamed_from.value[0] = '\0';
	md_arr->renamed_to.operation = MD_NOP;
	md_arr->renamed_to.value[0] = '\0';
	md_arr->sha256.operation = MD_NOP;
	md_arr->sha256.value[0] = '\0';
	md_arr->supercedes.operation = MD_NOP;
	md_arr->supercedes.value[0] = '\0';
}

/* Receive the :?metadata contents into given memory */
static FILE *md_load(char *buffer, intmax_t buffer_sz, char *paf)
{
	// is file too large? should realloc()
	FILE *fp;

	fp = fopen(paf, "r");
	if (fp != NULL) {
		buffer[0] = '\0';
		if (fread(buffer, buffer_sz, 1, fp))
			buffer[buffer_sz] = '\0';
		fclose(fp);
		return fp;
	}
	return fp;
}

/* Create sufficient space to store the metadata */
static void *md_malloc(intmax_t * buffer_sz, const char *paf)
{
	struct stat st;

	if (stat(paf, &st) == 0)
		*buffer_sz = st.st_size;
	else
		return NULL;

	return malloc(*buffer_sz + 1);
}

/* Finished bringing in the metadata */
static void md_unload(char *buffer)
{
	free(buffer);
}

/*
 * The metadata files contain data about the file or directory to
 * which they refer. They are treated as one line per config option
 * in the form
 * [<whitespace>]<name>[whitespace][=[<whitespace>]<value>[<whitespace]]\n
 * such that the = and value are optional.
 *
 * Instead of repeatedly parsing the metadata buffer as previously,
 * receive useful parts of the file into a known structure that can be
 * interrogated just the once
 */
static int
md_getIntoStructure(struct metadata_array *md_arr,
		    const struct dpr_state *dpr_data, char *buffer)
{
	struct metadata_multiple *not_via_p;
	struct metadata_multiple *next_p;
	char debug_w[] = "intermediate";
	char *eq;
	char *line_p1, *line_p2;
	char *name_p1, *name_p2;
	char *value_p1, *value_p2;
	bool rv, done;

	DEBUGi('2') debug_msg(dpr_data,
			      "%s(): entered, buffer of \"%s\"\n",
			      __func__, buffer);

	// iterate over each line in turn,
	// and make a name = value pair from each

	md_reset(md_arr);

	if (*buffer == '\0') {
		DEBUGi('2') debug_msg(dpr_data,
				      "%s(): completed, empty buffer\n",
				      __func__);
		return false;
	}

	not_via_p = &md_arr->not_via;
	done = false;
	rv = false;
	line_p1 = buffer;
	strcpy(debug_w, "intermediate");

	do {
		// obtain the next line
		line_p2 = strchr(line_p1, '\n');
		if (line_p2 == NULL) {
			line_p2 = line_p1 + strlen(line_p1);
			strcpy(debug_w, "end");
			done = true;

		} else {
			strcpy(debug_w, "intermediate");
		}
		DEBUGi('2') debug_msg(dpr_data,
				      "%s(): found %s line: \"%.*s\"\n",
				      __func__, debug_w,
				      (int)(long long)line_p2 -
				      (long long)line_p1, line_p1);

		// split as name = value
		eq = strchr(line_p1, '=');
		if (eq == NULL) {
			name_p1 = line_p1;
			name_p2 = line_p2;
			value_p1 = value_p2 = line_p2;

		} else {
			name_p1 = line_p1;
			name_p2 = eq - 1;
			DEBUGi('2') debug_msg(dpr_data,
					      "%s(): name=\"%.*s\"\n",
					      __func__,
					      (int)(long long)name_p2 -
					      (long long)name_p1 + 1, name_p1);
			value_p1 = eq + 1;
			value_p2 = line_p2;
			DEBUGi('2') debug_msg(dpr_data,
					      "%s(): value=\"%.*s\"\n",
					      __func__,
					      (int)(long long)value_p2 -
					      (long long)value_p1 + 1,
					      value_p1);
		}

		// trim whitespace fore and aft
		while (isspace(*name_p1))
			name_p1++;
		while (isspace(*value_p1)) {
			DEBUGi('2') debug_msg(dpr_data,
					      "%s(): remove char\"%x\"\n",
					      __func__, *value_p1);
			value_p1++;
		}

		while (isspace(*name_p2))
			name_p2--;
		while (isspace(*value_p2)) {
			DEBUGi('2') debug_msg(dpr_data,
					      "%s(): remove char\"%x\"\n",
					      __func__, *value_p2);
			value_p2--;
		}

		DEBUGi('2') debug_msg(dpr_data, "%s(): name=\"%.*s\"\n",
				      __func__,
				      (int)(long long)name_p2 -
				      (long long)name_p1 + 1, name_p1);
		DEBUGi('2') debug_msg(dpr_data, "%s(): value=\"%.*s\"\n",
				      __func__,
				      (int)(long long)value_p2 -
				      (long long)value_p1 + 1, value_p1);

		if (strncmp(name_p1, METADATA_KEY_BEYONDUSEON,
			    strlen(METADATA_KEY_BEYONDUSEON) - 3) == 0) {
			md_arr->beyond_use_on.operation = MD_NOP;
			strncpy(md_arr->beyond_use_on.value, value_p1,
				value_p2 - value_p1 + 1);
			md_arr->beyond_use_on.value[value_p2 - value_p1 + 1] =
			    '\0';

		} else if (strncmp(name_p1, METADATA_KEY_DELETED,
				   strlen(METADATA_KEY_DELETED) - 3) == 0) {
			md_arr->deleted.operation = MD_NOP;
			strncpy(md_arr->deleted.value, value_p1,
				value_p2 - value_p1 + 1);
			md_arr->deleted.value[value_p2 - value_p1 + 1] = '\0';

		} else if (strncmp(name_p1, METADATA_KEY_LLID,
				   strlen(METADATA_KEY_LLID) - 3) == 0) {
			md_arr->llid.operation = MD_NOP;
			strncpy(md_arr->llid.value, value_p1,
				value_p2 - value_p1 + 1);
			md_arr->llid.value[value_p2 - value_p1 + 1] = '\0';

		} else if (strncmp(name_p1, METADATA_KEY_NOTVIA,
				   strlen(METADATA_KEY_NOTVIA) - 3) == 0) {
			// not-via different as multiple directives allowed
			DEBUGi('2') debug_msg
			    (dpr_data,
			     "%s(): \"%.*s\" may have several values\n",
			     __func__,
			     (int)(long long)name_p2 - (long long)name_p1 + 1,
			     name_p1);

			not_via_p->operation = MD_NOP;
			strncpy(not_via_p->value, value_p1,
				value_p2 - value_p1 + 1);
			not_via_p->value[value_p2 - value_p1 + 1] = '\0';

			// improve by storing length of chain and passing to md_free()
			// to limit recursion
			next_p =
			    (struct metadata_multiple *)malloc(sizeof(*next_p));

			next_p->operation = MD_NOP;
			next_p->next = NULL;
			next_p->value[0] = '\0';
			not_via_p->next = (struct metadata_multiple *)next_p;
			not_via_p = next_p;

		} else if (strncmp(name_p1, METADATA_KEY_PAYLOADLOC,
				   strlen(METADATA_KEY_PAYLOADLOC) - 3) == 0) {
			md_arr->payload_loc.operation = MD_NOP;
			strncpy(md_arr->payload_loc.value, value_p1,
				value_p2 - value_p1 + 1);
			md_arr->payload_loc.value[value_p2 - value_p1 + 1] =
			    '\0';

		} else if (strncmp(name_p1, METADATA_KEY_ORIGINALDIR,
				   strlen(METADATA_KEY_ORIGINALDIR) - 3) == 0) {
			md_arr->original_dir.operation = MD_NOP;
			strncpy(md_arr->original_dir.value, value_p1,
				value_p2 - value_p1 + 1);
			md_arr->original_dir.value[value_p2 - value_p1 + 1] =
			    '\0';

		} else if (strncmp(name_p1, METADATA_KEY_RENAMEDFROM,
				   strlen(METADATA_KEY_RENAMEDFROM) - 3) == 0) {
			md_arr->renamed_from.operation = MD_NOP;
			strncpy(md_arr->renamed_from.value, value_p1,
				value_p2 - value_p1 + 1);
			md_arr->renamed_from.value[value_p2 - value_p1 + 1] =
			    '\0';

		} else if (strncmp(name_p1, METADATA_KEY_RENAMEDTO,
				   strlen(METADATA_KEY_RENAMEDTO) - 3) == 0) {
			md_arr->renamed_to.operation = MD_NOP;
			strncpy(md_arr->renamed_to.value, value_p1,
				value_p2 - value_p1 + 1);
			md_arr->renamed_to.value[value_p2 - value_p1 + 1] =
			    '\0';

		} else if (strncmp(name_p1, METADATA_KEY_SHA256,
				   strlen(METADATA_KEY_SHA256) - 3) == 0) {
			md_arr->sha256.operation = MD_NOP;
			strncpy(md_arr->sha256.value, value_p1,
				value_p2 - value_p1 + 1);
			md_arr->sha256.value[value_p2 - value_p1 + 1] = '\0';

		} else if (strncmp(name_p1, METADATA_KEY_SUPERCEDES,
				   strlen(METADATA_KEY_SUPERCEDES) - 3) == 0) {
			md_arr->supercedes.operation = MD_NOP;
			strncpy(md_arr->supercedes.value, value_p1,
				value_p2 - value_p1 + 1);
			md_arr->supercedes.value[value_p2 - value_p1 + 1] =
			    '\0';

		} else {
			DEBUGi('2') debug_msg
			    (dpr_data,
			     "%s(): found line with no hits: \"%.*s\" %d\n",
			     __func__,
			     (int)(long long)line_p2 - (long long)line_p1,
			     line_p1, line_p2 - line_p1);
			if (line_p2 - line_p1 > 0) {
				mstrcat(&md_arr->others, dpr_data, line_p1);
			}
		}
		// and prepare for next loop
		line_p1 = line_p2 + 1;

	} while (done == false);

	DEBUGi('2') debug_msg(dpr_data, "%s(): completed\n", __func__);
	return rv;
}

/*
 * Knowing where the metadata (either :Fmetadata or :Dmetadata) is to be
 * saved, convert the metadata_array structure into that file
 */
static int
saveMetadataToFile(const char *metadata_paf, struct metadata_array *md_arr)
{
	FILE *fp;

	// create :latest/:Fmetadata, fill as required
	DEBUGe('2') debug_msg(DPR_DATA,
			      "%s() writing out to %s\n", __func__,
			      metadata_paf);
	fp = fopen(metadata_paf, "w");
	if (fp == NULL) {
		DEBUGe('2') debug_msg(DPR_DATA,
				      "[WARNING] %s() unable to open \"%s\"\n",
				      __func__, metadata_paf);
		return -1;
	}
	// Handle directives that only allow one key=value pair per file
	if (*md_arr->beyond_use_on.value != '\0') {
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s() beyond_use_on \"%s\"\n",
				      __func__, md_arr->beyond_use_on.value);
		fwrite(METADATA_KEY_BEYONDUSEON,
		       strlen(METADATA_KEY_BEYONDUSEON), 1, fp);
		fwrite(md_arr->beyond_use_on.value,
		       strlen(md_arr->beyond_use_on.value), 1, fp);
		fwrite("\n", strlen("\n"), 1, fp);
	}

	if (*md_arr->deleted.value != '\0') {
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s() deleted \"%s\"\n",
				      __func__, md_arr->deleted.value);
		fwrite(METADATA_KEY_DELETED, strlen(METADATA_KEY_DELETED), 1,
		       fp);
		fwrite(md_arr->deleted.value, strlen(md_arr->deleted.value), 1,
		       fp);
		fwrite("\n", strlen("\n"), 1, fp);
	}

	if (*md_arr->llid.value != '\0') {
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s() llid \"%s\"\n",
				      __func__, md_arr->llid.value);
		fwrite(METADATA_KEY_LLID, strlen(METADATA_KEY_LLID), 1, fp);
		fwrite(md_arr->llid.value, strlen(md_arr->llid.value), 1, fp);
		fwrite("\n", strlen("\n"), 1, fp);
	}

	if (*md_arr->payload_loc.value != '\0') {
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s() payload_loc \"%s\"\n",
				      __func__, md_arr->payload_loc.value);
		fwrite(METADATA_KEY_PAYLOADLOC, strlen(METADATA_KEY_PAYLOADLOC),
		       1, fp);
		fwrite(md_arr->payload_loc.value,
		       strlen(md_arr->payload_loc.value), 1, fp);
		fwrite("\n", strlen("\n"), 1, fp);
	}

	if (*md_arr->original_dir.value != '\0') {
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s() original_dir \"%s\"\n",
				      __func__, md_arr->original_dir.value);
		fwrite(METADATA_KEY_ORIGINALDIR,
		       strlen(METADATA_KEY_ORIGINALDIR), 1, fp);
		fwrite(md_arr->original_dir.value,
		       strlen(md_arr->original_dir.value), 1, fp);
		fwrite("\n", strlen("\n"), 1, fp);
	}

	if (*md_arr->renamed_from.value != '\0') {
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s() renamed_from \"%s\"\n",
				      __func__, md_arr->renamed_from.value);
		fwrite(METADATA_KEY_RENAMEDFROM,
		       strlen(METADATA_KEY_RENAMEDFROM), 1, fp);
		fwrite(md_arr->renamed_from.value,
		       strlen(md_arr->renamed_from.value), 1, fp);
		fwrite("\n", strlen("\n"), 1, fp);
	}

	if (*md_arr->renamed_to.value != '\0') {
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s() renamed_to \"%s\"\n",
				      __func__, md_arr->renamed_to.value);
		fwrite(METADATA_KEY_RENAMEDTO, strlen(METADATA_KEY_RENAMEDTO),
		       1, fp);
		fwrite(md_arr->renamed_to.value,
		       strlen(md_arr->renamed_to.value), 1, fp);
		fwrite("\n", strlen("\n"), 1, fp);
	}

	if (*md_arr->sha256.value != '\0') {
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s() sha256 \"%s\"\n",
				      __func__, md_arr->sha256.value);
		fwrite(METADATA_KEY_SHA256, strlen(METADATA_KEY_SHA256), 1, fp);
		fwrite(md_arr->sha256.value, strlen(md_arr->sha256.value), 1,
		       fp);
		fwrite("\n", strlen("\n"), 1, fp);
	}

	if (*md_arr->supercedes.value != '\0') {
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s() supercedes \"%s\"\n",
				      __func__, md_arr->supercedes.value);
		fwrite(METADATA_KEY_SUPERCEDES, strlen(METADATA_KEY_SUPERCEDES),
		       1, fp);
		fwrite(md_arr->supercedes.value,
		       strlen(md_arr->supercedes.value), 1, fp);
		fwrite("\n", strlen("\n"), 1, fp);
	}
	// handle the not_via chain - each not_via can reference the next in
	// a forwardly connected linked list. Write out each in turn
	struct metadata_multiple *mp = &md_arr->not_via;
	do {
		if (mp->value[0] != '\0') {
			DEBUGe('2') debug_msg(DPR_DATA,
					      "%s() not_via \"%s\"\n",
					      __func__, mp->value);
			fwrite(METADATA_KEY_NOTVIA, strlen(METADATA_KEY_NOTVIA),
			       1, fp);
			fwrite(mp->value, strlen(mp->value), 1, fp);
			fwrite("\n", strlen("\n"), 1, fp);
		}
		mp = mp->next;
	} while (mp != NULL);

	// write out all the other data: this stuff tends to be for IT's
	// benefit, it won't be processed by dprfs
	if (md_arr->others.string_len > 0) {
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s() others \"%s\"\n",
				      __func__, md_arr->others.string);
		fwrite(md_arr->others.string, strlen(md_arr->others.string), 1,
		       fp);
	}

	fclose(fp);
	DEBUGe('2') debug_msg(DPR_DATA, "%s() exit \n", __func__);
	return 0;
}

/*
 * We create a linkedlist of identical directives when we read them
 * in, malloc()ing as we go. Recurse to the last link, free() it,
 * and repeat back down the chain
 */
static void md_free(struct metadata_multiple *md_mul_p)
{
	// go through the arr freeing each "next" in turn
	if (md_mul_p == NULL)
		return;

	if (md_mul_p->next == NULL) {
		return;
	}

	md_free(md_mul_p->next);
	free(md_mul_p->next);
}

/*
 * See if a string appears in the linkedlist of directives we took
 * in with the added wrinkle that these will be paths: process
 * considering that "/path/" and "/path" are to be treated as
 * identical
 */
static bool
md_isPathInChain(struct dpr_state *dpr_data,
		 struct metadata_multiple *md_mul_p, const char *value)
{
	int len;

	// go through the arr freeing each "next" in turn
	if (md_mul_p == NULL)
		return false;

	//test form "/path/\0";
	DEBUGi('2') debug_msg
	    (dpr_data,
	     "%s() compares not-via=\"%s\" w/ o_paf=\"%s\"\n",
	     __func__, md_mul_p->value, value);
	if (strcmp(md_mul_p->value, value) == 0)
		return true;

	// test form "/path\n" (\n to exlude form /pathX)
	len = strlen(md_mul_p->value) - 1;
	if (len > 0
	    && md_mul_p->value[len] == '\n'
	    && strncmp(md_mul_p->value, value, len == 0)) {
		DEBUGi('2') debug_msg
		    (dpr_data,
		     "%s() compares saw not-via=\"%s\" w/ o_paf=\"%s\" len=\"%d\"\n",
		     __func__, md_mul_p->value, value, len);
		return true;
	}

	if (md_mul_p->next == NULL) {
		DEBUGi('2') debug_msg(dpr_data,
				      "%s() no more to look at \n", __func__);
		return false;
	}

	DEBUGi('2') debug_msg(dpr_data, "%s() looking at next\n", __func__);
	return md_isPathInChain(dpr_data, md_mul_p->next, value);
}

/*
 * We use this for not_via, adding a new not_via structure if we
 * haven't dealt with the value already, and then forward linking
 * it to the next for later use.
 */
#define notviaChainToIncludePaf md_addPathToChain
static bool
md_addPathToChain(struct metadata_multiple *md_mul_p, const char *value)
{
	if (strcmp(value, md_mul_p->value) == 0) {
		// already got it
		return true;
	}

	if (md_mul_p->next != NULL) {
		// not got it, check next
		return md_addPathToChain(md_mul_p->next, value);
	}
	// reached the end without finding it, add it in
	md_mul_p->next = (struct metadata_multiple *)
	    malloc(sizeof(*md_mul_p->next));
	md_mul_p->next->operation = MD_NOP;
	strcpy(md_mul_p->next->value, value);
	md_mul_p->next->next = NULL;
	return true;
}

/*
 * When a not_via path is no longer needed, we blank its value: it's
 * left to later to free() its malloc()
 */
#define notviaChainToExcludePaf md_removePathFromChain
static bool
md_removePathFromChain(struct metadata_multiple *md_mul_p, const char *value)
{
	if (strcmp(value, md_mul_p->value) == 0) {
		// got a match - remove it and look for more
		md_mul_p->operation = MD_NOP;
		md_mul_p->value[0] = '\0';
		return md_removePathFromChain(md_mul_p->next, value);

	} else if (md_mul_p->next != NULL) {
		// not got it, check next
		return md_addPathToChain(md_mul_p->next, value);
	}
	// reached the end without finding it
	return true;
}

/*
 * If the metadata indicates a timestamp after which access is to be denied.
 * test whether that's happened here
 */
static bool
md_checkBeyondUse(struct dpr_state *dpr_data, struct metadata_single *md_sin_p)
{
	char currentTS[TIMESTAMP_SIZE] = "";

	DEBUGi('2') debug_msg(dpr_data, "%s() entered\n", __func__);
	if (md_sin_p == NULL)
		return false;

	DEBUGi('2') debug_msg(dpr_data, "%s() 2\n", __func__);
	if (md_sin_p->value[0] == '\0')
		return false;

	DEBUGi('2') debug_msg(dpr_data, "%s() 3\n", __func__);
	getCondensedSystemUTime(currentTS);

	DEBUGi('2') debug_msg
	    (dpr_data,
	     "%s() checking current=\"%s\" vs value=\"%s\"\n",
	     __func__, currentTS, md_sin_p->value);

	if (strcmp(currentTS, md_sin_p->value) > 0)
		return true;

	return false;
}

/* ------------------------------------------------------------ */

/*
 * Calculate consecutive revisions from AA0000 to AA9999 through
 * AB00000 and so on until ZZ99999. No more increments afterwards.
 * Roughly 67.6 million increments
 */
static void incAndAssignRevision(char *revision, const char *linkTarget)
{
	strncpy(revision, linkTarget, REVISION_NAME_LEN);
	revision[REVISION_NAME_LEN] = '\0';

	revision[6]++;
	if (revision[6] != ':')
		return;
	revision[6] = '0';

	revision[5]++;
	if (revision[5] != ':')
		return;
	revision[5] = '0';

	revision[4]++;
	if (revision[4] != ':')
		return;
	revision[4] = '0';

	revision[3]++;
	if (revision[3] != ':')
		return;
	revision[3] = '0';

	revision[2]++;
	if (revision[2] != ':')
		return;
	revision[2] = '0';

	revision[1]++;
	if (revision[1] != '[')
		return;
	revision[1] = 'A';

	revision[0]++;
	if (revision[0] != '[')
		return;

	strcpy(revision, REVISION_NAME_MAXIMUM);
	return;

}

/*
 * Given a path formed by lhs . rhs, form an opinion about what
 * kind of filesystem entry it represents: a linkedlist, a
 * directory, etc
 */
static int
dpr_xlateWholePath_whatIsThis(const struct dpr_state *dpr_data,
			      struct dpr_xlate_data *dxd, const char *lhs,
			      const char *rhs, bool ignoreState)
{
	struct stat fileStat;
	char paf[PATH_MAX] = "";
	int len;

	dxd->is_osx_bodge = false;
	if (*rhs != '\0') {
		/* having a rhs implies the lhs is a directory as "lhs/rhs" */
		DEBUGi('3') debug_msg(dpr_data,
				      "%s(): confirmed handling a directory\n",
				      __func__);
		return DPRFS_FILETYPE_DIR;
	}
	// lhs = something
	// rhs = \0
	// could be a normal directory, a linkedlist, an osx bodged
	// linkedlist, a datastore or an ordinary file
	//  depending on lstat and presence of lhs/:latest,
	//  lhs_d/:latest and datastore extensions

	/* reform path to be checked */
	strcpy(paf, dxd->rootdir);
	if (dxd->is_part_file == true) {
		strcpy(dxd->relpath_sha256, "");
		catSha256ToStr(dxd->relpath_sha256, dpr_data, dxd->relpath);
		strcat(paf, "/");
		strcat(paf, dxd->relpath_sha256);
		strcat(paf, ":");

	} else {
		strcat(paf, dxd->relpath);
	}

	strcat(paf, lhs);
	DEBUGi('3') debug_msg(dpr_data,
			      "%s(): Checking nature of \"%s\"\n", __func__,
			      paf);
	len = strlen(paf);

	if (strcmp(paf + len - 6, ".accdb") == 0) {
		// .*\.accdb$ is Microsft Access
		strcpy(dxd->finalpath, lhs);
		return DPRFS_FILETYPE_DS;
	}

	if (strcmp(paf + len - 7, ".laccdb") == 0) {
		// .*\.laccdb$ is Microsft Access lock file
		strcpy(dxd->finalpath, lhs);
		return DPRFS_FILETYPE_DS;
	}

	if (strcmp(paf + len - 2, "_d") == 0) {
		/* named as an osx bodge, only need to check file itself */
		dxd->is_osx_bodge = true;
		DEBUGi('3') debug_msg(dpr_data,
				      "%s(): \"%s\" arrives an OSX bodge\n",
				      __func__, paf);

		if (stat(paf, &fileStat) == -1) {
			/* doesnt actually appear in fs */
			if (ignoreState == false) {
				// We care if actually exists (as with looking
				// to access a specific existing file) so reset
				// the dxd and return as it's missing
				resetDxd(dxd);
				DEBUGi('3') debug_msg
				    (dpr_data,
				     "%s(): doesn't exist, and we can't ignore it, reset and return\n",
				     __func__);
				return -1;	//ut_010
			}
			// else we don't care (as with creating a directory
			// thought not to exist already, or wildcard) so
			// leave the dxd populated
			strcpy(dxd->finalpath, lhs);
			misc_debugDxd(dpr_data, '3', dxd,
				      "Doesn't exist but we can ignore that. Return ",
				      __func__);
			return -1;	//ut_003
		}
	} else {
		/* not named a bodge, so check it itself, and it itself_d  */
		if (stat(paf, &fileStat) == -1) {
			/* doesn't exist under a non-_d name */
			DEBUGi('3') debug_msg(dpr_data,
					      "%s(): \"%s\" 404 so check if OSX bodged\n",
					      __func__, paf);
			strcat(paf, "_d");
			if (stat(paf, &fileStat) == -1) {
				/* doesn't exist as an osx bodge either */
				if (ignoreState == false) {
					// We care if actually exists (as with looking
					// to access a specific existing file) so reset
					// the dxd and return as it's missing
					resetDxd(dxd);
					DEBUGi('3') debug_msg
					    (dpr_data,
					     "%s(): doesn't exist, and we can't ignore it, reset and return\n",
					     __func__);
					return -1;	//ut_010
				}
				// else we don't care (as with creating a directory
				// thought not to exist already, or wildcard) so
				// leave the dxd populated
				strcpy(dxd->finalpath, lhs);
				misc_debugDxd(dpr_data, '3', dxd,
					      " Doesn't exist but we can ignore that. Return ",
					      __func__);
				return -1;	//ut_003

			} else {
				dxd->is_osx_bodge = true;
				DEBUGi('3') debug_msg(dpr_data,
						      "%s(): \"%s\" is an OSX bodge\n",
						      __func__, paf);
			}
		}
	}

	// file or directory does exist: so is it a directory,
	// linkedlist or other?
	if (S_ISDIR(fileStat.st_mode)) {
		// is a dir. Linkedlist? Or simple Directory?
		char latest_paf[PATH_MAX] = "";
		strcpy(latest_paf, paf);
		strcat(latest_paf, "/:latest");
		if (stat(latest_paf, &fileStat) == -1) {
			// not present, so is a simple directory
			if (ignoreState == true)
				// we have a real directory and it's not used
				// as a linkedlist; we're ignoring state which
				// means this is mkdir etc, not getattr etc
				// so fill in the missing puzzle piece
				strcpy(dxd->finalpath, lhs);

			DEBUGi('3') debug_msg
			    (dpr_data,
			     "%s(): confirmed \"%s\" is a simple directory\n",
			     __func__, paf);
			return DPRFS_FILETYPE_DIR;
		}
		// "/:latest" present so must be a linkedlist.
		DEBUGi('3') debug_msg(dpr_data,
				      "%s(): confirmed handling a linkedlist\n",
				      __func__);
		return DPRFS_FILETYPE_LL;
	}
	// this is likely a filename
	return DPRFS_FILETYPE_OTHER;
}

static bool establishTempNessCheck(char *original_paf)
{
	char *p;
	unsigned int op_len;
	bool rv = false;

	op_len = strlen(original_paf);
#if USE_TDRIVE
	if (op_len >= 1) {
		// .*\.tmp$ includes Microsoft Office
		p = original_paf + strlen(original_paf) - 1;
		if (*p == '~') {
			rv = true;
		}
	}
	if (op_len >= 2) {
		p = original_paf + op_len;
		while (*p != '/' && p != original_paf)
			p--;
		// ^.*/~$.* includes Microsoft Office
		if (p[0] == '/' && p[1] == '~' && p[2] == '$') {
			rv = true;
		}
	}
	if (op_len >= 4) {
		// .*\.tmp$ includes Microsoft Office
		p = original_paf + strlen(original_paf) - 4;
		if (strcmp(p, ".tmp") == 0) {
			rv = true;
		}
	}
	if (op_len >= 5) {
		// .*\.part$ includes Dolphin on GNU/Linux
		p = original_paf + strlen(original_paf) - 5;
		if (strcmp(p, ".part") == 0) {
			rv = true;
		}
	}
	if (op_len >= 6) {
		// .*\.accdb$ is Microsoft Access database
		p = original_paf + strlen(original_paf) - 6;
		if (strcmp(p, ".accdb") == 0) {
			rv = true;
		}
	}
	if (op_len >= 7) {
		// .*\.laccdb$ is the Microsoft Access lockfile
		p = original_paf + strlen(original_paf) - 7;
		if (strcmp(p, ".laccdb") == 0) {
			rv = true;
		}
	}
	if (op_len >= 15) {
		// ^/\.TemporaryItems/.* is from OSX
		p = original_paf + strlen(original_paf) - 15;
		if (strcmp(p, ".TemporaryItems") == 0) {
			rv = true;
		}
	}
	if (op_len >= 17) {
		// ^/\.TemporaryItems/.* is from OSX
		if (strncmp(original_paf, "/.TemporaryItems/", 17) == 0) {
			rv = true;
		}
	}
	if (op_len >= 65) {
		// :<sha256>- I used to mark part files
		DEBUGi('3') debug_msg(DPR_DATA,
				      "%s(): checking if long file is part file '%s'\n",
				      original_paf, __func__);

		p = original_paf;
		if (p[64] == ':') {
			DEBUGi('3') debug_msg(DPR_DATA,
					      "%s(): colon found\n",
					      original_paf, __func__);
			bool found = true;
			int count = 63;
			for (; count >= 0; count--) {
				if (!((p[count] >= '0' && p[count] <= '9') ||
				      (p[count] >= 'a' && p[count] <= 'f'))) {
					DEBUGi('3') debug_msg(DPR_DATA,
							      "%s(): illegal char at pos %d \n",
							      count, __func__);
					found = false;
				}
			}
			if (found == true) {
				rv = true;
				DEBUGi('3') debug_msg(DPR_DATA,
						      "%s(): part file confirmed \n",
						      count, __func__);
			}
		}
	}
#endif				// #if USE_TDRIVE
	return rv;
}

static void establishTempNess(char *original_paf, struct dpr_xlate_data *dxd)
{
	if (establishTempNessCheck(original_paf) == true) {
		misc_debugDxd(dpr_data, '3', dxd,
			      "about to change rootdir ", __func__);
		strcpy(dxd->rootdir, TMP_PATH);
		dxd->is_part_file = true;

	} else {
		strcpy(dxd->rootdir, dpr_data->rootdir);
		dxd->is_part_file = false;
	}
}

static void establishAccdbNess(char *original_paf, struct dpr_xlate_data *dxd)
{
	char *p;
	unsigned int op_len;

	op_len = strlen(original_paf);
	dxd->is_accdb = false;
	if (op_len >= 6) {
		// .*\.accdb$ is Microsoft Access database
		p = original_paf + strlen(original_paf) - 6;
		if (strcmp(p, ".accdb") == 0) {
			dxd->is_accdb = true;
		}
	}
	return;
}

/*
 * AM: Given a paf from the user (in_gpath), calculate the paf (rpath)
 * needed to implement DPRFS on top of a regular filesystem.
 *
 * dpr_xlateWholePath() recursively processes a path, filling in the
 * supplied dxd structure until such time as
 * a: no more path is available for processing
 * b: too many recursions have been made, possibly indicating some error
 *
 * The dxd structure:
 * .timestamp is already filled in right after creation
 * First recursion:
 * .rootdir is filled in with the gdrive path by default, but may change
 *          to the temp folder if the filename ends ".part"
 * .is_part_file follows .rootdir, being false by default, true if
 *               ending with ".part"
 * .finalpath is filled in with everything to the right of the final
 *           path separator "/", everything if there's no separator.
 *           It could be a filename or directoryname, we can't tell
 * All recursions:
 * .relpath is built up from the path, ex the filename, supplied by
 *          in_gpath. At each recursion we separate out the
 *          directoryinview and remainder at the first path separator.
 *          We append that directoryinview to .relpath and look in the
 *          actual filesystem for any :Dmetadata file at that location:
 *          if one is found, it's examined for directives; if none is
 *          found we recurse with the remainder of the path.
 *          Directives that can impact are:
 *           deleted = true|false
 *           not-via = <path>
 *           points-to = <path>
 *           beyond-use = CCYYMMYYDDHHMMSSUUUUUU
 * deleted: metadata directive indicating that the user issued a delete
 *          against the immediate linkedlist or directory. This would
 *          normally cause the function to return an empty dxd ("file or
 *          directory not found")
 * not-via: metadata directive requiring the software, if attempting to
 *          process the matching path, to return an empty dxd. This is
 *          used in a rename of directory or linkedlist /A to /B: with
 *          points-to above both /A and /B will work - but with /A's
 *          metadata saying "not-via = /A", only /B will work, attempts
 *          to use /A will result in the empty dxd
 * points-to: metadata directive acting much like a symlink. When
 *            processing directory or linkedlist /A, on finding
 *            points-to = /B, the function is to restart processing
 *            against /B. This has a cost of +1 depth, such that loops
 *            (/A points to /B and /B points to /A) and excessive
 *            chains will cause processing to abandon after too many.
 *        [ ] This implicity limits the number of renames to AWP_DEPTH_MAX
 *            although redoing renaming so all points-to references go
 *            to the original rather than previous would remove this limit
 * beyond-use: If either :Dmetadata or :Fmetadata being reviewed contains
 *             this directive, and the current timestamp (of form
 *             CCYYMMYYDDHHMMSSUUUUUU) is greater than or equal to the
 *             beyond-use value, then dprfs will report "access denied" by
 *             means of mandatory redirection to a directory or file (as
 *             appropriate) with chmod 0.
 *             This feature provides a mechanism for compliance with Data
 *             Protection legislation that allows data to be kept, but put
 *             beyond the ordinary use of the data processor as in the
 *             United Kingdom.
 *             Note 1: "Access denied" is the next least worst alternative
 *             after treating the file or directory as deleted: the
 *             highest-level file or directory remains visible so the
 *             office manager can tell the data "is there", but by doing so
 *             use-agents may be led into asking for upgraded credentials
 *             when no credentials could obtain access. Once data is
 *             "beyond use", it takes offline action to obtain access.
 *             Note 2: There is no mechanism as yet by which a user can
 *             make a file or directory "beyond use"
 */
static void
dpr_xlateWholePath(struct dpr_xlate_data *dxd, struct dpr_state *dpr_data,
		   const char *in_gpath, bool ignoreState, int depth,
		   char *original_paf, int whatDoWithOriginalDir)
{
	const char *cleaned;
	/* If present, remove /path/to/rdrive */
	cleaned = in_gpath;
	if (strncmp(cleaned, dpr_data->rootdir, dpr_data->rootdir_len) == 0)
		cleaned = cleaned + dpr_data->rootdir_len;
	dpr_cleanedXlateWholePath(dxd, dpr_data, cleaned, ignoreState,
				  depth, original_paf, whatDoWithOriginalDir);
	return;
}

static void
dpr_cleanedXlateWholePath(struct dpr_xlate_data *dxd,
			  struct dpr_state *dpr_data, const char *in_gpath,
			  bool ignoreState, int depth, char *original_paf,
			  int whatDoWithOriginalDir)
{
	struct metadata_array md_arr_d = MD_ARR_INIT;
	struct metadata_array md_arr_f = MD_ARR_INIT;
	char *buffer;
	intmax_t buffer_sz;
	FILE *fp;

	DEBUGi('3') debug_msg(dpr_data,
			      "%s(): New recursion into \"%s\", depth \"%d\"\n",
			      __func__, in_gpath, depth);

	if (depth-- == 0) {
		// quick and dirty check that recursion doesn't result in
		// an infinite loop. Dirty, because it limits number of
		// Subdirectories.
		// Improve by testing for repeated visits to the same
		// value of in_gpath, suggestive of a circular loop
		goto reset_free_and_return;	//ut_009
	}

	if (in_gpath[0] == '\0') {
		// no more lhss to look at; ascend back with dxd as-is
		misc_debugDxd(dpr_data, '3', dxd,
			      "1: final recursion sees", __func__);
		goto free_and_return;	//ut_001
	}
	// Every recursion sees a splitting of the given path into an
	// lhs (everything upto and inc the first "/", or everything
	// if not present) and rhs (everything else)
	char lhs[PATH_MAX] = "";
	char rhs[PATH_MAX] = "";
	char *stroke_p = strchr(in_gpath, '/');
	if (stroke_p != NULL) {
		if (stroke_p != in_gpath) {
			strncpy(lhs, in_gpath, stroke_p - in_gpath + 1);
			lhs[stroke_p - in_gpath + 1] = '\0';

		} else {
			strcpy(lhs, "/");
		}
		strcpy(rhs, stroke_p + 1);

	} else {
		strcpy(lhs, in_gpath);
		strcpy(rhs, "");
	}
	DEBUGi('3') debug_msg(dpr_data, "%s(): lhs=\"%s\"\n", __func__, lhs);
	DEBUGi('3') debug_msg(dpr_data, "%s(): rhs=\"%s\"\n", __func__, rhs);

	if (strcmp(lhs, "/") == 0) {
		// /path/to/linkedlist
		// lhs = /
		// rhs = path/to/linkedlist
		DEBUGi('3') debug_msg(dpr_data, "%s(): handling root dir\n",
				      __func__);
		resetDxd(dxd);

		if (original_paf == NULL) {
			// store the path the user requested for not-via use
			char internal_original_paf[PATH_MAX] = "";
			original_paf = internal_original_paf;
			strcpy(original_paf, in_gpath);

		}
		// set up rootdir
		strcpy(dxd->relpath, "/");
		strcpy(dxd->relpath_sha256, "");
		catSha256ToStr(dxd->relpath_sha256, dpr_data, dxd->relpath);
		establishTempNess(original_paf, dxd);
		establishAccdbNess(original_paf, dxd);

		misc_debugDxd(dpr_data, '3', dxd, "3a: ", __func__);
		dpr_xlateWholePath(dxd, dpr_data, rhs, ignoreState, depth,
				   original_paf, whatDoWithOriginalDir);
		goto free_and_return;	//ut_002
	}

	int lhsType = dpr_xlateWholePath_whatIsThis(dpr_data, dxd, lhs, rhs,
						    ignoreState);
	if (lhsType == -1)
		goto free_and_return;

	if (lhsType == DPRFS_FILETYPE_DS) {
		dxd->dprfs_filetype = DPRFS_FILETYPE_DS;
		DEBUGi('3') debug_msg(dpr_data, "%s(): Datastore handler\n",
				      __func__);
		misc_debugDxd(dpr_data, '3', dxd,
			      "1.continuing datastore processing. Return ",
			      __func__);
		strcpy(dxd->finalpath, lhs);
		goto free_and_return;
	}

	if (lhsType == DPRFS_FILETYPE_LL) {
		dxd->dprfs_filetype = DPRFS_FILETYPE_LL;
		DEBUGi('3') debug_msg(dpr_data, "%s(): Linkedlist handler\n",
				      __func__);
		misc_debugDxd(dpr_data, '3', dxd,
			      "1.continuing linkedlist processing. Return ",
			      __func__);
		// :Latest present: This is a linkedlist
		//
		// Check if there's :Fmetadata to look at
		char fmetadata_paf[PATH_MAX] = "";
		char latestLink[PATH_MAX] = "";
		char linkTarget[PATH_MAX] = "";
		strcpy(fmetadata_paf, dxd->rootdir);
		if (dxd->is_part_file == true) {
			strcpy(dxd->relpath_sha256, "");
			catSha256ToStr(dxd->relpath_sha256, dpr_data,
				       dxd->relpath);
			strcat(fmetadata_paf, "/");
			strcat(fmetadata_paf, dxd->relpath_sha256);
			strcat(fmetadata_paf, ":");

		} else {
			strcat(fmetadata_paf, dxd->relpath);
		}
		strcat(fmetadata_paf, lhs);
		if (dxd->is_osx_bodge)
			addOsxBodgeExtIfNecc(fmetadata_paf);
		strcat(fmetadata_paf, "/:latest");
		strcpy(latestLink, fmetadata_paf);
		strcat(fmetadata_paf, "/");
		strcat(fmetadata_paf, FMETADATA_FILENAME);

		DEBUGi('3') debug_msg(dpr_data, "  latestLink %s\n",
				      latestLink);
		if (getLinkTarget(linkTarget, dpr_data, latestLink) == -1)
			strcpy(dxd->revision, REVISION_NAME_MINIMUM);
		else
			incAndAssignRevision(dxd->revision, linkTarget);

		DEBUGi('3') debug_msg(dpr_data, "  next revision %s\n",
				      dxd->revision);

		DEBUGi('3') debug_msg(dpr_data, "  fmetadatapaf %s\n",
				      fmetadata_paf);

		strcpy(dxd->finalpath, lhs);

		misc_debugDxd(dpr_data, '3', dxd,
			      "2.continuing linkedlist processing. Return ",
			      __func__);
		buffer = md_malloc(&buffer_sz, fmetadata_paf);
		fp = md_load(buffer, buffer_sz, fmetadata_paf);
		if (fp == NULL) {
			DEBUGi('3') debug_msg(dpr_data,
					      "  Unable to open \"%s\"\n",
					      fmetadata_paf);
			DEBUGi('3') debug_msg(dpr_data, "    ERROR %s\n",
					      strerror(errno));
			goto reset_free_and_return;
		}
		// there IS :Fmetadata - observe what it says
		md_getIntoStructure(&md_arr_f, dpr_data, buffer);
		md_unload(buffer);

		if (md_checkBeyondUse(dpr_data, &md_arr_f.beyond_use_on) ==
		    true) {
			// :Fmetadata excludes user from visiting by a given path: test
			// and respond if that's happening
			DEBUGi('3') debug_msg
			    (dpr_data,
			     "[WARNING] attempt to access beyond-use file at \"%s\"\n",
			     original_paf);
			accessDeniedDxdFile(dpr_data, dxd);
			goto free_and_return;	//ut_015
		}

		DEBUGi('3') debug_msg
		    (dpr_data,
		     "%s(): md_arr.deleted.value[0]=\"%s\"\n",
		     __func__, md_arr_f.deleted.value);
		if (ignoreState != true && md_arr_f.deleted.value[0] == 't')
			// As deleted, return null string: file or directory not found and head back
			goto delete_free_and_return;	//ut_017

		if (ignoreState != true &&
		    md_isPathInChain(dpr_data, &md_arr_f.not_via, original_paf)
		    == true) {
			// ignoreState needed here as otherwise could not rename back
			// to a previous name
			DEBUGi('3') debug_msg
			    (dpr_data,
			     "  :Fmetadata: access to not-via linkedlist declined \"%s\".\n",
			     original_paf);
			goto reset_free_and_return;	//ut_019
		}

		if (ignoreState != true && md_arr_f.renamed_to.value[0] != '\0') {
			// renamed-to being used is conclusive evidence that this link
			// is not the last in the list, even though it's possibly at
			// the head of a the list in view
			DEBUGi('3') debug_msg
			    (dpr_data,
			     "  :Fmetadata: renamed-to non-null - ignore this link\n");
			goto reset_free_and_return;
		}

		if (md_arr_f.payload_loc.value[0] != '\0') {
			// The payload-loc directive is a bit different in that it
			// doesn't point at the linkedlist that owns the payload,
			// rather it points directly at the file constituting
			// the payload, even if that file is not in the ll's head
			strcpy(dxd->payload, md_arr_f.payload_loc.value);

			// As the payload may exist at a different rootdir than
			// the dxd being used to access it, then at runtime we
			// reform the payload location as an absolute path and file
			// instead of the relative location provided in :Fmetadata
			char payload_paf[PATH_MAX] = "";
			char *p;
			strcpy(payload_paf, dxd->payload);
			p = strchr(payload_paf + 1, '/');
			*p = '\0';
			DEBUGi('3') debug_msg
			    (dpr_data,
			     "  :Fmetadata: assessing payload temp ness of %s\n",
			     payload_paf + 1);
			if (establishTempNessCheck(payload_paf) == true) {
				strcpy(dxd->payload_root, TMP_PATH);
			} else {
				strcpy(dxd->payload_root, dpr_data->rootdir);
			}

			goto free_and_return;
		}

		misc_debugDxd(dpr_data, '3', dxd,
			      "Finished linkedlist processing. Return ",
			      __func__);
		goto free_and_return;
	}

	if (lhsType == DPRFS_FILETYPE_DIR) {
		dxd->dprfs_filetype = DPRFS_FILETYPE_DIR;
		DEBUGi('3') debug_msg(dpr_data,
				      " Ordinary directory handler\n");
		// :Latest not present: this is an ordinary directory
		//
		// check if there's :Dmetadata to look at
		char dmetadata_paf[PATH_MAX] = "";
		strcpy(dmetadata_paf, dxd->rootdir);
		strcat(dmetadata_paf, dxd->relpath);
		strcat(dmetadata_paf, lhs);
		if (*rhs == '\0')
			strcat(dmetadata_paf, "/");
		strcat(dmetadata_paf, DMETADATA_FILENAME);

		buffer = md_malloc(&buffer_sz, dmetadata_paf);
		fp = md_load(buffer, buffer_sz, dmetadata_paf);
		if (fp == NULL) {
			DEBUGi('3') debug_msg(dpr_data,
					      " No :Dmetadata at %s\n",
					      dmetadata_paf);
			// no :Dmetadata so accept as is
			if (*lhs != '\0') {
				strcat(dxd->relpath, lhs);
				strcpy(dxd->relpath_sha256, "");
				catSha256ToStr(dxd->relpath_sha256, dpr_data,
					       dxd->relpath);
				dxd->finalpath[0] = '\0';
				misc_debugDxd(dpr_data, '3', dxd,
					      " process further ", __func__);
				dpr_xlateWholePath(dxd, dpr_data, rhs,
						   ignoreState, depth,
						   original_paf,
						   whatDoWithOriginalDir);
				goto free_and_return;	//ut_005 on the way back
			}

		} else {
			// there IS :Dmetadata - observe what it says
			md_getIntoStructure(&md_arr_d, dpr_data, buffer);
			md_unload(buffer);

			if (md_checkBeyondUse(dpr_data, &md_arr_d.beyond_use_on)
			    == true) {
				// :Dmetadata excludes user from visiting by a given path: test
				// and respond if that's happening
				DEBUGi('3') debug_msg
				    (dpr_data,
				     "[WARNING] attempt to access beyond-use file at \"%s\"\n",
				     original_paf);
				accessDeniedDxdDir(dpr_data, dxd);
				goto free_and_return;	//ut_015
			}

			DEBUGi('3') debug_msg
			    (dpr_data,
			     "%s(): md_arr_d.deleted.value[0]=\"%s\"\n",
			     __func__, md_arr_d.deleted.value);

			if (ignoreState != true
			    && md_arr_d.deleted.value[0] == 't')
				// As deleted, return null string: file or directory not found and head back
				goto delete_free_and_return;	//ut_017

			strcpy(dxd->finalpath, lhs);

			if (ignoreState != true && md_isPathInChain
			    (dpr_data, &md_arr_d.not_via, original_paf) == true)
			{
				// ignoreState needed here as otherwise could not rename back
				// to a previous name
				DEBUGi('3') debug_msg
				    (dpr_data,
				     "  :Dmetadata: access to not-via directory declined \"%s\".\n",
				     original_paf);
				goto reset_free_and_return;	//ut_019
			}

			/* handle original-dir directive if non empty */
			if (whatDoWithOriginalDir == OBSERVE_ORIGINAL_DIR &&
			    md_arr_d.original_dir.value[0] != '\0') {
				/* handle original-dir directive if non empty */
				char new_path[PATH_MAX] = "";
				long idx;
				strcpy(new_path, md_arr_d.original_dir.value);
				if (rhs[0] != '\0')
					strcat(new_path, "/");
				strcat(new_path, rhs);
				idx = strlen(new_path) - strlen(in_gpath);

				/* if new path same not same as old path which does allow
				   for a > b > c > a loops, admittedly */
				if (idx >= 0 &&
				    strcmp(new_path + idx, in_gpath) == 0) {
					// Only look at lastmost directory
					DEBUGi('3') debug_msg
					    (dpr_data,
					     " Skipping original-dir that points to self\n");

				} else {
					/* if there'a an original-dir, restart processing completely */
					struct dpr_xlate_data dxd2 = DXD_INIT;
					dpr_xlateWholePath(&dxd2, dpr_data,
							   new_path,
							   ignoreState, depth,
							   original_paf,
							   whatDoWithOriginalDir);

					dxd->deleted = dxd2.deleted;
					dxd->dprfs_filetype =
					    dxd2.dprfs_filetype;
					strcpy(dxd->finalpath, dxd2.finalpath);
					dxd->is_accdb = dxd2.is_accdb;
					dxd->is_osx_bodge = dxd2.is_osx_bodge;
					dxd->is_part_file = dxd2.is_part_file;
					strcpy(dxd->payload, dxd2.payload);
					strcpy(dxd->payload_root,
					       dxd2.payload_root);
					strcpy(dxd->originaldir,
					       dxd2.originaldir);
					strcpy(dxd->relpath, dxd2.relpath);
					strcpy(dxd->relpath_sha256,
					       dxd2.relpath_sha256);
					strcpy(dxd->revision, dxd2.revision);
					strcpy(dxd->rootdir, dxd2.rootdir);
					strcpy(dxd->timestamp, dxd2.timestamp);
					goto free_and_return;
				}
			}

			if (*rhs != '\0') {
				// still more to look at - recurse down
				strcat(dxd->relpath, dxd->finalpath);
				strcpy(dxd->relpath_sha256, "");
				catSha256ToStr(dxd->relpath_sha256, dpr_data,
					       dxd->relpath);
				*dxd->finalpath = '\0';
				dpr_xlateWholePath(dxd, dpr_data, rhs,
						   ignoreState, depth,
						   original_paf,
						   whatDoWithOriginalDir);
				goto free_and_return;
			}
			goto free_and_return;	//ut_018 on way back
		}
	}

	dxd->dprfs_filetype = DPRFS_FILETYPE_OTHER;
	strcpy(dxd->finalpath, lhs);

	misc_debugDxd(dpr_data, '3', dxd,
		      " confirm as wildcard or filename. Return ", __func__);
	goto free_and_return;	//ut_012

 reset_free_and_return:
	resetDxd(dxd);
	goto free_and_return;

 delete_free_and_return:
	dxd->deleted = true;
	goto free_and_return;

 free_and_return:
	mstrfree(&md_arr_d.others);
	md_free(&md_arr_d.not_via);
	mstrfree(&md_arr_f.others);
	md_free(&md_arr_f.not_via);
	return;
}

///////////////////////////////////////////////////////////
//
// Prototypes for all these functions, and the C-style comments,
// come indirectly from /usr/include/fuse.h
//

/////////////////////////////////////
// files

/*
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 *
 * Introduced in version 2.5
 *
 * AM: A created file normally goes where the user says, but with DPRFS
 * we construct a special tree for it to live in first.
 * User sees /path/to/file
 * DPRFS sees /path/to/file/timestamp/file
 */
// gonna add a sha256 hash here, which identifies the file being created
// throughout its life. This provides a possibility for faster locating
// of the current "most up to date" version of a file even with renaming
// etc. For the mo, use the fpath name. Incredibly dangerous as then
// anyone can magic up such a filename as to cause a collision, but for
// now it'll do.
static void
createLLID(struct metadata_single *single, const struct dpr_state *dpr_data,
	   const char *gpath)
{
	single->operation = MD_NOP;
	catSha256ToStr(single->value, dpr_data, gpath);
	single->value[SHA256_DIGEST_LENGTH * 2 + 1] = '\0';
}

/*
 * Create a file: entry point for FUSE. The only thing I'll
 * want to ensure is that such a new file has a new llid
 */
static int
fsus_create(const char *gpath, mode_t mode, struct fuse_file_info *fi)
{
	struct metadata_array md_arr = MD_ARR_INIT;
	bool create_file_itself = true;
	unsigned int payload_loc_src = PAYLOAD_LOC_SRC_NEW;
	int rv;

	DEBUGe('1') debug_msg(DPR_DATA,
			      LOG_DIVIDER "%s() entry: gpath=\"%s\"\n",
			      __func__, gpath);

	createLLID(&md_arr.llid, DPR_DATA, gpath);
	createLLID(&md_arr.sha256, DPR_DATA, gpath);

	rv = fsus_create_core(gpath, mode, fi, create_file_itself, &md_arr,
			      payload_loc_src);

	DEBUGe('1') debug_msg(DPR_DATA, "%s() exit, rv=\"%d\"\n\n", __func__,
			      rv);

	mstrfree(&md_arr.others);
	md_free(&md_arr.not_via);
	return rv;
}

/*
 * Create a new file: dprfs entry point. As such, the llid and
 * others ought already to have been handled. We won't want the
 * creation sequence to actually create the file - only the
 * :Xmetadata - as this code only used during a rename, and we
 * save disk by using "points-to" instead of repeating the
 * payload
 */
static int
fsus_create_with_metadata(const char *gpath, mode_t mode,
			  struct fuse_file_info *fi,
			  struct metadata_array *md_arr,
			  unsigned int payload_loc_src)
{
	bool create_file_itself = false;

	return fsus_create_core(gpath, mode, fi, create_file_itself, md_arr,
				payload_loc_src);
}

/*
 * create a new linkedlist set of directories, with metadata
 * inclueded. Don't create the payload unless asked
 */
static int
fsus_create_core(const char *gpath, mode_t mode, struct fuse_file_info *fi,
		 bool create_file_itself, struct metadata_array *md_arr,
		 unsigned int payload_loc_src)
{
	struct dpr_xlate_data dxd = DXD_INIT;
	int rv;

	DEBUGe('2') debug_msg(DPR_DATA, LOG_DIVIDER
			      "%s(gpath=\"%s\", mode=0%03o)\n",
			      __func__, gpath, mode);

	ea_flarrs_addElement(DPR_DATA, gpath);
	forensicLogChangesComing(DPR_DATA, CREAT_KEY, gpath);

	dpr_xlateWholePath(&dxd, DPR_DATA, gpath, true, XWP_DEPTH_MAX, NULL,
			   OBSERVE_ORIGINAL_DIR);
	if (dxd.dprfs_filetype == DPRFS_FILETYPE_LL
	    || dxd.dprfs_filetype == DPRFS_FILETYPE_NA
	    || dxd.dprfs_filetype == DPRFS_FILETYPE_DIR) {
		/* a paf with no fs object sees DIR reported as */
		/* thats the last object that is observed while */
		/* looking for it. The LL gets reported when we */
		/* it exists, perhaps as a deleted object */
		makeAndPopulateNewRevisionTSDir(DPR_DATA, gpath, &dxd, md_arr,
						LINKEDLIST_CREATE,
						payload_loc_src);
		rv = 0;
		if (create_file_itself)
			rv = fsus_create_core_ll(&dxd, mode, fi);

	} else if (dxd.dprfs_filetype == DPRFS_FILETYPE_DS) {
		rv = 0;
		if (create_file_itself)
			rv = fsus_create_core_ds(&dxd, mode, fi);

	} else {
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s unexpected dxd.dprfs_filetype=\"%d\"\n",
				      __func__, dxd.dprfs_filetype);
		rv = -1;
	}
	DEBUGe('2') debug_msg(DPR_DATA,
			      "%d %s completes, rv=\"%d\"\n\n", getpid(),
			      __func__, rv);

	return rv;
}

/* Investigation shows that MSOffice on OSX cannot handle creat() but */
/* instead wants O_LARGEFILE | O_EXCL | O_CREAT | O_RDWR, of which */
/* only the last three have meaning to stock linux. Use of creat() */
/* would see MSO+OSX delete documents shortly after saving */
static int fsus_create_core_ds(struct dpr_xlate_data *dxd, mode_t mode,
			       struct fuse_file_info *fi)
{
	char accdb[PATH_MAX] = "";
	char paf[PATH_MAX] = "";
	int fp;
	int rv;

	getPafForOrdinaryFile(paf, *dxd);
	DEBUGe('2') debug_msg(DPR_DATA, LOG_DIVIDER "%s() paf=\"%s\"\n",
			      __func__, paf);

	if (!dxd->is_accdb) {
		fp = open(paf, fi->flags, mode);

	} else {
		strcpy(accdb, DATASTORE_PATH);
		strcat(accdb, "/");
		getPafForFinalPathWithOsxBodge(accdb, *dxd);
		if (symlink(accdb, paf) == -1) {
			DEBUGe('2') debug_msg
			    (DPR_DATA,
			     "%s() unable to symlink target=\"%s\" to paf=\"%s\"\n",
			     __func__, accdb, paf);
			rv = -1;
			goto error;
		}

		DEBUGe('3') debug_msg
		    (DPR_DATA, "%s() accdb target \"%s\"\n", __func__, accdb);
		DEBUGe('3') debug_msg
		    (DPR_DATA, "%s() accdb softlink \"%s\"\n", __func__, paf);

		int flags = O_EXCL;
		flags = ~flags;
		flags &= fi->flags;
		flags &= ~O_NOFOLLOW;
		fp = open(paf, flags, mode);
	}

	DEBUGe('2') debug_msg
	    (DPR_DATA, "%s() receives fd=\"%d\"\n", __func__, fp);

	if (fp == -1) {
		rv = -1;
		dpr_error("fsus_create_core_ds creat");
		DEBUGe('2') debug_msg
		    (DPR_DATA,
		     "%s() unable to creat() file \"%s\"\n", __func__, paf);

	} else {
		rv = 0;
		fi->fh = fp;
		ea_filetype_addElement(DPR_DATA, fi->fh, dxd->dprfs_filetype);
		DEBUGe('2') debug_msg
		    (DPR_DATA,
		     " \"%s\" gets file descriptor \"%" PRIu64 "\"\n",
		     paf, ea_shadowFile_getValueOrKey(DPR_DATA, fi));
	}
 error:
	return rv;
}

static int fsus_create_core_ll(struct dpr_xlate_data *dxd, mode_t mode,
			       struct fuse_file_info *fi)
{
	char paf[PATH_MAX] = "";
	int fp;
	int rv;

	getLinkedlistLatestLinkedlistFile(paf, *dxd);
	DEBUGe('2') debug_msg(DPR_DATA, LOG_DIVIDER "%s() paf=\"%s\"\n",
			      __func__, paf);

	fp = open(paf, fi->flags | O_SYNC, mode);
	DEBUGe('2') debug_msg
	    (DPR_DATA, "%d %s() receives fd=\"%d\"\n", getpid(), __func__, fp);

	if (fp == -1) {
		rv = -1;
		dpr_error("fsus_create_core_ll creat");
		DEBUGe('2') debug_msg
		    (DPR_DATA,
		     "%s() unable to creat() file \"%s\"\n", __func__, paf);

	} else {
		rv = 0;
		fi->fh = fp;
		ea_filetype_addElement(DPR_DATA, fi->fh, DPRFS_FILETYPE_LL);
		DEBUGe('2') debug_msg
		    (DPR_DATA,
		     " \"%s\" gets file descriptor \"%" PRIu64 "\"\n",
		     paf, ea_shadowFile_getValueOrKey(DPR_DATA, fi));
	}
	return rv;
}

/*
 * Create a linkedlist structure on the rdrive, or extend it
 * as specified
 */
static int
makeAndPopulateNewRevisionTSDir(struct dpr_state *dpr_data,
				const char *gpath,
				struct dpr_xlate_data *dxd,
				struct metadata_array *md_arr, int watdo,
				unsigned int payload_loc_src)
{
	char ll_l_fm_lnk[PATH_MAX] = "";
	char ll_l_fm_ts_file[PATH_MAX] = "";
	char ll_l_lnk[PATH_MAX] = "";
	char ll_name[PATH_MAX] = "";
	char rts_dir[PATH_MAX] = "";
	char ll_rts_dir[PATH_MAX] = "";
	char ll_l_lnk_target[PATH_MAX] = "";
	char fm_ts_file[PATH_MAX] = "";
	char rel_ll_rts_file[PATH_MAX] = "";
	int rv;
	bool existingLinkPresent = false;

	DEBUGe('2') debug_msg(DPR_DATA, "%s(): %s\n", __func__, gpath);

	int len = strlen(dxd->finalpath);
	if (len >= 8 && strcmp(dxd->finalpath + len - 8, ".numbers") == 0)
		goto enbodge_for_osx;
	if (len >= 6 && strcmp(dxd->finalpath + len - 6, ".pages") == 0)
		goto enbodge_for_osx;
	if (len >= 4 && strcmp(dxd->finalpath + len - 4, ".key") == 0)
		goto enbodge_for_osx;
	goto nobodge_for_osx;
 enbodge_for_osx:
	dxd->is_osx_bodge = true;
	DEBUGe('2') debug_msg(DPR_DATA,
			      "%s(): \"%s\" is osx bodged\n",
			      __func__, dxd->finalpath);
 nobodge_for_osx:

	getLinkedlistLatestFMetadataLnk(ll_l_fm_lnk, *dxd);
	getLinkedlistLatestFMetadataLnkTS(ll_l_fm_ts_file, *dxd);
	getLinkedlistLatestLnk(ll_l_lnk, *dxd);
	getLinkedlistName(ll_name, *dxd);
	getRevisionTSDir(rts_dir, *dxd);
	getLinkedlistRevisionTSDir(ll_rts_dir, *dxd);
	getFMetadataTSFile(fm_ts_file, *dxd);
	rv = getLinkTarget(ll_l_lnk_target, dpr_data, ll_l_lnk);
	if (watdo == LINKEDLIST_CREATE && rv == 0) {
		existingLinkPresent = true;
		incAndAssignRevision(dxd->revision, ll_l_lnk_target);
		*dxd->payload = '\0';
		*dxd->payload_root = '\0';
	}
	DEBUGe('2') debug_msg(DPR_DATA,
			      "%s(): ll_l_fm_lnk=\"%s\"\n",
			      __func__, ll_l_fm_lnk);
	DEBUGe('2') debug_msg(DPR_DATA,
			      "%s(): ll_l_fm_ts_file=\"%s\"\n",
			      __func__, ll_l_fm_ts_file);
	DEBUGe('2') debug_msg(DPR_DATA,
			      "%s(): ll_l_lnk=\"%s\"\n", __func__, ll_l_lnk);
	DEBUGe('2') debug_msg(DPR_DATA,
			      "%s(): ll_name=\"%s\"\n", __func__, ll_name);
	DEBUGe('2') debug_msg(DPR_DATA,
			      "%s(): rts_dir=\"%s\"\n", __func__, rts_dir);
	DEBUGe('2') debug_msg(DPR_DATA,
			      "%s(): ll_rts_dir=\"%s\"\n",
			      __func__, ll_rts_dir);
	DEBUGe('2') debug_msg(DPR_DATA,
			      "%s(): fm_ts_file=\"%s\"\n",
			      __func__, fm_ts_file);
	DEBUGe('2') debug_msg(DPR_DATA,
			      "%s(): ll_l_lnk_target=\"%s\"\n",
			      __func__, rts_dir);

	if (watdo == LINKEDLIST_EXTEND || watdo == LINKEDLIST_CREATE) {
		if (payload_loc_src == PAYLOAD_LOC_SRC_NEW) {	// new loc pls
			getRelLinkedlistRevisionTSFile(rel_ll_rts_file, *dxd);
			strcpy(md_arr->payload_loc.value, rel_ll_rts_file);
		}		// else PAYLOAD_LOC_SRC_FWD bring last payload-loc forward
	}
	if (watdo == LINKEDLIST_CREATE) {
		// create directory named after the file
		rv = mkdir(ll_name, getDefaultDirMode());
		if (rv == -1) {
			DEBUGe('2') debug_msg
			    (DPR_DATA,
			     "[WARNING] %s()(1) can't mkdir \"%s\" saying \"%s\"\n",
			     __func__, ll_name, strerror(errno));
			dpr_error
			    ("makeAndPopulateNewRevisionTSDir making ll_name");
		}
	}
	// now make a new head
	// create directory named after current timestamp
	rv = mkdir(ll_rts_dir, getDefaultDirMode());
	if (rv == -1) {
		DEBUGe('2') debug_msg
		    (DPR_DATA,
		     "[WARNING] %s()(2) can't mkdir \"%s\" saying \"%s\"(%d) uid:%d, euid:%d\n",
		     __func__, ll_rts_dir, strerror(errno), errno, getuid(),
		     geteuid());

		dpr_error("makeAndPopulateNewRevisionTSDir making rts_dir");
	}
	// softlink to timestamp directory with :latest
	if (watdo == LINKEDLIST_EXTEND
	    || (watdo == LINKEDLIST_CREATE && existingLinkPresent == true))
		rv = unlink(ll_l_lnk);

	rv = symlink(rts_dir, ll_l_lnk);
	if (rv == -1) {
		DEBUGe('2') debug_msg
		    (DPR_DATA,
		     "[WARNING] %s() can't symlink \"%s\" to \"%s\"\n",
		     __func__, rts_dir, ll_l_lnk, strerror(errno));
		dpr_error("makeAndPopulateNewRevisionTSDir doing symlink");
	}

	if (watdo == LINKEDLIST_EXTEND)
		// overwrite any previous
		strcpy(md_arr->supercedes.value, ll_l_lnk_target);

	// create :latest/:Fmetadata-<ts>, fill as required
	if (saveMetadataToFile(ll_l_fm_ts_file, md_arr) != 0)
		return -1;

	// softlink to timestamp directory with :latest
	rv = unlink(ll_l_fm_lnk);
	rv = symlink(fm_ts_file, ll_l_fm_lnk);
	return rv;
}

/* Remove a file */
/*
 * AM: called when user deletes a file. Cascading responsibilities
 * in DPRFS, as we hide with metadata, not unlinking
 */
static int fsus_unlink(const char *gpath)
{
	struct dpr_xlate_data dxd = DXD_INIT;
	int rv;

	DEBUGe('1') debug_msg(DPR_DATA,
			      LOG_DIVIDER "%s(gpath=\"%s\"\n", __func__, gpath);
	forensicLogChangesComing(DPR_DATA, UNLINK_KEY, gpath);

	// log and obtain the directory to remove
	dpr_xlateWholePath(&dxd, DPR_DATA, gpath, true, XWP_DEPTH_MAX, NULL,
			   OBSERVE_ORIGINAL_DIR);

	if (dxd.dprfs_filetype == DPRFS_FILETYPE_LL) {
		rv = fsus_unlink_ll(gpath, &dxd);

	} else if (dxd.dprfs_filetype == DPRFS_FILETYPE_DS) {
		rv = fsus_unlink_ds(&dxd);

	} else {
		rv = -1;
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s()unexpected dxd.dprfs_filetype=\"%d\"\n",
				      __func__, dxd.dprfs_filetype);
	}
	rs_inc(DPR_DATA, &DPR_DATA->delstats_p);

	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s completes, rv=\"%d\"\n\n", __func__, rv);
	return rv;
}

static int fsus_unlink_ds(struct dpr_xlate_data *dxd)
{
	char paf[PATH_MAX] = "";
	int rv;

	getPafForOrdinaryFile(paf, *dxd);
	rv = unlink(paf);
	if (rv == -1)
		dpr_error("fsus_unlink_ds unlink");
	return rv;
}

static int fsus_unlink_ll(const char *gpath, struct dpr_xlate_data *dxd)
{
	struct metadata_array md_arr = MD_ARR_INIT;
	char from_ll_name[PATH_MAX] = "";
	char to_ll_l_lnk[PATH_MAX] = "";
	char linkTarget[PATH_MAX] = "";
	int rv;

	if (dxd->is_part_file == true) {
		getLinkedlistName(from_ll_name, *dxd);
		if (strncmp(from_ll_name, TMP_PATH, strlen(TMP_PATH)) != 0)
			return -1;
		rv = rmrf(from_ll_name);

	} else {
		strcpy(md_arr.deleted.value, "true");

		getLinkedlistLatestLnk(to_ll_l_lnk, *dxd);
		getLinkTarget(linkTarget, DPR_DATA, to_ll_l_lnk);
		incAndAssignRevision(dxd->revision, linkTarget);
		*dxd->payload = '\0';
		*dxd->payload_root = '\0';

		rv = makeAndPopulateNewRevisionTSDir(DPR_DATA, gpath, dxd,
						     &md_arr, LINKEDLIST_EXTEND,
						     0);
	}

	return rv;
}

/*
 * Add a later :Dmetadata containing specified string
 */
static int
saveDMetadataToFile(struct dpr_state *dpr_data, struct dpr_xlate_data dxd,
		    struct metadata_array *md_arr)
{
	char prv_ll_dm_lnk[PATH_MAX] = "";
	char ll_dm_lnk[PATH_MAX] = "";
	char ll_dm_lnk_target[PATH_MAX] = "";
	char ll_dm_ts_file[PATH_MAX] = "";
	char dm_ts_file[PATH_MAX] = "";
	char original_dir[PATH_MAX] = "";
	int rv;

	DEBUGe('2') debug_msg(DPR_DATA, "%s()\n", __func__);

	if (md_arr->original_dir.value[0] == '\0') {
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s(): new original-dir added\n",
				      __func__);
		getRelLinkedlistName(original_dir, dxd);
		strcpy(md_arr->original_dir.value, original_dir);
	}

	getLinkedlistDMetadataLnk(prv_ll_dm_lnk, dxd);
	DEBUGe('2') debug_msg(DPR_DATA, "%s(): %s\n", __func__, prv_ll_dm_lnk);
	getLinkTarget(ll_dm_lnk_target, dpr_data, prv_ll_dm_lnk);

	getLinkedlistDMetadataTSFile(ll_dm_ts_file, dxd);
	getDMetadataTSFile(dm_ts_file, dxd);

	if (*ll_dm_lnk_target != '\0')
		strcat(md_arr->supercedes.value, ll_dm_lnk_target);

	if (saveMetadataToFile(ll_dm_ts_file, md_arr) != 0)
		return -1;

	getLinkedlistDMetadataLnk(ll_dm_lnk, dxd);

	unlink(ll_dm_lnk);
	rv = symlink(dm_ts_file, ll_dm_lnk);

	return rv;
}

/*
 * Accept number of files/directories changed and update the
 * counters file to match.
 * I seem to remember a linux kernel feature that would keep
 * count of things, but can't find it. Could have been a
 * simple hello world driver I suppose
 */
static void util_beyonduseUpdateCounters(const int files, const int dirs)
{
	// read in what the statistics file currently says
	const char *errstr;
	FILE *fp;
	char paf[PATH_MAX] = "";
	char *buffer_in;
	intmax_t buffer_in_sz;
	char stats[sizeof(__LONG_LONG_MAX__) * 2 + 3] = "";
	char *eoblurb;
	char *eofiles;
	char *eodirs;
	unsigned long long num_files;
	unsigned long long num_dirs;
	int rv;

	strcpy(paf, DPR_DATA->rootdir);
	strcat(paf, BEYOND_USE_COUNTERS_RELPAF);

	buffer_in = md_malloc(&buffer_in_sz, paf);
	fp = md_load(buffer_in, buffer_in_sz, paf);

	// find our way to the start of the line following "--\n"
	eoblurb = strchr(buffer_in, '-');
	eoblurb = strchr(eoblurb, '\n');
	eoblurb++;

	// make two pointers: one looks at the old number of files,
	// the second at the old number of directories. Add \0
	// following both so strtonum can use them
	eofiles = strchr(eoblurb, '\n');
	*eofiles = '\0';
	eofiles++;
	eodirs = strchr(eofiles, '\n');
	if (eodirs != NULL)
		*eodirs = '\0';

	// make long longs of both numers
	num_files = strtonum(eoblurb, 0ULL, __LONG_LONG_MAX__, &errstr);
	if (errstr != NULL)
		num_files = -1;
	num_dirs = strtonum(eofiles, 0ULL, __LONG_LONG_MAX__, &errstr);
	if (errstr != NULL) {
		num_dirs = -1;
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s(): buffer_in %s\n%s\n",
				      __func__, errstr, eofiles);
	}
	// handle if there seem to be no numbers
	if (num_files == -1ULL || num_dirs == -1ULL) {
		DEBUGe('2') debug_msg
		    (DPR_DATA,
		     "[WARNING]  %s(): %s seems to be corrupted\n",
		     __func__, paf);

	} else {
		// Rebuild the statistics file and write it back out: briefly
		// set the mode rw-r----- for the purpose as user shouldn't
		// really have write access
		num_files += (long long)files;
		num_dirs += (long long)dirs;
		sprintf(stats, "%llu\n%llu\n", num_files, num_dirs);

		rv = chmod(paf, 0640);
		if (rv == -1)
			fprintf
			    (stderr,
			     "[WARNING] makeBeyondUseFiles() can't chmod \"%s\" saying \"%s\"\n",
			     paf, strerror(errno));
		fp = fopen(paf, "w");
		fwrite(buffer_in, eoblurb - buffer_in, 1, fp);
		fwrite(stats, strlen(stats), 1, fp);
		fclose(fp);
		rv = chmod(paf, 0440);
		if (rv == -1)
			fprintf
			    (stderr,
			     "[WARNING] makeBeyondUseFiles() can't chmod \"%s\" saying \"%s\"\n",
			     paf, strerror(errno));
	}
	md_unload(buffer_in);
	return;
}

/*
 * Put a linkedlist beyond use
 */
static int
dprfs_beyonduse_ll(struct dpr_state *dpr_data, const char *gpath,
		   struct dpr_xlate_data dxd)
{
	struct metadata_array md_arr = MD_ARR_INIT;
	char timestamp[TIMESTAMP_SIZE] = "";
	int rv;

	DEBUGe('2') debug_msg(DPR_DATA, "%s(): %s\n", __func__, gpath);

	// create :latest/:Fmetadata with supercedes line
	getCondensedSystemUTime(timestamp);
	strcat(md_arr.beyond_use_on.value, timestamp);
	rv = makeAndPopulateNewRevisionTSDir(dpr_data, gpath, &dxd, &md_arr,
					     LINKEDLIST_EXTEND, 0);
	rs_inc(DPR_DATA, &DPR_DATA->renstats_p);
	util_beyonduseUpdateCounters(1, 0);

	return rv;
}

/*
 * Put a directory beyond use
 */
static int
dprfs_beyonduse_dir(struct dpr_state *dpr_data, struct dpr_xlate_data dxd)
{
	struct metadata_array md_arr = MD_ARR_INIT;
	char ll_dm_lnk[PATH_MAX] = "";
	char ll_dm_ts_file[PATH_MAX] = "";
	char timestamp[TIMESTAMP_SIZE] = "";
	int rv;

	DEBUGe('2') debug_msg(DPR_DATA, "%s()\n", __func__);

	getLinkedlistDMetadataLnk(ll_dm_lnk, dxd);
	DEBUGe('2') debug_msg(DPR_DATA,
			      "%s(): ll_dm_lnk=\"%s\"\n", __func__, ll_dm_lnk);

	getDMetadataTSFile(ll_dm_ts_file, dxd);

	// softlink to timestamp directory with :latest
	rv = unlink(ll_dm_lnk);
	rv = symlink(ll_dm_ts_file, ll_dm_lnk);

	// create :latest/:Dmetadata with deleted in place
	getCondensedSystemUTime(timestamp);
	strcat(md_arr.beyond_use_on.value, timestamp);

	rv = saveDMetadataToFile(dpr_data, dxd, &md_arr);

	rs_inc(DPR_DATA, &DPR_DATA->renstats_p);
	util_beyonduseUpdateCounters(0, 1);

	return rv;
}

/*
 * Only linkedlists and directories can be put beyond use: test what
 * we have and pass it out; if we have something else return as if
 * successful
 */
static int dprfs_beyonduse(struct dpr_state *dpr_data, const char *path)
{
	struct dpr_xlate_data dxd = DXD_INIT;

	dpr_xlateWholePath(&dxd, DPR_DATA, path, false, XWP_DEPTH_MAX, NULL,
			   OBSERVE_ORIGINAL_DIR);
	if (dxd.dprfs_filetype == DPRFS_FILETYPE_LL) {
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s() LL (gpath=\"%s\")\n",
				      __func__, path);
		return dprfs_beyonduse_ll(dpr_data, path, dxd);
	}

	if (dxd.dprfs_filetype == DPRFS_FILETYPE_DS) {
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s() DS ignored (gpath=\"%s\")\n",
				      __func__, path);
		return 0;
	}

	if (dxd.dprfs_filetype == DPRFS_FILETYPE_DIR) {
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s() DIR(gpath=\"%s\")\n",
				      __func__, path);
		return dprfs_beyonduse_dir(dpr_data, dxd);
	}
	return 0;
}

/*
 * Rename a directory
 *
 * In a linkedlist system, this means the new :Dmetadata file
 * will reveal the previous, now invisible directory.
 */
static int
fsus_rename_dir(struct dpr_state *dpr_data, struct dpr_xlate_data *dxdto,
		struct dpr_xlate_data *dxdfrom, const char *newpath,
		const char *oldpath, int whatDoWithOriginalDir)
{
	struct metadata_array from_md_arr = MD_ARR_INIT;
	struct metadata_array to_md_arr = MD_ARR_INIT;
	char *buffer;
	intmax_t buffer_sz;
	char dm_ts_file[PATH_MAX] = "";
	char to_rel_ll_name[PATH_MAX] = "";
	char to_ll_dm_lnk[PATH_MAX] = "";
	char from_ll_dm_lnk[PATH_MAX] = "";
	char from_rel_ll_name[PATH_MAX] = "";
	char ll_dm_ts_file[PATH_MAX] = "";
	char original_dir[PATH_MAX] = "";
	int rv;

	/* FROM: read in dmetadata */
	getRelLinkedlistName(to_rel_ll_name, *dxdto);
	getLinkedlistDMetadataLnk(from_ll_dm_lnk, *dxdfrom);
	buffer = md_malloc(&buffer_sz, from_ll_dm_lnk);
	md_load(buffer, buffer_sz, from_ll_dm_lnk);
	md_getIntoStructure(&from_md_arr, dpr_data, buffer);
	md_unload(buffer);

	/* FROM: update metadata */
	strcpy(from_md_arr.renamed_to.value, to_rel_ll_name);
	notviaChainToIncludePaf(&from_md_arr.not_via,
				from_md_arr.original_dir.value);
	notviaChainToIncludePaf(&from_md_arr.not_via, oldpath);

	/* FROM: save out metadata */
	dxd_initialiseTimestamp(dxdfrom);
	getDMetadataTSFile(dm_ts_file, *dxdfrom);
	getLinkedlistDMetadataTSFile(ll_dm_ts_file, *dxdfrom);
	if (saveMetadataToFile(ll_dm_ts_file, &from_md_arr) != 0) {
		rv = -1;

	} else {
		unlink(from_ll_dm_lnk);
		rv = symlink(dm_ts_file, from_ll_dm_lnk);

		/* TO: read in dmetadata */
		getRelLinkedlistName(from_rel_ll_name, *dxdfrom);
		getLinkedlistDMetadataLnk(to_ll_dm_lnk, *dxdto);
		buffer = md_malloc(&buffer_sz, to_ll_dm_lnk);
		md_load(buffer, buffer_sz, to_ll_dm_lnk);
		md_unload(buffer);	// huh?? Not used?

		/* TO: update metadata */
		getPafForFinalPath(original_dir, *dxdfrom);
		strcpy(from_md_arr.renamed_from.value, oldpath);
		to_md_arr.llid.operation = from_md_arr.llid.operation;
		strcpy(to_md_arr.llid.value, from_md_arr.llid.value);

		to_md_arr.original_dir.operation =
		    from_md_arr.original_dir.operation;
		strcpy(to_md_arr.original_dir.value,
		       from_md_arr.original_dir.value);
		notviaChainToExcludePaf(&to_md_arr.not_via, newpath);

		/* TO: save out metadata */
		fsus_mkdir_with_metadata(dpr_data, to_rel_ll_name,
					 getDefaultDirMode(), &to_md_arr,
					 whatDoWithOriginalDir);
		rs_inc(DPR_DATA, &DPR_DATA->renstats_p);
	}

	mstrfree(&to_md_arr.others);
	md_free(&to_md_arr.not_via);
	mstrfree(&from_md_arr.others);
	md_free(&from_md_arr.not_via);
	return rv;
}

/*
 * Rename a linkedlist.
 *
 * In a linkedlist system, this means the new :Fmetadata file
 * will reveal the previous, now invisible linkedlist.
 *
 * In the special case of ".part" files, we're renaming from
 * a linkedlist in the temporary directory: that is to be
 * deleted from there.
 *
 * In both cases, the :Fmetadata links back to the previous
 * linkedlist with the name excluding ".part", if present
 */
static int
fsus_rename_ll(struct dpr_state *dpr_data, struct dpr_xlate_data *dxdto,
	       struct dpr_xlate_data *dxdfrom_prv,
	       struct dpr_xlate_data *dxdfrom, const char *oldpath,
	       const char *newpath, bool isPartFile)
{
	struct metadata_array from_md_arr = MD_ARR_INIT;
	struct metadata_array to_md_arr = MD_ARR_INIT;
	struct dpr_xlate_data dxdtmp = DXD_INIT;
	char *buffer;
	intmax_t buffer_sz;
	char prv_ll_l_fm_lnk[PATH_MAX] = "";
	char prv_ll_l_lnk[PATH_MAX] = "";
	char to_rel_ll_name[PATH_MAX] = "";
	char to_ll_l_ll_file[PATH_MAX] = "";
	char to_ll_l_lnk[PATH_MAX] = "";
	char from_ll_l_fm_ts_1_file[PATH_MAX] = "";
	char from_ll_l_fm_ts_2_file[PATH_MAX] = "";
	char from_ll_l_lnk[PATH_MAX] = "";
	char from_ll_l_lnk_target[PATH_MAX] = "";
	char from_ll_rts_dir[PATH_MAX] = "";
	char from_ll_l_ll_file[PATH_MAX] = "";
	char from_ll_name[PATH_MAX] = "";
	char fm_ts_file[PATH_MAX] = "";
	char revts_dir[PATH_MAX] = "";
	char from_linkTarget[PATH_MAX] = "";
	char to_linkTarget[PATH_MAX] = "";
	char tmp_ll_dir[PATH_MAX] = "";
	int rv;

	/* Final update to FROM ll */
	/* FROM: Read in */
	dxd_copy(&dxdtmp, dxdfrom_prv);
	getRelLinkedlistDir(tmp_ll_dir, dxdtmp);
	getRevisionTSDir(revts_dir, dxdtmp);

	DEBUGe('2') debug_msg(DPR_DATA, "tmp_ll_dir \"%s\"\n", tmp_ll_dir);
	DEBUGe('2') debug_msg(DPR_DATA, "revts_dir \"%s\"\n", revts_dir);

	if (dxdto->is_part_file == false)
		getRelLinkedlistName(to_rel_ll_name, *dxdto);
	else
		getPafForFinalPathWithOsxBodgeIgnoreTemp(to_rel_ll_name,
							 *dxdto);

	getLinkedlistLatestFMetadataLnk(prv_ll_l_fm_lnk, *dxdfrom_prv);
	getLinkedlistLatestLnk(prv_ll_l_lnk, *dxdfrom_prv);
	getLinkedlistLatestLnk(to_ll_l_lnk, *dxdto);

	buffer = md_malloc(&buffer_sz, prv_ll_l_fm_lnk);
	md_load(buffer, buffer_sz, prv_ll_l_fm_lnk);

	getLinkTarget(from_linkTarget, dpr_data, prv_ll_l_lnk);
	getLinkTarget(to_linkTarget, dpr_data, to_ll_l_lnk);

	incAndAssignRevision(dxdfrom->revision, from_linkTarget);
	if (*to_linkTarget == '\0')
		strcpy(to_linkTarget, REVISION_NAME_MINIMUM);
	else
		incAndAssignRevision(dxdto->revision, to_linkTarget);

	md_getIntoStructure(&from_md_arr, dpr_data, buffer);
	md_unload(buffer);

	/* FROM: Update */
	if (isPartFile == false) {
		strcpy(from_md_arr.renamed_to.value, to_rel_ll_name);
		notviaChainToIncludePaf(&from_md_arr.not_via, oldpath);
	}
	/* FROM: Save out */
	dxd_initialiseTimestamp(dxdfrom_prv);
	getFMetadataTSFile(fm_ts_file, *dxdfrom_prv);

	getLinkedlistLatestFMetadataLnkTS(from_ll_l_fm_ts_2_file, *dxdfrom_prv);

	if (saveMetadataToFile(from_ll_l_fm_ts_2_file, &from_md_arr) != 0) {
		rv = -1;

	} else {
		rv = unlink(prv_ll_l_fm_lnk);
		rv = symlink(fm_ts_file, prv_ll_l_fm_lnk);

		/* Final update to TO ll */
		/* TO: Read in */

		/* TO: Update */
		getLinkedlistLatestLnk(from_ll_l_lnk, dxdtmp);
		getLinkTarget(from_ll_l_lnk_target, dpr_data, from_ll_l_lnk);

		getLinkedlistRevisionTSDir(from_ll_rts_dir, dxdtmp);
		getLinkedlistLatestFMetadataLnkTS
		    (from_ll_l_fm_ts_1_file, dxdtmp);

		unsigned int payload_loc_src;
		if (isPartFile == true) {
			/* Again, we renamed-from an rdrive file not */
			/* from the /tmp file we otherwise might */
			payload_loc_src = PAYLOAD_LOC_SRC_NEW;

		} else {
			notviaChainToExcludePaf(&to_md_arr.not_via, newpath);
			strcpy(from_md_arr.renamed_from.value, oldpath);

			strcpy((char *)&to_md_arr.payload_loc.value,
			       (char *)&from_md_arr.payload_loc.value);
			payload_loc_src = PAYLOAD_LOC_SRC_FWD;
		}

		to_md_arr.llid.operation = from_md_arr.llid.operation;
		strcpy(to_md_arr.llid.value, from_md_arr.llid.value);

		DEBUGe('2') debug_msg(DPR_DATA, "tmp_ll_dir \"%s\"\n",
				      tmp_ll_dir);
		DEBUGe('2') debug_msg(DPR_DATA, "revts_dir \"%s\"\n",
				      revts_dir);

		/* TO: Save out */
		fsus_create_with_metadata(to_rel_ll_name,
					  getModeBodge(), NULL,
					  &to_md_arr, payload_loc_src);

		if (isPartFile == true) {
			/* Remove the /tmp structure */
			getLinkedlistLatestLinkedlistFile
			    (from_ll_l_ll_file, *dxdfrom);
			/* getLinkedlistLatestLinkedlistFile(to_ll_l_ll_file, *dxdto); */
			getPafForRootDir(to_ll_l_ll_file, *dxdto);
			strcat(to_ll_l_ll_file, to_md_arr.payload_loc.value);
			getLinkedlistName(from_ll_name, *dxdfrom);

			DEBUGe('2') debug_msg(DPR_DATA,
					      "to_ll_l_ll_file \"%s\"\n",
					      to_ll_l_ll_file);
			DEBUGe('2') debug_msg(DPR_DATA,
					      "from_ll_l_ll_file \"%s\"\n",
					      from_ll_l_ll_file);

			rv = cp(to_ll_l_ll_file, from_ll_l_ll_file);
			DEBUGe('2') debug_msg(dpr_data,
					      "%s(): returns %d\n",
					      __func__, rv);

			rmrf(from_ll_name);
		}
		/* else we won't need to copy payload over */
		rs_inc(DPR_DATA, &DPR_DATA->renstats_p);
	}

	mstrfree(&from_md_arr.others);
	md_free(&from_md_arr.not_via);
	mstrfree(&to_md_arr.others);
	md_free(&to_md_arr.not_via);
	DEBUGe('2') debug_msg(DPR_DATA, "%s() exit\n", __func__);
	return rv;
}

/*
 * Rename entry point from fuse.
 *
 * Work out what filesystem objects are to be renamed
 * and call the helpers fsus_rename_dir(), fsus_rename_ll()
 * if appropriate.
 */
static int fsus_rename(const char *fulloldpath, const char *fullnewpath,
		       unsigned int flags)
{
	int rv;
	DEBUGe('1') debug_msg(DPR_DATA, LOG_DIVIDER "%s entry\n", __func__);
	DEBUGe('2') debug_msg(DPR_DATA, " rename \"%s\" -> \"%s\"\n",
			      fulloldpath, fullnewpath);

	rv = fsus_rename_core(fulloldpath, fullnewpath, flags, USERSIDE);

	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s completes, rv=\"%d\"\n\n", __func__, rv);
	return rv;
}

static int fsus_rename_core(const char *fulloldpath,
			    const char *fullnewpath, unsigned int flags,
			    int whatDoWithOriginalDir)
{
	struct dpr_xlate_data dxdfrom_prv = DXD_INIT;
	struct dpr_xlate_data dxdfrom_new = DXD_INIT;
	/* struct dpr_xlate_data dxdto_prv = DXD_INIT; */
	struct dpr_xlate_data dxdto_new = DXD_INIT;
	struct stat fileStat;
	const char *oldpath = fulloldpath + DPR_DATA->rootdir_len;
	const char *newpath = fullnewpath + DPR_DATA->rootdir_len;
	char dxdfrom_prv_paf[PATH_MAX] = "";
	char dxdto_new_paf[PATH_MAX] = "";
	char dxdfrom_prv_latest_paf[PATH_MAX] = "";
	int type = DPRFS_FILETYPE_NA;
	int rv = 0;

	DEBUGe('1') debug_msg(DPR_DATA,
			      LOG_DIVIDER
			      "%s(oldpath=\"%s\", newpath=\"%s\")\n",
			      __func__, oldpath, newpath);

	/* When we have renameat2() in libc, then we can implement flags */
	if (flags) {
		rv = -EINVAL;
		goto complete;
	}

	forensiclog_msg("rename: %s -> %s\n", oldpath, newpath);

	dpr_xlateWholePath(&dxdfrom_prv, DPR_DATA, oldpath, false,
			   XWP_DEPTH_MAX, NULL, OBSERVE_ORIGINAL_DIR);

	dpr_xlateWholePath(&dxdto_new, DPR_DATA, newpath, true,
			   XWP_DEPTH_MAX, NULL, IGNORE_ORIGINAL_DIR);

	dxd_copy(&dxdfrom_new, &dxdfrom_prv);

	misc_debugDxd(DPR_DATA, '2', &dxdfrom_prv, " dxdfrom_prv: ", __func__);
	misc_debugDxd(DPR_DATA, '2', &dxdfrom_new, " dxdfrom_new: ", __func__);
	misc_debugDxd(DPR_DATA, '2', &dxdto_new, " dxdto_new: ", __func__);

	// determine what we're renaming: dir, file or linkedlist
	getLinkedlistName(dxdfrom_prv_paf, dxdfrom_prv);
	getLinkedlistName(dxdto_new_paf, dxdto_new);

	if (stat(dxdfrom_prv_paf, &fileStat) == -1) {
		DEBUGe('2') debug_msg(DPR_DATA,
				      "[WARNING] %s(): stat of paf=\"%s\" failed\n",
				      __func__, dxdfrom_prv_paf);
		rv = -EINVAL;
		goto complete;
	}
	if (S_ISDIR(fileStat.st_mode)) {
		// S_ISDIR is true for dirs and linkedlists
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s(): stat reports is a dir\n",
				      __func__);

		// presence of :latest determines if dir or linkedlist
		getLinkedlistLatestLnk(dxdfrom_prv_latest_paf, dxdfrom_prv);

		// can access a :latest file? Linkedlist if so, dir otherwise
		if (lstat(dxdfrom_prv_latest_paf, &fileStat) == -1) {
			DEBUGe('2') debug_msg
			    (DPR_DATA,
			     "%s(): reports error accessing paf=\"%s\", so not a linkedlist\n",
			     __func__, dxdfrom_prv_latest_paf);
			type = DPRFS_FILETYPE_DIR;

		} else {
			DEBUGe('2') debug_msg
			    (DPR_DATA,
			     "%s(): reports \"%s\" is not a link\n",
			     __func__, dxdfrom_prv_latest_paf);
			type = DPRFS_FILETYPE_LL;
		}

	} else {
		// if not a dir or linkedlist, it can only be an ordinary file, etc
		type = DPRFS_FILETYPE_OTHER;
	}

	// Now do the rename for each type
	if (type == DPRFS_FILETYPE_DIR) {
		// rename a directory
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s(): attempt rename of directory\n",
				      __func__);
		fsus_rename_dir(DPR_DATA, &dxdto_new, &dxdfrom_prv,
				newpath, oldpath, whatDoWithOriginalDir);
		rv = 0;
		goto complete;
	}

	if (type == DPRFS_FILETYPE_LL) {
		// rename a linkedlist

		// we handle renames of ".part" files differently: ".parts" are
		// naturally temporary files as used by some SMB/CIFS implementations
		//
		// bodge required as dpr_create_with.. uses gpaths for now
		rv = 0;
		if (dxdfrom_prv.is_part_file == true) {
			DEBUGe('2') debug_msg
			    (DPR_DATA,
			     "%s(): attempt rename of temp linkedlist\n",
			     __func__);
			rv = fsus_rename_ll(DPR_DATA, &dxdto_new,
					    &dxdfrom_prv, &dxdfrom_new,
					    oldpath, newpath, true);

		} else {
			DEBUGe('2') debug_msg
			    (DPR_DATA,
			     "%s(): attempt rename of normal linkedlist\n",
			     __func__);
			rv = fsus_rename_ll(DPR_DATA, &dxdto_new,
					    &dxdfrom_prv, &dxdfrom_new,
					    oldpath, newpath, false);
		}

		if (strstr(newpath, "$beyonduse") != NULL) {
			DEBUGe('2') debug_msg(DPR_DATA,
					      "%s(): calls beyonduse as well\n",
					      __func__);
			rv = dprfs_beyonduse(DPR_DATA, newpath);
		}

		goto complete;
	}
	// Doesn't seem to be special so handle as a normal rename.
	// This probably shouldn't be included in the final thing
	DEBUGe('2') debug_msg
	    (DPR_DATA,
	     "[WARNING] %s(): attempt rename of something else\n", __func__);

	rv = rename(dxdfrom_prv_paf, dxdto_new_paf);
	if (rv == -1)
		dpr_error("fsus_rename rename");
	rs_inc(DPR_DATA, &DPR_DATA->renstats_p);

	goto complete;

 complete:
	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s(): completes, rv=\"%d\"\n\n", __func__, rv);
	return rv;
}

/*
 * Change the size of a file
 *
 * AM: called when user saves a file such as to overwrite itself.
 * Truncate is unusual as it makes sense in classical filesystem
 * where we might want the file zero-sized and refilled, but
 * in DPRFS we'd go straight from original contents in the
 * penultimate link to refilled in the head without needing
 * to truncate.
 * We're going to implement truncate in the classical way here
 * anyway, but a better implementation should rethink it.
 *
 * Truncate via SMB only seems to zerolen as copying up a
 * shortened file doesn't fire the newsize not zero warning.
 * I suspect, but haven't proved to myself, that samba calls
 * this routine to make an empty file in the DPRFS, then
 * calls fsus_write above to dump data into it. Strictly
 * speaking that shouldn't be allowed
 */
static int fsus_recreate(const char *gpath, struct fuse_file_info *fi)
{
	bool reloading = true;
	off_t newsize = -1;
	return fsus_truncate_core(gpath, newsize, reloading, fi);
}

static int fsus_truncate(const char *gpath, off_t newsize,
			 struct fuse_file_info *fi)
{
	bool reloading = false;
	int rv;

	DEBUGe('1') debug_msg(DPR_DATA,
			      LOG_DIVIDER
			      "%s(gpath=\"%s\" size=\"%d\" fh=\"%d\"\n",
			      __func__, gpath, (long)newsize, fi->fh);

	rv = fsus_truncate_core(gpath, newsize, reloading, fi);

	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s() completes, rv=\"%d\"\n\n", __func__, rv);

	return rv;
}

static int fsus_truncate_core(const char *gpath, off_t newsize, bool reloading,
			      struct fuse_file_info *fi)
{
	struct dpr_xlate_data dxdfrom = DXD_INIT;
	int rv = 0;

	DEBUGe('1') debug_msg(DPR_DATA,
			      LOG_DIVIDER
			      "%s(gpath=\"%s\" size=\"%d\"\n", __func__,
			      gpath, (long)newsize);

	if (reloading == false) {
		forensicLogChangesComing(DPR_DATA, TRUNCATE_KEY, gpath);

	} else {
		/* Copying old to new would change modified time */
		forensicLogChangesComing(DPR_DATA, RECREATE_KEY, gpath);
	}

	dpr_xlateWholePath(&dxdfrom, DPR_DATA, gpath, false,
			   XWP_DEPTH_MAX, NULL, OBSERVE_ORIGINAL_DIR);

	rv = 0;
	if (dxdfrom.dprfs_filetype == DPRFS_FILETYPE_LL) {
		if (fi->fh == 0) {
			rv = fsus_truncate_core_ll(gpath, &dxdfrom, newsize,
						   fi);

		} else {
			rv = fsus_truncate_core_ll_fh(gpath, &dxdfrom, newsize,
						      fi);
		}

	} else if (dxdfrom.dprfs_filetype == DPRFS_FILETYPE_DS) {
		rv = fsus_truncate_core_ds(&dxdfrom, newsize, fi);

	} else {
		rv = -1;
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s() unexpected dxd.dprfs_filetype=\"%d\"\n",
				      __func__, dxdfrom.dprfs_filetype);
	}

	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s() completes, rv=\"%d\"\n\n", __func__, rv);

	return rv;
}

static int fsus_truncate_core_ll(const char *gpath,
				 struct dpr_xlate_data *dxdfrom, off_t newsize,
				 struct fuse_file_info *fi)
{
	struct metadata_array md_arr = MD_ARR_INIT;
	struct dpr_xlate_data dxdto = DXD_INIT;
	FILE *fp;
	char from_ll_revts_ll_file[PATH_MAX] = "";
	char to_ll_revts_ll_file[PATH_MAX] = "";
	char ll_l_ll_file[PATH_MAX] = "";
	char ll_l_fm_lnk[PATH_MAX] = "";
	char ll_l_lnk[PATH_MAX] = "";
	char linkTarget[PATH_MAX] = "";
	char *buffer;
	intmax_t buffer_sz;
	int rv;

	getLinkedlistLatestFMetadataLnk(ll_l_fm_lnk, *dxdfrom);
	DEBUGe('2') debug_msg(DPR_DATA,
			      "%s(): ll_l_fm_lnk=\"%s\"\n",
			      __func__, ll_l_fm_lnk);
	getLinkedlistLatestLnk(ll_l_lnk, *dxdfrom);

	buffer = md_malloc(&buffer_sz, ll_l_fm_lnk);
	fp = md_load(buffer, buffer_sz, ll_l_fm_lnk);

	getLinkTarget(linkTarget, DPR_DATA, ll_l_lnk);

	md_getIntoStructure(&md_arr, DPR_DATA, buffer);
	*md_arr.deleted.value = '\0';
	*md_arr.not_via.value = '\0';
	*md_arr.renamed_from.value = '\0';
	*md_arr.renamed_to.value = '\0';
	md_unload(buffer);

	dxd_copy(&dxdto, dxdfrom);

	if (linkTarget != '\0') {
		DEBUGe('3') debug_msg(DPR_DATA,
				      "%s(): linkTarget=\"%s\"\n",
				      __func__, linkTarget);
		getLinkedlistRevtsLinkedlistFile(from_ll_revts_ll_file,
						 DPR_DATA, *dxdfrom);
		incAndAssignRevision(dxdto.revision, linkTarget);
		*dxdto.payload = '\0';
		*dxdto.payload_root = '\0';
		*md_arr.payload_loc.value = '\0';
		getLinkedlistRevisionTSFile(to_ll_revts_ll_file, dxdto);
	}
	// create :latest/file
	makeAndPopulateNewRevisionTSDir(DPR_DATA, gpath, &dxdto,
					&md_arr, LINKEDLIST_EXTEND,
					PAYLOAD_LOC_SRC_NEW);

	if (newsize != 0) {
		DEBUGe('3') debug_msg(DPR_DATA,
				      "%d %s(): cp from=\"%s\" to=\"%s\"\n",
				      getpid(), __func__, from_ll_revts_ll_file,
				      to_ll_revts_ll_file);
		rv = cp(to_ll_revts_ll_file, from_ll_revts_ll_file);
		DEBUGe('3') debug_msg(DPR_DATA,
				      "%s(): copy rv=\"%d\"\n", __func__, rv);
		if (newsize > 0) {
			DEBUGe('3') debug_msg(DPR_DATA,
					      "%s(): not truncate sz=\"%d\"\n",
					      __func__, newsize,
					      to_ll_revts_ll_file);
			rv = truncate(to_ll_revts_ll_file, newsize);
		}

	} else {
		getLinkedlistLatestLinkedlistFile(ll_l_ll_file, dxdto);

		DEBUGe('3') debug_msg(DPR_DATA,
				      "%s(): make zero len file \"%s\"\n",
				      __func__, ll_l_ll_file);

		fp = fopen(ll_l_ll_file, "w");
		if (fp == NULL) {
			rv = -1;
			dpr_error("dpr_truncate_core_ll creat");

		} else {
			rv = fclose(fp);
		}
	}
	mstrfree(&md_arr.others);
	return rv;
}

static int fsus_truncate_core_ll_fh(const char *gpath,
				    struct dpr_xlate_data *dxdfrom,
				    off_t newsize,
				    struct fuse_file_info *orig_fi)
{
	struct fuse_file_info fi;
	int rv;
	DEBUGe('1') debug_msg
	    (DPR_DATA,
	     LOG_DIVIDER "%s() entry gpath=\"%s\" fd=\"%" PRIu64
	     "\" newsize=\"%lld\"\n", __func__, gpath,
	     ea_shadowFile_getValueOrKey(DPR_DATA, orig_fi), newsize);

	// might need to reload?
	if (ea_str_getValueForKey(DPR_DATA, gpath) != NULL) {
		DEBUGe('2') debug_msg
		    (DPR_DATA, LOG_DIVIDER
		     " reload required for \"%s\")\n", gpath);

		fi.flags = O_RDWR;
		fsus_recreate(gpath, &fi);
		fsus_open_shadow(gpath, &fi);

		// have reopened a new file so no more need to reload at
		// next write
		ea_str_removeElementByValue(DPR_DATA, gpath);
	}
	// logging after so it's clear when the actual write happens,
	// which is some time after entering the routine given we may
	// need to reload
	forensicLogChangesComing(DPR_DATA, WRITE_KEY, gpath);

	// no need to get fpath on this one, since I work from fi->fh not the gpath
	DEBUGe('3') debug_msg
	    (DPR_DATA,
	     LOG_DIVIDER " fsus_truncate_core_ll_fh fd=\"%" PRIu64
	     "\" size=\"%d\"\n", ea_shadowFile_getValueOrKey(DPR_DATA, orig_fi),
	     newsize);

	rv = ftruncate(ea_shadowFile_getValueOrKey(DPR_DATA, orig_fi), newsize);
	if (rv == -1)
		dpr_error("fsus_truncate_core_ll_fh truncate");

	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s() completes, rv=\"%d\"\n\n", __func__, rv);
	return rv;
}

static int fsus_truncate_core_ds(struct dpr_xlate_data *dxd, off_t newsize,
				 struct fuse_file_info *fi)
{
	char paf[PATH_MAX];
	int rv;

	getPafForOrdinaryFile(paf, *dxd);
	rv = truncate(paf, newsize);
	if (rv == -1)
		dpr_error("fsus_truncate_core_ds truncate");
	return rv;
}

/*
 * Open file
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filedescriptor in the fuse_file_info structure,
 * which will be passed to all file operations.
 *
 * Changed in version 2.2
 */
static int fsus_open_shadow(const char *gpath, struct fuse_file_info *fi)
{
	bool useShadowFD = true;
	DEBUGe('1') debug_msg(DPR_DATA, LOG_DIVIDER
			      "%s(gpath\"%s\", fi=0x%08x flags=\"%d\")\n",
			      __func__, gpath, fi, fi->flags);
	return fsus_open_core(gpath, fi, useShadowFD);
}

static int fsus_open(const char *gpath, struct fuse_file_info *fi)
{
	bool useShadowFD = false;
	int rv;
	DEBUGe('1') debug_msg(DPR_DATA, LOG_DIVIDER
			      "%s(gpath\"%s\", fi=0x%08x)\n", __func__,
			      gpath, fi);
	rv = fsus_open_core(gpath, fi, useShadowFD);

	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s() completes, rv=\"%d\"\n\n", __func__, rv);
	return rv;
}

static int
fsus_open_core(const char *gpath, struct fuse_file_info *fi, bool useShadowFD)
{
	struct dpr_xlate_data dxd = DXD_INIT;
	char ll_l_ll_file[PATH_MAX] = "";
	int flags;
	int fp;

	DEBUGe('1') debug_msg(DPR_DATA,
			      LOG_DIVIDER
			      "%s(gpath\"%s\", fi=0x%08x)\n", __func__,
			      gpath, fi);

	dpr_xlateWholePath(&dxd, DPR_DATA, gpath, false, XWP_DEPTH_MAX,
			   NULL, OBSERVE_ORIGINAL_DIR);
	if (dxd.dprfs_filetype == DPRFS_FILETYPE_LL)
		getLinkedlistLatestLinkedlistFile(ll_l_ll_file, dxd);
	else
		// can't use gpath straight, given payload-at redirect
		getPafForOrdinaryFile(ll_l_ll_file, dxd);

	flags = fi->flags & ~O_NOFOLLOW;
	DEBUGe('2') debug_msg(DPR_DATA,
			      "%s(): linkedlist_paf=\"%s\" flags=\"%d\"\n",
			      __func__, ll_l_ll_file, flags);
	fp = open(ll_l_ll_file, flags, getModeBodge());

	DEBUGe('2') debug_msg
	    (DPR_DATA, "%s() receives fd=\"%d\"\n", __func__, fp);
	if (fp == -1) {
		fp = dpr_error("fsus_open open");
		goto finish;
	}

	ea_flarrs_addElement(DPR_DATA, gpath);
	ea_str_addElement(DPR_DATA, gpath);

	if (useShadowFD) {
		ea_shadowFile_addElement(DPR_DATA, fi->fh, fp);

	} else {
		fi->fh = fp;
		ea_filetype_addElement(DPR_DATA, fi->fh, dxd.dprfs_filetype);
		ea_backup_gpath_addElement(DPR_DATA, fi->fh, gpath);
	}
	fp = 0;

 finish:
	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s() completes, fp=\"%d\"\n\n", __func__, fp);
	return fp;
}

/*
 * Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 *
 * AM: Don't let the gpath arg being passed in fool you too - the
 * DPRFS related changes are done in fsus_open() elsewhere, with a
 * file descriptor to the correct file left in fi->fh.
 * Thus: open - write - write - write - close and
 * open - read - read - read - close
 * Suggesting a solution to the ".part" problem: adapt dpr_close()
 * to copy from a non-DPRFS location into place.
 */
// I don't fully understand the documentation above -- it doesn't
// match the documentation for the read() system call which says it
// can return with anything up to the amount of data requested. nor
// with the dprfs code which returns the amount of data also
// returned by read.
static int
fsus_read(const char *gpath, char *buf, size_t size, off_t offset,
	  struct fuse_file_info *fi)
{
	int rv;

	DEBUGe('1') debug_msg
	    (DPR_DATA, LOG_DIVIDER
	     "%s(gpath=\"%s\", buf=0x%08x, size=%d, offset=%lld, fd=\"%"
	     PRIu64 "\")\n\n", __func__, gpath, buf, size, offset,
	     ea_shadowFile_getValueOrKey(DPR_DATA, fi));
	// no need to get fpath on this one, since I work from fi->fh not the gpath

	rv = pread(ea_shadowFile_getValueOrKey(DPR_DATA, fi), buf, size,
		   offset);
	if (rv == -1)
		dpr_error("fsus_read read");

	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s() completes, rv=\"%d\"\n\n", __func__, rv);
	return rv;
}

/*
 * Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Changed in version 2.2
 *
 * As  with read(), the documentation above is inconsistent with the
 * documentation for the write() system call.
 *
 * AM: see notes to fsus_read() above
 */
static int
fsus_write(const char *gpath, const char *buf, size_t size,
	   off_t offset, struct fuse_file_info *fi)
{
	unsigned int filetype;
	int rv;

	DEBUGe('1') debug_msg
	    (DPR_DATA,
	     LOG_DIVIDER "%s() entry gpath=\"%s\" fd=\"%" PRIu64
	     "\" offset=\"%lld\"\n", __func__, gpath,
	     ea_shadowFile_getValueOrKey(DPR_DATA, fi), offset);

	filetype = ea_filetype_getValueForKey(DPR_DATA, fi);
	DEBUGe('1') debug_msg
	    (DPR_DATA,
	     LOG_DIVIDER "%s() filetype \"%d\"\n", __func__, filetype);
	// might need to reload?
	if (filetype == DPRFS_FILETYPE_LL
	    && ea_str_getValueForKey(DPR_DATA, gpath) != NULL) {
		// Mint/dolphin will already have truncated this file, Windows
		// won't.
		// Do what we're told, ie, we won't make value judgements
		// on whether to keep any of successive truncates: this costs
		// us one inode each time it happens

		// conduct the reload
		DEBUGe('2') debug_msg
		    (DPR_DATA, LOG_DIVIDER
		     " reload required for \"%s\")\n", gpath);

		fsus_recreate(gpath, fi);
		fsus_open_shadow(gpath, fi);

		// have reopened a new file so no more need to reload at
		// next write
		ea_str_removeElementByValue(DPR_DATA, gpath);
	}
	// logging after so it's clear when the actual write happens,
	// which is some time after entering the routine given we may
	// need to reload
	forensicLogChangesComing(DPR_DATA, WRITE_KEY, gpath);

	// no need to get fpath on this one, since I work from fi->fh not the gpath

	DEBUGe('3') debug_msg
	    (DPR_DATA, LOG_DIVIDER " pwrite fd=\"%" PRIu64 "\"\n",
	     ea_shadowFile_getValueOrKey(DPR_DATA, fi));

	rv = pwrite(ea_shadowFile_getValueOrKey(DPR_DATA, fi), buf,
		    size, offset);
	if (rv == -1)
		dpr_error("fsus_write pwrite");

	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s() completes, rv=\"%d\"\n\n", __func__, rv);
	return rv;
}

/*
 * Possibly flush cached data
 *
 * BIG NOTE: This is not equivalent to fsync().  It's not a
 * request to sync dirty data.
 *
 * Flush is called on each close() of a file descriptor.  So if a
 * filesystem wants to return write errors in close() and the file
 * has cached dirty data, this is a good place to write back data
 * and return any errors.  Since many applications ignore close()
 * errors this is not always useful.
 *
 * NOTE: The flush() method may be called more than once for each
 * open().  This happens if more than one file descriptor refers
 * to an opened file due to dup(), dup2() or fork() calls.  It is
 * not possible to determine if a flush is final, so each flush
 * should be treated equally.  Multiple write-flush sequences are
 * relatively rare, so this shouldn't be a problem.
 *
 * Filesystems shouldn't assume that flush will always be called
 * after some writes, or that if will be called at all.
 *
 * Changed in version 2.2
 *
 * AM: Should this have fflush() around? No sign of it in the
 * original, only code for logging
 * Note I've seen flush called with a working file descriptor
 * but no gpath .. if that happens I reuse a copy stored
 * earlier
 */
static int fsus_flush(const char *gpath, struct fuse_file_info *fi)
{
	char *gpath_ptr;
	(void)gpath;
	int res;

	DEBUGe('1') debug_msg(DPR_DATA,
			      LOG_DIVIDER "%d %s(gpath=\"%s\")\n",
			      getpid(), __func__, gpath);

	if (gpath == NULL) {
		// no need to get fpath on this one, since I work from fi->fh not the gpath
		gpath_ptr = ea_backup_gpath_getValueForKey(DPR_DATA, fi);
		forensicLogChangesComing(DPR_DATA, FLUSH_KEY, gpath_ptr);
		ea_backup_gpath_removeElementByValue(DPR_DATA, gpath_ptr);

	} else {
		forensicLogChangesComing(DPR_DATA, FLUSH_KEY, gpath);
		ea_backup_gpath_removeElementByValue(DPR_DATA, gpath);
	}

	/* This is called from every close on an open file, so call the
	   close on the underlying filesystem.  But since flush may be
	   called multiple times for an open file, this must not really
	   close the file.  This is important if used on a network
	   filesystem like NFS which flush the data/metadata on close() */
	res = close(dup(fi->fh));
	if (res == -1)
		return -errno;

	DEBUGe('1') debug_msg(DPR_DATA,
			      LOG_DIVIDER "%d %s(gpath=\"%s\", fd=\"%"
			      PRIu64 "\")\n\n", getpid(), __func__, gpath,
			      ea_shadowFile_getValueOrKey(DPR_DATA, fi));
	return 0;
}

/*
 * Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
static int fsus_release(const char *gpath, struct fuse_file_info *fi)
{
	unsigned long shadowFile_fd;
	unsigned long fd;
	int len;
	int rv;

	DEBUGe('1') debug_msg(DPR_DATA,
			      LOG_DIVIDER
			      "%d %s(gpath=\"%s\" fd=\"%" PRIu64 "\")\n",
			      getpid(), __func__, gpath,
			      ea_shadowFile_getValueOrKey(DPR_DATA, fi));

	// We need to close the file.  Had we allocated any resources
	// (buffers etc) we'd need to free them here as well.
	shadowFile_fd = ea_shadowFile_getValueOrKey(DPR_DATA, fi);
	fd = fi->fh;

	rv = close(fd);
	if (rv == -1)
		dpr_error("fsus_release fd");

	if (shadowFile_fd != fd) {
		rv = close(shadowFile_fd);
		if (rv == -1)
			dpr_error("fsus_release shadowFile_fd");
		ea_shadowFile_removeElementByValue(DPR_DATA, fd, shadowFile_fd);
	}

	forensicLogChangesApplied(DPR_DATA, gpath);
	ea_flarrs_removeElementByValue(DPR_DATA, gpath);
	ea_filetype_removeElementByKey(DPR_DATA, fd);
	ea_filetype_removeElementByKey(DPR_DATA, shadowFile_fd);
	ea_str_removeElementByValue(DPR_DATA, gpath);

	len = strlen(gpath);
	if (strcmp(gpath + len - 6, ".accdb") == 0) {
		// .*\.accdb$ is Microsft Access
		backupDatastore(gpath);
	}

	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s completes, rv=\"%d\"\n\n", __func__, rv);
	return rv;
}

/*
 * Recursive directory creation with thanks to Nico Golde
 * http://nion.modprobe.de/blog/archives/357-Recursive-directory-creation.html
 */
static void backupDatastore(const char *gpath)
{
	struct dpr_xlate_data dxd = DXD_INIT;
	struct timeval time_now;
	struct tm *time_str_tm;
	char sourcepaf[PATH_MAX] = "";
	char destpaf[PATH_MAX] = "";
	char tmp[PATH_MAX] = "";
	char output[80] = "";
	char *p = NULL;
	size_t len;

	DEBUGe('1') debug_msg(DPR_DATA,
			      LOG_DIVIDER "%s(gpath=\"%s\")\n",
			      __func__, gpath);

	dpr_xlateWholePath(&dxd, DPR_DATA, gpath, false, XWP_DEPTH_MAX,
			   NULL, OBSERVE_ORIGINAL_DIR);
	if (dxd.dprfs_filetype != DPRFS_FILETYPE_DS) {
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s unexpected dxd.dprfs_filetype=\"%d\"\n",
				      __func__, dxd.dprfs_filetype);

	} else {
		/* dxd.rootdir points to /tmp/dprfs */
		strcpy(destpaf, DPR_DATA->rootdir);
		strcat(destpaf, dxd.relpath);
		strcat(destpaf, dxd.finalpath);
		strcat(destpaf, "-backups/");

		gettimeofday(&time_now, NULL);
		time_str_tm = gmtime(&time_now.tv_sec);

		sprintf(output, "%04i/%02i/%02i/%02i%02i%02i.%06i",
			time_str_tm->tm_year + 1900,
			time_str_tm->tm_mon + 1, time_str_tm->tm_mday,
			time_str_tm->tm_hour, time_str_tm->tm_min,
			time_str_tm->tm_sec, (int)time_now.tv_usec);

		strncat(destpaf, output, 11);
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s() backup path=\"%s\"\n",
				      __func__, destpaf);

		snprintf(tmp, sizeof(tmp), "%s", destpaf);
		len = strlen(tmp);
		if (tmp[len - 1] == '/')
			tmp[len - 1] = 0;
		for (p = tmp + 1; *p; p++)
			if (*p == '/') {
				*p = 0;
				mkdir(tmp, S_IRWXU);
				*p = '/';
			}
		mkdir(tmp, S_IRWXU);

		strcat(destpaf, &output[11]);
		strcat(destpaf, "-");
		strcat(destpaf, dxd.finalpath);
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s() backup to paf=\"%s\"\n",
				      __func__, destpaf);

		getPafForOrdinaryFile(sourcepaf, dxd);
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s() backup from paf=\"%s\"\n",
				      __func__, sourcepaf);

		cp(destpaf, sourcepaf);
	}
	DEBUGe('1') debug_msg(DPR_DATA, "%s() completes\n", __func__);
}

/////////////////////////////////////
// Directories

/*
 * mkdir entry point from fuse
 *
 * No metadata, so pass as blank
 */
static int fsus_mkdir(const char *gpath, mode_t mode)
{
	struct metadata_array md_arr = MD_ARR_INIT;
	struct stat sb;
	char currentTS[TIMESTAMP_SIZE] = "";
	char gpathwithts[PATH_MAX] = "";
	int rv;

	DEBUGe('1') debug_msg(DPR_DATA,
			      LOG_DIVIDER "%s(gpath=\"%s\")\n",
			      __func__, gpath);

	// modify expected paf to include a timestamp
	getCondensedSystemUTime(currentTS);
	strcpy(gpathwithts, gpath);
	strcat(gpathwithts, "-");
	strcat(gpathwithts, currentTS);

	// now create the timestamed dir
	createLLID(&md_arr.llid, DPR_DATA, gpathwithts);
	createLLID(&md_arr.sha256, DPR_DATA, gpathwithts);
	rv = fsus_mkdir_core(DPR_DATA, gpathwithts, mode, &md_arr, SERVERSIDE);
	if (rv == 0) {
		// and now rename into the user's paf
		if (lstat(gpath, &sb) == -1) {
			// if dest doesn't exist, link back to dir-ts
			rv = fsus_rename_core(gpathwithts, gpath, 0,
					      SERVERSIDE);

		} else {
			// if the dest does exist, link back to previous
			rv = fsus_rename_core(gpathwithts, gpath, 0, USERSIDE);
		}
	}

	mstrfree(&md_arr.others);
	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s completes, rv=\"%d\"\n\n", __func__, rv);

	return rv;
}

/*
 * mkdir entry point when called within dprfs
 *
 * Note metadata passed along for creation
 */
static int
fsus_mkdir_with_metadata(struct dpr_state *dpr_data, const char *gpath,
			 mode_t mode, struct metadata_array *md_arr,
			 int whatDoWithOriginalDir)
{
	return fsus_mkdir_core(dpr_data, gpath, mode, md_arr,
			       whatDoWithOriginalDir);
}

/*
 * Make directory along with any new metadata
 *
 * If recreating an old folder, mark it as undeleted too
 */
static int
fsus_mkdir_core(struct dpr_state *dpr_data, const char *gpath,
		mode_t mode, struct metadata_array *md_arr,
		int whatDoWithOriginalDir)
{
	struct dpr_xlate_data dxdprev = DXD_INIT;
	struct dpr_xlate_data dxdorig = DXD_INIT;
	char ll_name[PATH_MAX] = "";
	int rv = 0;

	DEBUGi('1') debug_msg(dpr_data,
			      LOG_DIVIDER "%s(gpath=\"%s\")\n",
			      __func__, gpath);

	forensicLogChangesComing(dpr_data, MKDIR_KEY, gpath);

	dpr_xlateWholePath(&dxdorig, dpr_data, gpath, true,
			   XWP_DEPTH_MAX, NULL, OBSERVE_ORIGINAL_DIR);
	if (whatDoWithOriginalDir == SERVERSIDE) {
		dpr_xlateWholePath(&dxdprev, dpr_data, gpath, true,
				   XWP_DEPTH_MAX, NULL, OBSERVE_ORIGINAL_DIR);

	} else {
		dpr_xlateWholePath(&dxdprev, dpr_data, gpath, true,
				   XWP_DEPTH_MAX, NULL, IGNORE_ORIGINAL_DIR);
	}
	if (dxdprev.deleted == true) {
		strcpy(md_arr->deleted.value, "false");

		DEBUGi('2') debug_msg(dpr_data, "was deleted \n#");
		DEBUGi('2') debug_msg(dpr_data, "gpath = %s\n", gpath);
		rv = saveDMetadataToFile(dpr_data, dxdprev, md_arr);

	} else {
		DEBUGi('2') debug_msg(dpr_data, "was not deleted \n");

		// make the directory
		getLinkedlistName(ll_name, dxdorig);
		rv = mkdir(ll_name, getDefaultDirMode());

		if (rv == -1) {
			dpr_error("dpr_mkdir mkdir");
		}

		rv = saveDMetadataToFile(dpr_data, dxdprev, md_arr);
	}

	DEBUGe('1') debug_msg(dpr_data,
			      "%s completes, rv=\"%d\"\n\n", __func__, rv);
	return rv;
}

/*
 * Remove a directory
 *
 * In this prototype, only the top level directory is
 * marked deleted; other systems will recursively mark
 * all child objects as deleted.
 *
 *  My decision is about safety: there's no benefit to
 * a recursive delete when any access would go by the
 * top level directory, and so hit the deleted flag.
 *  The other filesystem's choice is predicated on
 * obtaining more space.
 */
static int fsus_rmdir(const char *gpath)
{
	struct metadata_array md_arr = MD_ARR_INIT;
	struct dpr_xlate_data dxd = DXD_INIT;
	int rv;

	DEBUGe('1') debug_msg(DPR_DATA,
			      LOG_DIVIDER "%s(gpath=\"%s\"\n", __func__, gpath);

	forensicLogChangesComing(DPR_DATA, RMDIR_KEY, gpath);

	// log and obtain the directory to remove
	dpr_xlateWholePath(&dxd, DPR_DATA, gpath, true, XWP_DEPTH_MAX,
			   NULL, OBSERVE_ORIGINAL_DIR);
	// create :latest/:Fmetadata with supercedes line
	strcpy(md_arr.deleted.value, "true");
	rv = saveDMetadataToFile(DPR_DATA, dxd, &md_arr);

	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s completes, rv=\"%d\"\n\n", __func__, rv);
	return rv;
}

/* Open directory
 *
 * This method should check if the open operation is permitted for
 * this  directory
 *
 * Introduced in version 2.3
 *
 */
static int fsus_opendir(const char *gpath, struct fuse_file_info *fi)
{
	struct dpr_xlate_data dxd = DXD_INIT;
	char ll_name[PATH_MAX] = "";
	int rv = 0;
	int res;

	struct xmp_dirp *d = malloc(sizeof(struct xmp_dirp));
	if (d == NULL)
		return -ENOMEM;

	DEBUGe('1') debug_msg(DPR_DATA,
			      LOG_DIVIDER "%s(gpath=\"%s\")\n",
			      __func__, gpath);
	dpr_xlateWholePath(&dxd, DPR_DATA, gpath, false, XWP_DEPTH_MAX,
			   NULL, OBSERVE_ORIGINAL_DIR);
	misc_debugDxd(DPR_DATA, '2', &dxd, " dxd: ", __func__);
	getLinkedlistName(ll_name, dxd);

	DEBUGe('2') debug_msg(DPR_DATA, "%s(fpath=\"%s\")\n", __func__,
			      ll_name);

	d->lookit = XMPDIRP_NORMAL;
	d->dp = opendir(ll_name);
	if (d->dp == NULL) {
		res = -errno;
		free(d);
		return res;
	}
	d->offset = 0;
	d->entry = NULL;

	d->shadow_dp = opendir(TMP_PATH);
	if (d->shadow_dp == NULL) {
		res = -errno;
		free(d);
		return res;
	}
	d->shadow_offset = 0;
	d->shadow_entry = NULL;

	ea_filetype_addElement(DPR_DATA, fi->fh, DPRFS_FILETYPE_DIR);
	fi->fh = (unsigned long)d;

	DEBUGe('2') debug_msg(DPR_DATA,
			      "%s() fd=\"%d\" restat=\"%d\" dp=\"%d\" shadowdp=\"%d\"\n",
			      __func__, fi->fh, rv, d->dp, d->shadow_dp);
	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s() completes, rv=\"%d\"\n\n", __func__, rv);
	return 0;
}

/*
 * Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 *
 * AM: note the code uses the filedescriptor from opendir and otherwise
 * ignores what gpath says
 */
static int
fsus_readdir(const char *gpath, void *buf, fuse_fill_dir_t filler,
	     off_t offset, struct fuse_file_info *fi,
	     enum fuse_readdir_flags flags)
{
	struct dpr_xlate_data dxd = DXD_INIT;
	struct xmp_dirp *d = get_dirp(fi);
	char gpathandstroke[PATH_MAX] = "";
	char rel_ll_name[PATH_MAX] = "";
	char hashel[PATH_MAX] = "";
	char dname[PATH_MAX] = "";
	char paf[PATH_MAX] = "";
	int len;
	int rv;
	(void)gpath;

	DEBUGe('1') debug_msg(DPR_DATA,
			      LOG_DIVIDER
			      "%s(gpath=\"%s\" offset=\"%d\" lookit=\"%d\")\n",
			      __func__, gpath, offset, d->lookit);

	rv = 0;
	if (d->lookit == XMPDIRP_EXHAUSTED)
		goto complete_all;

	if (offset == -1 || d->offset == -1)
		goto shadow;

	// once again, no /need/ for xlateWholePath -- but note that I need to cast fi->fh
	dpr_xlateWholePath(&dxd, DPR_DATA, gpath, false, XWP_DEPTH_MAX,
			   NULL, OBSERVE_ORIGINAL_DIR);
	// Every directory contains at least two entries: . and ..  If my
	// first call to the system readdir() returns NULL I've got an
	// error; near as I can tell, that's the only condition under
	// which I can get an error from readdir()
	DEBUGe('2') debug_msg(DPR_DATA,
			      "%s fd=\"%d\" restat=\"%d\" offset=\"%d\"\n",
			      __func__, fi->fh, rv, offset);
	// This will copy the entire directory into the buffer.  The loop exits
	// when either the system readdir() returns NULL, or filler()
	// returns something non-zero.  The first case just means I've
	// read the whole directory; the second means the buffer is full.
	getRelLinkedlistName(rel_ll_name, dxd);
	DEBUGe('2') debug_msg(DPR_DATA, " -- 1checking status of %s\n",
			      rel_ll_name);

	if (d->lookit == XMPDIRP_SHADOW)
		goto shadow;

	if (offset != d->offset) {
		seekdir(d->dp, offset);
		d->entry = NULL;
		d->offset = offset;
	}
	while (1) {
		struct stat st;
		off_t nextoff;
		enum fuse_fill_dir_flags fill_flags = 0;

		if (!d->entry) {
			errno = 0;
			d->entry = readdir(d->dp);
			if (errno != 0)
				return 0;
			if (!d->entry) {
				d->lookit = XMPDIRP_SHADOW;
				d->entry = NULL;
				d->offset = -1;
				break;
			}
		}
		DEBUGe('2') debug_msg(DPR_DATA,
				      " -- 2checking status of %s\n",
				      d->entry->d_name);

		// readdir should ignore :Dmetadata & :Fmetadata
		// as neither should be available to the gdrive
		// user, and both would cause rmdir to report
		// non-empty.
		if (strncmp(d->entry->d_name, ":Dmetadata", 10) == 0)
			goto dun_lookin;
		if (strncmp(d->entry->d_name, ":Fmetadata", 10) == 0)
			goto dun_lookin;

		// check status of file before adding it in: linkedlists
		// and directories may be marked "deleted"
		strcpy(paf, rel_ll_name);
		if (paf[0] != '/' || paf[1] != '\0')
			// skip a second separator if the path is "/"
			strcat(paf, "/");
		strcat(paf, d->entry->d_name);
		strcpy(dname, d->entry->d_name);

		if (strcmp(d->entry->d_name, "..") == 0)
			goto filler;
		if (strcmp(d->entry->d_name, ".") == 0)
			goto filler;

		dpr_xlateWholePath(&dxd, DPR_DATA, paf, false,
				   XWP_DEPTH_MAX, NULL, OBSERVE_ORIGINAL_DIR);

		if (*dxd.rootdir == '\0' || dxd.deleted == true) {
			DEBUGe('2') debug_msg(DPR_DATA, " skipping\n", paf);
			goto dun_lookin;
		}

		if (dxd.dprfs_filetype == DPRFS_FILETYPE_LL) {
			// reverse out bodge to accomodate OSX
			len = strlen(dname);
			if (len >= 10
			    && strcmp(dname + len - 10, ".numbers_d") == 0)
				goto unbodge_for_osx_1;
			if (len >= 8
			    && strcmp(dname + len - 8, ".pages_d") == 0)
				goto unbodge_for_osx_1;
			if (len >= 6 && strcmp(dname + len - 6, ".key_d") == 0)
				goto unbodge_for_osx_1;
			goto nobodge_for_osx_1;
 unbodge_for_osx_1:
			dname[len - 2] = '\0';
		}
 nobodge_for_osx_1:
 filler:
#ifdef HAVE_FSTATAT
		if (flags & FUSE_READDIR_PLUS) {
			char ll_l_ll_file[PATH_MAX] = "";
			int res;

			if (dxd.dprfs_filetype == DPRFS_FILETYPE_LL)
				getLinkedlistLatestLinkedlistFile
				    (ll_l_ll_file, dxd);
			else
				// can't use gpath straight, given payload-at redirect
				getPafForOrdinaryFile(ll_l_ll_file, dxd);
			res =
			    fstatat(dirfd(d->dp), ll_l_ll_file, &st,
				    AT_SYMLINK_NOFOLLOW);
			if (res != -1)
				fill_flags |= FUSE_FILL_DIR_PLUS;
		}
#endif				/* #ifdef HAVE_FSTATAT */
		if (!(fill_flags & FUSE_FILL_DIR_PLUS)) {
			memset(&st, 0, sizeof(st));
			st.st_ino = d->entry->d_ino;
			st.st_mode = d->entry->d_type << 12;
		}
		nextoff = telldir(d->dp);
		if (filler(buf, dname, &st, nextoff, fill_flags))
			break;
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s %d: nomral A calling filler with name %s\n",
				      __FILE__, __LINE__, dname);

		d->offset = nextoff;
 dun_lookin:
		d->entry = NULL;
	}

	/* now repeat for files stored in /tmp/drpfs */
	const char *cleaned;
 shadow:
	cleaned = gpath;
	if (strncmp(cleaned, DPR_DATA->rootdir, DPR_DATA->rootdir_len)
	    == 0)
		cleaned = cleaned + DPR_DATA->rootdir_len;

	strcpy(gpathandstroke, cleaned);
	/* if pas doesn't begin at root, or pas not 1 char long */
	if (gpathandstroke[0] != '/' || gpathandstroke[1] != '\0')
		strcat(gpathandstroke, "/");
	catSha256ToStr(hashel, DPR_DATA, gpathandstroke);
	strcat(hashel, ":");

	/* tmpdir = opendir(TMP_PATH); */
	DEBUGe('2') debug_msg(DPR_DATA,
			      "2 %s() fd=\"%d\" dir=\"%s\" cleaned=\"%s\" hashel=\"%s\"\n, gpas=\"%s\"\n",
			      __func__, d->shadow_dp, TMP_PATH, cleaned,
			      hashel, gpathandstroke);
	if (d->shadow_dp == NULL) {
		dpr_error("2fsus_readdir shadow_d->dp");
		goto complete_all;
	}

	if (d->shadow_offset == -1)
		goto complete_all;
	// This will copy the entire directory into the buffer.  The loop exits
	// when either the system readdir() returns NULL, or filler()
	// returns something non-zero.  The first case just means I've
	// read the whole directory; the second means the buffer is full.
	if (offset != d->shadow_offset) {
		seekdir(d->shadow_dp, 0);
		d->shadow_entry = NULL;
		d->shadow_offset = 0;
	}
	while (1) {
		struct stat st;
		off_t nextoff = 0;
		enum fuse_fill_dir_flags fill_flags = 0;

		if (!d->shadow_entry) {
			errno = 0;
			d->shadow_entry = readdir(d->shadow_dp);
			if (errno != 0)
				return 0;
			if (!d->shadow_entry) {
				d->shadow_offset = -1;
				break;
			}
		}

		DEBUGe('2') debug_msg(DPR_DATA,
				      " (tmp) found: \"%s\"\n",
				      d->shadow_entry->d_name);

		// readdir should ignore :Dmetadata & :Fmetadata
		// as neither should be available to the gdrive
		// user, and both would cause rmdir to report
		// non-empty. Also ignore ., .., and any other object
		// not at least one byte longer than the hash and hyphen
		//
		// This should at least cover access-denied-file,
		// access-denied-dir, . and ..
		if (strlen(d->shadow_entry->d_name) <
		    SHA256_DIGEST_LENGTH * 2 + 2) {
			DEBUGe('2') debug_msg(DPR_DATA,
					      " TMP too short, skipping %s\n",
					      d->shadow_entry->d_name);
			goto tmp_dun_lookin;
		}

		/* ignore if file doesn't belong to this dir */
		DEBUGe('2') debug_msg(DPR_DATA,
				      " TMP checking if %s belongs here, with hashel \"%s\"\n",
				      d->shadow_entry->d_name, hashel);

		if (strncmp
		    (d->shadow_entry->d_name, hashel,
		     SHA256_DIGEST_LENGTH * 2 + 1) != 0) {
			DEBUGe('2') debug_msg(DPR_DATA, " nope\n");
			// why does this goto cause a reduction, when the one above doesmt?
			goto tmp_dun_lookin;
		}

		strcpy(paf, rel_ll_name);
		if (paf[0] != '/' || paf[1] != '\0')
			strcat(paf, "/");
		strcat(paf,
		       &d->shadow_entry->d_name[SHA256_DIGEST_LENGTH * 2 + 1]);
		strcpy(dname,
		       &d->shadow_entry->d_name[SHA256_DIGEST_LENGTH * 2 + 1]);
		DEBUGe('2') debug_msg(DPR_DATA,
				      " checking status of %s\n", dname);

		dpr_xlateWholePath(&dxd, DPR_DATA, paf, false,
				   XWP_DEPTH_MAX, NULL, OBSERVE_ORIGINAL_DIR);
		if (*dxd.rootdir == '\0' || dxd.deleted == true) {

			DEBUGe('2') debug_msg(DPR_DATA, " skipping\n", paf);
			goto tmp_dun_lookin;
		}
#ifdef HAVE_FSTATAT
		if (flags & FUSE_READDIR_PLUS) {
			char ll_l_ll_file[PATH_MAX] = "";
			int res;

			if (dxd.dprfs_filetype == DPRFS_FILETYPE_LL)
				getLinkedlistLatestLinkedlistFile
				    (ll_l_ll_file, dxd);
			else
				// can't use gpath straight, given payload-at redirect
				getPafForOrdinaryFile(ll_l_ll_file, dxd);
			if (dxd.is_accdb) {
				/* all accdb files are softlinked from */
				/* /tmp/dprfs/file.accdb to */
				/* /home/smb/accdb/file.accdb */
				res =
				    fstatat(dirfd(d->shadow_dp),
					    ll_l_ll_file, &st, 0);
			} else {
				res =
				    fstatat(dirfd(d->shadow_dp),
					    ll_l_ll_file, &st,
					    AT_SYMLINK_NOFOLLOW);
			}
			if (res != -1)
				fill_flags |= FUSE_FILL_DIR_PLUS;
		}
#endif				/* #ifdef HAVE_FSTATAT */
		if (!(fill_flags & FUSE_FILL_DIR_PLUS)) {
			memset(&st, 0, sizeof(st));
			st.st_ino = d->shadow_entry->d_ino;
			st.st_mode = d->shadow_entry->d_type << 12;
		}
		nextoff = telldir(d->shadow_dp);
		if (filler(buf, dname, &st, nextoff, fill_flags))
			break;
		// tests indicate only d->shadow_entry->d_name with filename can go in: the paths
		// later used are supplied from elsewhere, not from here
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s %d: temp A calling filler with name %s\n",
				      __FILE__, __LINE__, dname);
 tmp_dun_lookin:
		d->shadow_entry = NULL;
		d->shadow_offset = nextoff;
	}
	rv = 0;
 complete_all:
	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s() completes, rv=\"%d\" offset=\"%d\" shadow_offset=\"%d\"\n\n",
			      __func__, rv, d->offset, d->shadow_offset);
	return rv;
}

/*
 * Release directory
 *
 * Introduced in version 2.3
 */
static int fsus_releasedir(const char *gpath, struct fuse_file_info *fi)
{
	struct dpr_xlate_data dxd = DXD_INIT;
	struct xmp_dirp *d = get_dirp(fi);
	char filename_dir[PATH_MAX] = "";
	int rv;
	(void)gpath;

	rv = 0;
	DEBUGe('1') debug_msg(DPR_DATA,
			      LOG_DIVIDER "%s(gpath=\"%s\")\n",
			      __func__, gpath);
	dpr_xlateWholePath(&dxd, DPR_DATA, gpath, false, XWP_DEPTH_MAX,
			   NULL, OBSERVE_ORIGINAL_DIR);
	getLinkedlistName(filename_dir, dxd);
	DEBUGe('2') debug_msg(DPR_DATA,
			      "%s(fpath=\"%s\")\n", __func__, filename_dir);

	closedir(d->dp);
	closedir(d->shadow_dp);
	free(d);

	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s() completes, rv=\"%d\"\n\n", __func__, rv);
	return rv;
}

/////////////////////////////////////
// links

/*
 * Read the target of a symbolic link
 *
 * The buffer should be filled with a null terminated string.  The
 * buffer size argument includes the space for the terminating
 * null character.  If the linkname is too long to fit in the
 * buffer, it should be truncated.  The return value should be 0
 * for success.
 */
// Note the system readlink() will truncate and lose the terminating
// null.  So, the size passed to to the system readlink() must be one
// less than the size passed to fsus_readlink()
// fsus_readlink() code by Bernardo F Costa (thanks!)
static int fsus_readlink(const char *gpath, char *link, size_t size)
{
	struct dpr_xlate_data dxd = DXD_INIT;
	char ll_name[PATH_MAX] = "";
	int rv;

	DEBUGe('1') debug_msg(DPR_DATA,
			      LOG_DIVIDER "%s(gpath=\"%s\")\n",
			      __func__, gpath);
	dpr_xlateWholePath(&dxd, DPR_DATA, gpath, true, XWP_DEPTH_MAX,
			   NULL, OBSERVE_ORIGINAL_DIR);

	getLinkedlistName(ll_name, dxd);
	DEBUGe('2') debug_msg(DPR_DATA,
			      "%s(fpath=\"%s\")\n", __func__, ll_name);

	rv = readlink(ll_name, link, size - 1);
	if (rv == -1) {
		dpr_error("fsus_readlink readlink");
	} else {
		link[rv] = '\0';
		rv = 0;
	}

	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s() completes, link=\"%s\" rv=\"%d\"\n\n",
			      __func__, link, rv);
	return rv;
}

/*
 * Create a symbolic link
 */
// The parameters here are a little bit confusing, but do correspond
// to the symlink() system call.  The 'gpath' is where the link points,
// while the 'link' is the link itself.  So we need to leave the gpath
// unaltered, but insert the link into the mounted directory.
/* AM: this code incomplete: links not yet supported */
static int fsus_symlink(const char *gpath, const char *link)
{
	struct dpr_xlate_data dxd = DXD_INIT;
	struct dpr_xlate_data dxd2 = DXD_INIT;
	char gpath2[PATH_MAX] = "";
	char ll_name[PATH_MAX] = "";
	char ll_name2[PATH_MAX] = "";
	int rv;

	forensiclog_msg("symlink: %s -> %s\n", link, gpath);
	DEBUGe('1') debug_msg(DPR_DATA,
			      LOG_DIVIDER "%s(gpath=\"%s\")\n",
			      __func__, gpath);
	forensicLogChangesComing(DPR_DATA, SYMLINK_KEY, gpath);

	strcpy(gpath2, gpath);

	dpr_xlateWholePath(&dxd, DPR_DATA, gpath, true, XWP_DEPTH_MAX,
			   NULL, OBSERVE_ORIGINAL_DIR);
	dpr_xlateWholePath(&dxd2, DPR_DATA, gpath2, true, XWP_DEPTH_MAX,
			   NULL, OBSERVE_ORIGINAL_DIR);
	getLinkedlistName(ll_name, dxd);
	DEBUGe('2') debug_msg(DPR_DATA, "%s(fpath=\"%s\")\n", __func__,
			      ll_name);
	getLinkedlistName(ll_name2, dxd2);
	DEBUGe('2') debug_msg(DPR_DATA, "%s(fpath=\"%s\")\n", __func__,
			      ll_name2);

	rv = symlink(ll_name2, ll_name);
	if (rv == -1)
		dpr_error("fsus_symlink symlink");

	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s() completes, rv=\"%d\"\n\n", __func__, rv);
	return rv;
}

/*
 * Create a hard link to a file
 */
static int fsus_link(const char *gpath, const char *newpath)
{
	struct dpr_xlate_data dxdold = DXD_INIT;
	struct dpr_xlate_data dxdnew = DXD_INIT;
	char old_ll_name[PATH_MAX] = "";
	char new_ll_name[PATH_MAX] = "";
	int rv;

	forensiclog_msg("(hard)link: %s -> %s\n", newpath, gpath);
	DEBUGe('1') debug_msg(DPR_DATA,
			      LOG_DIVIDER "%s(gpath=\"%s\")\n",
			      __func__, gpath);
	forensicLogChangesComing(DPR_DATA, LINK_KEY, gpath);

	dpr_xlateWholePath(&dxdold, DPR_DATA, gpath, true,
			   XWP_DEPTH_MAX, NULL, OBSERVE_ORIGINAL_DIR);
	dpr_xlateWholePath(&dxdnew, DPR_DATA, newpath, true,
			   XWP_DEPTH_MAX, NULL, OBSERVE_ORIGINAL_DIR);
	getLinkedlistName(old_ll_name, dxdold);
	DEBUGe('2') debug_msg(DPR_DATA,
			      "%s(fpath=\"%s\")\n", __func__, old_ll_name);
	getLinkedlistName(new_ll_name, dxdnew);
	DEBUGe('2') debug_msg(DPR_DATA,
			      "%s(fpath=\"%s\")\n", __func__, new_ll_name);

	rv = link(old_ll_name, new_ll_name);
	if (rv == -1)
		dpr_error("fsus_link link");

	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s completes, rv=\"%d\"\n\n", __func__, rv);
	return rv;
}

/////////////////////////////////////
// metadata

/* Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 *
 * AM: when one lstats, one lstats a dirname or filename. In DPRFS,
 * both these may represent a linkedlist given effect by futher
 * dirnames and filenames, so we may have a race. Later will need
 * to add a utility to ensure that the Gdrive lstat matches the
 * Rdrive head
 */
static int fsus_getattr(const char *gpath, struct stat *statbuf,
			struct fuse_file_info *fi)
{
	const char *gpath2 = gpath + DPR_DATA->rootdir_len;
	struct dpr_xlate_data dxd = DXD_INIT;
	char ll_name[PATH_MAX] = "";
	char ll_l_ll_file[PATH_MAX] = "";
	int rv;

	DEBUGe('1') debug_msg(DPR_DATA,
			      LOG_DIVIDER "%s() entry: gpath2=\"%s\"\n",
			      __func__, gpath2);

	dpr_xlateWholePath(&dxd, DPR_DATA, gpath2, false, XWP_DEPTH_MAX,
			   NULL, OBSERVE_ORIGINAL_DIR);
	// in this situation, a getattr against a linkedlist should visit the head file,
	// a getattr against anything else should visit the linkedlist name.
	// An easy way to solve this is to store in dxd whether this is a linkedlist or
	// not, then switch based on that
	if (dxd.dprfs_filetype == DPRFS_FILETYPE_LL) {
		getLinkedlistLatestLinkedlistFile(ll_l_ll_file, dxd);
		rv = stat(ll_l_ll_file, statbuf);
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s() ll fpath=\"%s\")\n",
				      __func__, ll_l_ll_file);

	} else {
		getPafForOrdinaryFile(ll_name, dxd);
		if (dxd.deleted == true)
			*ll_name = '\0';
		rv = stat(ll_name, statbuf);
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s() !ll fpath=\"%s\")\n",
				      __func__, ll_name);
	}

	if (rv != 0)
		rv = dpr_level_error('1', "fsus_getattr lstat");

	DEBUGe('1') debug_msg(DPR_DATA, "%s() exit, rv=\"%d\"\n\n",
			      __func__, rv);
	return rv;
}

/* Change the permission bits of a file */
static int fsus_chmod(const char *gpath, mode_t mode, struct fuse_file_info *fi)
{
	struct dpr_xlate_data dxd = DXD_INIT;
	int rv;

	DEBUGe('1') debug_msg(DPR_DATA,
			      LOG_DIVIDER
			      "%s(gpath=\"%s\", mode=0%03o)\n",
			      __func__, gpath, mode);

	forensicLogChangesComing(DPR_DATA, CHMOD_KEY, gpath);

	dpr_xlateWholePath(&dxd, DPR_DATA, gpath, false, XWP_DEPTH_MAX,
			   NULL, OBSERVE_ORIGINAL_DIR);
	if (dxd.dprfs_filetype == DPRFS_FILETYPE_LL) {
		rv = fsus_chmod_ll(&dxd, mode);

	} else if (dxd.dprfs_filetype == DPRFS_FILETYPE_DS
		   || dxd.dprfs_filetype == DPRFS_FILETYPE_DIR) {
		rv = fsus_chmod_ds(&dxd, mode);

	} else {
		rv = -1;
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s() unexpected dxd.dprfs_filetype=\"%d\"\n",
				      __func__, dxd.dprfs_filetype);
	}
	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s() completes, rv=\"%d\"\n\n", __func__, rv);
	return rv;
}

static int fsus_chmod_ds(struct dpr_xlate_data *dxd, mode_t mode)
{
	char paf[PATH_MAX] = "";
	int rv;

	getPafForOrdinaryFile(paf, *dxd);
	DEBUGe('2') debug_msg(DPR_DATA, "%s(fpath=\"%s\")\n", __func__, paf);

	rv = chmod(paf, mode);
	if (rv == -1)
		dpr_error("fsus_chmod_ds chmod");
	return rv;
}

static int fsus_chmod_ll(struct dpr_xlate_data *dxd, mode_t mode)
{
	char paf[PATH_MAX] = "";
	int rv;

	getLinkedlistLatestLinkedlistFile(paf, *dxd);
	DEBUGe('2') debug_msg(DPR_DATA, "%s(fpath=\"%s\")\n", __func__, paf);

	rv = chmod(paf, mode);
	if (rv == -1)
		dpr_error("fsus_chmod_ll chmod_ll");
	return rv;
}

/* Change the owner and group of a file */
static int fsus_chown(const char *gpath, uid_t uid, gid_t gid,
		      struct fuse_file_info *fi)
{
	struct dpr_xlate_data dxd = DXD_INIT;
	char ll_name[PATH_MAX] = "";
	int rv;

	DEBUGe('1') debug_msg(DPR_DATA,
			      LOG_DIVIDER "%s(gpath=\"%s\")\n",
			      __func__, gpath);

	forensicLogChangesComing(DPR_DATA, CHOWN_KEY, gpath);

	dpr_xlateWholePath(&dxd, DPR_DATA, gpath, true, XWP_DEPTH_MAX,
			   NULL, OBSERVE_ORIGINAL_DIR);
	getLinkedlistName(ll_name, dxd);
	DEBUGe('2') debug_msg(DPR_DATA, "%s(fpath=\"%s\")\n", __func__,
			      ll_name);

	rv = lchown(ll_name, uid, gid);
	if (rv == -1)
		dpr_error("fsus_chown chown");

	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s() completes, rv=\"%d\"\n\n", __func__, rv);
	return rv;
}

/*
 * Get file system statistics
 *
 * The 'f_frsize', 'f_favail', 'f_fsid' and 'f_flag' fields are ignored
 *
 * Replaced 'struct statfs' parameter with 'struct statvfs' in
 * version 2.5
 *
 * AM: My reading is that this returns the same result for any
 * arbitrary path and filename in the mount; it looks like
 * samba only tests against "/". So, no immediate need to
 * translate gpath to rpath
 */
static int fsus_statfs(const char *gpath, struct statvfs *statv)
{
	struct dpr_xlate_data dxd = DXD_INIT;
	char ll_name[PATH_MAX] = "";
	int rv;

	DEBUGe('1') debug_msg(DPR_DATA, "%s(gpath=\"%s\")\n", __func__, gpath);
	dpr_xlateWholePath(&dxd, DPR_DATA, gpath, true, XWP_DEPTH_MAX,
			   "", OBSERVE_ORIGINAL_DIR);
	// get stats for underlying filesystem
	getLinkedlistName(ll_name, dxd);
	DEBUGe('2') debug_msg(DPR_DATA, "%s(fpath=\"%s\")\n", __func__,
			      ll_name);

	rv = statvfs(ll_name, statv);
	if (rv == -1)
		dpr_error("fsus_statfs statvfs");

	DEBUGe('1') debug_msg(DPR_DATA, "%s() returns rv=\"%d\")\n\n",
			      __func__, rv);
	return rv;
}

/*
 * Check file access permissions
 *
 * This will be called for the access() system call.  If the
 * 'default_permissions' mount option is given, this method is not
 * called.
 *
 * This method is not called under Linux kernel versions 2.4.x
 *
 * Introduced in version 2.5
 */
static int fsus_access(const char *gpath, int mask)
{
	struct dpr_xlate_data dxd = DXD_INIT;
	char ll_name[PATH_MAX] = "";
	int rv;

	DEBUGe('1') debug_msg(DPR_DATA,
			      LOG_DIVIDER "%s(gpath=\"%s\")\n",
			      __func__, gpath);
	dpr_xlateWholePath(&dxd, DPR_DATA, gpath, true, XWP_DEPTH_MAX,
			   NULL, OBSERVE_ORIGINAL_DIR);
	getLinkedlistName(ll_name, dxd);
	DEBUGe('2') debug_msg(DPR_DATA, "%s(fpath=\"%s\")\n", __func__,
			      ll_name);

	rv = access(ll_name, mask);
	if (rv == -1)
		dpr_error("fsus_access access");

	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s() completes, rv=\"%d\"\n\n", __func__, rv);
	return rv;
}

#if DPRFS_SUPPORT_XATTRS
#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
//
// AM: I have implemented the xattr operations, but I'm left puzzled by
// how they're meant to interact with the underlying filesystem. So,
// fsus_getxattr seems to correctly pass through requests, returning
// -ENODATA on my test system which does have xattrs enabled. But then
// removexattr passes back EPERM which I understand to mean "This type
// of object does not support extended attributes."
// listxattr has not been used at all. The other three have been used
// but create oddities especially with stderr output.
//
// So as of 25oct2017, have disabled them :-)
static int
fsus_setxattr(const char *gpath, const char *name, const char *value,
	      size_t size, int flags)
{
	const char *gpath2 = gpath + DPR_DATA->rootdir_len;
	struct dpr_xlate_data dxd = DXD_INIT;
	char ll_l_ll_file[PATH_MAX] = "";
	char ll_name[PATH_MAX] = "";
	int ierrno;
	int rv;

	DEBUGe('1') debug_msg(DPR_DATA,
			      LOG_DIVIDER "%s(gpath=\"%s\")\n",
			      __func__, gpath2);
	dpr_xlateWholePath(&dxd, DPR_DATA, gpath2, true, XWP_DEPTH_MAX,
			   NULL, OBSERVE_ORIGINAL_DIR);

	// in this situation, a getxattr against a linkedlist should visit the head file,
	// a getxattr against anything else should visit the linkedlist name.
	// An easy way to solve this is to store in dxd whether this is a linkedlist or
	// not, then switch based on that
	if (dxd.dprfs_filetype == DPRFS_FILETYPE_LL) {
		getLinkedlistLatestLinkedlistFile(ll_l_ll_file, dxd);
		rv = lsetxattr(ll_l_ll_file, name, value, size, flags);
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s() ll fpath=\"%s\")\n",
				      __func__, ll_l_ll_file);

	} else {
		getPafForOrdinaryFile(ll_name, dxd);
		if (dxd.deleted == true)
			*ll_name = '\0';
		rv = lsetxattr(ll_l_ll_file, name, value, size, flags);
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s() !ll fpath=\"%s\")\n",
				      __func__, ll_name);
	}

	ierrno = errno;
	if (rv < 0) {
		dpr_level_error('2', "fsus_setxattr lsetxattr");
		if (ierrno == ENODATA) {
			rv = ierrno;
		}
	}
	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s completes, rv=\"%d\"\n\n", __func__, rv);
	return rv;
}

/* Get extended attributes */
// Note that getxattr normally returns -1 and errno on error, however, it can
// also return <-1, and does. Without returning -ENODATA when errno=ENODATA,
// ls will output ls: /var/lib/samba/usershares/gdrive: Operation not permitted
// on stderr.
static int
fsus_getxattr(const char *gpath, const char *name, char *value, size_t size)
{
	const char *gpath2 = gpath + DPR_DATA->rootdir_len;
	struct dpr_xlate_data dxd = DXD_INIT;
	char ll_l_ll_file[PATH_MAX] = "";
	char ll_name[PATH_MAX] = "";
	int ierrno;
	int rv;

	DEBUGe('1') debug_msg(DPR_DATA,
			      LOG_DIVIDER "%s(gpath=\"%s\")\n",
			      __func__, gpath2);
	dpr_xlateWholePath(&dxd, DPR_DATA, gpath2, true, XWP_DEPTH_MAX,
			   NULL, OBSERVE_ORIGINAL_DIR);

	// in this situation, a getxattr against a linkedlist should visit the head file,
	// a getxattr against anything else should visit the linkedlist name.
	// An easy way to solve this is to store in dxd whether this is a linkedlist or
	// not, then switch based on that
	if (dxd.dprfs_filetype == DPRFS_FILETYPE_LL) {
		getLinkedlistLatestLinkedlistFile(ll_l_ll_file, dxd);
		rv = lgetxattr(ll_l_ll_file, name, value, size);
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s() ll fpath=\"%s\")\n",
				      __func__, ll_l_ll_file);

	} else {
		getPafForOrdinaryFile(ll_name, dxd);
		if (dxd.deleted == true)
			*ll_name = '\0';
		rv = lgetxattr(ll_name, name, value, size);
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s() !ll fpath=\"%s\")\n",
				      __func__, ll_name);
	}

	ierrno = errno;
	if (rv < 0) {
		dpr_level_error('2', "fsus_getxattr lgetxattr");
		if (ierrno == ENODATA) {
			rv = ierrno;
		}
	}
	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s completes, rv=\"%d\"\n\n", __func__, rv);
	return rv;
}

/* List extended attributes */
static int fsus_listxattr(const char *gpath, char *list, size_t size)
{
	const char *gpath2 = gpath + DPR_DATA->rootdir_len;
	struct dpr_xlate_data dxd = DXD_INIT;
	char ll_l_ll_file[PATH_MAX] = "";
	char ll_name[PATH_MAX] = "";
	int ierrno;
	int rv;

	DEBUGe('1') debug_msg(DPR_DATA,
			      LOG_DIVIDER "%s(gpath=\"%s\")\n",
			      __func__, gpath2);
	dpr_xlateWholePath(&dxd, DPR_DATA, gpath2, true, XWP_DEPTH_MAX,
			   NULL, OBSERVE_ORIGINAL_DIR);

	// in this situation, a getxattr against a linkedlist should visit the head file,
	// a getxattr against anything else should visit the linkedlist name.
	// An easy way to solve this is to store in dxd whether this is a linkedlist or
	// not, then switch based on that
	if (dxd.dprfs_filetype == DPRFS_FILETYPE_LL) {
		getLinkedlistLatestLinkedlistFile(ll_l_ll_file, dxd);
		rv = llistxattr(ll_l_ll_file, list, size);
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s() ll fpath=\"%s\")\n",
				      __func__, ll_l_ll_file);

	} else {
		getPafForOrdinaryFile(ll_name, dxd);
		if (dxd.deleted == true)
			*ll_name = '\0';
		rv = llistxattr(ll_l_ll_file, list, size);
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s() !ll fpath=\"%s\")\n",
				      __func__, ll_name);
	}

	ierrno = errno;
	if (rv < 0) {
		dpr_level_error('2', "fsus_listxattr llistxattr");
		if (ierrno == ENODATA) {
			rv = ierrno;
		}
	}
	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s completes, rv=\"%d\"\n\n", __func__, rv);
	return rv;
}

/* Remove extended attributes */
static int fsus_removexattr(const char *gpath, const char *name)
{
	const char *gpath2 = gpath + DPR_DATA->rootdir_len;
	struct dpr_xlate_data dxd = DXD_INIT;
	char ll_l_ll_file[PATH_MAX] = "";
	char ll_name[PATH_MAX] = "";
	int ierrno;
	int rv;

	DEBUGe('1') debug_msg(DPR_DATA,
			      LOG_DIVIDER "%s(gpath=\"%s\" name=\"%s\")\n",
			      __func__, gpath2, name);
	dpr_xlateWholePath(&dxd, DPR_DATA, gpath2, true, XWP_DEPTH_MAX,
			   NULL, OBSERVE_ORIGINAL_DIR);

	// in this situation, a getxattr against a linkedlist should visit the head file,
	// a getxattr against anything else should visit the linkedlist name.
	// An easy way to solve this is to store in dxd whether this is a linkedlist or
	// not, then switch based on that
	if (dxd.dprfs_filetype == DPRFS_FILETYPE_LL) {
		getLinkedlistLatestLinkedlistFile(ll_l_ll_file, dxd);
		rv = lremovexattr(ll_l_ll_file, name);
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s() ll fpath=\"%s\")\n",
				      __func__, ll_l_ll_file);

	} else {
		getPafForOrdinaryFile(ll_name, dxd);
		if (dxd.deleted == true)
			*ll_name = '\0';
		rv = lremovexattr(ll_l_ll_file, name);
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s() !ll fpath=\"%s\")\n",
				      __func__, ll_name);
	}

	ierrno = errno;
	if (rv < 0) {
		DEBUGe('1') debug_msg(DPR_DATA,
				      "%s rv=\"%d\"%d\"\n", __func__, rv,
				      ierrno);
		dpr_level_error('2', "fsus_removexattr lremovexattr");
		if (ierrno == ENODATA) {
			rv = ierrno;
		}
		if (ierrno == EPERM) {
			DEBUGe('1') debug_msg(DPR_DATA,
					      "%s EPERM FOUND rv=\"%d\"%d\"\n",
					      __func__, rv, ierrno);
		}
	}
	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s completes, rv=\"%d\"\n\n", __func__, rv);
	return rv;
}
#endif				/* #ifdef HAVE_SETXATTR */
#endif				/* #if DPRFS_SUPPORT_XATTRS */

/////////////////////////////////////
// f* functions (Samba doesnt seem to use)

/* Synchronize directory contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data
 *
 * Introduced in version 2.3
 */

/* Synchronize file contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data.
 *
 * Changed in version 2.2
 */
static int
fsus_fsync(const char *gpath, int datasync, struct fuse_file_info *fi)
{
	int rv;

	DEBUGe('1') debug_msg(DPR_DATA, "%s(gpath=\"%s\")\n", __func__, gpath);

	// some unix-like systems (notably freebsd) don't have a datasync call
#ifdef HAVE_FDATASYNC
	if (datasync)
		rv = fdatasync(ea_shadowFile_getValueOrKey(DPR_DATA, fi));
	else
#endif				/* #ifdef HAVE_FDATASYNC */
		rv = fsync(ea_shadowFile_getValueOrKey(DPR_DATA, fi));

	if (rv == -1)
		dpr_error("fsus_fsync fsync");

	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s completes, rv=\"%d\"\n\n", __func__, rv);
	return rv;
}

/////////////////////////////////////
// misc

/*
 * Create a file node
 *
 * There is no create() operation, mknod() will be called for
 * creation of all non-directory, non-symlink nodes.
 */
// shouldn't that comment be "if" there is no.... ?
static int fsus_mknod(const char *gpath, mode_t mode, dev_t dev)
{
	struct dpr_xlate_data dxd = DXD_INIT;
	char ll_name[PATH_MAX] = "";
	int rv;

	DEBUGe('1') debug_msg(DPR_DATA,
			      LOG_DIVIDER "%s(gpath=\"%s\")\n",
			      __func__, gpath);

	forensicLogChangesComing(DPR_DATA, MKNOD_KEY, gpath);

	dpr_xlateWholePath(&dxd, DPR_DATA, gpath, true, XWP_DEPTH_MAX,
			   NULL, OBSERVE_ORIGINAL_DIR);
	// On Linux this could just be 'mknod(gpath, mode, rdev)' but this
	//  is more portable
	getLinkedlistName(ll_name, dxd);
	DEBUGe('2') debug_msg(DPR_DATA,
			      "%s(fpath=\"%s\")\n", __func__, ll_name);

	if (S_ISREG(mode)) {
		rv = open(ll_name, O_CREAT | O_EXCL | O_WRONLY, mode);
		if (rv == -1) {
			dpr_error("fsus_mknod open");

		} else {
			rv = close(rv);
			if (rv == -1)
				dpr_error("fsus_mknod close");
		}

	} else if (S_ISFIFO(mode)) {
		rv = mkfifo(ll_name, mode);
		if (rv == -1)
			dpr_error("fsus_mknod mkfifo");

	} else {
		rv = mknod(ll_name, mode, dev);
		if (rv == -1)
			dpr_error("fsus_mknod mknod");
	}

	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s() completes, rv=\"%d\"\n\n", __func__, rv);
	return rv;
}

#ifdef HAVE_UTIMENSAT
static int fsus_utimens(const char *gpath, const struct timespec ts[2],
			struct fuse_file_info *fi)
{
	struct dpr_xlate_data dxd = DXD_INIT;
	int rv;

	DEBUGe('1') debug_msg(DPR_DATA,
			      LOG_DIVIDER "%s(gpath=\"%s\")\n",
			      __func__, gpath);

	forensicLogChangesComing(DPR_DATA, UTIME_KEY, gpath);

	dpr_xlateWholePath(&dxd, DPR_DATA, gpath, true, XWP_DEPTH_MAX,
			   NULL, OBSERVE_ORIGINAL_DIR);
	rv = -1;
	if (dxd.dprfs_filetype == DPRFS_FILETYPE_LL)
		rv = fsus_utimens_ll(&dxd, ts);

	else if (dxd.dprfs_filetype == DPRFS_FILETYPE_DS)
		rv = fsus_utimens_ds(&dxd, ts);

	else
		DEBUGe('2') debug_msg(DPR_DATA,
				      "%s() unexpected dxd.dprfs_filetype=\"%d\"\n",
				      __func__, dxd.dprfs_filetype);
	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s completes, rv=\"%d\"\n\n", __func__, rv);
	return rv;
}

static int fsus_utimens_ll(struct dpr_xlate_data *dxd,
			   const struct timespec ts[2])
{
	char paf[PATH_MAX] = "";
	int rv;

	getLinkedlistName(paf, *dxd);
	DEBUGe('2') debug_msg(DPR_DATA, "%s(fpath=\"%s\")\n", __func__, paf);

	rv = utimensat(0, paf, ts, AT_SYMLINK_NOFOLLOW);
	if (rv == -1)
		return -errno;

	return rv;
}

static int fsus_utimens_ds(struct dpr_xlate_data *dxd,
			   const struct timespec ts[2])
{
	char paf[PATH_MAX] = "";
	int rv;

	getPafForOrdinaryFile(paf, *dxd);
	DEBUGe('2') debug_msg(DPR_DATA, "%s(fpath=\"%s\")\n", __func__, paf);

	rv = utimensat(0, paf, ts, AT_SYMLINK_NOFOLLOW);
	if (rv == -1)
		return -errno;

	return rv;
}
#endif				//#ifdef HAVE_UTIMENSAT

static int xmp_read_buf(const char *gpath, struct fuse_bufvec **bufp,
			size_t size, off_t offset, struct fuse_file_info *fi)
{
	struct fuse_bufvec *src;
	(void)gpath;
	int rv;

	DEBUGe('1') debug_msg(DPR_DATA,
			      LOG_DIVIDER "%s(gpath=\"%s\")\n",
			      __func__, gpath);

	src = malloc(sizeof(struct fuse_bufvec));
	if (src == NULL)
		return -ENOMEM;

	*src = FUSE_BUFVEC_INIT(size);

	src->buf[0].flags = FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK;
	src->buf[0].fd = ea_shadowFile_getValueOrKey(DPR_DATA, fi);
	src->buf[0].pos = offset;

	*bufp = src;
	rv = 0;

	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s completes, rv=\"%d\"\n\n", __func__, rv);
	return rv;
}

static int xmp_write_buf(const char *gpath, struct fuse_bufvec *buf,
			 off_t offset, struct fuse_file_info *fi)
{
	struct fuse_bufvec dst = FUSE_BUFVEC_INIT(fuse_buf_size(buf));
	unsigned int filetype;
	int rv;
	(void)gpath;

	DEBUGe('1') debug_msg
	    (DPR_DATA,
	     LOG_DIVIDER "%d %s() entry gpath=\"%s\" fd=\"%" PRIu64
	     "\" offset=\"%lld\"\n", getpid(), __func__, gpath,
	     ea_shadowFile_getValueOrKey(DPR_DATA, fi), offset);

	filetype = ea_filetype_getValueForKey(DPR_DATA, fi);

	DEBUGe('1') debug_msg
	    (DPR_DATA, "%s() filetype \"%d\"\n", __func__, filetype);

	// might need to reload?
	if (filetype == DPRFS_FILETYPE_LL
	    && ea_str_getValueForKey(DPR_DATA, gpath) != NULL) {
		// Mint/dolphin will already have truncated this file, Windows
		// won't.
		// Do what we're told, ie, we won't make value judgements
		// on whether to keep any of successive truncates: this costs
		// us one inode each time it happens

		// conduct the reload
		DEBUGe('2') debug_msg
		    (DPR_DATA, LOG_DIVIDER
		     " reload required for \"%s\")\n", gpath);

		fsus_recreate(gpath, fi);
		fsus_open_shadow(gpath, fi);

		// have reopened a new file so no more need to reload at
		// next write
		ea_str_removeElementByValue(DPR_DATA, gpath);
	}
	// logging after so it's clear when the actual write happens,
	// which is some time after entering the routine given we may
	// need to reload
	forensicLogChangesComing(DPR_DATA, WRITE_KEY, gpath);

	// no need to get fpath on this one, since I work from fi->fh not the gpath
	DEBUGe('3') debug_msg
	    (DPR_DATA, " pwrite fd=\"%" PRIu64 "\"\n",
	     ea_shadowFile_getValueOrKey(DPR_DATA, fi));

	dst.buf[0].flags = FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK;
	dst.buf[0].fd = ea_shadowFile_getValueOrKey(DPR_DATA, fi);
	dst.buf[0].pos = offset;

	rv = fuse_buf_copy(&dst, buf, FUSE_BUF_SPLICE_NONBLOCK);
	if (rv == -1)
		dpr_error("xmp_write_buf pwrite");

	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s() completes, rv=\"%d\"\n\n", __func__, rv);
	return rv;
}

#ifdef HAVE_POSIX_FALLOCATE
static int fsus_fallocate(const char *gpath, int mode,
			  off_t offset, off_t length, struct fuse_file_info *fi)
{
	(void)gpath;
	int rv;

	DEBUGe('1') debug_msg(DPR_DATA,
			      LOG_DIVIDER "%s(gpath=\"%s\")\n",
			      __func__, gpath);

	/* forensicLogChangesComing(DPR_DATA, FALLOCATE_KEY, gpath); */

	if (mode) {
		rv = -EOPNOTSUPP;
	} else {
		rv = -posix_fallocate(ea_shadowFile_getValueOrKey
				      (DPR_DATA, fi), offset, length);
	}

	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s() completes, rv=\"%d\"\n\n", __func__, rv);
	return rv;
}
#endif				/* #ifdef HAVE_POSIX_FALLOCATE */

static int fsus_flock(const char *path, struct fuse_file_info *fi, int op)
{
	(void)path;
	int res;
	int rv;

	DEBUGe('1') debug_msg(DPR_DATA,
			      LOG_DIVIDER "%d %s(gpath=\"%s\")\n", getpid(),
			      __func__, path);

	res = flock(ea_shadowFile_getValueOrKey(DPR_DATA, fi), op);
	rv = 0;
	if (res == -1) {
		rv = -errno;
	}

	DEBUGe('1') debug_msg(DPR_DATA,
			      "%s() completes, rv=\"%d\"\n\n", __func__, rv);
	return rv;
}

/////////////////////////////////////
// filesystem

static struct fuse_operations xmp_oper = {
	/* directories */
	.mkdir = fsus_mkdir,
	.rmdir = fsus_rmdir,
	.opendir = fsus_opendir,
	.readdir = fsus_readdir,
	.releasedir = fsus_releasedir,

	/* files and directories */
	.rename = fsus_rename,

	/* misc */
	.getattr = fsus_getattr,
	.chmod = fsus_chmod,
	.chown = fsus_chown,
	/* Just a placeholder, don't set */// huh???
	.statfs = fsus_statfs,
	.flush = fsus_flush,
	.fsync = fsus_fsync,
	.access = fsus_access,
	.mknod = fsus_mknod,

	/* links */
	.link = fsus_link,
	.symlink = fsus_symlink,
	.unlink = fsus_unlink,
	.readlink = fsus_readlink,

	/* housekeeping */
	/* .init = xmp_init, // dprfs happy to use FUSE's own .init */
#if DPRFS_SUPPORT_XATTRS
#ifdef HAVE_SETXATTR
	.setxattr = fsus_setxattr,
	.getxattr = fsus_getxattr,
	.listxattr = fsus_listxattr,
	.removexattr = fsus_removexattr,
#endif				/* #ifdef HAVE_SETXATTR */
#endif				/* #if DPRFS_SUPPORT_XATTRS */

	/* files */
	.truncate = fsus_truncate,
	.create = fsus_create,
	.open = fsus_open,
	.read = fsus_read,
	.write = fsus_write,
	.release = fsus_release,

	/* dprfs only */
#ifdef HAVE_UTIMENSAT
	.utimens = fsus_utimens,
#endif				/* #ifdef HAVE_UTIMENSAT */
#ifdef HAVE_POSIX_FALLOCATE
	.fallocate = fsus_fallocate,	//?
#endif				/* #ifdef HAVE_POSIX_FALLOCATE */
	.flock = fsus_flock,	//?
	.read_buf = xmp_read_buf,	//?
	.write_buf = xmp_write_buf,	//?
};

// OMGWTFBBQ
// I can hardly believe I did this...
#include "unittests.c"
// OMGWTFBBQ

/*
 * Accept in dprfs options and reconstruct both to dprfs-only
 * options (rdrive, debug level, argc), fuse only options
 * (gdrive, -o, argc) and joint options (program name.)
 * Some sanity checks and report
 */
static int
getCommandLineIntoOptions(struct internal_options *options, int *argc,
			  char *argv[])
{
	int a;
	int b;

	/* dprfs call looks like ... */
	/* ./dprfs_fh /var/lib/samba/usershares/bdrive -o allow_root -o modules=subdir -o subdir=/var/lib/samba/usershares/rdrive */
	/* was looking like */
	/* src/dprfs -d 1 -o allow_root -r /var/lib/samba/usershares/rdrive -g /var/lib/samba/usershares/gdrive */
	/* -d = taken over by FUSE, use -D */
	/* -g = [0] */
	/* -h = taken over by FUSE too */
	/* -o = taken over by FUSE, we use for allow_root */
	/* -r = subdir=[xxx] */
	b = 0;
	for (a = 1; a < *argc; a++) {
		if (strcmp(argv[a], "-D") == 0) {
			b = a;
			options->debuglevel = (char)*argv[a + 1];
			*argv[a] = '\0';
			*argv[a + 1] = '\0';

		} else if (strcmp(argv[a], "-h") == 0) {
			goto usage;

		} else if (strncmp(argv[a], "subdir=", 7) == 0) {
			options->rdrive = (char *)realpath(argv[a] + 7, NULL);
			if (options->rdrive == NULL) {
				fprintf(stderr,
					"Unable to access rdrive: %s\n",
					strerror(errno));
				return -1;
			}
		}
	}
	if (options->rdrive == NULL) {
		fprintf(stderr, "No -r <rdrive>\n");
		goto usage;
	}

	if (b != 0) {
		for (; b < *argc; b++) {
			argv[b] = argv[b + 2];
		}
		*argc -= 2;
	}
	options->rdrive_len = strlen(options->rdrive);

	return 0;
 usage:
	fprintf(stderr,
		"Suggested usage:  dprfs mountPoint -o allow_root modules_subdir -o subdir=rootDir\n");
	return -1;
}

/////////////////////////////////////
// Rolling Stats

/*
 * It may be of use to know how many operations occur over transient periods:
 * 500 deletes in half a second may be worth investigating. Note: we only
 * note what's happened - triggering an action (an email, an alert etc) is
 * not addressed here, and probably shouldn't be.
 *
 * ATM we record just deletes and renames, but this can work for any event.
 * The method is to use modulo arithmetic to divide a timestamp in 3:
 * Unix timestamp 0x57891F93 with a period of 16, and a history of 256 divides
 * into [57891][F9][3].
 *  [0x3] means this is the 3rd of 16 seconds. All 16 seconds will see the
 *        same counter incremented.
 *  [0xF9] means this is the 249th of 256 entries we'll keep. That is, the
 *         previous 248 entries will no longer be updated, and there's another
 *         7 entries we will keep in future before the next change.
 *  [0x57891] means this is the 358,545th change. This is used relatively, so
 *            when the change is made, the 256 current entries are copied into
 *            an historical data set, the original 256 entries will be zeroed,
 *            and the new change - 0x57892 - recorded.
 *  Thus, if we record an operation at the moment of a change, while keeping
 * four entries, we'll have [----][1---]: The historic set is empty, three of
 * the current set are empty because the clock's not advanced yet, and the
 * first of that current set records the operation.
 *  If we then record two operations three periods later we'll end up with
 * [----][1--2].
 *  Assuming a change and three new operations, we'll see the current set
 * moved to the historic set, and the new operations logged: [1--2][3---].
 * Any data in the historic set gets lost. Did you want it? Poll for it.
 * This method means that if you say you want to record four periods, the
 * code can give you a minimum 4 (and a maximum 8) such records along with
 * the timestamp they started - 0x57891000
 */

void rs_initialise(struct dpr_state *dpr_data, struct rolling_stats *rs,
		   unsigned int history_sz, unsigned int period_secs)
{
	unsigned int halfsize_bits;
	unsigned int period_bits;

	/* Simple sanity check */
	if (history_sz < 1)
		history_sz = 1;
	if (period_secs < 1)
		period_secs = 1;

	rs->period = period_secs;

	period_bits = period_secs;
	period_bits--;
	rs->period_bits = 0;
	while (period_bits > 0) {
		period_bits >>= 1;
		rs->period_bits++;
	}

	halfsize_bits = rs->halfsize = history_sz / 2;
	halfsize_bits--;
	rs->halfsize_bits = 0;
	while (halfsize_bits > 0) {
		halfsize_bits >>= 1;
		rs->halfsize_bits++;
	}

	rs->history_sz = rs->halfsize * 2;
	rs->data_p = malloc(sizeof(*rs->data_p) * rs->history_sz);
	rs->change = 0;
}

void rs_free(struct rolling_stats rs)
{
	free(rs.data_p);
}

void rs_clear_all(struct rolling_stats *rs)
{
	int a;
	for (a = 0; a < rs->history_sz; a++)
		rs->data_p[a] = 0;
}

void rs_clear_current(struct rolling_stats *rs)
{
	int a;
	for (a = rs->halfsize; a < rs->history_sz; a++)
		rs->data_p[a] = 0;
}

void rs_inc(struct dpr_state *dpr_data, struct rolling_stats *rs)
{
	time_t secs = time(NULL);

	// The change is the timestamp shifted right such that before each
	// flushing of data to history, the change has incremented by exactly 1
	unsigned long change = secs >> (rs->period_bits + rs->halfsize_bits);

	if (rs->change == change - 1) {
		// As the last change was the one before this change, flush
		// the current data to history. So: [AAA][BBB] -> [BBB][000]
		int a;
		for (a = 0; a < rs->halfsize; a++)
			rs->data_p[a] = rs->data_p[a + rs->halfsize];
		rs_clear_current(rs);
		rs->change = change;

	} else if (rs->change != change) {
		// It's been too long since last change, so blank it
		// all and start again
		rs_clear_all(rs);
		rs->change = change;
	}
	rs->data_p[rs->halfsize +
		   ((secs >> rs->period_bits) & (rs->halfsize - 1))]++;

	char out[RS_DELETE_HISTORY + 1];
	int a;
	for (a = rs->history_sz - 1; a >= 0; a--) {
		if (rs->data_p[a] == 0) {
			out[a] = '-';
		} else {
			out[a] = rs->data_p[a] + '0';
		}
	}
	out[RS_DELETE_HISTORY] = '\0';
	DEBUGi('3') debug_msg(dpr_data, "rolling_stats: %s\n", out);
}

/////////////////////////////////////
// Signal handling

/*
 * Only SIGUSR1 at this point. If dprfs has been keeping rolling stats,
 * then the handler will flush them to file. To start with, dprfs dumps
 * it in the shared memory FS as it's faster than the regular FS and
 * it's how drpfs can share data with userspace.
 * By only dumping on signal, no need for dprfs to do that dumping
 * other than when needed.
 * Users should prefer to use the "historic" data set - the current
 * set is ... current ... and data cannot be considered complete.
 * This might change later by use of semaphores, noting which figures
 * can no longer be changed etc.
 */

static void dump_rs(struct rolling_stats rs, char *filename)
{
	unsigned long change;
	char buffer[100];
	FILE *pFile;
	time_t secs;
	int offset;
	int a;

	secs = time(NULL);
	pFile = fopen(filename, "w");

	// The change is the timestamp shifted right such that before
	// each flushing of data to history, the change has incremented
	// by exactly 1
	change = secs >> (rs.period_bits + rs.halfsize_bits);
	sprintf(buffer, "Now: %lx\n", change);
	fwrite(buffer, sizeof(char), strlen(buffer), pFile);

	// No recent data? Skip
	if (rs.change < change - 1)
		goto no_recent_data;

	// If two changes since event, nothing will have flushed
	// current data to historic. Use offset to handle it.
	offset = 0;
	if (rs.change == change - 1) {
		offset = rs.halfsize;
		sprintf(buffer, "Historic: %lx \n", rs.change);

	} else {
		sprintf(buffer, "Historic: %lx \n", rs.change - 1);
	}

	// show whatever history records
	fwrite(buffer, sizeof(char), strlen(buffer), pFile);
	for (a = offset; a < offset + rs.halfsize - 1; a++) {
		sprintf(buffer, "%lx \n", rs.data_p[a]);
		fwrite(buffer, sizeof(char), strlen(buffer), pFile);
	}

	if (rs.change != change)
		goto no_recent_data;
	// show what current period records
	sprintf(buffer, "Current: %lx \n", rs.change);
	fwrite(buffer, sizeof(char), strlen(buffer), pFile);

	for (a = rs.halfsize; a < rs.history_sz - 1; a++) {
		sprintf(buffer, "%lx \n", rs.data_p[a]);
		fwrite(buffer, sizeof(char), strlen(buffer), pFile);
	}
 no_recent_data:
	fclose(pFile);
}

static void multi_handler(int sig, siginfo_t * siginfo, void *context)
{
	if (sig == SIGUSR1) {
		dump_rs(dpr_data->delstats_p, RS_DELETE_FILE);
		dump_rs(dpr_data->renstats_p, RS_RENAME_FILE);
	}
}

int main(int argc, char *argv[])
{
	struct internal_options options = OPTIONS_INIT;
	int rv;

	umask(0);
	// dprfs doesn't do any access checking on its own (the comment
	// blocks in fuse.h mention some of the functions that need
	// accesses checked -- but note there are other functions, like
	// chown(), that also need checking!).  Since running dprfs as root
	// will therefore open Metrodome-sized holes in the system
	// security, we'll check if root is trying to mount the filesystem
	// and refuse if it is.  The somewhat smaller hole of an ordinary
	// user doing it with the allow_other flag is still there because
	// I don't want to parse the options string.
	if ((getuid() == 0) || (geteuid() == 0)) {
		fprintf(stderr,
			"Running DPRFS as root opens unnacceptable security holes\n");
		return -1;
	}

	dpr_data = malloc(sizeof(*dpr_data));
	if (dpr_data == NULL) {
		perror("main calloc");
		return -1;
	}
	if ((rv = getCommandLineIntoOptions(&options, &argc, argv)) != 0)
		goto options_release;

	dpr_data->debugfile = debug_open();
	dpr_data->forensiclogfile = forensiclog_open();
	dpr_data->rootdir = options.rdrive;
	dpr_data->rootdir_len = options.rdrive_len;
	dpr_data->debuglevel = options.debuglevel;
	ea_str_initialise(dpr_data);
	ea_flarrs_initialise(dpr_data);
	ea_shadowFile_initialise(dpr_data);
	ea_filetype_initialise(dpr_data);
	ea_backup_gpath_initialise(dpr_data);

	DEBUGi('0') debug_msg(dpr_data, "Starting DPRFS\n");

#ifdef RS_DELETE_SUPPORT
	rs_initialise(dpr_data, &dpr_data->delstats_p,
		      RS_DELETE_HISTORY, RS_DELETE_PERIOD);
	rs_clear_all(&dpr_data->delstats_p);
#endif
#ifdef RS_RENAME_SUPPORT
	rs_initialise(dpr_data, &dpr_data->renstats_p,
		      RS_RENAME_HISTORY, RS_RENAME_PERIOD);
	rs_clear_all(&dpr_data->renstats_p);
#endif

	// make the /tmp/dprfs directory
	makeDirectory(TMP_PATH);
	makeBeyondUseFiles(dpr_data->rootdir, BEYOND_USE_RELPATH,
			   BEYOND_USE_README_FILE, "en-GB");
	makeAccessDeniedConstructs(TMP_PATH "/" ACCESS_DENIED_FILE,
				   TMP_PATH "/" ACCESS_DENIED_DIR);

	// SIGUSR1 flushes rolling stats
	siga.sa_sigaction = *multi_handler;
	siga.sa_flags |= SA_SIGINFO;
	if (sigaction(SIGUSR1, &siga, NULL) != 0) {
		printf("error sigaction()");
		rv = errno;
		goto options_release;
	}
#if RUN_AS_UNIT_TESTS
	fprintf(stderr, "starting tests\n");
	if (unittests_main(dpr_data->debugfile) == 0) {
		fprintf(stderr, "===========================\n\n");
		fprintf(stderr, "tests completed okay\n");
	}
#else				/* #if RUN_AS_UNIT_TESTS */
	// turn over control to fuse
	fprintf(stderr, "About to call fsus_main\n");
	fuse_main(argc, argv, &xmp_oper, dpr_data);
	fprintf(stderr, "%s() returning\n", __func__);
#endif				/* #if RUN_AS_UNIT_TESTS #else */

 options_release:
	ea_backup_gpath_release(dpr_data);
	ea_filetype_release(dpr_data);
	ea_shadowFile_release(dpr_data);
	ea_flarrs_release(dpr_data);
	ea_str_release(dpr_data);

#ifdef RS_RENAME_SUPPORT
	rs_free(dpr_data->renstats_p);
#endif
#ifdef RS_DELETE_SUPPORT
	rs_free(dpr_data->delstats_p);
#endif

	free(options.rdrive);
	DEBUGi('0') debug_msg(dpr_data, "Clean shut down of DPRFS\n");
	fclose(dpr_data->debugfile);
	fclose(dpr_data->forensiclogfile);
	free(dpr_data);
	return rv;
}
