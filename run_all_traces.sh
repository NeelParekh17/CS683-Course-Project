#!/bin/bash

# Paths
BIN_PATH="./bin/champsim"
TRACE_DIR="/home/neel/Desktop/assignments/architecture/Project/UCP_ISCA24/traces"
RESULTS_DIR="/home/neel/Desktop/assignments/architecture/Project/UCP_ISCA24/results"

# Parameters
WARMUP=1000000
SIM=1000000
MAX_PARALLEL=8

# Create results directory if not exists
mkdir -p "$RESULTS_DIR"

# Counter to limit parallel jobs
count=0

# Loop through all .xz traces
for trace in "$TRACE_DIR"/*.xz; do
    trace_name=$(basename "$trace" .xz)
    out_file="$RESULTS_DIR/${trace_name}.txt"

    echo "Running trace: $trace_name"

    # Run Champsim in background and redirect output
    "$BIN_PATH" \
        --warmup_instructions "$WARMUP" \
        --simulation_instructions "$SIM" \
        "$trace" > "$out_file" 2>&1 &

    ((count++))

    # Wait if 8 parallel jobs are already running
    if (( count % MAX_PARALLEL == 0 )); then
        wait
    fi
done

# Wait for remaining jobs
wait

echo "All traces completed."
