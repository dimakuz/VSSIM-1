#include "ssd.h"
#include "common.h"
#include <assert.h>

#define RUN_TEST(SETUP, FUNC)   if(SETUP) popen("rm data/*.dat", "r"); \
						        SSD_INIT(); \
 								FUNC; \
								if(SETUP) SSD_TERM()

extern int32_t* mapping_table;
extern double total_write_count;
extern double total_gc_write_count;
/**
 * simple test that writes all sectors in the device randomly
 */
int test_access()
{
	int ret, i, lba;
	size_t written = 0;

	// write entire device 
	printf("total sectors: %lu sectors per page: %u\n", SECTOR_NB, SECTORS_PER_PAGE);
	for(i=0;i<SECTOR_NB;i+=SECTORS_PER_PAGE){
		if ((i/SECTORS_PER_PAGE) % 1024*10==0){
			LOG("wrote %.3lf of device", (double)i  / (double)SECTOR_NB);
		}

		lba = rand() % SECTOR_NB;
		lba = (lba * SECTORS_PER_PAGE) / SECTORS_PER_PAGE;
		// lba = i % (SECTOR_NB - SECTORS_PER_PAGE);
		SSD_WRITE(SECTORS_PER_PAGE, lba);
		written += 1;
	}

	printf("wrote seq %lx\n", written);
	printf("Actual write amplification %.3lf\n", (total_write_count+total_gc_write_count)/(double)written);
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

extern int controlled_compr_en;
extern double controlled_compr_factor;

int main(int argc, char *argv[]){
	int setup = 1;
	size_t pages;

	if (argc >= 2) {
		cache_enable = strtol(argv[1], NULL, 10);
	} else {
		cache_enable = 1;
	}
	if (argc >= 3) {
		controlled_compr_factor = strtod(argv[2], NULL);
		controlled_compr_en = 1;
	} else {
		controlled_compr_en = 0;
		controlled_compr_factor = 0.0;
	}
	if (argc >= 4) {
		pages = strtol(argv[2], NULL, 10);
	} else {
		pages = 1;
	}

	printf("running test with cache=%u,fixed alpha?=%u alpha=%f, pages=%lu\n",
		   	cache_enable, controlled_compr_en, controlled_compr_factor, pages);
	RUN_TEST(setup, test_access());
	return 0;
}
