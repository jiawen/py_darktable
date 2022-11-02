from loadTMP import loadTMP
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
