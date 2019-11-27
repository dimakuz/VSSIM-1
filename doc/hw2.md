Homework Set 2
==============

Part 1
------

### A ###

Based on survey of the code, the SSD is built of the following componnents (from outermost to innermost)
configuration is declared on `CONFIG/vssim_config_manager.c`, and is specified in ssd.conf
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
The function `FTL_INIT()` initialize the mapping tables:
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
`valid_array` keeps block pages state
`empty_block_list` keep track of empty blocks in ssd
`victim_block_list` blocks for GC

* Write: 
	* `GET_MAPPING_INFO` fetch current current ppn to lpn mapping.
	* `GET_NEW_PAGE` get a new page from empty/victim  list.
	* `UPDATE_OLD_PAGE_MAPPING` if page is not new update (unmap) the `inverse_mapping_table` lpn to -1, and set page state to `INVALID`.
	* `UPDATE_NEW_PAGE_MAPPING` set new ppn <-> lpn mapping for`mapping_table` and `inverse_mapping_table`, and set page state to `VALID` and block to `DATA_BLOCK`.
	
* Read:
	* `GET_MAPPING_INFO` fetch current current ppn to lpn mappin, for reading.
	
* Garbage Collection:
	* `SELECT_VICTIM_BLOCK` get block from victim blocks.
	* `GET_NEW_PAGE` get a new page to copy victim block data into it.
	* `GET_INVERSE_MAPPING_INFO` get the old ppn mapping.
	* `UPDATE_NEW_PAGE_MAPPING` set new ppn <-> lpn mapping for`mapping_table` and `inverse_mapping_table`, and set page state to `VALID` and block to `DATA_BLOCK`.
	* `UPDATE_BLOCK_STATE` after earsing old block set state to empty.
	* ` INSERT_EMPTY_BLOCK` add to empty block list
	
### E ###

These are the delay values found in `CONFIG/vssim_config_manager.c`:
```
/* NAND Flash Delay */
int REG_WRITE_DELAY;
int CELL_PROGRAM_DELAY;
int REG_READ_DELAY;
int CELL_READ_DELAY;
int BLOCK_ERASE_DELAY;
int CHANNEL_SWITCH_DELAY_W;
int CHANNEL_SWITCH_DELAY_R; 
```

And these are the values configured in `CONFIG\ssd.conf`:

```
REG_WRITE_DELAY         82
CELL_PROGRAM_DELAY      940
REG_READ_DELAY          82
CELL_READ_DELAY         140
BLOCK_ERASE_DELAY       2000
                                  
CHANNEL_SWITCH_DELAY_R      16
CHANNEL_SWITCH_DELAY_W      33 
```


* Read delay flow:
	* `SSD_CH_SWITCH_DELAY` - delay for switching channel if needed. 
	defined by `CHANNEL_SWITCH_DELAY_R` configuration. delay=`16`
	*   `SSD_CELL_READ_DELAY` - delay for reading from cell.
	defined by `CELL_READ_DELAY` configuration. delay=`140`
	* `SSD_REG_READ_DELAY` - delay for reading from register. 
	defined by `REG_READ_DELAY` configuration. delay=`82`
	
	
### F ###

The original `tests.c` only writes to the ssd, and no GC cycles are running.
so if we change the read delay nothing will happen. we changed the configuration of `CELL_PROGRAM_DELAY` from `940` to `1880` and executed:

`time ./tests`


| CELL_PROGRAM_DELAY  | Run Time  |
|---|---|
| 940  |  `0m49.025s` | 
| 1880 | `1m48.477s` |

In the results we can see that doubling the `CELL_PROGRAM_DELAY` doubled the run time of the tests.

### G ###
The best performance 

|Config Name| Original Value| New Value | Run Time  |
|---|---|---|---|
|-|-|-| `35.664s`| 
|CELL_PROGRAM_DELAY|940 | 470|`23.730s`|
|REG_WRITE_DELAY|82 | 41|`34.509s`|
|CELL_READ_DELAY|140 | 70|`35.454s`|
|SSD_REG_READ_DELAY|82 | 41|`35.831s`|
|BLOCK_ERASE_DELAY|2000 | 1000|`35.470s`|
|CHANNEL_SWITCH_DELAY_R|16 | 8|`35.128s`|
|CHANNEL_SWITCH_DELAY_W|33 | 17|`34.852s`|




	
	
	



 
	