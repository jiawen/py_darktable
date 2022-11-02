import numpy as np
import os
import minimal_pipe_mit5k
import tmp2tiff

_MIT_5K_ROOT = "/media/shared/data/MIT-Adobe-FiveK/fivek_dataset/raw_photos"
_DST_ROOT = "/media/shared/data/acr_hyperparams_mit5k_sweep/contrast_sweep"

# 0 rawprepare
# 1 temperature      *
# 2 highlights       *
# 3 demosaic
# 4 flip             *
# 5 exposure         *
# 6 colorin
# 7 sharpen          *
# 8 colorbalancergb  *
# [skip] 9 filmicrgb *
# 10 colorout
def convert_tmp2tiff(src_dir, dst_prefix):
    for stage in [
            'temperature_bayer',
            'highlights_bayer',
            'colorbalancergb',
            'exposure',
            'colorin',
            'sharpen',
            'colorbalancergb',
            'colorout'
    ]:
        for suffix in ["in", "out"]:
            in_file = os.path.join(src_dir, stage) + f'_{suffix}.tmp'
            out_file = f'{dst_prefix}_{stage}_{suffix}.tif'
            print(f"Converting {in_file} -> {out_file}")
            tmp2tiff.tmp2tiff(in_file, out_file)

def get_sorted_src_paths():
  src_paths = []
  for dir, subdir, files in os.walk(_MIT_5K_ROOT):
    for file in files:
      if file.endswith('.dng'):
        src_paths.append(os.path.join(dir, file))
  return sorted(src_paths)

def main():
  src_paths = get_sorted_src_paths()
  # print('basename: ', os.path.basename(src_paths[0]))

  start_idx = 0
  num_tasks = 1000

  for i in range(start_idx, start_idx + num_tasks):
    src_dng_path = src_paths[i]
    print(f"Reading: {src_dng_path}")
    base_name = os.path.basename(src_dng_path)

    raw_prepare_params, temperature_params = minimal_pipe_mit5k.read_dng_params(src_dng_path)
    print(raw_prepare_params, temperature_params)

    for contrast in np.linspace(-0.5, 0.9, 7):
      dst_prefix = f"{_DST_ROOT}/contrast_sweep/{base_name}_contrast=[{contrast:.3f}]"
      dst_png_path = dst_prefix + ".png"

      if os.path.exists(dst_png_path):
        print(f"Skipping: {dst_png_path} because it already exists")
      else:
        print(f"Processing: index {i:0>5}: {dst_png_path}")
        minimal_pipe_mit5k.contrast_only_pipe(src_dng_path, raw_prepare_params, temperature_params, float(contrast), dst_png_path)
        convert_tmp2tiff('/tmp', dst_prefix)

if __name__ == '__main__':
    main()
