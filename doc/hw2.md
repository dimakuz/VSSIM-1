Homework Set 2
==============

Part 1
------

### A ###

Based on survey of the code, the SSD is built of the following componnents (from outermost to innermost)
configuration is declared on CONFIG/vssim_config_manager.c, and is specified in ssd.conf
```
/* SSD Configuration */
int SECTOR_SIZE;
int PAGE_SIZE;

int64_t SECTOR_NB;
int PAGE_NB;
int FLASH_NB;
int BLOCK_NB;
int CHANNEL_NB;
int PLANES_PER_FLASH;

```

1. Flash chips (referenced by `FLASH_NB` - chips in drive) - the number of flash memories in a whole SSD.
2. Planes (references by `PLANES_PER_FLASH`) - the number of planes per flash memory.
3. Blocks - (references by `BLOCK_NB`) - the number of blocks per flash memory.
4. Pages - (references by ,`PAGE_NB`, `PAGE_SIZE`) - number of pages and the size of one page.
5. Sectors - (references by `SECTOR_SIZE`) - the size of one sector. 


### B ###
The following is based on the currenly enabled code:

* `SSD_WRITE` calls into FTL code (`ssd.c:SSD_WRITE`)
* Inside FTL code, the following is performed: (`ftl.c:_FTL_WRITE`)
  * IO request is allocated
  * A new page for writing the data is allocated
  * If write covers the whole page, a regular page write is issued (`ssd_io_manager.c:SSD_PAGE_WRITE`)
    * Channel and register required for access are calculated
    * Channel is enabled
    * Cell/channel/register access times are recorded (`ssd_io_manager.c/SSD_*_RECORD`)
    * Flash chip is accessed (`ssd_io_manager.c:SSD_FLASH_ACCESS`)
      * Selected register is accessed (`ssd_io_manager.c:SSD_REG_ACCESS`)
        * Register and cell are written to.
  * Otherwise, write is not a whole page, and old page is not invalid - a partial page write is issued (`ssd_io_manager.c:SSD_PARTIAL_PAGE_WRITE`)
    * Similar to whole page case, except a read is peformed first to get missing data.
  * Page mapping table and inverse page mapping tables for old page are invalidated (`ftl_mapping_manager.c:UPDATE_OLD_PAGE_MAPPING`)
  * Page mapping table and inverse page mapping tables are update with the information of the new physical page number (`ftl_mapping_manager.c:UPDATE_NEW_PAGE_MAPPING`)
  * Garbage collection check is triggered (`ftl_gc_manager.c:GC_CHECK`)
    * If number of empty blocks is less than total number of planes in all flash chips, garbage collection is triggered (`ftl_gc_manager.c:GARBAGE_COLLECTION`)

### C ###
* `SSD_READ` calls into FTL_CODE (`ssd.c:SSD_READ`)
* Inside FTL code, the following is performed: (`ftl.c:_FTL_READ`)
  * Mapping of the requested page is retrieved (`ftl_mapping_manager.c:GET_MAPPING_INFO`)
  * If mapping is invalid, read fails
  * Otherwise, IO request is allocated
  * Page read is attempted on the block and flash chip based on the physical block returned by the mapping (`ssd_io_manager.c:SSD_PAGE_READ`)
    * Channel and register required for access are calculated
    * Channel is enabled
    * Cell/channel/register access times are recorded (`ssd_io_manager.c/SSD_*_RECORD`)

### D ###
The function `FTL_INIT()` initialize these tables:
```
        INIT_MAPPING_TABLE();                                                                                 
        INIT_INVERSE_MAPPING_TABLE();                                                                         
        INIT_BLOCK_STATE_TABLE();                                                                             
        INIT_VALID_ARRAY();                                                                                   
        INIT_EMPTY_BLOCK_LIST();                                                                              
        INIT_VICTIM_BLOCK_LIST(); 
```

`mapping_table` maps logical pages (lpn) to  pyhsical pages (ppn)
`inverse_mapping_table` maps ppn to lpn
`block_state_table` keeps the state of each block
``

``

* Write:
	* `GET_MAPPING_INFO` fetch current current ppn to lpn mapping.
	* `UPDATE_OLD_PAGE_MAPPING` if page is not new update (unmap) the `inverse_mapping_table` lpn to -1, and set block state to `INVALID`.
	* `UPDATE_NEW_PAGE_MAPPING` set new `mapping_table` and `inverse_mapping_table`, and set block state to `VALID`|`DATA_BLOCK`
	
* Read: