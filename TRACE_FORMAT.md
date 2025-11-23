# ChampSim Trace File Format Guide

## Overview

ChampSim trace files are **binary files** that contain instruction-level execution traces. Each instruction is stored as a fixed-size binary record.

## File Characteristics

- **Compression**: Typically compressed with `.xz` (LZMA)
- **Uncompressed size**: ~64 bytes per instruction
- **Compressed size**: <1 byte per instruction (highly compressed)
- **Format**: Little-endian binary

## Standard Format (64 bytes per instruction)

```
Offset  Size  Field                   Description
------  ----  ----------------------  ----------------------------------
0       8     ip                      Instruction Pointer (Program Counter)
8       1     is_branch               Is this a branch? (0=No, 1=Yes)
9       1     branch_taken            Was branch taken? (0=No, 1=Yes)
10      2     destination_registers   Output register IDs (0=unused)
12      4     source_registers        Input register IDs (0=unused)
16      16    destination_memory      Memory write addresses (0=none)
32      32    source_memory           Memory read addresses (0=none)
```

## Field Descriptions

### IP (Instruction Pointer)
- **Size**: 8 bytes (unsigned long long)
- **Description**: Virtual address of the instruction being executed
- **Example**: `0x00000000003f000c`

### is_branch
- **Size**: 1 byte (unsigned char)
- **Values**: 0 (not a branch) or 1 (is a branch)
- **Description**: Indicates if this instruction is a branch instruction

### branch_taken
- **Size**: 1 byte (unsigned char)
- **Values**: 0 (not taken) or 1 (taken)
- **Description**: For branch instructions, indicates if the branch was taken
- **Note**: Only meaningful when `is_branch = 1`

### destination_registers
- **Size**: 2 bytes (array of 2 unsigned chars)
- **Description**: Register IDs that this instruction writes to
- **Special values**: 0 indicates unused slot
- **Example**: `[26, 0]` means writes to register 26 only

### source_registers
- **Size**: 4 bytes (array of 4 unsigned chars)
- **Description**: Register IDs that this instruction reads from
- **Special values**: 0 indicates unused slot
- **Example**: `[1, 2, 0, 0]` means reads from registers 1 and 2

### destination_memory
- **Size**: 16 bytes (array of 2 unsigned long long)
- **Description**: Virtual addresses that this instruction writes to
- **Special values**: 0 indicates no memory write
- **Example**: `[0x00000000033fca08, 0x0000000000000000]` means writes to one address

### source_memory
- **Size**: 32 bytes (array of 4 unsigned long long)
- **Description**: Virtual addresses that this instruction reads from
- **Special values**: 0 indicates no memory read
- **Example**: `[0xf484a10, 0x0, 0x0, 0x0]` means reads from one address

## Special Register Numbers

- **Register 6**: Stack Pointer
- **Register 25**: Flags register
- **Register 26**: Instruction Pointer

## CloudSuite Format (80 bytes per instruction)

For SPARC-based CloudSuite traces, use the `--cloudsuite` flag:
- 4 destination registers (instead of 2)
- 4 destination memory addresses (instead of 2)
- Additional 2-byte ASID (Address Space ID) field

## Using the Trace Viewer

A Python script `view_trace.py` is provided to view trace contents:

### View Instructions in Detail
```bash
python3 view_trace.py traces/compute_int_11.xz -n 10
```

### View Instructions in Compact Format
```bash
python3 view_trace.py traces/compute_int_11.xz -n 100 -c
```

### Get Trace Statistics
```bash
python3 view_trace.py traces/compute_int_11.xz -s
```

### Example Output (Compact Format)
```
[     0] 0x00000000003f000c | BR(N) | D_R[26] | S_R[26, 67]
[     1] 0x00000000003f0010 | D_R[67]
[     7] 0x0000000000671e84 | D_R[1] | S_R[4, 3] | S_M['0x33fca08']
```

Legend:
- `BR(N)` = Branch Not Taken
- `BR(T)` = Branch Taken
- `D_R[...]` = Destination Registers
- `S_R[...]` = Source Registers
- `D_M[...]` = Destination Memory
- `S_M[...]` = Source Memory

### Example Output (Statistics)
```
Trace Statistics for: traces/compute_int_11.xz
======================================================================
Total Instructions:    102,632,256
Unique IPs:            121,130
Branch Instructions:   23,800,260 (23.19%)
  Taken:               13,034,602 (54.77% of branches)
Memory Reads:          21,212,319 (20.67%)
Memory Writes:         9,507,406 (9.26%)
```

## Creating Traces

Traces are created using the Intel PIN tool (`tracer/pin/champsim_tracer.cpp`):

```bash
pin -t obj-intel64/champsim_tracer.so -o output.trace -s 100000 -t 1000000 -- ./program
```

Options:
- `-o`: Output trace filename
- `-s`: Number of instructions to skip before tracing
- `-t`: Number of instructions to trace

## Manual Parsing

If you want to parse traces manually in another language:

1. **Decompress**: `xz -dc trace.xz > trace.bin`
2. **Read in chunks**: Read 64-byte blocks
3. **Parse binary**: Use little-endian format matching the C struct

### Python Example
```python
import struct

with open('trace.bin', 'rb') as f:
    data = f.read(64)
    (ip, is_branch, branch_taken, 
     dst_r0, dst_r1, 
     src_r0, src_r1, src_r2, src_r3,
     dst_m0, dst_m1,
     src_m0, src_m1, src_m2, src_m3) = struct.unpack('<Q 2B 2B 4B 2Q 4Q', data)
```

## Notes

- All multi-byte values are stored in **little-endian** format
- Unused array slots are filled with 0
- Memory addresses are virtual addresses
- Register numbering is architecture-specific
- The trace format is optimized for simulation, not human readability
