#ifndef BITMAP_H_
#define BITMAP_H_

#include "frontier.h"
#include "merged_csr.h"

typedef struct {
  bool *bitmap;
  uint32_t bitmap_size;
} Bitmap;

Bitmap *bitmap_init(uint32_t num_vertices);
void frontier_to_bitmap(Bitmap *b, MergedCSR *merged_csr, Frontier *f,
                        int thread_id);
void bitmap_to_frontier(Bitmap *b, MergedCSR *merged_csr, Frontier *f,
                        int thread_id);

#endif
