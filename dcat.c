//#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <libgen.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "daos.h"
#include "daos_fs.h"
#include "gurt/common.h"

#define CHECK_SUCCESS(x) \
do { \
	int status = (x); \
	if (status != 0) { \
		fprintf(stderr, "operation failed at %s:%d: %d\n", __FILE__, __LINE__, status); \
		if (status < sys_nerr) { \
			fprintf(stderr, "%s\n", sys_errlist[status]); \
		} \
		return EXIT_FAILURE; \
	} \
} while (0);

#ifndef MAP_HUGE_2MB
#define MAP_HUGE_SHIFT 26
#define MAP_HUGE_2MB (21 << MAP_HUGE_SHIFT)
#endif

void* alloc_buffer(daos_size_t* size)
{
	const int map_flags = MAP_PRIVATE | MAP_ANONYMOUS;
	const int prot = PROT_READ | PROT_WRITE;

	/* try huge page (2MB) first */
	*size = 1 << 21;
	void *retval = mmap(NULL, *size, prot, map_flags | MAP_HUGETLB | MAP_HUGE_2MB, -1, 0);
	if (retval != MAP_FAILED) {
		return retval;
	}

	retval= mmap(NULL, *size, prot, map_flags, -1, 0);
	if (retval != MAP_FAILED) {
		return retval;
	}

	*size = 0;
	return NULL;
}

int main(int argc, char** argv)
{
	assert(getenv("DAOS_POOL") != NULL);
	assert(getenv("DAOS_GROUP") != NULL);
	assert(getenv("DAOS_SVCL") != NULL);

	uuid_t p_uuid, c_uuid;
	CHECK_SUCCESS(uuid_parse(getenv("DAOS_POOL"), p_uuid));
	CHECK_SUCCESS(uuid_parse(getenv("DAOS_CONT"), c_uuid));

	CHECK_SUCCESS(daos_init());

	d_rank_list_t *svc_ranks = daos_rank_list_parse(getenv("DAOS_SVCL"), ",");

	daos_handle_t pool_handle;
	const char *grp = getenv("DAOS_GROUP");
	CHECK_SUCCESS(daos_pool_connect(p_uuid, grp, svc_ranks, DAOS_PC_RW, &pool_handle, NULL, NULL));

	dfs_t *dfs;
	dfs_obj_t *f;
	daos_handle_t cont_handle;
	CHECK_SUCCESS(daos_cont_open(pool_handle, c_uuid, DAOS_COO_RW, &cont_handle, NULL, NULL));

	CHECK_SUCCESS(dfs_mount(pool_handle, cont_handle, O_RDONLY, &dfs));

	char* path = argv[1];
	fprintf(stderr, "opening: %s\n", path);

	mode_t mode;
	struct stat sb;
	CHECK_SUCCESS(dfs_lookup(dfs, path, O_RDONLY, &f, &mode, &sb));
	fprintf(stderr, "%zu bytes in file (stat)\n", sb.st_size);

	daos_size_t read_size, buf_size;
	char* buffer = alloc_buffer(&buf_size);
	daos_off_t offset = 0;
	d_iov_t iov = { buffer, buf_size, buf_size };
	d_sg_list_t sgl = { 1, 0, &iov };

	do {
		sgl.sg_nr_out = 0;
		read_size = buf_size;
		CHECK_SUCCESS(dfs_read(dfs, f, &sgl, offset, &read_size, NULL));
		write(STDOUT_FILENO, buffer, read_size);
		offset += read_size;
	} while (sgl.sg_nr_out > 0 && read_size == buf_size);

	CHECK_SUCCESS(dfs_release(f));
	CHECK_SUCCESS(dfs_umount(dfs));

	CHECK_SUCCESS(daos_cont_close(cont_handle, NULL))
	CHECK_SUCCESS(daos_pool_disconnect(pool_handle, NULL));
	d_rank_list_free(svc_ranks);

	daos_fini();

	return EXIT_SUCCESS;
}
