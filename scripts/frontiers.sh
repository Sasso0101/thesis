#!/bin/bash

# Ensure the script exits if any command fails
set -e

# Array of datasets to process
DATASETS=(
  "datasets/GAP-road.mtx"
  "datasets/Geo_1438.mtx"
  "datasets/Hook_1498.mtx"
  "datasets/PFlow_742.mtx"
  "datasets/asia_osm.mtx"
  "datasets/europe_osm.mtx"
  "datasets/rgg_n_2_22_s0.mtx"
)

# Directory for log files
LOG_DIR="frontiers"
mkdir -p "$LOG_DIR"

# Path to the executable
BFS_EXEC="openmp/bin/bfs"

# Check if the executable exists
if [ ! -f "$BFS_EXEC" ]; then
  echo "Error: BFS executable not found at $BFS_EXEC"
  exit 1
fi

# Iterate over each dataset
for dataset in "${DATASETS[@]}"; do
  if [ ! -f "$dataset" ]; then
    echo "Warning: Dataset file not found at $dataset. Skipping."
    continue
  fi

  # Extract the base name of the dataset file (e.g., "GAP-road")
  BASENAME=$(basename "$dataset" .mtx)
  
  # Define the log file path
  LOG_FILE="${LOG_DIR}/${BASENAME}.log"
  
  # Print a status message
  echo "Running BFS on ${dataset}, logging to ${LOG_FILE}"
  
  # Run the command and append the output to the log file
  # This creates the log file if it doesn't exist.
  "$BFS_EXEC" "${dataset}" 1 reference >> "$LOG_FILE"
done

echo "All datasets processed successfully."