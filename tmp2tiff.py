from loadTMP import loadTMP
import os
import sys
import tifffile


def tmp2tiff(input_file, output_file):
    im = loadTMP(input_file)
    print(f"{input_file} has shape: {im.shape}")
    output_channels = im.shape[-1]
    output_channels = min(output_channels, 3)
    print(f"clipping to {output_channels} channel(s)")
    im = im[0, :, :, :output_channels]
    print(f'Writing: {output_file}')
    tifffile.imwrite(output_file, im)
    print("Done.")


def tmp2tiff_dir(dir):
    for prefix in [
            #'temperature_bayer',
            #'highlights_bayer',
            #'filmicrgb',  # TODO(etseng): Remove me.
            #'sharpen',
            #'exposure',
            'colorbalancergb',
    ]:
        for suffix in ["in", "out"]:
            in_file = os.path.join(dir, prefix) + f'_{suffix}.tmp'
            out_file = os.path.join(dir, prefix) + f'_{suffix}.tif'
            print(f"Converting {in_file} -> {out_file}")
            tmp2tiff(in_file, out_file)


tmp2tiff_dir(sys.argv[1])
