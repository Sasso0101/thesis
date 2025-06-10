#ifndef GRAPH_H
#define GRAPH_H

#include "config.h"
#include "mmio.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  uint32_t num_vertices;
  uint32_t num_edges;
  uint32_t *row_ptr;
  uint32_t *col_idx;
} GraphCSR;

GraphCSR *import_mtx(char *filename, uint32_t metadata_size, uint64_t max_size);

#endif
