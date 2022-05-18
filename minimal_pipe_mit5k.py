import darktable_pipe
import numpy as np


def minimal_pipe(src_dng, output_tif):
    # TODO(etseng): You need to parse these out of the DNG using exiftool.
    raw_prepare_params = darktable_pipe.RawPrepareParams()
    raw_prepare_params.black_levels = darktable_pipe.make_black_levels(
        0, 0, 0, 0)
    raw_prepare_params.white_point = 4095

    # TODO(etseng): You need to parse these out of the DNG using exiftool as
    # "As Shot Neutral". TemperatureParams contains gains, which is 1 / the as shot neutral
    temperature_params = darktable_pipe.TemperatureParams()
    temperature_params.red = 1.0 / 0.469725
    temperature_params.green = 1.0 / 1.0
    temperature_params.blue = 1.0 / 0.646465

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
    minimal_pipe('/Users/jiawen/Downloads/a0001-jmac_DSC1459.dng',
                 '/tmp/minimal_pipe.jpg')

    # colorbalancergb.contrast sweep
    """
    for contrast in np.linspace(-0.9, 0.9, 7):
        # Need to cast np.float64 amount to Python's native float so it can be
        # hexified properly.
        contrast_only_pipe('/Users/jiawen/Downloads/a0001-jmac_DSC1459.dng',
                           float(contrast), f'/tmp/contrast_{contrast}.tif')
    """

    # sharpen.amount sweep
    """
    for amount in np.linspace(0.0, 10.0, 11):
        # Need to cast np.float64 amount to Python's native float so it can be
        # hexified properly.
        sharpen_only_pipe('/Users/jiawen/Downloads/a0001-jmac_DSC1459.dng',
                          float(amount), f'/tmp/sharpen_amount_{amount}.tif')
    """
