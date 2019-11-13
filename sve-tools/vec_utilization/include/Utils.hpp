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

enum lineType { SVE_LINE, AARCH64_LINE, END_OF_FILE };

enum lineFields { SEQ_NUMBER = 0,
    THREAD_ID = 1,
    IS_BUNDLE = 2,
    IS_WRITE = 3,
    DATA_SIZE = 4,
    DATA_ADDRESS = 5,
    PC = 6
};

void explodeSveLine ( std::string &line, std::vector<std::string> &explodedLine ) {
    explodedLine = std::vector<std::string>();
    std::stringstream ss(line);
    while ( ss.good() ) {
        std::string substr;
        std::getline ( ss, substr, ',' );
        explodedLine.push_back ( substr );
    }
}

void explodeAarch64Line ( std::string &line, std::vector<std::string> &explodedLine ) {
    // Deal with the first : separator
    explodedLine = std::vector<std::string>();
    std::stringstream ss(line);
    std::string firstValue, restOfLine;
    std::getline(ss, firstValue, ':');
    explodedLine.push_back(firstValue);
    std::getline(ss, restOfLine, ':');

    // Now process the rest of the line as usual
    ss = std::stringstream(restOfLine);
    while ( ss.good() ) {
        std::string substr;
        std::getline ( ss, substr, ',' );
        explodedLine.push_back ( substr );
    }
}

bool isGatherScatterStart ( std::string &line ) {
    std::string substr;
    std::stringstream ss(line);
    // Now, get the third value of the line
    std::getline( ss, substr, ',');
    std::getline( ss, substr, ',');
    std::getline( ss, substr, ',');

    if ( (std::stoi(substr) & 0x1) != 0 ) { // Gather/Scatter start
        return true;
    }

    return false;
}

bool isGatherScatterEnd ( std::string &line ) {
    std::string substr;
    std::stringstream ss(line);
    // Now, get the third value of the line
    std::getline( ss, substr, ',');
    std::getline( ss, substr, ',');
    std::getline( ss, substr, ',');

    if ( (std::stoi(substr) & 0x4) != 0 ) { // Gather/Scatter end
        return true;
    }
    return false;
}

int getTypeOfLine(std::string &line) {
    if (line.find(":") != std::string::npos ) {
        return AARCH64_LINE;
    }
    return SVE_LINE;
}

int readLine(std::ifstream &is, std::string &line) {
#ifdef ENABLE_GZIP
#else
    std::getline(is, line);
    if ( line.find(":") != std::string::npos ) {
        return AARCH64_LINE;
    }

    if ( line.find(",") != std::string::npos ) {
        return SVE_LINE;
    }

    return END_OF_FILE;
#endif
}
