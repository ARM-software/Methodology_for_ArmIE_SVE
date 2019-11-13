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

# Common Stride Prefetcher Plugin for the SVE cache Simulator

# Avoids compatibility issues with Counter.most_common() and equally common strides between different python versions
def most_common(l):
    d = {}
    for e in l:
        if not e in d:
            d[e] = 0
        d[e] += 1
    sl = list(d.items())
    sl.sort(key=lambda x: (x[1], abs(x[0])), reverse=True)
    return sl[0][1]


def plugin_prefetch(UsedAddress):  # this MUST be the name given
    # Check the latest 100 used addresses
    if len(UsedAddress) > 100:
        begin = len(UsedAddress) - 100
    else:
        begin = 0

    stride = []
    # chunk of read addresses to examine for stride length
    chunk = UsedAddress[begin:]
    for i in range(0, len(chunk) - 1):
        length = chunk[i + 1] - chunk[i]
        if length != 0:
            stride.append(length)  # look for stride in reads
    if len(stride) > 0:
        commonStride = most_common(stride)
        fetch = chunk[len(chunk) - 1] + commonStride
    else:  # No fetch
        fetch = chunk[len(chunk) - 1]
    return fetch
