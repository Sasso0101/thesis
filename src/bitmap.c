#include "bitmap.h"
#include "config.h"
#include "frontier.h"

Bitmap *bitmap_init(uint32_t num_vertices) {
  Bitmap *b = malloc(sizeof(Bitmap));
  b->bitmap = malloc(sizeof(uint32_t) * num_vertices);
  b->bitmap_size = num_vertices;
  return b;
}

void frontier_to_bitmap(Bitmap *b, MergedCSR *merged_csr, Frontier *f,
                        int thread_id) {
  mer_t v;
  while ((v = frontier_pop_vertex(f, thread_id)) != VERT_MAX)
    b->bitmap[ID(merged_csr, v)] = true;
}

void bitmap_to_frontier(Bitmap *b, MergedCSR *merged_csr, Frontier *f,
                        int thread_id) {
  uint32_t vertices_per_thread = b->bitmap_size / MAX_THREADS;
  uint32_t start = thread_id * vertices_per_thread;
  uint32_t end = (thread_id + 1) * vertices_per_thread >= b->bitmap_size
                     ? (thread_id + 1) * vertices_per_thread
                     : b->bitmap_size;
  for (uint32_t i = start; i < end; i++) {
    if (b->bitmap[i]) {
      frontier_push_vertex(f, thread_id, merged_csr->row_ptr[i]);
    }
  }
}
