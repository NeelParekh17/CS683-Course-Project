#!/bin/bash

BIN_PATH="./bin/UCP"
TRACE_DIR="/home/neel/Desktop/assignments/architecture/Project/UCP_ISCA24/traces"
RESULTS_DIR="/home/neel/Desktop/assignments/architecture/Project/change_results_eviction_policy_updated2"

WARMUP=50000000
SIM=50000000
MAX_PARALLEL=16

mkdir -p "$RESULTS_DIR"

# Loop through all .xz traces
for trace in "$TRACE_DIR"/srv*.xz; do
    trace_name=$(basename "$trace" .xz)
    out_file="$RESULTS_DIR/${trace_name}.txt"

    echo "Starting trace: $trace_name"

    # Launch simulation in background
    "$BIN_PATH" \
        --warmup_instructions "$WARMUP" \
        --simulation_instructions "$SIM" \
        "$trace" > "$out_file" 2>&1 &

    # If running jobs >= MAX_PARALLEL, wait for one to finish before launching another
    while (( $(jobs -r | wc -l) >= MAX_PARALLEL )); do
        sleep 120  # check every 120 seconds
    done
done

# Wait for all remaining jobs to finish
wait
echo "All traces completed."
