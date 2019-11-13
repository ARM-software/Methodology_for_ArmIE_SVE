# ArmIE Custom Instrumentation Clients

All instrumentation clients provided here are for the Arm Instruction Emulator ([ArmIE](https://developer.arm.com/tools-and-software/server-and-hpc/arm-architecture-tools/arm-instruction-emulator)). They make full use of DynamoRIO's emulation API and can be used as is with the latest version of ArmIE.
The available clients were run and tested with ArmIE 19.2.

In order to add custom clients to your ArmIE package, please follow the tutorial available online: [Custom instrumentation clients tutorial](https://developer.arm.com/tools-and-software/server-and-hpc/arm-architecture-tools/arm-instruction-emulator/building-custom-analysis-instrumentation).

## Instruction Tracing

The `instrace_emulated` client uses the emulation API to generate a complete instruction trace file, with both SVE and aarch64 instruction information.

The instruction trace contains the following fields:
 - Instruction Address
 - Instruction Opcode
 - IsMemory

Where `isMemory` is a bit indicating if the instruction is a memory instruction or not.

A trace snippet is available below:
```
Format: <instr address>,;<opcode>,;<is memory>
0x400584,0xa5484160,1
0x400588,0xa5484141,1
0x40058c,0x85604140,1
0x400590,0x04a10000,0
0x400594,0xe5484180,1
```

To distinguish an SVE emulated instruction from an aarch64 instruction, the client uses different separators: A comma (,) for SVE instructions and a semicolon (;) for aarch64 instructions.

After adding the instruction trace client to ArmIE (follow the [guide](https://developer.arm.com/tools-and-software/server-and-hpc/arm-architecture-tools/arm-instruction-emulator/building-custom-analysis-instrumentation)), just execute the client as such:

```
$ armie -msve-vector-bits=512 -i libinstrace_emulated.so -- ./example_sve
```

Please note that the instruction tracing client requires a region-of-interest defined in the source code. To know more about this take a look at the '[Getting Started](https://developer.arm.com/tools-and-software/server-and-hpc/arm-architecture-tools/arm-instruction-emulator/get-started)' ArmIE page and this [blog post](https://community.arm.com/developer/tools-software/hpc/b/hpc-blog/posts/emulating-sve-on-armv8-using-dynamorio-and-armie).
