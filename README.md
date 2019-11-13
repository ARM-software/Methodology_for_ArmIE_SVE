# Methodology for ArmIE SVE

This repository provides the necessary tools to run the methodology presented in [[P1](#publications)].

We developed an application optimization methodology based on the Arm Instruction Emulator (ArmIE) [[R1](#references)] that uses timing-agnostic metrics to assess application quality. The goal of this methodology is to aid with porting applications towards SVE, in advance of hardware release.

The methodology uses both compiler information, ArmIE traces and processing tools to gather relevant information. In this repository, we include relevant ArmIE clients and processing tools used to acquire data presented in [[P1](#publications)].
Each tool includes its own README file and execution instructions, as well as sample tests for a quick run. All the provided trace samples were gathered with ArmIE 19.2. The provided sample code was compiled with the Arm HPC Compiler 19.2.

Check the **Publications**, **Materials** and **References** sections for additional information on the methodology work and how to use the tools.

## ArmIE Clients
This includes custom ArmIE clients not available in the main release version.
Such clients make full use of the DynamoRIO emulation API to ease compatibility with ArmIE. Instructions on how to add the custom clients to ArmIE can be found in the README file and in the ArmIE [documentation page](https://developer.arm.com/tools-and-software/server-and-hpc/arm-architecture-tools/arm-instruction-emulator/building-custom-analysis-instrumentation).

We provide the following ArmIE clients:

 - **Instruction tracing**

## SVE Tools
SVE tools are a set of post-processing applications that require ArmIE trace files. They provide useful functionality and more complex statistics that are used by the SVE methodology [[P1](#publications)].

The tools are written in C++ and Python. Check the provided README file for more information on how to build and run them. Trace samples are also provided. Below is the list of available SVE tools:

 - **Memtrace Merger** (Merges aarch64 and SVE traces generated by the ArmIE memory tracer)
 - **Memtrace Analyzer** (Analyzes a full memory trace and provides useful statistics)
 - **Vector Utilization** (Analyzes a full memory trace and provides the SVE vector utilization)
 - **FLOPs per Byte** (Analyzes a full memory & instruction trace and provides the average of floating point operations per byte)

 Note: The`FLOPs per Byte` tool relies on a complete memory and instruction trace file that cannot be  instrumented with the current version of ArmIE (<=19.2). Unfortunately, we cannot publish an ArmIE client that generates that type of traces at this point in time. We do however provide a sample memory & instruction trace to help understand the tool and the metric it generates.  

## SVE Cachesim
Cache simulator for SVE memory traces generated with ArmIE.
It models a single core multi-level cache system, with an optional prefetcher. We provide two sample models that are not tuned to any architecture.

Check the available README file for more information on how to run the cache simulator. Memory trace samples are provided, alongside the expected outputs.

## Publications

 - [P1] Tairum Cruz, M., Ruiz, D., Rusitoru, R., "Asvie: A timing-agnostic SVE optimization methodology", ProTools'19 workshop at SC'19, November, 2019.
 - [P2] [Emulating SVE on existing Armv8-A hardware using DynamoRIO and ArmIE](https://community.arm.com/developer/tools-software/hpc/b/hpc-blog/posts/emulating-sve-on-armv8-using-dynamorio-and-armie)
 - [P3] [Optimizing HPCG for Arm SVE](https://community.arm.com/developer/tools-software/hpc/b/hpc-blog/posts/optimizing-hpcg-for-arm-sve)

## Materials

 - [M1] [Arm SVE Tools Training (ISC'2019)](https://gitlab.com/arm-hpc/training/arm-sve-tools)

## References

 - [R1] [Arm Instruction Emulator (ArmIE)](https://developer.arm.com/tools-and-software/server-and-hpc/arm-architecture-tools/arm-instruction-emulator)

## License

This project is licensed under [Apache-2.0](https://www.apache.org/licenses/LICENSE-2.0). For more information, see LICENSE.txt.

## Contributions / Pull Requests

Contributions are accepted under Apache-2.0. Only submit contributions where you have authored all of the code. If you do this on work time, make sure you have your employer's approval.