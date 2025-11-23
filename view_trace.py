#!/usr/bin/env python3
"""
ChampSim Trace Viewer
Parses and displays the contents of ChampSim trace files in human-readable format.

Credits: Made with AI assistance to understand and visualize ChampSim trace files.
"""

import struct
import lzma
import sys
import argparse

class InputInstr:
    """Standard ChampSim instruction format (64 bytes)"""
    FORMAT = '<Q 2B 2B 4B 2Q 4Q'  # Little-endian format
    SIZE = 64
    
    def __init__(self, data):
        unpacked = struct.unpack(self.FORMAT, data)
        self.ip = unpacked[0]
        self.is_branch = unpacked[1]
        self.branch_taken = unpacked[2]
        self.destination_registers = list(unpacked[3:5])
        self.source_registers = list(unpacked[5:9])
        self.destination_memory = list(unpacked[9:11])
        self.source_memory = list(unpacked[11:15])
    
    def __str__(self):
        lines = [
            f"IP: 0x{self.ip:016x}",
            f"Branch: {bool(self.is_branch)}, Taken: {bool(self.branch_taken)}" if self.is_branch else "Branch: No",
        ]
        
        # Show non-zero destination registers
        dst_regs = [r for r in self.destination_registers if r != 0]
        if dst_regs:
            lines.append(f"Dest Regs: {dst_regs}")
        
        # Show non-zero source registers
        src_regs = [r for r in self.source_registers if r != 0]
        if src_regs:
            lines.append(f"Src Regs:  {src_regs}")
        
        # Show non-zero destination memory addresses
        dst_mem = [f"0x{addr:016x}" for addr in self.destination_memory if addr != 0]
        if dst_mem:
            lines.append(f"Dest Mem:  {dst_mem}")
        
        # Show non-zero source memory addresses
        src_mem = [f"0x{addr:016x}" for addr in self.source_memory if addr != 0]
        if src_mem:
            lines.append(f"Src Mem:   {src_mem}")
        
        return "\n  ".join(lines)
    
    def to_compact_str(self):
        """Compact one-line format"""
        parts = [f"0x{self.ip:016x}"]
        
        if self.is_branch:
            parts.append(f"BR({'T' if self.branch_taken else 'N'})")
        
        dst_regs = [r for r in self.destination_registers if r != 0]
        if dst_regs:
            parts.append(f"D_R{dst_regs}")
        
        src_regs = [r for r in self.source_registers if r != 0]
        if src_regs:
            parts.append(f"S_R{src_regs}")
        
        dst_mem = [f"0x{addr:x}" for addr in self.destination_memory if addr != 0]
        if dst_mem:
            parts.append(f"D_M{dst_mem}")
        
        src_mem = [f"0x{addr:x}" for addr in self.source_memory if addr != 0]
        if src_mem:
            parts.append(f"S_M{src_mem}")
        
        return " | ".join(parts)


class CloudsuiteInstr:
    """CloudSuite instruction format (80 bytes)"""
    FORMAT = '<Q 2B 4B 4B 4Q 4Q 2B'
    SIZE = 80
    
    def __init__(self, data):
        unpacked = struct.unpack(self.FORMAT, data)
        self.ip = unpacked[0]
        self.is_branch = unpacked[1]
        self.branch_taken = unpacked[2]
        self.destination_registers = list(unpacked[3:7])
        self.source_registers = list(unpacked[7:11])
        self.destination_memory = list(unpacked[11:15])
        self.source_memory = list(unpacked[15:19])
        self.asid = list(unpacked[19:21])
    
    def __str__(self):
        lines = [
            f"IP: 0x{self.ip:016x}",
            f"Branch: {bool(self.is_branch)}, Taken: {bool(self.branch_taken)}" if self.is_branch else "Branch: No",
            f"ASID: {self.asid}",
        ]
        
        dst_regs = [r for r in self.destination_registers if r != 0]
        if dst_regs:
            lines.append(f"Dest Regs: {dst_regs}")
        
        src_regs = [r for r in self.source_registers if r != 0]
        if src_regs:
            lines.append(f"Src Regs:  {src_regs}")
        
        dst_mem = [f"0x{addr:016x}" for addr in self.destination_memory if addr != 0]
        if dst_mem:
            lines.append(f"Dest Mem:  {dst_mem}")
        
        src_mem = [f"0x{addr:016x}" for addr in self.source_memory if addr != 0]
        if src_mem:
            lines.append(f"Src Mem:   {src_mem}")
        
        return "\n  ".join(lines)
    
    def to_compact_str(self):
        """Compact one-line format"""
        parts = [f"0x{self.ip:016x}"]
        
        if self.is_branch:
            parts.append(f"BR({'T' if self.branch_taken else 'N'})")
        
        parts.append(f"ASID{self.asid}")
        
        dst_regs = [r for r in self.destination_registers if r != 0]
        if dst_regs:
            parts.append(f"D_R{dst_regs}")
        
        src_regs = [r for r in self.source_registers if r != 0]
        if src_regs:
            parts.append(f"S_R{src_regs}")
        
        dst_mem = [f"0x{addr:x}" for addr in self.destination_memory if addr != 0]
        if dst_mem:
            parts.append(f"D_M{dst_mem}")
        
        src_mem = [f"0x{addr:x}" for addr in self.source_memory if addr != 0]
        if src_mem:
            parts.append(f"S_M{src_mem}")
        
        return " | ".join(parts)


def read_trace(filename, cloudsuite=False, max_instrs=None, compact=False):
    """Read and display trace file contents"""
    InstrClass = CloudsuiteInstr if cloudsuite else InputInstr
    instr_size = InstrClass.SIZE
    
    # Open file (handle .xz compression)
    if filename.endswith('.xz'):
        file_handle = lzma.open(filename, 'rb')
    else:
        file_handle = open(filename, 'rb')
    
    try:
        instr_count = 0
        while True:
            if max_instrs and instr_count >= max_instrs:
                break
            
            data = file_handle.read(instr_size)
            if len(data) < instr_size:
                break
            
            instr = InstrClass(data)
            
            if compact:
                print(f"[{instr_count:6d}] {instr.to_compact_str()}")
            else:
                print(f"{'='*70}")
                print(f"Instruction {instr_count}:")
                print(f"  {instr}")
            
            instr_count += 1
        
        print(f"\n{'='*70}")
        print(f"Total instructions read: {instr_count}")
        
    finally:
        file_handle.close()


def get_trace_stats(filename, cloudsuite=False):
    """Get statistics about the trace file"""
    InstrClass = CloudsuiteInstr if cloudsuite else InputInstr
    instr_size = InstrClass.SIZE
    
    if filename.endswith('.xz'):
        file_handle = lzma.open(filename, 'rb')
    else:
        file_handle = open(filename, 'rb')
    
    try:
        stats = {
            'total_instrs': 0,
            'branches': 0,
            'taken_branches': 0,
            'mem_reads': 0,
            'mem_writes': 0,
            'unique_ips': set(),
        }
        
        while True:
            data = file_handle.read(instr_size)
            if len(data) < instr_size:
                break
            
            instr = InstrClass(data)
            stats['total_instrs'] += 1
            stats['unique_ips'].add(instr.ip)
            
            if instr.is_branch:
                stats['branches'] += 1
                if instr.branch_taken:
                    stats['taken_branches'] += 1
            
            if any(addr != 0 for addr in instr.source_memory):
                stats['mem_reads'] += 1
            
            if any(addr != 0 for addr in instr.destination_memory):
                stats['mem_writes'] += 1
        
        print(f"\nTrace Statistics for: {filename}")
        print(f"{'='*70}")
        print(f"Total Instructions:    {stats['total_instrs']:,}")
        print(f"Unique IPs:            {len(stats['unique_ips']):,}")
        print(f"Branch Instructions:   {stats['branches']:,} ({100*stats['branches']/max(stats['total_instrs'],1):.2f}%)")
        print(f"  Taken:               {stats['taken_branches']:,} ({100*stats['taken_branches']/max(stats['branches'],1):.2f}% of branches)")
        print(f"Memory Reads:          {stats['mem_reads']:,} ({100*stats['mem_reads']/max(stats['total_instrs'],1):.2f}%)")
        print(f"Memory Writes:         {stats['mem_writes']:,} ({100*stats['mem_writes']/max(stats['total_instrs'],1):.2f}%)")
        
    finally:
        file_handle.close()


def main():
    parser = argparse.ArgumentParser(
        description='View and analyze ChampSim trace files',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # View first 10 instructions in detail
  python view_trace.py traces/compute_int_11.xz -n 10
  
  # View first 100 instructions in compact format
  python view_trace.py traces/compute_int_11.xz -n 100 -c
  
  # Get statistics only
  python view_trace.py traces/compute_int_11.xz -s
  
  # View CloudSuite format trace
  python view_trace.py traces/cloudsuite_trace.xz --cloudsuite -n 10
        """
    )
    
    parser.add_argument('tracefile', help='Path to trace file (.xz or uncompressed)')
    parser.add_argument('-n', '--num', type=int, metavar='N',
                        help='Number of instructions to display (default: all)')
    parser.add_argument('-c', '--compact', action='store_true',
                        help='Use compact one-line format')
    parser.add_argument('-s', '--stats', action='store_true',
                        help='Show statistics only (no instruction details)')
    parser.add_argument('--cloudsuite', action='store_true',
                        help='Parse as CloudSuite format (80 bytes) instead of standard (64 bytes)')
    
    args = parser.parse_args()
    
    if args.stats:
        get_trace_stats(args.tracefile, args.cloudsuite)
    else:
        read_trace(args.tracefile, args.cloudsuite, args.num, args.compact)


if __name__ == '__main__':
    main()
