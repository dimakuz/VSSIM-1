#include "ssd.h"
#include "common.h"
#include <assert.h>

#define RUN_TEST(SETUP, FUNC)   if(SETUP) popen("rm data/*.dat", "r"); \
						        SSD_INIT(); \
 								FUNC; \
								if(SETUP) SSD_TERM()

extern int32_t* mapping_table;

#define ACCESS_RANDOM 0
#define ACCESS_SEQUENTIAL 1
#define _KB (1<<10)

void usage(const char *progname) {
	printf("Usage:\n\t%s <1/2/4/8> <r/s>\n", progname);
}

/**
 * simple test that writes all sectors in the device sequentially
 */
int test_access(size_t req_size, int access_type)
{
	int ret, i, lba;
	// Convert to blocks
	req_size /= SECTOR_SIZE;
	// Limit to lowest 70%
	size_t max_lba = (SECTOR_NB * 7) / 10;

	// write entire device
	for (i=0 ;i < max_lba; i += req_size) {
		if ((i/req_size) % 1024*10==0){
			LOG("wrote %.3lf of device", (double)i  / (double) max_lba);
		}

		switch (access_type) {
		case ACCESS_SEQUENTIAL:
			lba = i % max_lba;
			break;
		case ACCESS_RANDOM:
			// Align to write-size and limit to [0, max_lba) range
			lba = ((random() / req_size) * req_size) % max_lba;
			break;
		}
		SSD_WRITE(req_size, lba);
	}
	return 0;
}

int main(int argc, char *argv[]){
	int setup = 1;
	size_t req_size;
	int access_type;

	if (argc != 3) {
		usage(argv[0]);
		exit(1);
	}

	switch (argv[1][0]) {
	case '1':
		req_size = 4 * _KB;
		break;
	case '2':
		req_size = 8 * _KB;
		break;
	case '4':
		req_size = 16 * _KB;
		break;
	case '8':
		req_size = 32 * _KB;
		break;
	default:
		usage(argv[0]);
		exit(1);
	}

	switch (argv[2][0]) {
	case 's':
		access_type = ACCESS_SEQUENTIAL;
		break;
	case 'r':
		access_type = ACCESS_RANDOM;
		break;
	default:
		usage(argv[0]);
		exit(1);
	}

	RUN_TEST(setup, test_access(req_size, access_type));

	return 0;
}
