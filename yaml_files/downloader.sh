#!/bin/bash

# Exit immediately if a command exits with a non-zero status.
set -e

# Create the target directory if it doesn't exist
mkdir -p datasets

# List of URLs to download
URLS=(
"https://suitesparse-collection-website.herokuapp.com/MM/GAP/GAP-road.tar.gz"
"https://suitesparse-collection-website.herokuapp.com/MM/DIMACS10/europe_osm.tar.gz"
"https://suitesparse-collection-website.herokuapp.com/MM/DIMACS10/asia_osm.tar.gz"
"https://suitesparse-collection-website.herokuapp.com/MM/DIMACS10/rgg_n_2_22_s0.tar.gz"
"https://suitesparse-collection-website.herokuapp.com/MM/Janna/Hook_1498.tar.gz"
"https://suitesparse-collection-website.herokuapp.com/MM/Janna/Geo_1438.tar.gz"
"https://suitesparse-collection-website.herokuapp.com/MM/Janna/PFlow_742.tar.gz"
)

# Loop through the URLs and process each one
for url in "${URLS[@]}"; do
  filename=$(basename "$url")
  echo "Downloading $filename..."
  
  # Download the file quietly
  wget -q "$url"
  
  echo "Extracting $filename into datasets/..."
  # Extract the archive into the datasets directory
  tar -xzf "$filename" -C datasets/
  
  # Remove the downloaded archive to save space
  rm "$filename"
done

echo "All datasets downloaded and extracted successfully."