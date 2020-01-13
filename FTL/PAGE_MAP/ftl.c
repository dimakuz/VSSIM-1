// File: ftl.c
// Date: 2014. 12. 03.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2014
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include "common.h"
#ifndef VSSIM_BENCH
#include "qemu-kvm.h"
#endif

#ifdef FTL_GET_WRITE_WORKLOAD
FILE* fp_write_workload;
#endif
#ifdef FTL_IO_LATENCY
FILE* fp_ftl_w;
FILE* fp_ftl_r;
#endif

int g_init = 0;
extern double ssd_util;

int _FTL_WRITE_REAL(int32_t sector_nb, unsigned int length);
static int _FTL_WRITE_PAGE(
	int32_t lba, unsigned long left_skip, unsigned long right_skip,
	int write_page_nb, int32_t *new_ppn_ret, int io_page_nb
);

int cache_enable = 1;
struct compressed_cache_entry {
	int32_t lba;
	size_t size;
};

#define _CACHE_CAPACITY 4096
#define _CACHE_ENTRIES 128
struct compressed_cache {
	size_t capacity;
	size_t used;
	size_t elements;
	struct compressed_cache_entry entries[_CACHE_ENTRIES];
};


int controlled_compr_en = 0;
double controlled_compr_factor = 0.0;

static double get_compr_factor() {
	if (controlled_compr_en) {
		return controlled_compr_factor;
	} else {
		return (double) rand() / (double) RAND_MAX;
	}
}

static struct compressed_cache cache;

static void compressed_cache_reset() {
	cache.capacity = _CACHE_CAPACITY;
	cache.used = 0;
	cache.elements = 0;
	for (size_t i = 0; i < _CACHE_ENTRIES; i++) {
		cache.entries[i].lba = -1;
		cache.entries[i].size = 0;
	}
}

static ssize_t compressed_cache_find(int32_t lba) {
	for (size_t i = 0; i < cache.elements; i++) {
		int32_t entry_lba = cache.entries[i].lba;
		if (entry_lba == lba)
			return i;
	}
	return -1;
}

static int compressed_cache_read(int32_t lba) {
	return (compressed_cache_find(lba) >= 0) ? SUCCESS : FAIL;
}

static int compressed_cache_write(int32_t lba, size_t size) {
	ssize_t prev_index = compressed_cache_find(lba);
	if (prev_index >= 0) {
		cache.used -= cache.entries[prev_index].size;
		cache.elements--;
		if (cache.elements > 0) {
			cache.entries[prev_index] = cache.entries[cache.elements];
		}
	}

	if (cache.used + size > cache.capacity)
		return FAIL;

	ssize_t next_vacant = cache.elements;
	if (next_vacant >= _CACHE_ENTRIES) {
		printf("ERROR: no vacant entry in cache\n");
		return FAIL;
	}

	cache.used += size;
	cache.elements++;
	cache.entries[next_vacant].lba = lba;
	cache.entries[next_vacant].size = size;
	if(lba + SECTORS_PER_PAGE > SECTOR_NB){
			printf("ERROR[%s] Exceed Sector number, %u, %u\n", __FUNCTION__, lba, SECTORS_PER_PAGE);	
	}

	return SUCCESS;
}

static int compressed_cache_flush() {
	int ret;
	for (ssize_t i = cache.elements - 1; i >= 0; i--) {
		int32_t entry_lba = cache.entries[i].lba;
		if (entry_lba == -1)
			continue;
		if(entry_lba + SECTORS_PER_PAGE > SECTOR_NB){
			printf("ERROR[%s] Exceed Sector number\n", __FUNCTION__);	
		}
		ret = _FTL_WRITE_REAL(entry_lba, SECTORS_PER_PAGE);
		if (ret == FAIL)
			return ret;

		cache.used -= cache.entries[i].size;
		cache.entries[i].lba = -1;
		cache.elements--;
	}
	return SUCCESS;
}

void FTL_INIT(void)
{
	if(g_init == 0){
        	printf("[%s] start\n", __FUNCTION__);

		INIT_SSD_CONFIG();

		INIT_MAPPING_TABLE();
		INIT_INVERSE_MAPPING_TABLE();
		INIT_BLOCK_STATE_TABLE();
		INIT_VALID_ARRAY();
		INIT_EMPTY_BLOCK_LIST();
		INIT_VICTIM_BLOCK_LIST();
		INIT_PERF_CHECKER();
		
#ifdef FTL_MAP_CACHE
		INIT_CACHE();
#endif
#ifdef FIRM_IO_BUFFER
		INIT_IO_BUFFER();
#endif
#ifdef MONITOR_ON
#ifndef LOCAL
		INIT_LOG_MANAGER();
#endif
#endif
		g_init = 1;
#ifdef FTL_GET_WRITE_WORKLOAD
		fp_write_workload = fopen("./data/p_write_workload.txt","a");
#endif
#ifdef FTL_IO_LATENCY
		fp_ftl_w = fopen("./data/p_ftl_w.txt","a");
		fp_ftl_r = fopen("./data/p_ftl_r.txt","a");
#endif
		SSD_IO_INIT();

		if (cache_enable)
			compressed_cache_reset();

		printf("[%s] complete\n", __FUNCTION__);
	}
}

void FTL_TERM(void)
{
	printf("[%s] start\n", __FUNCTION__);

#ifdef FIRM_IO_BUFFER
	TERM_IO_BUFFER();
#endif
	TERM_MAPPING_TABLE();
	TERM_INVERSE_MAPPING_TABLE();
	TERM_VALID_ARRAY();
	TERM_BLOCK_STATE_TABLE();
	TERM_EMPTY_BLOCK_LIST();
	TERM_VICTIM_BLOCK_LIST();
	TERM_PERF_CHECKER();

#ifdef MONITOR_ON
	TERM_LOG_MANAGER();
#endif

#ifdef FTL_IO_LATENCY
	fclose(fp_ftl_w);
	fclose(fp_ftl_r);
#endif
	printf("[%s] complete\n", __FUNCTION__);
}

void FTL_READ(int32_t sector_nb, unsigned int length)
{
	int ret;

#ifdef GET_FTL_WORKLOAD
	FILE* fp_workload = fopen("./data/workload_ftl.txt","a");
	struct timeval tv;
	struct tm *lt;
	double curr_time;
	gettimeofday(&tv, 0);
	lt = localtime(&(tv.tv_sec));
	curr_time = lt->tm_hour*3600 + lt->tm_min*60 + lt->tm_sec + (double)tv.tv_usec/(double)1000000;
	//fprintf(fp_workload,"%lf %d %ld %u %x\n",curr_time, 0, sector_nb, length, 1);
	fprintf(fp_workload,"%lf %d %u %x\n",curr_time, sector_nb, length, 1);
	fclose(fp_workload);
#endif
#ifdef FTL_IO_LATENCY
	int64_t start_ftl_r, end_ftl_r;
	start_ftl_r = get_usec();
#endif
	ret = _FTL_READ(sector_nb, length);
#ifdef FTL_IO_LATENCY
	end_ftl_r = get_usec();
	if(length >= 128)
		fprintf(fp_ftl_r,"%ld\t%u\n", end_ftl_r - start_ftl_r, length);
#endif
}


void FTL_WRITE(int32_t sector_nb, unsigned int length)
{
	int ret;

#ifdef GET_FTL_WORKLOAD
	FILE* fp_workload = fopen("./data/workload_ftl.txt","a");
	struct timeval tv;
	struct tm *lt;
	double curr_time;
	gettimeofday(&tv, 0);
	lt = localtime(&(tv.tv_sec));
	curr_time = lt->tm_hour*3600 + lt->tm_min*60 + lt->tm_sec + (double)tv.tv_usec/(double)1000000;
//	fprintf(fp_workload,"%lf %d %ld %u %x\n",curr_time, 0, sector_nb, length, 0);
	fprintf(fp_workload,"%lf %d %u %x\n",curr_time, sector_nb, length, 0);
	fclose(fp_workload);
#endif
#ifdef FTL_IO_LATENCY
	int64_t start_ftl_w, end_ftl_w;
	start_ftl_w = get_usec();
#endif
	ret = _FTL_WRITE(sector_nb, length);
#ifdef FTL_IO_LATENCY
	end_ftl_w = get_usec();
	if(length >= 128)
		fprintf(fp_ftl_w,"%ld\t%u\n", end_ftl_w - start_ftl_w, length);
#endif
}

int _FTL_READ(int32_t sector_nb, unsigned int length)
{
#ifdef FTL_DEBUG
	printf("[%s] Start\n", __FUNCTION__);
#endif

	if(sector_nb + length > SECTOR_NB){
		printf("Error[%s] Exceed Sector number\n", __FUNCTION__); 
		return FAIL;	
	}

	int32_t lpn;
	int32_t ppn;
	int32_t lba = sector_nb;
	unsigned int remain = length;
	unsigned int left_skip = sector_nb % SECTORS_PER_PAGE;
	unsigned int right_skip;
	unsigned int read_sects;

	unsigned int ret = FAIL;
	int read_page_nb = 0;
	int io_page_nb;


#ifdef FIRM_IO_BUFFER
	INCREASE_RB_FTL_POINTER(length);
#endif

	while(remain > 0){

		if(remain > SECTORS_PER_PAGE - left_skip){
			right_skip = 0;
		}
		else{
			right_skip = SECTORS_PER_PAGE - left_skip - remain;
		}
		read_sects = SECTORS_PER_PAGE - left_skip - right_skip;

		lpn = lba / (int32_t)SECTORS_PER_PAGE;
		ppn = GET_MAPPING_INFO(lpn);

		if(ppn == -1){
#ifdef FIRM_IO_BUFFER
			INCREASE_RB_LIMIT_POINTER();
#endif
			return FAIL;
		}

		lba += read_sects;
		remain -= read_sects;
		left_skip = 0;
	}

	io_alloc_overhead = ALLOC_IO_REQUEST(sector_nb, length, READ, &io_page_nb);

	remain = length;
	lba = sector_nb;
	left_skip = sector_nb % SECTORS_PER_PAGE;

	while(remain > 0){

		if(remain > SECTORS_PER_PAGE - left_skip){
			right_skip = 0;
		}
		else{
			right_skip = SECTORS_PER_PAGE - left_skip - remain;
		}
		read_sects = SECTORS_PER_PAGE - left_skip - right_skip;

		lpn = lba / (int32_t)SECTORS_PER_PAGE;

#ifdef FTL_MAP_CACHE
		ppn = CACHE_GET_PPN(lpn);
#else
		ppn = GET_MAPPING_INFO(lpn);
#endif

		if(ppn == -1){
#ifdef FTL_DEBUG
			printf("ERROR[%s] No Mapping info\n", __FUNCTION__);
#endif
		}
		if (!cache_enable || compressed_cache_read(lba) == FAIL)
			ret = SSD_PAGE_READ(CALC_FLASH(ppn), CALC_BLOCK(ppn), CALC_PAGE(ppn), read_page_nb, READ, io_page_nb);

#ifdef FTL_DEBUG
		if(ret == SUCCESS){
			printf("\t read complete [%u]\n",ppn);
		}
		else if(ret == FAIL){
			printf("ERROR[%s] %u page read fail \n",__FUNCTION__, ppn);
		}
#endif
		read_page_nb++;

		lba += read_sects;
		remain -= read_sects;
		left_skip = 0;

	}

	INCREASE_IO_REQUEST_SEQ_NB();

#ifdef FIRM_IO_BUFFER
	INCREASE_RB_LIMIT_POINTER();
#endif

#ifdef MONITOR_ON
	char szTemp[1024];
	sprintf(szTemp, "READ PAGE %d ", length);
	WRITE_LOG(szTemp);
#endif

#ifdef FTL_DEBUG
	printf("[%s] Complete\n", __FUNCTION__);
#endif

	return ret;
}

int _FTL_WRITE(int32_t sector_nb, unsigned int length){

	if(!cache_enable){
		return _FTL_WRITE_REAL(sector_nb, length);
	}

	if(sector_nb + length > SECTOR_NB){
		printf("ERROR[%s] Exceed Sector number\n", __FUNCTION__);
                return FAIL;
    }

	unsigned int remain = length;
	unsigned int left_skip = sector_nb % SECTORS_PER_PAGE;
	unsigned int right_skip;
	unsigned int write_sects;

	unsigned int ret = FAIL;
	int write_page_nb=0;
	int32_t lba = sector_nb;

	while(remain > 0){
		double compr_factor = get_compr_factor();
		size_t compr_size =  (size_t) PAGE_SIZE * compr_factor;
		
		if(remain > SECTORS_PER_PAGE - left_skip){
			right_skip = 0;
		}
		else{
			right_skip = SECTORS_PER_PAGE - left_skip - remain;
		}

		write_sects = SECTORS_PER_PAGE - left_skip - right_skip;

		
		ret = compressed_cache_write(lba, compr_size);
		if (ret == FAIL) {

			//printf("ERROR[%s] flushing cache\n", __FUNCTION__);
			compressed_cache_flush();
			ret = compressed_cache_write(lba, compr_size);
		}

		lba += write_sects;
		remain -= write_sects;
		if(remain > 0 && lba + SECTORS_PER_PAGE > SECTOR_NB){
            printf("ERROR[%s] Exceed Sector number, %u, %u, %u, %u, %u, %u, %u\n",
				   	__FUNCTION__, lba, SECTORS_PER_PAGE, sector_nb, length, write_sects, left_skip, right_skip);
		}
		left_skip = 0;
	}

	return ret;

}

int _FTL_WRITE_REAL(int32_t sector_nb, unsigned int length)
{
#ifdef FTL_DEBUG
	printf("[%s] Start\n", __FUNCTION__);
#endif

#ifdef FTL_GET_WRITE_WORKLOAD
	fprintf(fp_write_workload,"%d\t%u\n", sector_nb, length);
#endif

	int io_page_nb;

	if(sector_nb + length > SECTOR_NB){
		printf("ERROR[%s] Exceed Sector number: %d, %u\n", __FUNCTION__, sector_nb, length);
                return FAIL;
        }
	else{
		io_alloc_overhead = ALLOC_IO_REQUEST(sector_nb, length, WRITE, &io_page_nb);
	}

	int32_t lba = sector_nb;
	int32_t new_ppn;

	unsigned int remain = length;
	unsigned int left_skip = sector_nb % SECTORS_PER_PAGE;
	unsigned int right_skip;
	unsigned int write_sects;

	unsigned int ret = FAIL;
	int write_page_nb=0;

	while(remain > 0){

		if(remain > SECTORS_PER_PAGE - left_skip){
			right_skip = 0;
		}
		else{
			right_skip = SECTORS_PER_PAGE - left_skip - remain;
		}

		write_sects = SECTORS_PER_PAGE - left_skip - right_skip;

#ifdef FIRM_IO_BUFFER
		INCREASE_WB_FTL_POINTER(write_sects);
#endif
		
		ret = _FTL_WRITE_PAGE(lba, left_skip, right_skip, write_page_nb, &new_ppn, io_page_nb);
		write_page_nb++;
		lba += write_sects;
		remain -= write_sects;
		left_skip = 0;

	}

	INCREASE_IO_REQUEST_SEQ_NB();
#ifdef GC_ON
	GC_CHECK(CALC_FLASH(new_ppn), CALC_BLOCK(new_ppn));
#endif

#ifdef FIRM_IO_BUFFER
	INCREASE_WB_LIMIT_POINTER();
#endif

#ifdef MONITOR_ON
	char szTemp[1024];
	sprintf(szTemp, "WRITE PAGE %d ", length);
	WRITE_LOG(szTemp);
	sprintf(szTemp, "WB CORRECT %d", write_page_nb);
	WRITE_LOG(szTemp);
#endif

#ifdef FTL_DEBUG
	printf("[%s] End\n", __FUNCTION__);
#endif
	return ret;
}

static int _FTL_WRITE_PAGE(
	int32_t lba, unsigned long left_skip, unsigned long right_skip,
	int write_page_nb, int32_t *new_ppn_ret, int io_page_nb
) {
	int ret;
	int32_t lpn;
	int32_t new_ppn;
	int32_t old_ppn;

#ifdef WRITE_NOPARAL
		ret = GET_NEW_PAGE(VICTIM_NOPARAL, empty_block_table_index, &new_ppn);
#else
		ret = GET_NEW_PAGE(VICTIM_OVERALL, EMPTY_TABLE_ENTRY_NB, &new_ppn);
#endif

	if(ret == FAIL){
		printf("ERROR[%s] Get new page fail \n", __FUNCTION__);
		return FAIL;
	}

	lpn = lba / (int32_t)SECTORS_PER_PAGE;
	old_ppn = GET_MAPPING_INFO(lpn);

	if((left_skip || right_skip) && (old_ppn != -1)){
		ret = SSD_PAGE_PARTIAL_WRITE(
			CALC_FLASH(old_ppn), CALC_BLOCK(old_ppn), CALC_PAGE(old_ppn),
			CALC_FLASH(new_ppn), CALC_BLOCK(new_ppn), CALC_PAGE(new_ppn),
			write_page_nb, WRITE, io_page_nb);
	}
	else{
		ret = SSD_PAGE_WRITE(CALC_FLASH(new_ppn), CALC_BLOCK(new_ppn), CALC_PAGE(new_ppn), write_page_nb, WRITE, io_page_nb);
	}

	UPDATE_OLD_PAGE_MAPPING(lpn);
	UPDATE_NEW_PAGE_MAPPING(lpn, new_ppn);
	if (new_ppn_ret)
		*new_ppn_ret = new_ppn;

#ifdef FTL_DEBUG
	if(ret == SUCCESS){
			printf("\twrite complete [%d, %d, %d]\n",CALC_FLASH(new_ppn), CALC_BLOCK(new_ppn),CALC_PAGE(new_ppn));
	}
	else if(ret == FAIL){
			printf("ERROR[%s] %d page write fail \n",__FUNCTION__, new_ppn);
	}
#endif
	return ret;
}
