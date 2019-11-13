#!/usr/bin/python

# Copyright (c) 2019, Arm Limited and Contributors.
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from collections import defaultdict
import argparse
from argparse import RawTextHelpFormatter
import re
import os
import errno
import json
import gzip


def load_plugin(name):
    mod = __import__(name)
    return mod


def traceToInts(filename, cache_size, zip_trace):
    # create list of instruction addresses (as hex) and of data read addresses (converted to ints)

    # Trace Format:
    # <seq num>: <TID>, <is_Bundle>, <is_Write>, <data_size>, <data_address>, <PC>
    r_addresses = []  # data read addresses (int)

    # Process gzipped/unzipped memtraces
    if zip_trace:
        with gzip.open(filename, "rb") as trace:
            for line in trace:
                split = [item.strip()
                         for item in re.split(':|,', line.decode("utf-8"))]
                if len(split[3]) == 1:
                    if int(split[3]) == 0:  # read trace
                        # Address from trace (to be chunked)
                        r_addresses.append(int(split[5], 16))

                        # Chunk the read access into multiple reads if size is too large for cache
                        nextAddr = int(split[5], 16)  # iterator
                        # read_trace size - (cache lineSize * wordSize)
                        size = int(split[4]) - cache_size
                        while size > 0:
                            nextAddr += cache_size
                            size -= cache_size
                            # chunked memory addresses
                            r_addresses.append(nextAddr)
    else:
        with open(filename, "r") as trace:
            for line in trace:
                split = [item.strip() for item in re.split(':|,', line)]
                if len(split[3]) == 1:
                    if int(split[3]) == 0:  # read trace
                        # Address from trace (to be chunked)
                        r_addresses.append(int(split[5], 16))

                        # Chunk the read access into multiple reads if size is too large for cache
                        nextAddr = int(split[5], 16)  # iterator
                        # read_trace size - (cache lineSize * wordSize)
                        size = int(split[4]) - cache_size
                        while size > 0:
                            nextAddr += cache_size
                            size -= cache_size
                            # chunked memory addresses
                            r_addresses.append(nextAddr)

    return r_addresses


class CacheConfig:
    def __init__(self, cacheSize, lineSize, setSize, wordSize, latency, memLatency):
        self.cacheSize = cacheSize
        self.lineSize = lineSize
        self.setSize = setSize
        self.wordSize = wordSize
        self.latency = latency
        self.memLatency = memLatency


class Line:
    def __init__(self, config, startAddress):
        self.config = config
        self.startAddress = startAddress

    def hasLine(self, address):
        if address == self.startAddress:
            return True
        return False


class Set:
    def __init__(self, config):
        self.config = config
        self.lines = [Line(config, 0) for count in range(config.setSize)]

    def hasLine(self, lineAddress):

        # Check if the address is in this set
        for line in self.lines:
            if line.hasLine(lineAddress):
                return True

        for x in range(len(self.lines) - 1):
            # evict oldest line
            self.lines[x] = self.lines[x + 1]
        # add current line
        self.lines[len(self.lines) - 1] = Line(self.config, lineAddress)

        return False


class Cache:
    def __init__(self, config):
        self.config = config
        self.nextLevel = None
        self.sets = [Set(config) for count in range(
            int(config.cacheSize / (config.setSize * config.lineSize * config.wordSize)))]

        self.evictCounter = 0
        self.prefEvictCounter = 0

    def hasAddress(self, address, prefetchEnabled):
        lineAddress = address - (address %
                                 (self.config.lineSize * self.config.wordSize))
        if self.sets[lineAddress % len(self.sets)].hasLine(lineAddress):
            return True, self.config.latency
        else:  # record an evict for the counter
            if prefetchEnabled:
                self.prefEvictCounter += 1
            else:
                self.evictCounter += 1

        # if there are no more levels, record a total_misses
        if self.nextLevel is None:
            return False, self.config.memLatency
        else:
            # go to next level, if available
            if prefetchEnabled:
                return self.nextLevel.hasAddress(address, 1)
            else:
                return self.nextLevel.hasAddress(address, 0)

    # sets next level
    def setNext(self, nextCache):
        self.nextLevel = nextCache


def main():
    print("** Running SVE CacheSim **")
    # Args parser
    parser = argparse.ArgumentParser(description='SVE CacheSim\n \
  <usage> python sve-cachesim.py <memtrace-file> <cache-model.json> \n',
                                     formatter_class=RawTextHelpFormatter)
    parser.add_argument('memtrace', type=str, help='ArmIE memory trace file')
    parser.add_argument('model', type=str,
                        help='Cache model JSON file')
    parser.add_argument('-p', '--prefetch', type=str,
                        choices=['prefetch_commonStride'], help='')
    parser.add_argument('-o', '--output', type=str,
                        help='Output file (default: stdout)')
    parser.add_argument('-z', '--zipped', action='store_true',
                        help='processes gzipped memtraces')

    args = parser.parse_args()

    outputFile = args.output
    if outputFile:
        if "/" not in outputFile:
            outputFile = "./" + args.output

    json_data = open(args.model)
    data = json.load(json_data)
    UsedAddress = []
    cache_model = []

    # create empty cache model using data in config file
    for lev in range(0, data[0]["nlevels"]):
        cache_model.append(Cache(CacheConfig(data[lev + 1]["cachesize"], data[lev + 1]["linesize"], data[lev + 1]
                                             ["setsize"], data[lev + 1]["wordsize"], data[lev + 1]["latency"], data[lev + 1]["memlatency"])))

    hitcount = [0] * data[0]["nlevels"]
    accessCount = [0] * data[0]["nlevels"]
    prefetcherAccessCount = 0  # only LLC accesses
    total_hits = 0
    total_misses = 0
    totalCycles = 0
    fetch_level = data[len(data) - 1]["fetch_level"] - 1

    for lev in range(0, data[0]["nlevels"]):
        if lev != data[0]["nlevels"] - 1:
            cache_model[lev].setNext(cache_model[lev + 1])

    addresses = traceToInts(
        args.memtrace, cache_model[0].config.lineSize * cache_model[0].config.wordSize, args.zipped)

    # load in prefetch plugin if requested
    if args.prefetch:
        prefetch = load_plugin(args.prefetch)

    for addr in addresses:  # Parse each read address (includes chunks if any)
        # keep record of addresses accessed
        UsedAddress.append(addr)
        # addr is int (5k ints ~ 160kb)
        # Since the prefetcher is only using the latest 100 addresses,
        # Trim the list to save memory when using prefetcher
        if len(UsedAddress) >= 5000:
            UsedAddress = UsedAddress[4899:]

        # Use prefetcher if requested
        if args.prefetch:
            nextAddress = prefetch.plugin_prefetch(UsedAddress)

        isHit, latency = cache_model[0].hasAddress(
            addr, 0)  # non-prefetch access
        totalCycles += latency
        if isHit:
            total_hits += 1
            # Check which cache level got the hit based on latency
            for n in range(1, data[0]["nlevels"] + 1):
                accessCount[n - 1] += 1
                if latency == data[n]["latency"]:
                    hitcount[n - 1] += 1
                    break
        else:
            total_misses += 1
            for n in range(1, data[0]["nlevels"] + 1):
                accessCount[n - 1] += 1

        # check if prefetching requested
        if args.prefetch:
            # load prefetched address into requested level
            # according to the config file
            prefetcherAccessCount += 1
            isHit, latency = cache_model[fetch_level].hasAddress(
                nextAddress, 1)

    # create name for output file depending on code inputs
    configfile = args.model.split(".json")[0]
    # Supports launching cachesim from different dirs
    configfile = configfile.split("/")[-1]

    run_name = configfile + "-" + args.memtrace
    if args.prefetch:
        run_name = run_name + "." + args.prefetch

    print("========\n{}\n========".format(run_name))
    for n in range(0, data[0]["nlevels"]):
        print("l{} Hits\t\t{}".format(n + 1, hitcount[n]))
        print("l{} Accesses\t{}".format(n + 1, accessCount[n]))
        print("l{} Evicts\t{}".format(n + 1, cache_model[n].evictCounter))
        if args.prefetch and n == fetch_level:
            print("l{} Prefetch Evicts\t{}".format(
                n + 1, cache_model[n].prefEvictCounter))
        print("l{} Hit Rate\t{:.2%}".format(
            n + 1, 1.0 * hitcount[n] / accessCount[n]))
        print("l{} Miss Rate\t{:.2%}".format(n + 1, 1.0
                                             * (accessCount[n] - hitcount[n]) / accessCount[n]))
        print("l{} Evict Rate\t{:.2%}\n".format(
            n + 1, 1.0 * cache_model[n].evictCounter / accessCount[n]))

    print("Total Accesses\t{}".format(len(addresses)))
    print("Total Hits\t{}".format(total_hits))
    print("Total Misses\t{}".format(total_misses))
    print("Total Cycles\t{}".format(totalCycles))

    if outputFile:
        if not os.path.exists(os.path.dirname(outputFile)):
            try:
                os.makedirs(os.path.dirname(outputFile))
            except OSError as exc:  # Guard against race condition
                if exc.errno != errno.EEXIST:
                    raise
        with open(outputFile, 'w') as f:
            f.write("========\n{}\n========\n".format(run_name))
            for n in range(0, data[0]["nlevels"]):
                f.write("l{} Hits\t\t{}\n".format(n + 1, hitcount[n]))
                f.write("l{} Accesses\t{}\n".format(n + 1, accessCount[n]))
                f.write("l{} Evicts\t{}\n".format(
                    n + 1, cache_model[n].evictCounter))
                if args.prefetch and n == fetch_level:
                    f.write("l{} Prefetch Evicts\t{}".format(
                        n + 1, cache_model[n].prefEvictCounter))
                f.write("l{} Hit Rate\t{:.2%}\n".format(
                    n + 1, 1.0 * hitcount[n] / accessCount[n]))
                f.write("l{} Miss Rate\t{:.2%}\n".format(n + 1, 1.0
                                                         * (accessCount[n] - hitcount[n]) / accessCount[n]))
                f.write("l{} Evict Rate\t{:.2%}\n\n".format(
                    n + 1, 1.0 * cache_model[n].evictCounter / accessCount[n]))
            f.write("Total Accesses\t{}\n".format(len(addresses)))
            f.write("Total Hits\t{}\n".format(total_hits))
            f.write("Total Misses\t{}\n".format(total_misses))
            f.write("Total Cycles\t{}\n".format(totalCycles))


if __name__ == '__main__':
    main()
