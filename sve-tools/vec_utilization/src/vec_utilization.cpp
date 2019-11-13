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
#include <map>
#include <mutex>
#include <iostream>
#include <iomanip>

#include <pthread.h>

#define MIN_CHUNK_SIZE 10000

std::string outputFileName;
std::ofstream outputFile;
std::vector< std::vector<std::string> > chunks;

unsigned long totalAccesses;
unsigned long totalBytes;

std::mutex mutex;

// Key   => Bytes used
// Value => Counter
std::map<unsigned int, unsigned long> bundleInformation;

unsigned int VL;

// Threaded bundle analyzer
void *analyzeBundleChunk ( void *chunk ) {
    int *aux = (int*) chunk;
    int chunkToAnalyze = *aux;

    // Create local counters
    std::map<unsigned int, unsigned long> localBundleInformation;

    std::vector<std::string> expLine;
    for ( int i = 0; i < chunks[chunkToAnalyze].size(); i++ ) {
        // Grab a new line
        std::string line = chunks[chunkToAnalyze][i];
        // Explode the line
        int typeOfLine = getTypeOfLine(line);
        // No need to do anything for non-SVE lines
        if ( typeOfLine == SVE_LINE ) {
            explodeSveLine(line, expLine);

            // Increment SVE-specific counters
            /*
             * Check if it's a gather or a scatter
             */
            if ( (std::stoi(expLine[IS_BUNDLE]) & 0x1) != 0 ) { // scatter/gather start
                unsigned int bytes = 0;

                // Check the specific case of just 1 lane enabled
                if ( (std::stoi(expLine[IS_BUNDLE]) & 0x4) == 1 ) {
                    bytes += std::stoi(expLine[DATA_SIZE]);
                } else {
                    while ( (std::stoi(expLine[IS_BUNDLE]) & 0x4) == 0 ) {
                        bytes += std::stoi(expLine[DATA_SIZE]);

                        // Get new line
                        i++; line = chunks[chunkToAnalyze][i];
                        explodeSveLine(line, expLine);
                    }
                    // We reach here because the line was end of gather/scatter, so we still need
                    // to add those bytes
                    bytes += std::stoi(expLine[DATA_SIZE]);
                }

                // Increment counter
                if ( localBundleInformation.count(bytes) == 0 ) {
                    localBundleInformation[bytes] = 1;
                } else {
                    localBundleInformation[bytes]++;
                }

            } else { // this is not  a gather nor scatter, so it's a contiguous load/store
                // Get the number of bytes utilized and increment counter
                unsigned int bytes = std::stoi(expLine[DATA_SIZE]);
                if ( localBundleInformation.count(bytes) == 0 ) {
                    localBundleInformation[bytes] = 1;
                } else {
                    localBundleInformation[bytes]++;
                }
            }
        }
    }

    // Update global information
    // Protect this loop, concurrent threads not allowed here
    mutex.lock();
    for ( std::map<unsigned int,unsigned long>::iterator iter = localBundleInformation.begin(); iter != localBundleInformation.end(); iter++ ) {
        unsigned int bytes = iter->first;
        unsigned long counter = iter->second;
        if ( bundleInformation.count(bytes) == 0 ) { // key not present yet
            bundleInformation[bytes] = counter;
        } else {
            bundleInformation[bytes] += counter;
        }
        totalAccesses += counter;
        totalBytes += bytes * counter;
    }
    mutex.unlock();

    pthread_exit(NULL);
}

int main (int argc, char *argv[]) {
    Options opt;
    opt.readOptions(argc, argv);

    int concurrentThreads = opt.getConcurrentThreads();
    totalAccesses = 0;
    totalBytes = 0;

    VL = opt.getVL();
    pthread_t analyzeThreads[concurrentThreads];
    int chunkInUse = 0;

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
         * We only care about SVE lines
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
        } else { // non-SVE line, just read next line
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
            pthread_create(&analyzeThreads[chunkInUse], NULL, analyzeBundleChunk, (void*) value);
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
        pthread_create(&analyzeThreads[chunkInUse], NULL, analyzeBundleChunk, (void*) value);
        runningChunk[chunkInUse] = true;
    }

    // Need to wait for all the threads to finish now
    for ( int i = 0; i < concurrentThreads; i++ ) {
        if ( runningChunk[i] ) {
            pthread_join(analyzeThreads[i], NULL);
        }
        runningChunk[i] = false;
    }

    double avg_vl_utilization = ((double)(totalBytes*8) / (double)totalAccesses);

    /*
     * Print a report
     */
    if ( outputFileName.empty() ) {
        std::cout << std::fixed;
        std::cout << std::setprecision(4);
        std::cout << "VL (bits),#accesses,\%accesses" << std::endl;
        for ( std::map<unsigned int,unsigned long>::iterator iter = bundleInformation.begin(); iter != bundleInformation.end(); iter++ ) {
            unsigned int bytes = iter->first;
            unsigned long counter = iter->second;
            std::cout << bytes*8 << "," << counter << "," << ((double)counter/(double)totalAccesses)*100 << std::endl;
        }

        std::cout << std::endl;
        std::cout << "Total SVE Accesses      = " << totalAccesses << std::endl;
        std::cout << "Avg. Vector Utilization = " << avg_vl_utilization << " bits" << std::endl;
        std::cout << "Avg. Vector Utilization = " << (avg_vl_utilization / ((double)VL*8)) * 100.0 << "%" << std::endl;
    } else {
        outputFile << std::fixed;
        outputFile << std::setprecision(4);
        outputFile << "VL (bits),#accesses,\%accesses" << std::endl;
        for ( std::map<unsigned int,unsigned long>::iterator iter = bundleInformation.begin(); iter != bundleInformation.end(); iter++ ) {
            unsigned int bytes = iter->first;
            unsigned long counter = iter->second;
            outputFile << bytes*8 << "," << counter << "," << ((double)counter/(double)totalAccesses)*100 << std::endl;
        }

        outputFile << std::endl;
        outputFile << "Total SVE Accesses      = " << totalAccesses << std::endl;
        outputFile << "Avg. Vector Utilization = " << avg_vl_utilization << " bits" << std::endl;
        outputFile << "Avg. Vector Utilization = " << (avg_vl_utilization / ((double)VL*8)) * 100.0 << "%" << std::endl;
    }

    traceFile.close();
    if ( !outputFileName.empty() ) {
        outputFile.close();
    }

    return 0;
}
