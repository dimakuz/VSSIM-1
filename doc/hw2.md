Homework Set 2
==============

Part 1
------

### A ###

Based on survey of the code, the SSD is built of the following componnents (from outermost to innermost)

1. Flash chips (referenced by `FLASH_NB` - chips in drive)
2. Planes (references by `PLANES_PER_FLASH`) - 
3. Blocks - TODO
4. Sectors - TODO
5. Pages - TODO


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
