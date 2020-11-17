#include <iostream>
#include <iomanip>
#include <chrono>
#include <cassert>

#include <errno.h>

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
		std::cerr << "operation failed at " << __FILE__ << ":" << __LINE__ << " -> " \
			<< status << " " << strerror(status) << std::endl; \
		return EXIT_FAILURE; \
	} \
} while (0);

#ifndef MAP_HUGE_2MB
#define MAP_HUGE_SHIFT 26
#define MAP_HUGE_2MB (21 << MAP_HUGE_SHIFT)
#endif

static void* alloc_buffer(daos_size_t* size)
{
	constexpr int map_flags = MAP_PRIVATE | MAP_ANONYMOUS;
	constexpr int prot = PROT_READ | PROT_WRITE;

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


static int transfer(dfs_t *dfs, dfs_obj_t *src, int dst, int &us)
{
	daos_size_t read_size, buf_size;
	void* buffer = alloc_buffer(&buf_size);
	if (!buffer) {
		std::cerr << "could not allocate memory: " << strerror(errno) << std::endl;
		return EXIT_FAILURE;
	}

	daos_off_t offset = 0;
	d_iov_t iov = { buffer, buf_size, buf_size };
	d_sg_list_t sgl = { 1, 0, &iov };

	std::chrono::high_resolution_clock::time_point start, stop;
	start = std::chrono::high_resolution_clock::now();
	do {
		sgl.sg_nr_out = 0;
		read_size = buf_size;
		CHECK_SUCCESS(dfs_read(dfs, src, &sgl, offset, &read_size, NULL));
		write(dst, buffer, read_size);
		offset += read_size;
	} while (sgl.sg_nr_out > 0 && read_size == buf_size);

	stop = std::chrono::high_resolution_clock::now();
	us = std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count();

	munmap(buffer, buf_size);

	return EXIT_SUCCESS;
}


int main(int argc, char** argv)
{
	assert(getenv("DAOS_POOL") != NULL);
	assert(getenv("DAOS_CONT") != NULL);
	assert(getenv("DAOS_GROUP") != NULL);
	assert(getenv("DAOS_SVCL") != NULL);

	uuid_t p_uuid, c_uuid;
	CHECK_SUCCESS(uuid_parse(getenv("DAOS_POOL"), p_uuid));
	CHECK_SUCCESS(uuid_parse(getenv("DAOS_CONT"), c_uuid));

	/* open output */
	int target_fd = STDOUT_FILENO;
	if (argc > 2) {
		target_fd = open(argv[2], O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
		if (target_fd == -1) {
			perror("could not open destination");
			return EXIT_FAILURE;
		}
	}

	CHECK_SUCCESS(daos_init());

	d_rank_list_t *svc_ranks = daos_rank_list_parse(getenv("DAOS_SVCL"), ",");

	/* connect to pool, open (posix) container, and "mount" it */
	daos_handle_t pool_handle;
	const char *grp = getenv("DAOS_GROUP");
	CHECK_SUCCESS(daos_pool_connect(p_uuid, grp, svc_ranks, DAOS_PC_RO, &pool_handle, NULL, NULL));

	dfs_t *dfs;
	dfs_obj_t *f;
	daos_handle_t cont_handle;
	CHECK_SUCCESS(daos_cont_open(pool_handle, c_uuid, DAOS_COO_RO, &cont_handle, NULL, NULL));
	CHECK_SUCCESS(dfs_mount(pool_handle, cont_handle, O_RDONLY, &dfs));

	/* open input */
	char* path = argv[1];
	mode_t mode;
	struct stat sb;
	CHECK_SUCCESS(dfs_lookup(dfs, path, O_RDONLY, &f, &mode, &sb));
	if (!S_ISREG(sb.st_mode)) {
		std::cerr << "Error: not a file '" << path << "'" << std::endl;
		daos_fini();
		return EXIT_FAILURE;
	}

	int us;
	if (transfer(dfs, f, target_fd, us) == 0) {
		std::cerr
			<< sb.st_size << " bytes, "
			<< std::fixed << std::setprecision(0)
			<< us / 1000.0 << " ms, "
			<< std::showpoint << std::setprecision(1)
			<< (sb.st_size / (1024.0 * 1024.0)) / (us / (1000.0 * 1000.0)) << " MB/s" << std::endl;
	} else {
		std::cerr << "data transfer failed" << std::endl;
	}

	if (target_fd != STDOUT_FILENO) {
		close(target_fd);
	}

	CHECK_SUCCESS(dfs_release(f));
	CHECK_SUCCESS(dfs_umount(dfs));

	CHECK_SUCCESS(daos_cont_close(cont_handle, NULL))
	CHECK_SUCCESS(daos_pool_disconnect(pool_handle, NULL));
	d_rank_list_free(svc_ranks);

	daos_fini();

	return EXIT_SUCCESS;
}
