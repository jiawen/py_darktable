import darktable_pipe
import numpy as np


def minimal_pipe(src_dng, output_tif):
    params_dicts = {
        'filmicrgb_params': None,
        'colorbalancergb_params': None,
        'sharpen_params': None,
        'exposure_params': None,
        'highlights_params': None,
        'temperature_params': None
    }

    darktable_pipe.render(src_dng, output_tif, params_dicts)


def contrast_only_pipe(src_dng, contrast, output_tif):
    colorbalancergb_params = darktable_pipe.ColorBalanceRGBParams()
    colorbalancergb_params.contrast = contrast

    params_dicts = {
        'filmicrgb_params': None,
        'colorbalancergb_params': colorbalancergb_params,
        'sharpen_params': None,
        'exposure_params': None,
        'highlights_params': None,
        'temperature_params': None
    }

    darktable_pipe.render(src_dng, output_tif, params_dicts)


def sharpen_only_pipe(src_dng, amount, output_tif):
    sharpen_params = darktable_pipe.SharpenParams()
    sharpen_params.amount = amount

    params_dicts = {
        'filmicrgb_params': None,
        'colorbalancergb_params': None,
        'sharpen_params': sharpen_params,
        'exposure_params': None,
        'highlights_params': None,
        'temperature_params': None
    }

    darktable_pipe.render(src_dng, output_tif, params_dicts)


if __name__ == '__main__':
    """
    minimal_pipe('/Users/jiawen/Downloads/a0001-jmac_DSC1459.dng',
                 '/tmp/minimal_pipe.tif')
    """

    # colorbalancergb.contrast sweep
    for contrast in np.linspace(-0.9, 0.9, 7):
        # Need to cast np.float64 amount to Python's native float so it can be
        # hexified properly.
        contrast_only_pipe('/Users/jiawen/Downloads/a0001-jmac_DSC1459.dng',
                           float(contrast), f'/tmp/contrast_{contrast}.tif')

    # sharpen.amount sweep
    """
    for amount in np.linspace(0.0, 10.0, 11):
        # Need to cast np.float64 amount to Python's native float so it can be
        # hexified properly.
        sharpen_only_pipe('/Users/jiawen/Downloads/a0001-jmac_DSC1459.dng',
                          float(amount), f'/tmp/sharpen_amount_{amount}.tif')
    """
