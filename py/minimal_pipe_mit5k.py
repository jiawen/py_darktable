import darktable_pipe
import numpy as np
import rawpy
import sys

# Runs a parameter sweep through a minimal pipeline on a single image.
# The parameter sweep is done on two stages: contrast and sharpen amount.


def minimal_pipe(src_dng, raw_prepare_params, temperature_params, output_tif):
    params_dicts = {
        'filmicrgb_params': None,
        'colorbalancergb_params': None,
        'sharpen_params': None,
        'exposure_params': None,
        'highlights_params': None,
        'temperature_params': temperature_params,
        'raw_prepare_params': raw_prepare_params,
    }

    darktable_pipe.render(src_dng, output_tif, params_dicts)


def contrast_only_pipe(src_dng, raw_prepare_params, temperature_params,
                       contrast, output_tif):
    colorbalancergb_params = darktable_pipe.ColorBalanceRGBParams()
    colorbalancergb_params.contrast = contrast

    params_dicts = {
        'filmicrgb_params': None,
        'colorbalancergb_params': colorbalancergb_params,
        'sharpen_params': None,
        'exposure_params': None,
        'highlights_params': None,
        'temperature_params': temperature_params,
        'raw_prepare_params': raw_prepare_params,
    }

    darktable_pipe.render(src_dng, output_tif, params_dicts)


def sharpen_only_pipe(src_dng, raw_prepare_params, temperature_params, amount,
                      output_tif):
    sharpen_params = darktable_pipe.SharpenParams()
    sharpen_params.amount = amount

    params_dicts = {
        'filmicrgb_params': None,
        'colorbalancergb_params': None,
        'sharpen_params': sharpen_params,
        'exposure_params': None,
        'highlights_params': None,
        'temperature_params': temperature_params,
        'raw_prepare_params': raw_prepare_params,
    }

    darktable_pipe.render(src_dng, output_tif, params_dicts)


def read_dng_params(dng_file):
    raw_prepare_params = darktable_pipe.RawPrepareParams()
    temperature_params = darktable_pipe.TemperatureParams()

    with rawpy.imread(dng_file) as raw:
        raw_prepare_params.black_levels = [
            np.uint16(x) for x in raw.black_level_per_channel
        ]
        raw_prepare_params.white_point = int(raw.white_level)

        temperature_params.red = float(raw.camera_whitebalance[0])
        temperature_params.green = float(raw.camera_whitebalance[1])
        temperature_params.blue = float(raw.camera_whitebalance[2])
        # g2 is usually not present. DarkTable wants math.nan, rawpy returns 0
        # temperature_params.g2 = float(raw.camera_whitebalance[3])

    return raw_prepare_params, temperature_params


def main(argv):
    # test file from the MIT5K dataset
    # src_dng_path = '/Users/jiawen/Downloads/a0001-jmac_DSC1459.dng'
    src_dng_path = argv[1]

    raw_prepare_params, temperature_params = read_dng_params(src_dng_path)

    minimal_pipe(src_dng_path, raw_prepare_params, temperature_params,
                 '/tmp/minimal_pipe.tif')

    # colorbalancergb.contrast sweep
    for contrast in np.linspace(-0.9, 0.9, 7):
        # Need to cast np.float64 amount to Python's native float so it can be
        # hexified properly.
        contrast_only_pipe(src_dng_path,
                           raw_prepare_params, temperature_params,
                           float(contrast), f'/tmp/contrast_{contrast}.tif')

    # sharpen.amount sweep
    for amount in np.linspace(0.0, 10.0, 11):
        # Need to cast np.float64 amount to Python's native float so it can be
        # hexified properly.
        sharpen_only_pipe(src_dng_path, raw_prepare_params, temperature_params,
                          float(amount), f'/tmp/sharpen_amount_{amount}.tif')


if __name__ == '__main__':
    main(sys.argv)
