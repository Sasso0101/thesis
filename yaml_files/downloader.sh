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
  if [ ! -f "$filename" ]; then
    wget -q "$url"
  else
    echo "$filename already exists, skipping download."
  fi
  
  echo "Extracting $filename into datasets/..."
  # Extract the archive into the datasets directory
  tar -xzf "$filename" -C datasets/

  # Get the directory name from the filename (assuming .tar.gz extension)
  DIR_NAME=$(basename "$filename" .tar.gz)

  # Move the matrix file to the datasets directory
  mv "datasets/$DIR_NAME/$DIR_NAME.mtx" "datasets/"

  # Remove the extracted directory
  rm -rf datasets/"$DIR_NAME"

  # Remove the downloaded archive to save space
  rm "$filename"
done

echo "All datasets downloaded and extracted successfully."