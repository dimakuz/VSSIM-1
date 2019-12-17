#include "ssd.h"
#include "common.h"
#include <assert.h>

#define RUN_TEST(SETUP, FUNC)   if(SETUP) popen("rm data/*.dat", "r"); \
						        SSD_INIT(); \
 								FUNC; \
								if(SETUP) SSD_TERM()

extern int32_t* mapping_table;

/**
 * simple test that writes all sectors in the device randomly
 */
int test_access()
{
	int ret, i, lba;

	// write entire device 
	for(i=0;i<SECTOR_NB;i+=SECTORS_PER_PAGE){
		if ((i/SECTORS_PER_PAGE) % 1024*10==0){
			LOG("wrote %.3lf of device", (double)i  / (double)SECTOR_NB);
		}

		lba = rand() % SECTOR_NB;
		SSD_WRITE(SECTORS_PER_PAGE, lba);
	}

	printf("wrote seq\n");

	return 0;
}


/**
 * simple test that writes and re-reads tenth of all sectors in the device randomly
 */
int test_reread(size_t pages)
{
	int ret, i, lba;

	for(i=0;i<SECTOR_NB / 10;i+=SECTORS_PER_PAGE){
		if ((i/SECTORS_PER_PAGE) % 1024*10==0){
			LOG("wrote %.3lf of device", (double)i  / (double)SECTOR_NB);
		}

		size_t pnb = rand() % PAGE_NB;
		SSD_WRITE(SECTORS_PER_PAGE * pages, pnb * SECTORS_PER_PAGE);
		SSD_READ(SECTORS_PER_PAGE * pages, pnb * SECTORS_PER_PAGE);
	}
	return 0;
}

extern int cache_enable;
int main(int argc, char *argv[]){
	int setup = 1;
	size_t pages;

	if (argc >= 2) {
		cache_enable = strtol(argv[1], NULL, 10);
	} else {
		cache_enable = 1;
	}

	if (argc >= 3) {
		pages = strtol(argv[2], NULL, 10);
	} else {
		pages = 1;
	}

	RUN_TEST(setup, test_reread(pages));
	return 0;
}
