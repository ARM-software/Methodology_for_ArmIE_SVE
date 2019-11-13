#!/usr/bin/python3

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

# Number of Floating Point operations per byte of data accessed

# This app requires a meminstrace file and the vector length used.
# It outputs the average number of Floating Point instructions/ops per byte loaded from memory.
# To achieve this, it decodes the instrace and keeps track of all armv8+sve registers.
# Every time a FLOP is found, it increments the counter for all the src registers.
# Every time a load is found, it updates the FLOP/Byte metric and increments the
# byte size counter for the dest register.
# At the end of the trace, it updates the FLOP/Byte for all registers (if available)
# and outputs the arithmetic and harmonic means.

import argparse
from argparse import RawTextHelpFormatter
import gzip
import subprocess
import os
import re
import sys

DEBUG = False
DEBUG_EXTRA = False

# Range of registers available in AArch64+sve
z_range = 32
x_range = 31
w_range = 31
q_range = 16
d_range = 32

# Number of SVE lanes
MAXSIZE = 0

mc = ''


def opcodeMask(opcode):
    opc = int(opcode, 16)

    # Masks for SVE, SIMD and scalar float instructions
    sve_mask = 0x64000000
    # TODO this mask still includes non float SIMD and crypto instructions
    simd_float_mask = 0x0E000000

    if sve_mask == (sve_mask & opc):
        if DEBUG_EXTRA:
            print("Binary opc = {:#034b}".format(opc))
            print("SVE Mask   = {:#034b}".format(0x64000000))
            print("Mask out   = {:#034b}".format(sve_mask & opc))
        return True

    elif simd_float_mask == (simd_float_mask & opc):
        if DEBUG_EXTRA:
            print("Binary opc = {:#034b}".format(opc))
            print("SIMD Mask  = {:#034b}".format(0x0E000000))
            print("Mask out   = {:#034b}".format(simd_float_mask & opc))
        return True

    return False


# Registers are grouped into the different types.
# Each register has a counter for total #flops and #bytes
# All registers start with 1B accessed to avoid 'divide by 0' errors in the first access.
z_regs = [dict([('flops', 0.0), ('bytes', 0)]) for z in range(z_range)]
x_regs = [dict([('flops', 0.0), ('bytes', 0)]) for x in range(x_range)]
w_regs = [dict([('flops', 0.0), ('bytes', 0)]) for w in range(w_range)]
q_regs = [dict([('flops', 0.0), ('bytes', 0)]) for q in range(q_range)]
d_regs = [dict([('flops', 0.0), ('bytes', 0)]) for d in range(d_range)]

# Counters for FLOPs/Byte average
flops_byte_cntr = 0
flops_byte_g = []
flops_byte_inv = []

## Register Update functions ##


def updateRegFlops(reg, idx, flops):
    if('z' in reg):
        z_regs[idx]['flops'] += flops
    elif('x' in reg):
        x_regs[idx]['flops'] += flops
    elif('w' in reg):
        w_regs[idx]['flops'] += flops
    elif('q' in reg):
        q_regs[idx]['flops'] += flops
    elif('d' in reg):
        d_regs[idx]['flops'] += flops


def resetRegFlops(reg, idx):
    if('z' in reg):
        z_regs[idx]['flops'] = 0
    elif('x' in reg):
        x_regs[idx]['flops'] = 0
    elif('w' in reg):
        w_regs[idx]['flops'] = 0
    elif('q' in reg):
        q_regs[idx]['flops'] = 0
    elif('d' in reg):
        d_regs[idx]['flops'] = 0


def updateRegBytes(reg, idx, bytes):
    if('z' in reg):
        z_regs[idx]['bytes'] += bytes
    elif('x' in reg):
        x_regs[idx]['bytes'] += bytes
    elif('w' in reg):
        w_regs[idx]['bytes'] += bytes
    elif('q' in reg):
        q_regs[idx]['bytes'] += bytes
    elif('d' in reg):
        d_regs[idx]['bytes'] += bytes


def resetRegBytes(reg, idx):
    if('z' in reg):
        z_regs[idx]['bytes'] = 0
    elif('x' in reg):
        x_regs[idx]['bytes'] = 0
    elif('w' in reg):
        w_regs[idx]['bytes'] = 0
    elif('q' in reg):
        q_regs[idx]['bytes'] = 0
    elif('d' in reg):
        d_regs[idx]['bytes'] = 0


def updateFlopsByte(reg, idx, veclen):
    global flops_byte_cntr
    global flops_byte_g
    global flops_byte_inv

    num_el = veclen
    upd_cntr = 0
    if DEBUG:
        print("..\nUpdate FLOP/Byte")
        print("Prev flops_byte_g {}".format(flops_byte_g))
        print("Prev flops_byte_inv {}".format(flops_byte_inv))
    for i in range(MAXSIZE):
        if DEBUG:
            print("Num_elem = {}".format(num_el))
        if('z' in reg):
            if z_regs[idx]['flops'] != 0 and z_regs[idx]['bytes'] != 0:
                flops_byte_g[i] += (z_regs[idx]['flops']
                                    * num_el) / z_regs[idx]['bytes']
                flops_byte_inv[i] += z_regs[idx]['bytes'] / \
                    (z_regs[idx]['flops'] * num_el)
                upd_cntr = 1
                if DEBUG:
                    print("{} flops[{}]/Byte[{}]. new flops_g {} | inv={}. Cntr={}".format(reg, \
                         z_regs[idx]['flops'] * num_el, z_regs[idx]['bytes'], \
                         flops_byte_g[i], flops_byte_inv[i], flops_byte_cntr + 1))
        elif('x' in reg):
            if x_regs[idx]['flops'] != 0 and x_regs[idx]['bytes'] != 0:
                flops_byte_g[i] += (x_regs[idx]['flops']
                                    * num_el) / x_regs[idx]['bytes']
                flops_byte_inv[i] += x_regs[idx]['bytes'] / \
                    (x_regs[idx]['flops'] * num_el)
                upd_cntr = 1
                if DEBUG:
                    print("{} flops[{}]/Byte[{}]. new flops_g {} | inv={}. Cntr={}".format(reg, \
                         x_regs[idx]['flops'] * num_el, x_regs[idx]['bytes'], \
                         flops_byte_g[i], flops_byte_inv[i], flops_byte_cntr + 1))
        elif('w' in reg):
            if w_regs[idx]['flops'] != 0 and w_regs[idx]['bytes'] != 0:
                flops_byte_g[i] += (w_regs[idx]['flops']
                                    * num_el) / w_regs[idx]['bytes']
                flops_byte_inv[i] += w_regs[idx]['bytes'] / \
                    (w_regs[idx]['flops'] * num_el)
                upd_cntr = 1
                if DEBUG:
                    print("{} flops[{}]/Byte[{}]. new flops_g {} | inv={}. Cntr={}".format(reg, \
                         w_regs[idx]['flops'] * num_el, w_regs[idx]['bytes'], \
                         flops_byte_g[i], flops_byte_inv[i], flops_byte_cntr + 1))
        elif('q' in reg):
            if q_regs[idx]['flops'] != 0 and q_regs[idx]['bytes'] != 0:
                flops_byte_g[i] += (q_regs[idx]['flops']
                                    * num_el) / q_regs[idx]['bytes']
                flops_byte_inv[i] += q_regs[idx]['bytes'] / \
                    (q_regs[idx]['flops'] * num_el)
                upd_cntr = 1
                if DEBUG:
                    print("{} flops[{}]/Byte[{}]. new flops_g {} | inv={}. Cntr={}".format(reg, \
                         q_regs[idx]['flops'] * num_el, q_regs[idx]['bytes'], \
                         flops_byte_g[i], flops_byte_inv[i], flops_byte_cntr + 1))
        elif('d' in reg):
            if d_regs[idx]['flops'] != 0 and d_regs[idx]['bytes'] != 0:
                flops_byte_g[i] += (d_regs[idx]['flops']
                                    * num_el) / d_regs[idx]['bytes']
                flops_byte_inv[i] += d_regs[idx]['bytes'] / \
                    (d_regs[idx]['flops'] * num_el)
                upd_cntr = 1
                if DEBUG:
                    print("{} flops[{}]/Byte[{}]. new flops_g {} | inv={}. Cntr={}".format(reg, \
                         d_regs[idx]['flops'] * num_el, d_regs[idx]['bytes'], \
                         flops_byte_g[i], flops_byte_inv[i], flops_byte_cntr + 1))
        num_el /= 2
        i += 1
    if upd_cntr:  # Update the average cntr just once
        flops_byte_cntr += 1
    resetRegFlops(reg, idx)
    if DEBUG:
        print("..")


## Instruction decoding ##
def swap32(x):
    """Swap bytes in 32 bit integer."""
    return (((x << 24) & 0xFF000000) |
            ((x << 8) & 0x00FF0000) |
            ((x >> 8) & 0x0000FF00) |
            ((x >> 24) & 0x000000FF))


def encoding_to_bytes(encoding, reverse=True):
    """Format encoding.

    encoding_to_bytes(0x4000000) -> "0x00 0x00 0x00 0x04"
    encoding_to_bytes(0x4000000, reverse=False) -> "0x04 0x00 0x00 0x00"

    """
    h = '{:08x}'.format(swap32(encoding) if reverse else encoding)
    return ' '.join('0x{}'.format(h[i:i + 2]) for i in range(0, 8, 2))


def instr_decoder(ins_opcode):
    mc_args = [mc, '-disassemble', '-triple=aarch64', '-mattr=+sve']
    # Decode instruction to get register information
    echo = subprocess.Popen(('echo', encoding_to_bytes(
        int(ins_opcode, 0))), stdout=subprocess.PIPE)

    decline = subprocess.check_output(
        mc_args, stdin=echo.stdout, stderr=subprocess.STDOUT)

    decline = decline.decode('ascii')
    return decline


def main():
    parser = argparse.ArgumentParser(description='App to extract FLOPs/Byte from ArmIE traces.',
                                     formatter_class=RawTextHelpFormatter)
    parser.add_argument('meminstrace', type=str,
                        help='meminstrace input file.')
    # parser.add_argument('veclen', type=int, choices=[128, 256, 384, 512, 640, 768, 896,
    # 1024, 1152, 1280, 1408, 1536, 1664, 1792, 1920, 2048], help='SVE vector length.')
    parser.add_argument('veclen', type=int, choices=[
                        128, 256, 512, 1024, 2048], help='SVE vector length. (only powers of 2 allowed)')
    parser.add_argument('-q', '--quad', action='store_true',
                        help='reports quad-precision numbers')
    parser.add_argument('-z', '--zipped', action='store_true',
                        help='processes gzipped traces')

    args = parser.parse_args()

    global MAXSIZE
    global flops_byte_g
    global flops_byte_inv
    global mc
    # We compute FLOP/Byte assuming a constant number of lanes across all combinations
    # up until double or quad precision if requested.
    # TODO if predicate is available in the future, update this.
    if args.quad:  # Quad precision - Up to 16B per lane
        MAXSIZE = 5  # 1B, 2B, 4B, 8B, 16B lanes
    else:  # Double precision - up to 8B per lane
        MAXSIZE = 4  # 1B, 2B, 4B, 8B lanes

    flops_byte_g = [0.0] * MAXSIZE
    flops_byte_inv = [0.0] * MAXSIZE

    isMemTrace = 0  # Identifies a memory trace line

    # Read the memory and instruction trace files (gzip compatible)
    if args.zipped:
        meminstrace = gzip.open(args.meminstrace, 'r')
    else:
        meminstrace = open(args.meminstrace, 'r')

    # Set the llvm-mc location based on the env var
    mc = os.environ.get('LLVM_MC')
    if not mc:
        sys.stderr.write('error: LLVM_MC env variable not set\n')
        sys.exit(1)

    # Read the header lines of the trace file
    traceline = meminstrace.readline()
    while (traceline.startswith("Format") or traceline.startswith("Instrace") or traceline.startswith("Memtrace")):
        traceline = meminstrace.readline()

    while traceline:
        # Fully split the meminstrace line
        splitTrace = re.split(',|;', traceline)

        # Instrace lines
        if len(splitTrace) == 3:
            isMemTrace = 0
            # ins_pc = splitTrace[0]
            ins_opcode = splitTrace[1]
            ins_isMem = int(splitTrace[2])

            decline = instr_decoder(ins_opcode=ins_opcode)
            if 'invalid instruction encoding' in decline:
                # decline = 'UNKNOWN'
                traceline = meminstrace.readline()
                continue
            else:
                decline = decline.split('\t.text\n', 1)[
                    1].split('\n')[:-1][0].strip()

            if not ins_isMem:
                # If float instruction, increment #FLOPs
                if opcodeMask(ins_opcode):
                    if DEBUG_EXTRA:
                        print('==== {} -> {}'.format(ins_opcode, decline))

                    decline = re.split('\t', decline)[1]
                    decline = re.split(',', re.sub("\s+", "", decline))

                    # get list of dest and src registers from the decoded trace line
                    # dest_reg = decline[0]
                    src_regs = decline[1:]

                    # Only update FLOPS count for the source registers, if any
                    if src_regs:
                        for src in src_regs:
                            if re.match("[xzwsqd0-9.]+$", src):
                                if DEBUG:
                                    print("SRC REG[{}] of #{}".format(
                                        src, len(src_regs)))
                                reg_idx = int(re.search(r'\d+', src).group())
                                updateRegFlops(
                                    reg=src, idx=reg_idx, flops=(1.0 / len(src_regs)))

        # Memtrace lines - Calculate bytes accessed
        else:
            if isMemTrace:  # Part of the previous memory load instruction
                updateRegBytes(reg=mem_reg, idx=mem_reg_idx,
                               bytes=int(splitTrace[3]))
                traceline = meminstrace.readline()
                if DEBUG:
                    print("Recurrent load. Bytes cntr = {}".format(
                        int(splitTrace[3])))
                continue
            else:
                # Only consider memory loads
                if int(splitTrace[2]) == 1:  # Store
                    traceline = meminstrace.readline()
                    continue
                else:  # New Load memtrace
                    if DEBUG:
                        print('=== {}'.format(decline))

                    # Get the destination reg from decoded instrace
                    mem_reg = re.split(',', (re.split('\t', decline)[1]))[0]
                    if DEBUG:
                        print("mem_reg: {}".format(mem_reg))
                    # Regex to find valid registers (listed above)
                    # Ignores some registers, e.g. wzr/xzr
                    if re.match("(x|w|q|d|(\{ z))[0-9]", mem_reg):
                        # Get the destination register index
                        mem_reg_idx = int(re.search(r'\d+', mem_reg).group())
                    else:  # Ignore mem operation if there is no dest register
                        traceline = meminstrace.readline()
                        isMemTrace = 0
                        continue

                    # Update FLOPs/Byte for the loaded register
                    updateFlopsByte(reg=mem_reg, idx=mem_reg_idx,
                                    veclen=args.veclen / 8)

                    bytes_cntr = int(splitTrace[3])
                    if DEBUG:
                        print("New load. index[{}] Bytes = {}".format(
                            mem_reg_idx, bytes_cntr))

                    if bytes_cntr != 0:
                        isMemTrace = 1
                        # Reset and update the new byte size for the register
                        resetRegBytes(reg=mem_reg, idx=mem_reg_idx)
                        updateRegBytes(
                            reg=mem_reg, idx=mem_reg_idx, bytes=bytes_cntr)
                    else:  # All predicate lanes off (Empty LD/ST) - Ignore
                        isMemTrace = 0
                        traceline = meminstrace.readline()
                        continue

        traceline = meminstrace.readline()

    # Average FLOPs/Byte
    print("\n====\n Floating Point Operations per Byte loaded \n====\n")
    num_el = args.veclen / 8

    if max(flops_byte_inv) == 0:
        print("No floating point operations were identified...")
        sys.exit()

    for i in range(MAXSIZE):
        arith_mean = flops_byte_g[i] / flops_byte_cntr
        harmonic_mean = flops_byte_cntr / flops_byte_inv[i]
        print("[{:.0f} lanes, {:.0f} Bytes ea]".format(
            num_el, (args.veclen / 8) / num_el))
        print("Arithmetic mean = {:.4f}".format(arith_mean))
        print("Harmonic mean = {:.4f}".format(harmonic_mean))
        num_el /= 2

    # Do a last FLOP/Byte update on all registers
    for i in range(z_range):
        updateFlopsByte(reg='z', idx=i, veclen=args.veclen / 8)
    for i in range(x_range):
        updateFlopsByte(reg='x', idx=i, veclen=args.veclen / 8)
    for i in range(w_range):
        updateFlopsByte(reg='w', idx=i, veclen=args.veclen / 8)
    for i in range(q_range):
        updateFlopsByte(reg='q', idx=i, veclen=args.veclen / 8)
    for i in range(d_range):
        updateFlopsByte(reg='d', idx=i, veclen=args.veclen / 8)

    # Average FLOPs/Byte
    print("\n====\n FLOP/Byte with update at the end \n====\n")
    num_el = args.veclen / 8

    for i in range(MAXSIZE):
        arith_mean = flops_byte_g[i] / flops_byte_cntr
        harmonic_mean = flops_byte_cntr / flops_byte_inv[i]
        print("[{:.0f} lanes, {:.0f} Bytes ea]".format(
            num_el, (args.veclen / 8) / num_el))
        print("Arithmetic mean = {:.4f}".format(arith_mean))
        print("Harmonic mean = {:.4f}".format(harmonic_mean))
        num_el /= 2


if __name__ == '__main__':
    main()
