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
 
/* This example is to be compiled with SVE support and run through
 * the Arm Instruction Emulator (ArmIE) with the memory tracing client.
 */

#include <stdio.h>

#define N 42
int a[N], b[N], c[N], d[N], e[N], f[N];

#define __START_TRACE() { asm volatile (".inst 0x2520e020"); }
#define __STOP_TRACE() { asm volatile (".inst 0x2520e040"); }


int main(void) {
  d[0] = 2;
  e[0] = 4;
  f[0] = d[0]+e[0];

  for (int j=0; j<N; ++j)
    d[j] = e[j]+e[f[j]];

  __START_TRACE();
  a[0] = 0;
  b[0] = 1;
  c[0] = a[0] + b[0];

  for(int i=0; i<N; ++i)
    c[i] = i;

  for(int i=0; i<N; ++i){
    a[i] = b[i] + b[c[i]];
  }
__STOP_TRACE();
}
