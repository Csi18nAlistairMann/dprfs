/*------------------------------------------------------
 * Unit tests
 */
#if RUN_AS_UNIT_TESTS

int ut_001()
{
	char label[] = "Test can handle no path";
	char path[] = "";
	bool ignoreState = true;
	int depth = XWP_DEPTH_MAX;
	struct dpr_xlate_data dxd = DXD_INIT;
	struct dpr_state dpr_data = STATE_INIT;

	dpr_data.rootdir = malloc(strlen(R_PATH) + 1);
	strcpy(dpr_data.rootdir, R_PATH);
	dpr_xlateWholePath(&dxd, &dpr_data, path, ignoreState, depth, path);
	free(dpr_data.rootdir);

	return ut_compare_dxds(label, &dxd, "", "", "");
}

int ut_002()
{
	char label[] = "Test can handle \"/\"";
	char path[] = "/";
	bool ignoreState = true;
	int depth = XWP_DEPTH_MAX;
	struct dpr_xlate_data dxd = DXD_INIT;
	struct dpr_state dpr_data = STATE_INIT;

	dpr_data.rootdir = malloc(strlen(R_PATH) + 1);
	strcpy(dpr_data.rootdir, R_PATH);
	dpr_xlateWholePath(&dxd, &dpr_data, path, ignoreState, depth, path);
	free(dpr_data.rootdir);

	return ut_compare_dxds(label, &dxd, R_PATH, "/", "");
}

int ut_003()
{
	char label[] = "Test can handle \"/path\"";
	char path[] = "/path";
	bool ignoreState = true;
	int depth = XWP_DEPTH_MAX;
	struct dpr_xlate_data dxd = DXD_INIT;
	struct dpr_state dpr_data = STATE_INIT;

	dpr_data.rootdir = malloc(strlen(R_PATH) + 1);
	strcpy(dpr_data.rootdir, R_PATH);
	dpr_xlateWholePath(&dxd, &dpr_data, path, ignoreState, depth, path);
	free(dpr_data.rootdir);

	return ut_compare_dxds(label, &dxd, R_PATH, "/", "path");
}

int ut_004()
{
	char label[] = "Test can handle \"/path/\"";
	char path[] = "/path/";
	bool ignoreState = true;
	int depth = XWP_DEPTH_MAX;
	struct dpr_xlate_data dxd = DXD_INIT;
	struct dpr_state dpr_data = STATE_INIT;

	dpr_data.rootdir = malloc(strlen(R_PATH) + 1);
	strcpy(dpr_data.rootdir, R_PATH);
	dpr_xlateWholePath(&dxd, &dpr_data, path, ignoreState, depth, path);
	free(dpr_data.rootdir);

	return ut_compare_dxds(label, &dxd, R_PATH, "/", "path/");
}

int ut_005()
{
	char label[] = "Test can handle \"/path/to\"";
	char path[] = "/path/to";
	bool ignoreState = true;
	int depth = XWP_DEPTH_MAX;
	struct dpr_xlate_data dxd = DXD_INIT;
	struct dpr_state dpr_data = STATE_INIT;

	dpr_data.rootdir = malloc(strlen(R_PATH) + 1);
	strcpy(dpr_data.rootdir, R_PATH);
	dpr_xlateWholePath(&dxd, &dpr_data, path, ignoreState, depth, path);
	free(dpr_data.rootdir);

	return ut_compare_dxds(label, &dxd, R_PATH, "/path/", "to");
}

int ut_006()
{
	char label[] = "Test can handle \"/longer/path/to\"";
	char path[] = "/longer/path/to";
	bool ignoreState = true;
	int depth = XWP_DEPTH_MAX;
	struct dpr_xlate_data dxd = DXD_INIT;
	struct dpr_state dpr_data = STATE_INIT;

	dpr_data.rootdir = malloc(strlen(R_PATH) + 1);
	strcpy(dpr_data.rootdir, R_PATH);
	dpr_xlateWholePath(&dxd, &dpr_data, path, ignoreState, depth, path);
	free(dpr_data.rootdir);

	return ut_compare_dxds(label, &dxd, R_PATH, "/longer/path/", "to");
}

int ut_007()
{
	char label[] =
	    "Test can handle \"/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/t/u/v/w/x/y/z\"";
	char path[] = "/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/t/u/v/w/x/y/z";
	bool ignoreState = true;
	int depth = XWP_DEPTH_MAX;
	struct dpr_xlate_data dxd = DXD_INIT;
	struct dpr_state dpr_data = STATE_INIT;

	dpr_data.rootdir = malloc(strlen(R_PATH) + 1);
	strcpy(dpr_data.rootdir, R_PATH);
	dpr_xlateWholePath(&dxd, &dpr_data, path, ignoreState, depth, path);
	free(dpr_data.rootdir);

	return ut_compare_dxds(label, &dxd, R_PATH,
			       "/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/t/u/v/w/x/y/",
			       "z");
}

int ut_008()
{
	char label[] = "Test can handle \"/path/to/\"";
	char path[] = "/path/to/";	//extra stroke over ut_005
	bool ignoreState = true;
	int depth = XWP_DEPTH_MAX;
	struct dpr_xlate_data dxd = DXD_INIT;
	struct dpr_state dpr_data = STATE_INIT;

	dpr_data.rootdir = malloc(strlen(R_PATH) + 1);
	strcpy(dpr_data.rootdir, R_PATH);
	dpr_xlateWholePath(&dxd, &dpr_data, path, ignoreState, depth, path);
	free(dpr_data.rootdir);

	return ut_compare_dxds(label, &dxd, R_PATH, "/path/", "to/");
}

int ut_009()
{
	char label[] = "Test can handle depth limit of 3";
	char path[] = "/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/t/u/v/w/x/y/";
	bool ignoreState = true;
	int depth = XWP_DEPTH_MAX;
	struct dpr_xlate_data dxd = DXD_INIT;
	struct dpr_state dpr_data = STATE_INIT;

	dpr_data.rootdir = malloc(strlen(R_PATH) + 1);
	strcpy(dpr_data.rootdir, R_PATH);
	dpr_xlateWholePath(&dxd, &dpr_data, path, ignoreState, 3, "");
	free(dpr_data.rootdir);

	return ut_compare_dxds(label, &dxd, "", "", "");
}

int ut_010()
{
	char label[] = "Test can handle \"/path\" even if it doesn't exist";
	char path[] = "/path";
	bool ignoreState = false;	// note false, ut_003 is true
	int depth = XWP_DEPTH_MAX;
	struct dpr_xlate_data dxd = DXD_INIT;
	struct dpr_state dpr_data = STATE_INIT;

	dpr_data.rootdir = malloc(strlen(R_PATH) + 1);
	strcpy(dpr_data.rootdir, R_PATH);
	dpr_xlateWholePath(&dxd, &dpr_data, path, ignoreState, depth, path);
	free(dpr_data.rootdir);

	return ut_compare_dxds(label, &dxd, "", "", "");
}

int ut_011()
{
	char label[] = "Test can handle wildcard \"/tmp/*\"";
	char path[] = "/tmp/*";
	bool ignoreState = true;
	int depth = XWP_DEPTH_MAX;
	struct dpr_xlate_data dxd = DXD_INIT;
	struct dpr_state dpr_data = STATE_INIT;

	dpr_data.rootdir = malloc(strlen(R_PATH) + 1);
	strcpy(dpr_data.rootdir, R_PATH);
	dpr_xlateWholePath(&dxd, &dpr_data, path, ignoreState, depth, path);
	free(dpr_data.rootdir);

	return ut_compare_dxds(label, &dxd, R_PATH, "/tmp/", "*");
}

int ut_012()
{
	char label[] = "Test can handle normal file \"/test/testme.test.file\"";
	char path[] = "/test";
	char file[] = "/testme.test.file";
	bool ignoreState = false;
	int depth = XWP_DEPTH_MAX;
	struct dpr_xlate_data dxd = DXD_INIT;
	struct dpr_state dpr_data = STATE_INIT;

	char paf[PATH_MAX] = "";
	strcpy(paf, path);
	strcat(paf, file);
	char Paf[PATH_MAX] = "";
	strcpy(Paf, R_PATH);
	strcat(Paf, path);
	char paF[PATH_MAX] = "";
	strcpy(paF, R_PATH);
	strcat(paF, path);
	strcat(paF, file);
	int fh = mkdir(Paf, getModeBodge());
	forensiclog_msg("Created path %s, got response %d\n", Paf, fh);
	fh = creat(paF, getModeBodge());
	forensiclog_msg("Created file %s, got response %d\n", paF, fh);

	dpr_data.rootdir = malloc(strlen(R_PATH) + 1);
	strcpy(dpr_data.rootdir, R_PATH);
	dpr_xlateWholePath(&dxd, &dpr_data, paf, ignoreState, depth, paf);

	free(dpr_data.rootdir);
	unlink(paF);
	rmdir(Paf);

	return ut_compare_dxds(label, &dxd, R_PATH, "/test/",
			       "testme.test.file");
}

int ut_013()
{
	char label[] = "Test can handle deleted linkedlist";
	char filename[] = "linkedlist.test";
	char timestamp[] = "0123456789012346789";
	char metadata[] = "abc\nnothing = here\ndeleted = true\nlastline\n";

	char gpath[PATH_MAX];
	strcpy(gpath, "/");
	strcat(gpath, filename);
	char linkedlist_paf[PATH_MAX];
	strcpy(linkedlist_paf, R_PATH);
	strcat(linkedlist_paf, gpath);
	char timestamp_paf[PATH_MAX];
	strcpy(timestamp_paf, linkedlist_paf);
	strcat(timestamp_paf, "/");
	strcat(timestamp_paf, timestamp);
	char latest_paf[PATH_MAX];
	strcpy(latest_paf, linkedlist_paf);
	strcat(latest_paf, "/");
	strcat(latest_paf, LATEST_FILENAME);
	char head_paf[PATH_MAX];
	strcpy(head_paf, latest_paf), strcat(head_paf, "/");
	strcat(head_paf, filename);
	char fmetadata_paf[PATH_MAX];
	strcpy(fmetadata_paf, latest_paf), strcat(fmetadata_paf, "/");
	strcat(fmetadata_paf, FMETADATA_FILENAME);

	int rv = mkdir(linkedlist_paf, getModeBodge());
	forensiclog_msg("Created path %s, got response %d\n", linkedlist_paf,
			rv);
	rv = mkdir(timestamp_paf, getModeBodge());
	forensiclog_msg("Created path %s, got response %d\n", timestamp_paf,
			rv);
	rv = symlink(timestamp, latest_paf);
	FILE *fh = fopen(head_paf, "w");
	fwrite(label, strlen(label), 1, fh);
	fclose(fh);
	forensiclog_msg("Created file %s, got response %d\n", head_paf, fh);
	fh = fopen(fmetadata_paf, "w");
	fwrite(metadata, strlen(metadata), 1, fh);
	fclose(fh);
	forensiclog_msg("Created file %s, got response %d\n", fmetadata_paf,
			fh);

	bool ignoreState = false;
	int depth = XWP_DEPTH_MAX;
	struct dpr_xlate_data dxd = DXD_INIT;
	struct dpr_state dpr_data = STATE_INIT;

	dpr_data.rootdir = malloc(strlen(R_PATH) + 1);
	strcpy(dpr_data.rootdir, R_PATH);

	dpr_xlateWholePath(&dxd, &dpr_data, gpath, ignoreState, depth, gpath);

	free(dpr_data.rootdir);
	unlink(fmetadata_paf);
	unlink(head_paf);
	unlink(latest_paf);
	rmdir(timestamp_paf);
	rmdir(linkedlist_paf);

	if (dxd.deleted == true)
		return 0;
	else
		return -1;
}

int ut_017()
{
	char label[] = "Test can handle deleted directory";
	char dirname[] = "directory.test";
	char dmetadata[] = "abc\nnothing = here\ndeleted = true\nlastline\n";

	char gpath[PATH_MAX];
	strcpy(gpath, "/");
	strcat(gpath, dirname);
	char dirname_paf[PATH_MAX];
	strcpy(dirname_paf, R_PATH);
	strcat(dirname_paf, gpath);
	char dmetadata_paf[PATH_MAX];
	strcpy(dmetadata_paf, dirname_paf), strcat(dmetadata_paf, "/");
	strcat(dmetadata_paf, DMETADATA_FILENAME);

	int rv = mkdir(dirname_paf, getModeBodge());
	forensiclog_msg("Created path %s, got response %d\n", dirname_paf, rv);
	FILE *fh = fopen(dmetadata_paf, "w");
	fwrite(dmetadata, strlen(dmetadata), 1, fh);
	fclose(fh);
	forensiclog_msg("Created file %s, got response %d\n", dmetadata_paf,
			fh);

	bool ignoreState = false;
	int depth = XWP_DEPTH_MAX;
	struct dpr_xlate_data dxd = DXD_INIT;
	struct dpr_state dpr_data = STATE_INIT;

	dpr_data.rootdir = malloc(strlen(R_PATH) + 1);
	strcpy(dpr_data.rootdir, R_PATH);
	dpr_xlateWholePath(&dxd, &dpr_data, gpath, ignoreState, depth, gpath);

	free(dpr_data.rootdir);
	unlink(dmetadata_paf);
	rmdir(dirname_paf);

	if (dxd.deleted == true)
		return 0;
	else
		return -1;
}

int ut_021()
{
	char label[] = "Test can handle \"/longer/path/to.part\"";
	char path[] = "/longer/path/to.part";
	bool ignoreState = true;
	int depth = XWP_DEPTH_MAX;
	struct dpr_xlate_data dxd = DXD_INIT;
	struct dpr_state dpr_data = STATE_INIT;

	dpr_data.rootdir = malloc(strlen(TMP_PATH) + 1);
	strcpy(dpr_data.rootdir, TMP_PATH);
	dpr_xlateWholePath(&dxd, &dpr_data, path, ignoreState, depth, path);
	free(dpr_data.rootdir);

	return ut_compare_dxds(label, &dxd, TMP_PATH, "/longer/path/",
			       "to.part");
}

/* ---------------------------------------------------------------*/

int ut_compare_dxds(const char *label, struct dpr_xlate_data *dxd,
		    const char *rootdir, const char *relpath,
		    const char *finalpath)
{
	int rv = 0;
	fprintf(stderr, "[%s]", label);
	if (strcmp(dxd->rootdir, rootdir) != 0) {
		fprintf(stderr, "\nRootdir expected \"%s\" and got \"%s\"\n",
			rootdir, dxd->rootdir);
		rv = -1;
	}
	if (strcmp(dxd->relpath, relpath) != 0) {
		fprintf(stderr, "\nRelpath expected \"%s\" and got \"%s\"\n",
			relpath, dxd->relpath);
		rv = -1;
	}
	if (strcmp(dxd->finalpath, finalpath) != 0) {
		fprintf(stderr, "\nFinalpath expected \"%s\" and got \"%s\"\n",
			finalpath, dxd->finalpath);
		rv = -1;
	}
	if (rv == 0)
		fprintf(stderr, " OK\n===========================\n");
	else
		fprintf(stderr, " ERROR\n===========================\n");
	return rv;
}

int unittests_main()
{
	int rv = 0;

	rv |= ut_001();
	rv |= ut_002();
	rv |= ut_003();
	rv |= ut_004();
	rv |= ut_005();
	rv |= ut_006();
	rv |= ut_007();
	rv |= ut_008();
	rv |= ut_009();
	rv |= ut_010();
	rv |= ut_011();
	rv |= ut_012();
	rv |= ut_013();
	rv |= ut_017();
	rv |= ut_021();

	if (rv == 0)
		fprintf(stderr, " OK\n===========================\n");
	else
		fprintf(stderr, " ERROR\n===========================\n");
	return rv;
}

#endif
