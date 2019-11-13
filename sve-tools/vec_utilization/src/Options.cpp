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

/*
 * Private functions
 */
void printUsage() {
    std::cout << "vec utilization [OPTIONS] merged_memtrace_file" << std::endl;
    exit(1);
}

void printHelp() {
    std::cout << "vec utilization [OPTIONS] merged_memtrace_file" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "\t-t <threads>     Specify how many threads to use for parallel processing (default: 1)" << std::endl;
    std::cout << "\t-v <VL>          Vector length (required)" << std::endl;
    std::cout << "\t-o <outputFile>  Redirect output to <outputFile> (default: stdout)" << std::endl;
#ifdef ENABLE_GZIP
    std::cout << "\t-z               Input files are zipped. Output will be zipped as well (default: no zip)" << std::endl;
#endif
    std::cout << "\t-h               Print this help" << std::endl;
    exit(0);
}

/*
 * Public functions
 */
Options::Options() {
    outputFile = std::string();
    traceFile = std::string();
    vectorLength = 0;
    concurrentThreads = 1;
#ifdef ENABLE_GZIP
    zipped = false;
#endif
}

void Options::readOptions(int argc, char *argv[]) {
    int c;
    int fileFounds = 0;
    int optind2 = 1;
    while ( optind2 < argc ) {
#ifdef ENABLE_GZIP
        if (( c = getopt(argc, argv, "t:v:o:zh")) != -1 ) {
#else
        if (( c = getopt(argc, argv, "t:v:o:h")) != -1 ) {
#endif
            switch(c) {
                case 'o':
                    optind2++;
                    this->outputFile = std::string(argv[optind2]);
                    optind2++;
                    break;
#ifdef ENABLE_GZIP
                case 'z':
                    this->zipped = true;
                    optind2++;
                    break;
#endif
                case 'v':
                    optind2++;
                    // Vector length is passed in bits, while we will use it as bytes
                    this->vectorLength = std::stoi(argv[optind2]) / 8;
                    optind2++;
                    break;
                case 't':
                    optind2++;
                    this->concurrentThreads = std::stoi(argv[optind2]);
                    optind2++;
                    break;
                case 'h':
                    printHelp();
                    break;
                default:
                    printUsage();
                    break;
            }
        } else {
            if ( fileFounds == 0 ) {
                this->traceFile = std::string(argv[optind2]);
                if ( access(this->traceFile.c_str(), F_OK) == -1 ) {
                    std::cout << "Memory trace file not found! Exiting..." << std::endl;
                    exit(1);
                }

                fileFounds++;
            }
            optind2++;
        }
    }
    if ( fileFounds != 1 ) {
        printUsage();
    }
    if ( vectorLength == 0 ) {
        std::cout << "Vector length is required! Exiting..." << std::endl;
        exit(1);
    }
}

std::string Options::getTraceFile() {
    return traceFile;
}

std::string Options::getOutFile() {
    return outputFile;
}

unsigned int Options::getVL() {
    return vectorLength;
}

int Options::getConcurrentThreads() {
    return concurrentThreads;
}

#ifdef ENABLE_GZIP
bool Options::isZipped() {
    return zipped;
}
#endif
