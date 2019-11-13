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
#include <cassert>

#include <pthread.h>

#define BUFFER_SIZE 100000

std::vector<std::string> buffer_1;
std::vector<std::string> buffer_2;
std::string outputFileName;
std::ofstream outputFile;

// Threaded print
void *printBuffer(void *bufferId) {
    int *bufferToUse = (int*) bufferId;
    if ( *bufferToUse == 1 ) {
        if ( !outputFileName.empty() ) {
            for ( int i = 0; i < buffer_1.size(); i++ ) {
                outputFile << buffer_1[i] << std::endl;
            }
        } else {
            for ( int i = 0; i < buffer_1.size(); i++ ) {
                std::cout << buffer_1[i] << std::endl;
            }
        }
    } else if ( *bufferToUse == 2 ){
        if ( !outputFileName.empty() ) {
            for ( int i = 0; i < buffer_2.size(); i++ ) {
                outputFile << buffer_2[i] << std::endl;
            }
        } else {
            for ( int i = 0; i < buffer_2.size(); i++ ) {
                std::cout << buffer_2[i] << std::endl;
            }
        }
    }

    pthread_exit(NULL);
}

int main (int argc, char *argv[]) {
    Options opt;
    opt.readOptions(argc, argv);

    pthread_t writeThread;
    // Start pthread (won't do anything) just to make sure the first pthread_join call does not fail
    int *value = (int*) malloc(sizeof(int)*1);
    *value = 0;
    pthread_create(&writeThread, NULL, printBuffer, (void*) value);

    buffer_1 = std::vector<std::string>();
    buffer_2 = std::vector<std::string>();

    std::string sveFileName = opt.getSveTraceFile();
    std::string aarch64FileName = opt.getAarch64TraceFile();
    outputFileName = opt.getOutFile();
#ifdef ENABLE_GZIP
    bool zipped = opt.isZipped();
#endif
    bool useBuffer1 = true;

    std::cout << "########################################" << std::endl;
    std::cout << "#          SUMMARY                     #" << std::endl;
    std::cout << "########################################" << std::endl;
    std::cout << "# Aarch64 memtrace file: " << aarch64FileName << std::endl;
    std::cout << "# SVE memtrace file:     " << sveFileName << std::endl;
    std::cout << "# Output:                " << (outputFileName.empty() ? "stdout" : outputFileName) << std::endl;
#ifdef ENABLE_GZIP
    std::cout << "# Zipped files:          " << (isZipped ? "YES" : "NO") << std::endl;
#endif
    std::cout << "########################################" << std::endl;

    /*
     * First of all, open files
     */
    std::ifstream sveFile(sveFileName);
    std::ifstream aarch64File(aarch64FileName);

    if ( !outputFileName.empty() ) {
        outputFile = std::ofstream(outputFileName);
    }

    // Ignore first line of aarch64 file. It contains the headers
    passLine(aarch64File);

    std::string sveLine;
    std::string aarch64Line;

    unsigned long seqNumberAarch64;
    bool stopSVE = false;
    bool stopAarch64 = false;

    // Grab first line of SVE file.
    // This pair contains seqNumber in first, stop/start values in second
    std::pair<unsigned long,int> sveInfo = readSVELine(sveFile, sveLine);
    assert(sveInfo.second == -1);

    // Read first meaningful line for aarch64 file
    seqNumberAarch64 = readAarch64Line(aarch64File, aarch64Line);

    // Skip aarch64 memtrace till sequence number is equal or greater than sve sequence number
    while ( seqNumberAarch64 < sveInfo.first ) {
        seqNumberAarch64 = readAarch64Line(aarch64File, aarch64Line);
    }

    // Read the first meaningful line of the SVE file
    sveInfo = readSVELine(sveFile, sveLine);


    while ( !stopSVE || !stopAarch64 ) {
        // Now, if one file is already finished, just output the other one till you finish
        // If not, check sequence numbers, write the one that's smaller
        bool printSVE = false;
        bool printAarch64 = false;
        if ( stopAarch64 ) {
            printSVE = true;
        } else if ( stopSVE ) {
            printAarch64 = true;
        } else if ( sveInfo.first > seqNumberAarch64 ) {
            printAarch64 = true;
        } else {
            printSVE = true;
        }

        if ( printSVE ) {
            // Check if buffer is full, if so, print and swap buffers
            if ( useBuffer1 ) {
                buffer_1.push_back(std::string(sveLine));
            } else {
                buffer_2.push_back(std::string(sveLine));
            }
            sveInfo = readSVELine(sveFile, sveLine);
            if ( sveInfo.second == -2 || sveInfo.first == 0 ) {
                stopSVE = true;
            }
        }
        if ( printAarch64 ) {
            if ( useBuffer1) {
                buffer_1.push_back(std::string(aarch64Line));
            } else {
                buffer_2.push_back(std::string(aarch64Line));
            }
            seqNumberAarch64 = readAarch64Line(aarch64File, aarch64Line);
            if ( seqNumberAarch64 == 0 || (seqNumberAarch64 > sveInfo.first && stopSVE)  ) {
                stopAarch64 = true;
            }
        }
        if ( useBuffer1) {
            if ( buffer_1.size() > BUFFER_SIZE ) {
                pthread_join(writeThread, NULL);
                *value = 1;
                pthread_create(&writeThread, NULL, printBuffer, (void*) value);
                useBuffer1 = false;
                buffer_2 = std::vector<std::string>();
            }
        } else {
            if ( buffer_2.size() > BUFFER_SIZE ) {
                pthread_join(writeThread, NULL);
                *value = 2;
                pthread_create(&writeThread, NULL, printBuffer, (void*) value);
                useBuffer1 = true;
                buffer_1 = std::vector<std::string>();
            }
        }
    }

    // Need to print the remaining things in the buffer here
    pthread_join(writeThread, NULL);
    if ( useBuffer1) {
        *value = 1;
    } else {
        *value = 2;
    }
    pthread_create(&writeThread, NULL, printBuffer, (void*) value);
    pthread_join(writeThread, NULL);

    sveFile.close();
    aarch64File.close();
    if ( !outputFileName.empty() ) {
        outputFile.close();
    }

    return 0;
}
