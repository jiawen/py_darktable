#include <stdio.h>

#include "gui/gtk.h"

inline void dump_tmp(const float* buffer, const dt_iop_roi_t* roi, int channels, const char* filename) {
  fprintf(stderr, "Writing: %s\n", filename);
  FILE* f = g_fopen(filename, "wb");

  const int32_t type = 0; // float32
  const int32_t frames = 1;

  fwrite(&(roi->width), 4, 1, f);
  fwrite(&(roi->height), 4, 1, f);
  fwrite(&frames, 4, 1, f);
  fwrite(&channels, 4, 1, f);
  fwrite(&type, 4, 1, f);

  for (int c = 0; c < channels; c++) {
    for (int y = 0; y < roi->height; y++) {
      for (int x = 0; x < roi->width; x++) {
        const int index = y * roi->width * channels + x * channels + c;
        fwrite(&(buffer[index]), 4, 1, f);
      }
    }
  }
  fclose(f);
}

inline void debug_print_roi(const dt_iop_roi_t *const roi) {
  fprintf(stderr, "roi x = %d, y = %d, width = %d, height = %d, scale = %f\n",
    roi->x, roi->y, roi->width, roi->height, roi->scale);
}

inline void debug_print_color_matrix(const dt_colormatrix_t m) {
  // Apparently, they store things transposed... I think.
  // See dt_colormatrix_mul in dttypes.h.
  for (int i = 0; i < 4; ++i) {
      for (int j = 0; j < 4; ++j) {
        fprintf(stderr, "%.3f ", m[j][i]);  // Note [j][i] here. This could be wrong, ugh.
      }
      fprintf(stderr, "\n");
    }
}
