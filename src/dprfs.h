/* User functions */
int main(int argc, char *argv[]);
static int getCommandLineIntoOptions(struct internal_options *options,
				     int *argc, char *argv[]);

/* DPRFS Setup/Teardown */
static int dpr_error(char *str);
static int dpr_level_error(const int level, const char *str);
static int makeBeyondUseFiles(const char *rootdir, const char *relpath,
			      const char *filename, const char *i18n);
static int makeAccessDeniedConstructs(const char *paf, const char *dir);

/* Debugging */
static void misc_debugDxd(const struct dpr_state *dpr_data, char debuglevel,
			  struct dpr_xlate_data *dxd, const char *prepend,
			  const char *function_name);

/* Useful utilities */
static void catSha256ToStr(char *dest, const struct dpr_state *dpr_data,
			   const char *cleartext);
static mode_t getModeBodge();
static mode_t getDefaultDirMode();
static int makeDirectory(const char *paf);
static int cp(const char *to, const char *from);
static void getCondensedSystemUTime(char *output);
static void incAndAssignRevision(char *revision, const char *linkTarget);
static int getLinkTarget(char *target, const struct dpr_state *dpr_data,
			 const char *paf);
static void createLLID(struct metadata_single *single,
		       const struct dpr_state *dpr_data, const char *gpath);
static void util_beyonduseUpdateCounters(const int files, const int dirs);
static int unlink_cb(const char *fpath, const struct stat *sb, int typeflag,
		     struct FTW *ftwbuf);
static int rmrf(char *path);
static void addOsxBodgeExtIfNecc(char *paf);

/* Extensible array functions */
/* -- Shadow file descriptor for when we write file other than we opened*/
static void ea_shadowFile_initialise(struct dpr_state *dpr_data);
static void ea_shadowFile_release(struct dpr_state *dpr_data);
static int ea_shadowFile_extend(struct extensible_array_ptrs_to_longlong_arr_singles
				*fl_arr);
static int ea_shadowFile_findNextFreeSlot(struct dpr_state *dpr_data);
static void ea_shadowFile_addElement(struct dpr_state *dpr_data,
				     const unsigned long fd,
				     const unsigned long shadowFile_fd);
static void ea_shadowFile_removeElementByIndex(struct dpr_state *dpr_data,
					       int index);
static void ea_shadowFile_removeElementByValue(struct dpr_state *dpr_data,
					       const unsigned long fd,
					       const unsigned long
					       shadowFile_fd);
static unsigned long ea_shadowFile_getValueOrKey(struct dpr_state *dpr_data,
						 struct fuse_file_info *fi);

/* -- File tracks nature of the particular object opened, ll, dir etc */
static void ea_filetype_initialise(struct dpr_state *dpr_data);
static void ea_filetype_release(struct dpr_state *dpr_data);
static int ea_filetype_extend(struct extensible_array_ptrs_to_longlong_arr_singles
			      *filetype_arr);
static int ea_filetype_findNextFreeSlot(struct dpr_state *dpr_data);
static void ea_filetype_addElement(struct dpr_state *dpr_data,
				   const unsigned long fd,
				   const unsigned long filetype_fd);
static uint64_t ea_filetype_getValueForKey(struct dpr_state *dpr_data,
					   struct fuse_file_info *fi);

/* -- backup gpath for when fuse doesn't provide us original */
/* -- (flush() is only I know) */
static void ea_backup_gpath_initialise(struct dpr_state *dpr_data);
static void ea_backup_gpath_release(struct dpr_state *dpr_data);
static int ea_backup_gpath_extend(struct extensible_array_ptrs_to_backup_gpath_arr_singles
				  *fl_arr);
static int ea_backup_gpath_findNextFreeSlot(struct dpr_state *dpr_data);
static void ea_backup_gpath_addElement(struct dpr_state *dpr_data,
				       const unsigned long fd,
				       const char *backup_gpath);
static char *ea_backup_gpath_getValueForKey(struct dpr_state *dpr_data,
					    struct fuse_file_info *fi);
static void ea_backup_gpath_removeElementByIndex(struct dpr_state *dpr_data,
						 int index);
static void ea_backup_gpath_removeElementByValue(struct dpr_state *dpr_data,
						 const char *paf);

/* -- Forensic log notes for getting all onto one line */
static void ea_flarrs_initialise(struct dpr_state *dpr_data);
static void ea_flarrs_release(struct dpr_state *dpr_data);
static int ea_flarrs_extend(struct extensible_array_ptrs_to_fl_arr_singles
			    *fl_arr);
static int ea_flarrs_findNextFreeSlot(struct dpr_state *dpr_data);
static void ea_flarrs_addElement(struct dpr_state *dpr_data, const char *paf);
static void ea_flarrs_removeElementByIndex(struct dpr_state *dpr_data,
					   int index);
static void ea_flarrs_removeElementByValue(struct dpr_state *dpr_data,
					   const char *paf);
static void forensicLogChangesComing(struct dpr_state *dpr_data,
				     unsigned long index, const char *gpath);
static void forensicLogChangesApplied(struct dpr_state *dpr_data,
				      const char *gpath);

/* -- Potential Reload tracks if we may need to open shadow for file */
static void ea_str_initialise(struct dpr_state *dpr_data);
static void ea_str_release(struct dpr_state *dpr_data);
static int ea_str_extend(struct extensible_array_ptrs_to_string *pr_arr);
static int ea_str_findNextFreeSlot(struct dpr_state *dpr_data);
static void ea_str_addElement(struct dpr_state *dpr_data, const char *paf);
static void ea_str_removeElementByIndex(struct dpr_state *dpr_data, int index);
static void ea_str_removeElementByValue(struct dpr_state *dpr_data,
					const char *paf);
static char *ea_str_getValueForKey(struct dpr_state *dpr_data, const char *paf);

/* dpr_xlate_data functions */
static void dxd_initialiseTimestamp(struct dpr_xlate_data *dxd);
static void dxd_copy(struct dpr_xlate_data *to, struct dpr_xlate_data *from);
static void resetDxd(struct dpr_xlate_data *dxd);
static void accessDeniedDxdFile(struct dpr_state *dpr_data,
				struct dpr_xlate_data *dxd);
static void accessDeniedDxdDir(struct dpr_state *dpr_data,
			       struct dpr_xlate_data *dxd);

/* Mallocable string functions */
static void mstrfree(struct mallocable_string *mstr);
static void mstrreset(struct mallocable_string *mstr);
static void mstrcat(struct mallocable_string *mstr,
		    const struct dpr_state *dpr_data, const char *str);

/* Metadata array functions */
static void *md_malloc(intmax_t * buffer_sz, const char *paf);
static FILE *md_load(char *buffer, intmax_t buffer_sz, char *paf);
static void md_unload(char *buffer);
static void md_reset(struct metadata_array *md_arr);
static int md_getIntoStructure(struct metadata_array *md_arr,
			       const struct dpr_state *dpr_data, char *buffer);
static int saveMetadataToFile(const char *metadata_paf,
			      struct metadata_array *md_arr);
static int saveDMetadataToFile(struct dpr_state *dpr_data,
			       struct dpr_xlate_data dxd,
			       struct metadata_array *md_arr);
static void md_free(struct metadata_multiple *md_mul_p);
static bool md_isPathInChain(struct dpr_state *dpr_data,
			     struct metadata_multiple *md_mul_p,
			     const char *value);
static bool md_addPathToChain(struct metadata_multiple *md_mul_p,
			      const char *value);
static bool md_removePathFromChain(struct metadata_multiple *md_mul_p,
				   const char *value);
static bool md_checkBeyondUse(struct dpr_state *dpr_data,
			      struct metadata_single *md_sin_p);
static int makeAndPopulateNewRevisionTSDir(struct dpr_state *dpr_data,
					   const char *gpath,
					   struct dpr_xlate_data *dxd,
					   struct metadata_array *md_arr,
					   int watdo,
					   unsigned int payload_loc_src);

/* Path and File constructions */
static void getPafForFinalPath(char *paf, struct dpr_xlate_data dxd);
static void getPafForFinalPathWithOsxBodge(char *paf,
					   struct dpr_xlate_data dxd);
static void getPafForRelPath(char *paf, struct dpr_xlate_data dxd);
/* static void getTimestamp(char *paf, struct dpr_xlate_data dxd); */
static void getRevisionTSDir(char *paf, struct dpr_xlate_data dxd);
static void getPafForRootDir(char *paf, struct dpr_xlate_data dxd);
/* static void getPafForRelativeDir(char *paf, struct dpr_xlate_data dxd); */
static void getLinkedlistName(char *paf, struct dpr_xlate_data dxd);
static void getRelLinkedlistName(char *paf, struct dpr_xlate_data dxd);
static void getRelLinkedlistDir(char *paf, struct dpr_xlate_data dxd);
/* static void getLinkedlistTSDir(char *paf, struct dpr_xlate_data dxd); */
static void getLinkedlistRevisionTSDir(char *paf, struct dpr_xlate_data dxd);
static void getLinkedlistRevisionTSFile(char *paf, struct dpr_xlate_data dxd);
static void getRelLinkedlistRevisionTSFile(char *paf,
					   struct dpr_xlate_data dxd);
static void getLinkedlistLatestLnk(char *paf, struct dpr_xlate_data dxd);
static void getLinkedlistLatestFMetadataLnk(char *paf,
					    struct dpr_xlate_data dxd);
static void getLinkedlistLatestFMetadataLnkTS(char *paf,
					      struct dpr_xlate_data dxd);
static void getFMetadataTSFile(char *paf, struct dpr_xlate_data dxd);
static void getLinkedlistDMetadataLnk(char *paf, struct dpr_xlate_data dxd);
static void getDMetadataTSFile(char *paf, struct dpr_xlate_data dxd);
static void getLinkedlistDMetadataTSFile(char *paf, struct dpr_xlate_data dxd);
static void getLinkedlistLatestLinkedlistFile(char *paf,
					      struct dpr_xlate_data dxd);
static int getLinkedlistRevtsLinkedlistFile(char *paf,
					    const struct dpr_state *dpr_data,
					    struct dpr_xlate_data dxd);

/* Fuse entry points, plus immediate children */
static int fsus_link(const char *gpath, const char *newpath);
static int fsus_symlink(const char *gpath, const char *link);
static int fsus_readlink(const char *gpath, char *link, size_t size);
static int fsus_unlink(const char *gpath);
static int fsus_unlink_ds(struct dpr_xlate_data *dxd);
static int fsus_unlink_ll(const char *gpath, struct dpr_xlate_data *dxd);
static int fsus_getattr(const char *gpath, struct stat *statbuf,
                        struct fuse_file_info *fi);
static int fsus_chmod(const char *gpath, mode_t mode,
                        struct fuse_file_info *fi);
static int fsus_chmod_ds(struct dpr_xlate_data *dxd, mode_t mode);
static int fsus_chmod_ll(struct dpr_xlate_data *dxd, mode_t mode);
static int fsus_chown(const char *gpath, uid_t uid, gid_t gid,
                        struct fuse_file_info *fi);
#if HAVE_UTIMENSAT
static int fsus_utimens(const char *gpath, const struct timespec ts[2],
                        struct fuse_file_info *fi);
static int fsus_utimens_ds(struct dpr_xlate_data *dxd,
			   const struct timespec ts[2]);
static int fsus_utimens_ll(struct dpr_xlate_data *dxd,
			   const struct timespec ts[2]);
#endif
static int fsus_statfs(const char *gpath, struct statvfs *statv);
static int fsus_flush(const char *gpath, struct fuse_file_info *fi);
static int fsus_fsync(const char *gpath, int datasync,
		      struct fuse_file_info *fi);
static int fsus_access(const char *gpath, int mask);
static int fsus_mknod(const char *gpath, mode_t mode, dev_t dev);
#ifdef HAVE_SETXATTR
static int fsus_setxattr(const char *gpath, const char *name, const char *value,
			 size_t size, int flags);
static int fsus_getxattr(const char *gpath, const char *name, char *value,
			 size_t size);
static int fsus_listxattr(const char *gpath, char *list, size_t size);
static int fsus_removexattr(const char *gpath, const char *name);
#endif
static int fsus_recreate(const char *gpath);
static int fsus_truncate(const char *gpath, off_t newsize,
                        struct fuse_file_info *fi);
static int fsus_truncate_core(const char *gpath, off_t newsize, bool reloading);
static int fsus_truncate_core_ds(struct dpr_xlate_data *dxd, off_t newsize);
static int fsus_truncate_core_ll(const char *gpath,
				 struct dpr_xlate_data *dxdfrom, off_t newsize);
static int fsus_open_shadow(const char *gpath, struct fuse_file_info *fi);
static int fsus_open(const char *gpath, struct fuse_file_info *fi);
static int fsus_open_core(const char *gpath, struct fuse_file_info *fi,
			  bool useShadowFH);
static int fsus_read(const char *gpath, char *buf, size_t size, off_t offset,
		     struct fuse_file_info *fi);
static int fsus_write(const char *gpath, const char *buf, size_t size,
		      off_t offset, struct fuse_file_info *fi);
static int fsus_release(const char *gpath, struct fuse_file_info *fi);
/* static int fsus_release_core(const char *gpath, struct fuse_file_info *fi); */
static void backupDatastore(const char *gpath);
static int fsus_create(const char *gpath, mode_t mode,
		       struct fuse_file_info *fi);
static int fsus_create_with_metadata(const char *gpath, mode_t mode,
				     struct fuse_file_info *fi,
				     struct metadata_array *md_arr,
				     unsigned int payload_loc_src);
static int fsus_create_core(const char *gpath, mode_t mode,
			    struct fuse_file_info *fi, bool create_file_itself,
			    struct metadata_array *md_arr,
			    unsigned int payload_loc_src);
static int fsus_create_core_ds(struct dpr_xlate_data *dxd, mode_t mode,
			       struct fuse_file_info *fi);
static int fsus_create_core_ll(struct dpr_xlate_data *dxd, mode_t mode,
			       struct fuse_file_info *fi);
static int fsus_mkdir(const char *gpath, mode_t mode);
static int fsus_mkdir_with_metadata(struct dpr_state *dpr_data,
				    const char *gpath, mode_t mode,
				    struct metadata_array *md_arr,
				    int whatDoWithOriginalDir);
static int fsus_mkdir_core(struct dpr_state *dpr_data, const char *gpath,
			   mode_t mode, struct metadata_array *md_arr,
			   int whatDoWithOriginalDir);
static int fsus_rmdir(const char *gpath);
static int fsus_opendir(const char *gpath, struct fuse_file_info *fi);
static int fsus_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			off_t offset, struct fuse_file_info *fi,
			enum fuse_readdir_flags flags);
static int fsus_releasedir(const char *gpath, struct fuse_file_info *fi);
/* static int fsus_fsyncdir(const char *gpath, int datasync, */
/*			 struct fuse_file_info *fi); */
static int fsus_rename(const char *oldpath, const char *newpath,
		       unsigned int flags);
static int fsus_rename_core(const char *oldpath, const char *newpath,
			    unsigned int flags, int whatDoWithOriginalDir);
static int fsus_rename_dir(struct dpr_state *dpr_data,
			   struct dpr_xlate_data *dxdto,
			   struct dpr_xlate_data *dxdfrom, const char *newpath,
			   const char *oldpath, int whatDoWithOriginalDir);
static int fsus_rename_ll(struct dpr_state *dpr_data,
			  struct dpr_xlate_data *dxdto,
			  struct dpr_xlate_data *dxdfrom_prv,
			  struct dpr_xlate_data *dxdfrom, const char *oldpath,
			  const char *newpath, bool isPartFile);

/* Would add to filesystem as extensions */
static int dprfs_beyonduse(struct dpr_state *dpr_data, const char *path);
static int dprfs_beyonduse_ll(struct dpr_state *dpr_data, const char *gpath,
			      struct dpr_xlate_data dxd);
static int dprfs_beyonduse_dir(struct dpr_state *dpr_data,
			       struct dpr_xlate_data dxd);

/* Filesystem traversal */
static int dpr_xlateWholePath_whatIsThis(const struct dpr_state *dpr_data,
					 struct dpr_xlate_data *dxd,
					 const char *lhs, const char *rhs,
					 bool ignoreState);
static void dpr_xlateWholePath(struct dpr_xlate_data *dxd,
			       struct dpr_state *dpr_data, const char *in_gpath,
			       bool ignoreState, int depth, char *original_paf,
			       int whatDoWithOriginalDir);
static void dpr_cleanedXlateWholePath(struct dpr_xlate_data *dxd,
				      struct dpr_state *dpr_data,
				      const char *in_gpath, bool ignoreState,
				      int depth, char *original_paf,
				      int whatDoWithOriginalDir);
/* rolling stats */
void rs_initialise(struct dpr_state *dpr_data, struct rolling_stats *rs,
		   unsigned int history_sz, unsigned int period_secs);
void rs_free(struct rolling_stats rs);
void rs_clear_all(struct rolling_stats *rs);
void rs_clear_current(struct rolling_stats *rs);
void rs_inc(struct dpr_state *dpr_data, struct rolling_stats *rs);
