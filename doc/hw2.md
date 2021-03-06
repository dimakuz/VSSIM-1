Homework Set 2
==============

Code Assignment
------
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
The following is based on the currently enabled code:

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

`mapping_table` maps logical pages (lpn) to  physical pages (ppn)
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
	* `GET_MAPPING_INFO` fetch current current ppn to lpn mapping, for reading.
	
* Garbage Collection:
	* `SELECT_VICTIM_BLOCK` get block from victim blocks.
	* `GET_NEW_PAGE` get a new page to copy victim block data into it.
	* `GET_INVERSE_MAPPING_INFO` get the old ppn mapping.
	* `UPDATE_NEW_PAGE_MAPPING` set new ppn <-> lpn mapping for`mapping_table` and `inverse_mapping_table`, and set page state to `VALID` and block to `DATA_BLOCK`.
	* `UPDATE_BLOCK_STATE` after erasing old block set state to empty.
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
The best performance boost will be gained by decreasing `CELL_PROGRAM_DELAY` since the original tests.c only run write commands and `CELL_PROGRAM_DELAY` is the most significant delay.

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


### H ###
Features that are not implemented by the simulator:

1. Multi Level Cell
2-bit cell is called Multi-level Cell, allows for more data to be stored in each cell but harder to implement.
We can implement this by adding a Level Per Cell configuration, and adding more delay when writing data with `CELL_PROGRAM_DELAY`, and adding a delay configuration for writing different level.
3. FTL in host memory
Simulate FTL on the host RAM.
We can implement this by adding delay configuration for for all tables, described in **E**
4. No Host interface delay
Requests go directly to ftl
We can implement this by adding delay configuration.
5. Flash Cell/Block/Page/Die failure and degradation:
Flash cells have limited write capacity and can fail, should be able to set the failure rate for flash components. We can keep counters for each of component types we want to track degradation of. Then, based on the counters we can fail operations (either determinstically, e.g. fail after 100k writes, or stochastically, after 50k writes, 0.001% chance to fail).

Part 2
------
### A ###
For this question we've reduced the size of SSD. Our test ran as following

* written to the lowest 90% percent of the drive
* wrote number of requests equivalent to twice the size of the drive

We have noticed higher latency on the random accesses (1384 usec for sequential access vs 1648 usec for random access).
Upon closer examination of garbage collection mechanism, we see that random case does  about 38% extra writes to copy pages from victim blocks. This accounts for higher latency in regular writes.

Sequential case:
```
Average Write Latency	1384.999 us
Total writes	294892
Total GC writes 0
```

Random case:
```
Average Write Latency	1648.044 us
Total writes	294893
Total GC writes 112111
```

### B ###
From the below runs we can see there's direct correlation between lower overprovisioning and high latency / write amplification.

* The high latency is caused directly by high write amplification (extra writes per actual writes).
* Write amplification is caused by need to garbage collect blocks.
* Garbage collection rate is correlated with number of empty blocks.
* Overprovisioning ensures certain amount of unutized empty blocks, the higher over provisioning, the more blocks are available.

Our measurements directly support the above explanaition:

|  Provisioned blocks %  | Write Amplification | Latency  |
|------------------------|---------------------|----------|
| 0.95                   | 1.515               | 1833.535 |
| 0.9                    | 1.380               | 1701.049 |
| 0.85                   | 1.279               | 1397.688 |
| 0.8                    | 1.21                | 1338.217 |
| 0.75                   | 1.148               | 1272.378 |
| 0.7                    | 1.105               | 1221.058 |
| 0.65                   | 1.071               | 1151.604 |

![](hw2-q2-b.png)

### C ###
In this question we've chosen to tweak the GC_VICTIM_NB parameter. This parameter specifies how many blocks garbage collector will attempt to clean each time number of empty blocks falls under a threshold.

In question B, the default GC_VICTIM_NB was set to 20. In the below figures we're showing results for value of 10.

The change had the following effects:

* Negligibly lower write amplification
* Somewhat higher average latency

Our hypothesis behind this result is that write amplification is lower because the last occurence of garbage collection cleans less blocks (comparing absolute numbers of GC-related writes shows only couple of blocks worth of writes). As for the latency, we think that it has to do with a lot more writes invoking GC cycle that adds additional latency cost.


|  Provisioning % | Write Amplification | Latency  |
|-----------------|---------------------|----------|
| 0.95            | 1.511               | 2314.819 |
| 0.9             | 1.373               | 1864.565 |
| 0.85            | 1.274               | 1643.312 |
| 0.8             | 1.207               | 1621.185 |
| 0.75            | 1.147               | 1415.68  |
| 0.7             | 1.103               | 1368.114 |
| 0.65            | 1.07                | 1240.472 |


![](hw2-q2-c.png)

Dry Assignment
------

a. The throughput in the 38 second is over 500MB/s, this is strange since the benchmark results indicated that: 
*"the maximum write throughput of the mobile storage device is 160 MB/s"*.

b. Our hypothesis is that bulk writes to are waited on and the counters are only updated after the bulk write is completed. so if a large amount of data is written at once then the performance counters will update at once the data is written.
Going over the `blk-lib.c` source we can see that all of the write functions wait for completion
 
```
	/* Wait for bios in-flight */
	if (!atomic_dec_and_test(&bb.done))
		wait_for_completion(&wait);
```
c. We think this may be caused due to a large Garbage Collection that also erase the data on the blocks, this make sense if the device sees the erase as a write operation.
