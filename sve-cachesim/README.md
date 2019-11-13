#  SVE Cache Simulator

Cache simulator for SVE memory traces generated through [ArmIE](https://developer.arm.com/tools-and-software/server-and-hpc/arm-architecture-tools/arm-instruction-emulator).
It models a single core multi-level cache system, with an optional prefetcher.

The repository includes the cache simulator, two cache models, a stride-prefetcher model plugin and sample trace files previously obtained from a provided example code.

## Quick run

```
$ python sve-cachesim.py sample/memtrace.example.256.merged.log cache-models/2-level.json
```

## Usage

The simulator, `sve-cachesim.py`, takes an [ArmIE](https://developer.arm.com/tools-and-software/server-and-hpc/arm-architecture-tools/arm-instruction-emulator) memory trace file, a JSON cache model file and an optional prefetch plugin to generate statistics regarding the cache system.

```
<usage> python sve-cachesim.py <memtrace-file> <cache-model.json>

positional arguments:
  memtrace              ArmIE memory trace file
  model                 Cache model JSON file

optional arguments:
  -h, --help            show this help message and exit
  -p {prefetch_commonStride}, --prefetch {prefetch_commonStride}
  -o OUTPUT, --output OUTPUT
                        Output file (default: stdout)
  -z, --zipped          processes gzipped memtraces
```

A sample code is provided inside `sample-traces`, alongside its respective memory traces generated with ArmIE for SVE vector lengths of 256, 512 and 1024.

Two cache models are provided inside `cache-models`, one for a a 2-level cache system and another one for a 3-level cache.

A common-stride prefetcher (with an history of 100 accesses) is also available (`prefetch_commonStride`).

Below is the output on an execution of the 256-bit vector length example memory trace, with a 2-level cache model and prefetch. Keep in mind that the hit/miss/evict rates do not take the
prefetcher accesses into account.

```
$ python sve-cachesim.py sample-traces/memtrace.example.256.merged.log cache-models/2-level.json -p prefetch_commonStride

** Running SVE CacheSim **
========
2-level-sample-traces/memtrace.example.256.merged.log.prefetch_commonStride
========
l1 Hits       49
l1 Accesses   57
l1 Evicts     8
l1 Hit Rate   85.96%
l1 Miss Rate  14.04%
l1 Evict Rate 14.04%

l2 Hits       3
l2 Accesses   8
l2 Evicts     5
l2 Prefetch Evicts	5
l2 Hit Rate	  37.50%
l2 Miss Rate	62.50%
l2 Evict Rate	62.50%

Total Accesses 57
Total Hits     52
Total Misses   5
Total Cycles   885
```

## Cache Models
The cache model JSON files include the different cache levels and parameters per level, as well as information for the prefetcher, if included. Take a look at the models provided in `/cache-models`when starting editing your own.

The different parameters for the each of the levels of the cache model are:
- Cache size: Total cache Size
- Line Size: Number of words per line
- Word Size: in bytes
- Set Size: N-way associate cache
- Latency: Cache access latency in cycles
- Memory latency: Main memory access latency

You can also set the number of levels (`nlevels`) and the level that the prefetcher
loads to (`fetch_level`). Currently, the prefetcher setting `fetch_level_latency`
is not utilized.

Example of a configuration file:
```
[
  {
   "nlevels":2
  },
  {
   "level":1,
   "cachesize":32768,
   "linesize":16,
   "setsize":2,
   "wordsize":4,
   "latency":7,
   "memlatency":100
  },
 {
   "level":2,
   "cachesize":2097152,
   "linesize":16,
   "setsize":12,
   "wordsize":4,
   "latency":14,
   "memlatency":100
  },
  {
   "fetch_level":2,
   "fetch_level_latency":14
  }
]
```

## Prefetcher plugin
A common-stride prefetcher (`prefetch_commonStride.py`) is included with the SVE cache simulator. Take a look at its code to learn more on how to implement a different one.

When developing a new prefetcher plugin, the source file must be included in the
root directory of the cache simulator and its name must be also added to the list
of available prefetchers in `sve-cachesim.py`.

Apart from cache evictions, no other prefetcher statistics are gathered.
This applies to the hit/miss/evict rates too, which do not take into account any
prefetch accesses.

The available prefetcher (`prefetch_commonStride.py`)determines the most common
(non-zero) stride in address access (from the latest 100 accesses) and fetches
the next address based on the result.


## Sample memory traces
A simple C code is provided with its respective memory traces generated with the Arm Instruction Emulator ([ArmIE](https://developer.arm.com/tools-and-software/server-and-hpc/arm-architecture-tools/arm-instruction-emulator)). Check the ArmIE documentation to learn more on how to generate memory traces.

## License

This project is licensed under [Apache-2.0](https://www.apache.org/licenses/LICENSE-2.0). For more information, see LICENSE.txt.

## Contributions / Pull Requests

Contributions are accepted under Apache-2.0. Only submit contributions where you have authored all of the code. If you do this on work time, make sure you have your employer's approval.
