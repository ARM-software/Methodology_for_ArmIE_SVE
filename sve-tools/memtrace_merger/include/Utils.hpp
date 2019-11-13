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

#include <sstream>
#include <fstream>
#include <vector>
#include <iostream>
#include <exception>

std::pair<unsigned long,int> readSVELine(std::ifstream &is, std::string &line) {
#ifdef ENABLE_GZIP
#else
    if ( std::getline(is, line) ) {
        std::stringstream ss(line);
        std::vector<std::string> aux;
        while ( ss.good() ) {
            std::string substr;
            std::getline ( ss, substr, ',' );
            aux.push_back( substr );
        }

        //aux[0] contains the sequence number, aux[1] contains -1 if start trace, -2 if stop trace
        return std::pair<unsigned long, int>(std::stoul(aux[0]), std::stoi(aux[1]));
    }
    // If we got here, we've reach the end of the file, just set seq number to 0 and stop the trace with -2
    return std::pair<unsigned long, int>(0, -2);
#endif
}

// No need to return two positions here, start and stop trace values are only in the SVE trace
unsigned long readAarch64Line(std::ifstream &is, std::string &line) {
#ifdef ENABLE_GZIP
#else
    std::vector<std::string> aux;
    if ( std::getline(is, line) ) {
        std::stringstream ss(line);
        std::string substr;
        std::getline(ss, substr, ':');
        aux.push_back(substr);

        return std::stoul(aux[0]);
    }
    return 0;
#endif
}


void passLine(std::ifstream &is) {
#ifdef ENABLE_GZIP
#else
    std::string s;
    std::getline(is, s);
#endif
}
