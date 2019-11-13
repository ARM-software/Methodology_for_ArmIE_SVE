/*
 * Copyright (c) 2019, Arm Limited and Contributors.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Options.hpp"
#include "Utils.hpp"

#include <fstream>
#include <vector>
#include <atomic>
#include <cassert>

#include <pthread.h>

#define MIN_CHUNK_SIZE 10000

std::string outputFileName;
std::ofstream outputFile;
std::vector< std::vector<std::string> > chunks;

unsigned int VL;

std::atomic<unsigned long> totalAccesses;
std::atomic<unsigned long> aarch64Accesses;
std::atomic<unsigned long> aarch64Loads;
std::atomic<unsigned long> aarch64Stores;
std::atomic<unsigned long> sveAccesses;
std::atomic<unsigned long> gathersFullPredicate;
std::atomic<unsigned long> gathersWithDisabledLanes;
std::atomic<unsigned long> scattersFullPredicte;
std::atomic<unsigned long> scattersWithDisabledLanes;
std::atomic<unsigned long> contigLoadsFullPredicate;
std::atomic<unsigned long> contigLoadsWithDisabledLanes;
std::atomic<unsigned long> contigStoresFullPredicate;
std::atomic<unsigned long> contigStoresWithDisabledLanes;

// Threaded analyzer
void *analyzeChunk ( void *chunk ) {
    int *aux = (int*) chunk;
    int chunkToAnalyze = *aux;

    // Create local counters
    unsigned long localTotalAccesses = 0;
    unsigned long localAarch64Accesses = 0;
    unsigned long localAarch64Loads = 0;
    unsigned long localAarch64Stores = 0;
    unsigned long localSveAccesses = 0;
    unsigned long localGathersFullPredicate = 0;
    unsigned long localGathersWithDisabledLanes = 0;
    unsigned long localScattersFullPredicte = 0;
    unsigned long localScattersWithDisabledLanes = 0;
    unsigned long localContigLoadsFullPredicate = 0;
    unsigned long localContigLoadsWithDisabledLanes = 0;
    unsigned long localContigStoresFullPredicate = 0;
    unsigned long localContigStoresWithDisabledLanes = 0;

    std::vector<std::string> expLine;
    for ( int i = 0; i < chunks[chunkToAnalyze].size(); i++ ) {
        // Grab a new line
        std::string line = chunks[chunkToAnalyze][i];
        // Explode the line
        int typeOfLine = getTypeOfLine(line);
        if ( typeOfLine == AARCH64_LINE ) {
            explodeAarch64Line ( line, expLine );
            // Increment counters specific to aarch64 lines
            if ( std::stoi(expLine[IS_WRITE]) == 1 ) {
                localAarch64Stores++;
            } else {
                localAarch64Loads++;
            }
            localAarch64Accesses++;
        } else { // SVE_LINE
            explodeSveLine(line, expLine);

            // Increment SVE-specific counters
            /*
             * Check if it's a gather or a scatter
             */
            if ( (std::stoi(expLine[IS_BUNDLE]) & 0x1) != 0 ) { // scatter/gather start
                int bytesUsed = 0;

                // Check the specific case of just 1 lane enabled
                if ( (std::stoi(expLine[IS_BUNDLE]) & 0x4) == 1 ) {
                    bytesUsed += std::stoi(expLine[DATA_SIZE]);
                } else {
                    while ( (std::stoi(expLine[IS_BUNDLE]) & 0x4) == 0 ) {
                        bytesUsed += std::stoi(expLine[DATA_SIZE]);

                        // Get new line
                        i++; line = chunks[chunkToAnalyze][i];
                        explodeSveLine(line, expLine);
                    }
                    // We reach here because the line was end of gather/scatter, so we still need
                    // to add those bytes
                    bytesUsed += std::stoi(expLine[DATA_SIZE]);
                }

                if ( std::stoi(expLine[IS_WRITE]) == 1 ) {
                    if ( bytesUsed == VL ) {
                        localScattersFullPredicte++;
                    } else {
                        localScattersWithDisabledLanes++;
                    }
                } else {
                    if ( bytesUsed == VL ) {
                        localGathersFullPredicate++;
                    } else {
                        localGathersWithDisabledLanes++;
                    }
                }
            } else { // this is not  a gather nor scatter, so it's a contiguous load/store
                if ( std::stoi(expLine[IS_WRITE]) ) {
                    if ( std::stoi(expLine[DATA_SIZE]) == VL ) {
                        localContigStoresFullPredicate++;
                    } else {
                        localContigStoresWithDisabledLanes++;
                    }
                } else {
                    if ( std::stoi(expLine[DATA_SIZE]) == VL ) {
                        localContigLoadsFullPredicate++;
                    } else {
                        localContigLoadsWithDisabledLanes++;
                    }
                }
            }
            localSveAccesses++;
        }
        localTotalAccesses++;
    }

    // Update global counters
    totalAccesses += localTotalAccesses;
    aarch64Accesses += localAarch64Accesses;
    aarch64Loads += localAarch64Loads;
    aarch64Stores += localAarch64Stores;
    sveAccesses += localSveAccesses;
    gathersFullPredicate += localGathersFullPredicate;
    gathersWithDisabledLanes += localGathersWithDisabledLanes;
    scattersFullPredicte += localScattersFullPredicte;
    scattersWithDisabledLanes += localScattersWithDisabledLanes;
    contigLoadsFullPredicate += localContigLoadsFullPredicate;
    contigLoadsWithDisabledLanes += localContigLoadsWithDisabledLanes;
    contigStoresFullPredicate += localContigStoresFullPredicate;
    contigStoresWithDisabledLanes += localContigStoresWithDisabledLanes;

    pthread_exit(NULL);
}

int main (int argc, char *argv[]) {
    Options opt;
    opt.readOptions(argc, argv);

    int concurrentThreads = opt.getConcurrentThreads();

    VL = opt.getVL();
    pthread_t analyzeThreads[concurrentThreads];
    int chunkInUse = 0;

    totalAccesses = 0;
    aarch64Accesses = 0;
    sveAccesses = 0;
    gathersFullPredicate = 0;
    gathersWithDisabledLanes = 0;
    scattersFullPredicte = 0;
    scattersWithDisabledLanes = 0;
    contigLoadsFullPredicate = 0;
    contigLoadsWithDisabledLanes = 0;
    contigStoresFullPredicate = 0;
    contigStoresWithDisabledLanes = 0;

    chunks = std::vector< std::vector<std::string> >(concurrentThreads);

    std::vector<bool> runningChunk(concurrentThreads, false);

    std::string traceFileName = opt.getTraceFile();
    outputFileName = opt.getOutFile();
#ifdef ENABLE_GZIP
    bool zipped = opt.isZipped();
#endif
    bool useBuffer1 = true;

    std::cout << "########################################" << std::endl;
    std::cout << "#          SUMMARY                     #" << std::endl;
    std::cout << "########################################" << std::endl;
    std::cout << "# VL:                    " << VL * 8 << " bits" << std::endl;
    std::cout << "# Memtrace file:         " << traceFileName << std::endl;
    std::cout << "# Output:                " << (outputFileName.empty() ? "stdout" : outputFileName) << std::endl;
#ifdef ENABLE_GZIP
    std::cout << "# Zipped files:          " << (isZipped ? "YES" : "NO") << std::endl;
#endif
    std::cout << "########################################" << std::endl;

    /*
     * First of all, open files
     */
    std::ifstream traceFile(traceFileName);
    if ( !outputFileName.empty() ) {
        outputFile = std::ofstream(outputFileName);
    }

    std::string line;
    std::vector<std::string> chunkContents;
    chunkContents.clear();

    int typeOfLine;
    typeOfLine = readLine(traceFile, line);
    while ( typeOfLine != END_OF_FILE ) {
        /*
         * Check the line and add it to the chunk
         */
        if ( typeOfLine == SVE_LINE ) {
            // Check if it is a gather/scatter
            if ( isGatherScatterStart(line) ) {
                // Process the whole gather/scatter
                chunkContents.push_back(line);
                while ( ! isGatherScatterEnd(line) ) {
                    typeOfLine = readLine(traceFile, line);
                    chunkContents.push_back(line);
                }
                typeOfLine = readLine(traceFile, line);
            } else {
                // If is not a gather/scatter, just add the line to the chunk
                chunkContents.push_back(line);
                typeOfLine = readLine(traceFile, line);
            }
        } else if ( typeOfLine == AARCH64_LINE ) {
            // Just add the line to the chunk
            chunkContents.push_back(line);
            typeOfLine = readLine(traceFile, line);
        }

        /*
         * If we've completed a chunk, spawn a thread to process it
         */
        if ( chunkContents.size() >= MIN_CHUNK_SIZE ) {
            if ( runningChunk[chunkInUse] ) { // if the thread is running, wait for it
                pthread_join( analyzeThreads[chunkInUse], NULL);
            }
            chunks[chunkInUse] = std::vector<std::string>(chunkContents);
            chunkContents.clear();
            // Spawn an analysis thread
            int *value = (int*) malloc(sizeof(int)*1);
            *value = chunkInUse;
            pthread_create(&analyzeThreads[chunkInUse], NULL, analyzeChunk, (void*) value);
            runningChunk[chunkInUse] = true;
            chunkInUse++;
            if ( chunkInUse == concurrentThreads ) {
                chunkInUse = 0;
            }
        }
    }

    // Check if we reach EOF before filling a chunk
    if ( chunkContents.size() <= MIN_CHUNK_SIZE ) {
        if ( runningChunk[chunkInUse] ) {
            pthread_join( analyzeThreads[chunkInUse], NULL);
        }
        chunks[chunkInUse] = std::vector<std::string>(chunkContents);
        chunkContents.clear();
        int *value = (int*) malloc(sizeof(int)*1);
        *value = chunkInUse;
        pthread_create(&analyzeThreads[chunkInUse], NULL, analyzeChunk, (void*) value);
        runningChunk[chunkInUse] = true;
    }

    // Need to wait for all the threads to finish now
    for ( int i = 0; i < concurrentThreads; i++ ) {
        if ( runningChunk[i] ) {
            pthread_join(analyzeThreads[i], NULL);
        }
        runningChunk[i] = false;
    }

    // Load information
    unsigned long totalLoads = aarch64Loads + contigLoadsFullPredicate + contigLoadsWithDisabledLanes + gathersFullPredicate + gathersWithDisabledLanes;
    unsigned long sveLoads = contigLoadsFullPredicate + contigLoadsWithDisabledLanes + gathersFullPredicate + gathersWithDisabledLanes;
    unsigned long sveContiguousLoads = contigLoadsWithDisabledLanes + contigLoadsFullPredicate;


    unsigned long sveGathers = gathersFullPredicate + gathersWithDisabledLanes;

    // Store information
    unsigned long totalStores = aarch64Stores + contigStoresFullPredicate + contigStoresWithDisabledLanes + scattersFullPredicte + scattersWithDisabledLanes;
    unsigned long sveStores = contigStoresFullPredicate + contigStoresWithDisabledLanes + scattersFullPredicte + scattersWithDisabledLanes;
    unsigned long sveContiguousStores = contigStoresFullPredicate + contigStoresWithDisabledLanes;;
    unsigned long sveScatters = scattersFullPredicte + scattersWithDisabledLanes;

    // Total information
    unsigned long sveContiguousAllLanes = contigLoadsFullPredicate + contigStoresFullPredicate;
    unsigned long sveContiguousDisLanes = contigLoadsWithDisabledLanes + contigStoresWithDisabledLanes;
    unsigned long sveContiguous = sveContiguousAllLanes + sveContiguousDisLanes;
    unsigned long sveGathersScattersAllLanes = gathersFullPredicate + scattersFullPredicte;
    unsigned long sveGathersScattersDisLanes = gathersWithDisabledLanes + scattersWithDisabledLanes;
    unsigned long sveGathersScatters = sveGathersScattersAllLanes + sveGathersScattersDisLanes;

    /*
     * Print a report
     */
    if ( outputFileName.empty() ) {
        // Header
        std::cout << "load/store/total,Total,SVE,non-SVE,SVE-contiguous,SVE-contig-allLanes,SVE-contig-disLanes,SVE-gather/scatter,SVE-gather/scatter-allLanes,SVE-gather/scatter-disLanes" << std::endl;

        // Load info
        std::cout << "load," << totalLoads << "," << sveLoads << "," << aarch64Loads << "," << sveContiguousLoads << "," << contigLoadsFullPredicate << "," << contigLoadsWithDisabledLanes << "," << sveGathers << "," << gathersFullPredicate << "," << gathersWithDisabledLanes << std::endl;

        // Store info
        std::cout << "store," << totalStores << "," << sveStores << "," << aarch64Stores << "," << sveContiguousStores << "," << contigStoresFullPredicate << "," << contigStoresWithDisabledLanes << "," << sveScatters << "," << scattersFullPredicte << "," << scattersWithDisabledLanes << std::endl;

        // Total info
        std::cout << "total," << totalAccesses << "," << sveAccesses << "," << aarch64Accesses << "," << sveContiguous << "," << sveContiguousAllLanes << "," << sveContiguousDisLanes << "," << sveGathersScatters << "," << sveGathersScattersAllLanes << "," << sveGathersScattersDisLanes << std::endl;

    } else {
        // Header
        outputFile << "load/store/total,Total,SVE,non-SVE,SVE-contiguous,SVE-contig-allLanes,SVE-contig-disLanes,SVE-gather/scatter,SVE-gather/scatter-allLanes,SVE-gather/scatter-disLanes" << std::endl;

        // Load info
        outputFile << "load," << totalLoads << "," << sveLoads << "," << aarch64Loads << "," << sveContiguousLoads << "," << contigLoadsFullPredicate << "," << contigLoadsWithDisabledLanes << "," << sveGathers << "," << gathersFullPredicate << "," << gathersWithDisabledLanes << std::endl;

        // Store info
        outputFile << "store," << totalStores << "," << sveStores << "," << aarch64Stores << "," << sveContiguousStores << "," << contigStoresFullPredicate << "," << contigStoresWithDisabledLanes << "," << sveScatters << "," << scattersFullPredicte << "," << scattersWithDisabledLanes << std::endl;

        // Total info
        outputFile << "total," << totalAccesses << "," << sveAccesses << "," << aarch64Accesses << "," << sveContiguous << "," << sveContiguousAllLanes << "," << sveContiguousDisLanes << "," << sveGathersScatters << "," << sveGathersScattersAllLanes << "," << sveGathersScattersDisLanes << std::endl;
    }

    traceFile.close();
    if ( !outputFileName.empty() ) {
        outputFile.close();
    }

    return 0;
}
