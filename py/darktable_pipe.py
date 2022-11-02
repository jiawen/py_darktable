import math
import numpy as np
import os
import struct
import subprocess
import tempfile
from dataclasses import dataclass, field, fields
from sys import platform

# Point me to darktable-cli for 3.8.
if platform == "linux" or platform == "linux2":
    # For running inside the Docker image.
    _DARKTABLE_CLI = "/github/darktable/build/bin/darktable-cli"
elif platform == "darwin":
    _DARKTABLE_CLI = "/Applications/darktable.app/Contents/MacOS/darktable-cli"
else:
    raise NotImplementedError("platform not supported")

_FMT_STR = '''<?xml version="1.0" encoding="UTF-8"?>
<x:xmpmeta xmlns:x="adobe:ns:meta/" x:xmptk="XMP Core 4.4.0-Exiv2">
 <rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">
  <rdf:Description rdf:about=""
    xmlns:exif="http://ns.adobe.com/exif/1.0/"
    xmlns:xmp="http://ns.adobe.com/xap/1.0/"
    xmlns:xmpMM="http://ns.adobe.com/xap/1.0/mm/"
    xmlns:darktable="http://darktable.sf.net/"
   exif:DateTimeOriginal="2020:08:27 10:09:35"
   xmp:Rating="1"
   darktable:import_timestamp="1651274350"
   darktable:change_timestamp="-1"
   darktable:export_timestamp="-1"
   darktable:print_timestamp="-1"
   darktable:xmp_version="4"
   darktable:raw_params="0"
   darktable:auto_presets_applied="1"
   darktable:history_end="1000"
   darktable:iop_order_version="2">
   <darktable:history>
    <rdf:Seq>
     <rdf:li
      darktable:num="0"
      darktable:operation="rawprepare"
      darktable:enabled="1"
      darktable:modversion="1"
      darktable:params="{raw_prepare_params}"
      darktable:multi_name=""
      darktable:multi_priority="0"
      darktable:blendop_version="11"
      darktable:blendop_params="gz14eJxjYIAACQYYOOHEgAZY0QVwggZ7CB6pfNoAAE8gGQg="/>
     <rdf:li
      darktable:num="1"
      darktable:operation="demosaic"
      darktable:enabled="1"
      darktable:modversion="4"
      darktable:params="0000000000000000000000000500000001000000cdcc4c3e"
      darktable:multi_name=""
      darktable:multi_priority="0"
      darktable:blendop_version="11"
      darktable:blendop_params="gz14eJxjYIAACQYYOOHEgAZY0QVwggZ7CB6pfNoAAE8gGQg="/>
     <rdf:li
      darktable:num="2"
      darktable:operation="colorin"
      darktable:enabled="1"
      darktable:modversion="7"
      darktable:params="gz48eJzjYhgFowABWAbaAaNgwAEANOwADw=="
      darktable:multi_name=""
      darktable:multi_priority="0"
      darktable:blendop_version="11"
      darktable:blendop_params="gz14eJxjYIAACQYYOOHEgAZY0QVwggZ7CB6pfNoAAE8gGQg="/>
     <rdf:li
      darktable:num="3"
      darktable:operation="colorout"
      darktable:enabled="1"
      darktable:modversion="5"
      darktable:params="gz35eJxjZBgFo4CBAQAEEAAC"
      darktable:multi_name=""
      darktable:multi_priority="0"
      darktable:blendop_version="11"
      darktable:blendop_params="gz14eJxjYIAACQYYOOHEgAZY0QVwggZ7CB6pfNoAAE8gGQg="/>
     <rdf:li
      darktable:num="4"
      darktable:operation="gamma"
      darktable:enabled="1"
      darktable:modversion="1"
      darktable:params="0000000000000000"
      darktable:multi_name=""
      darktable:multi_priority="0"
      darktable:blendop_version="11"
      darktable:blendop_params="gz14eJxjYIAACQYYOOHEgAZY0QVwggZ7CB6pfNoAAE8gGQg="/>
     <rdf:li
      darktable:num="5"
      darktable:operation="temperature"
      darktable:enabled="{enable_temperature}"
      darktable:modversion="3"
      darktable:params="{temperature_params}"
      darktable:multi_name=""
      darktable:multi_priority="0"
      darktable:blendop_version="11"
      darktable:blendop_params="gz14eJxjYIAACQYYOOHEgAZY0QVwggZ7CB6pfNoAAE8gGQg="/>
     <rdf:li
      darktable:num="6"
      darktable:operation="highlights"
      darktable:enabled="{enable_highlights}"
      darktable:modversion="2"
      darktable:params="{highlights_params}"
      darktable:multi_name=""
      darktable:multi_priority="0"
      darktable:blendop_version="11"
      darktable:blendop_params="gz13eJxjYGBgYARiCQYYOOHEgAZY0QVwggZ7CB6pfNoAAFDAGQk="/>
     <rdf:li
      darktable:num="7"
      darktable:operation="sharpen"
      darktable:enabled="{enable_sharpen}"
      darktable:modversion="1"
      darktable:params="{sharpen_params}"
      darktable:multi_name=""
      darktable:multi_priority="0"
      darktable:blendop_version="11"
      darktable:blendop_params="gz13eJxjYGBgYAJiCQYYOOHEgAZY0QVwggZ7CB6pfNoAAFJgGQo="/>
     <rdf:li
      darktable:num="8"
      darktable:operation="filmicrgb"
      darktable:enabled="{enable_filmicrgb}"
      darktable:modversion="5"
      darktable:params="{filmicrgb_params}"
      darktable:multi_name=""
      darktable:multi_priority="0"
      darktable:blendop_version="11"
      darktable:blendop_params="gz10eJxjYGBgYAFiCQYYOOHEgAZY0QVwggZ7CB6pfOygYtaVAyCMi08IAAB/xiOk"/>
     <rdf:li
      darktable:num="9"
      darktable:operation="exposure"
      darktable:enabled="{enable_exposure}"
      darktable:modversion="6"
      darktable:params="{exposure_params}"
      darktable:multi_name=""
      darktable:multi_priority="0"
      darktable:blendop_version="11"
      darktable:blendop_params="gz10eJxjYGBgYAFiCQYYOOHEgAZY0QVwggZ7CB6pfOygYtaVAyCMi08IAAB/xiOk"/>
     <rdf:li
      darktable:num="10"
      darktable:operation="flip"
      darktable:enabled="1"
      darktable:modversion="2"
      darktable:params="ffffffff"
      darktable:multi_name=""
      darktable:multi_priority="0"
      darktable:blendop_version="11"
      darktable:blendop_params="gz14eJxjYIAACQYYOOHEgAZY0QVwggZ7CB6pfNoAAE8gGQg="/>
     <rdf:li
      darktable:num="11"
      darktable:operation="colorbalancergb"
      darktable:enabled="{enable_colorbalancergb}"
      darktable:modversion="4"
      darktable:params="{colorbalancergb_params}"
      darktable:multi_name=""
      darktable:multi_priority="0"
      darktable:blendop_version="11"
      darktable:blendop_params="000000000400000018000000000000000000c84200000000000000000000000000000000050000000000000000000000000000000000000000000000000000000000000000000000000000000000803f0000803f00000000000000000000803f0000803f00000000000000000000803f0000803f00000000000000000000803f0000803f00000000000000000000803f0000803f00000000000000000000803f0000803f00000000000000000000803f0000803f00000000000000000000803f0000803f00000000000000000000803f0000803f00000000000000000000803f0000803f00000000000000000000803f0000803f00000000000000000000803f0000803f00000000000000000000803f0000803f00000000000000000000803f0000803f00000000000000000000803f0000803f00000000000000000000803f0000803f0000000000000000000000000000000000000000000000000000000000000000789ad4c0789ad4c00000000000000000789ad4c0789ad4c000000000000000000000000000000000000000000000000000000000000000000000000000000000"/>
    </rdf:Seq>
   </darktable:history>
  </rdf:Description>
 </rdf:RDF>
</x:xmpmeta>
'''


def to_hex_string(x):
    if type(x) == bytes:
        # Convert each byte into two hex characters and concat into a string.
        return ''.join([format(byte, '02x') for byte in x])
    elif type(x) == float:
        return to_hex_string(struct.pack('f', x))
    elif type(x) == int or type(x) == bool:
        # Pack bool as int (4 bytes).
        return to_hex_string(struct.pack('i', x))
    elif type(x) == np.uint16:
        return to_hex_string(struct.pack('H', x))
    elif type(x) == list:
        return ''.join([to_hex_string(y) for y in x])
    else:
        raise ValueError("Unsupported type: %s" % type(x))


def default_blendif_parameters():
    return [
        0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0,
        1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0,
        0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0,
        1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0,
        0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0
    ]


@dataclass
class BlendParams:
    # dt_develop_mask_mode_t
    mask_mode: int = 0
    # dt_develop_blend_colorspace_t
    blend_cst: int = 0
    # dt_develop_blend_mode_t
    blend_mode: int = 0x18  # DEVELOP_BLEND_NORMAL2
    blend_parameter: float = 0.0
    opacity: float = 100.0

    mask_combine: int = 0  # DEVELOP_COMBINE_NORM_EXCL
    mask_id: int = 0
    blendif: int = 0
    feathering_radius: float = 0.0
    feathering_guide: int = 0x05  # DEVELOP_MASK_GUIDE_IN_AFTER_BLUR
    blur_radius: float = 0
    contrast: float = 0
    brightness: float = 0
    details: float = 0

    # offset = 14 (in 4-byte words)
    reserved: list[int] = field(default_factory=lambda: [0] * 3)

    # 64 floats
    # offset = 17
    blendif_parameters: list[float] = field(
        default_factory=default_blendif_parameters)

    # 16 floats
    # offset = 81
    blendif_boost_factors: list[float] = field(
        default_factory=lambda: [0.0] * 16)

    # dt_dev_operation_t
    # Must be 20 bytes
    # offset = 97
    raster_mask_source: bytes = field(default_factory=lambda: bytes(20))

    raster_mask_instance: int = 0
    raster_mask_id: int = 0

    raster_mask_invert: bool = False

    def to_hex_string(self):
        return to_hex_string([getattr(self, fd.name) for fd in fields(self)])


@dataclass
class RawPrepareParams:
    # Parse these out of Default Crop Origin using exiftool
    x: int = 0
    y: int = 0
    # Parse these out of Default Crop Size using exiftool
    width: int = 0
    height: int = 0
    # Parse this out as "Black Level" and "Black Level Repeat Dim".
    # If repeat dim is (1, 1)), then it's a single value and just repeat it 4 times.
    # If it's (2, 2), then repeat it's 4 values.
    # Use make_black_levels() to get the type right.
    black_levels: list[np.uint16] = field(
        default_factory=lambda: [np.uint16(0)] * 4)
    # Actually uint16 as well, but because of struct alignment, it's packed as an int.
    white_point: int = 0

    def to_hex_string(self):
        return to_hex_string([getattr(self, fd.name) for fd in fields(self)])


def make_black_levels(bl_0, bl_1, bl_2, bl_3):
    return [np.uint16(bl_0), np.uint16(bl_1), np.uint16(bl_2), np.uint16(bl_3)]


@dataclass
class ColorBalanceRGBParams:
    # v1 params: 24 deprecated floats but they seem to have some effect in the code, ugh.
    shadows_Y: float = 0.0
    shadows_C: float = 0.0
    shadows_H: float = 0.0
    midtones_Y: float = 0.0
    midtones_C: float = 0.0
    midtones_H: float = 0.0
    highlights_Y: float = 0.0
    highlights_C: float = 0.0
    highlights_H: float = 0.0
    global_Y: float = 0.0
    global_C: float = 0.0
    global_H: float = 0.0
    shadows_weight: float = 1.0
    white_fulcrum: float = 0.0
    highlights_weight: float = 1.0
    chroma_shadows: float = 0.0
    chroma_highlights: float = 0.0
    chroma_global: float = 0.0
    chroma_midtones: float = 0.0
    saturation_global: float = 0.0
    saturation_highlights: float = 0.0
    saturation_midtones: float = 0.0
    saturation_shadows: float = 0.0
    hue_angle: float = 0.0

    # v2 params: 4 deprecated floats
    brilliance_global: float = 0.0
    brilliance_highlights: float = 0.0
    brilliance_midtones: float = 0.0
    brilliance_shadows: float = 0.0

    # v3 params: 1 deprecated param
    mask_grey_fulcrum: float = 0.1845

    # v4 params (current)
    vibrance: float = 0.0  # $MIN: -1.0 $MAX: 1.0 $DEFAULT: 0 $DESCRIPTION: "global vibrance"
    grey_fulcrum: float = 0.1845  # $MIN:  0.0 $MAX: 1.0 $DEFAULT: 0.1845 $DESCRIPTION: "contrast gray fulcrum"
    contrast: float = 0.0  # $MIN: -1.0 $MAX: 1.0 $DEFAULT: 0. $DESCRIPTION: "contrast"

    def to_hex_string(self):
        return to_hex_string([getattr(self, fd.name) for fd in fields(self)])


# blend_params for exposure and filmicrgb modules
def make_exposure_filmicrgb_blend_params() -> BlendParams:
    # From _blend_init_blendif_boost_parameters, set blendif_boost_factors[
    # DEVELOP_BLENDIF_Jz_in,
    # DEVELOP_BLENDIF_Cz_in,
    # DEVELOP_BLENDIF_Jz_out,
    # DEVELOP_BLENDIF_Cz_out
    # ] to -6.64385619f

    boost_factors = [0.0] * 16
    boost_factors[8] = -6.64385619
    boost_factors[9] = -6.64385619
    boost_factors[12] = -6.64385619
    boost_factors[13] = -6.64385619

    return BlendParams(
        blend_cst=4,  # BLEND_CS_RGB_SCENE
        blendif_boost_factors=boost_factors)


# blend_params for highlights module
def make_highlights_blend_params() -> BlendParams:
    return BlendParams(
        blend_cst=1,  # BLEND_CS_RAW
    )


# blend_params for sharpen module
def make_sharpen_blend_params() -> BlendParams:
    return BlendParams(
        blend_cst=2,  # BLEND_CS_LAB
    )


# dt_iop_exposure_params_t
@dataclass
class ExposureParams:
    # In C, they are enums:
    # MANUAL = 0     "manual"
    # DEFLICKER = 1  "automatic"
    mode: int = 0

    black: float = 0.0
    exposure: float = 0.0
    deflicker_percentile: float = 50.0
    deflicker_target_level: float = -4.0

    compensate_exposure_bias: bool = False

    def to_hex_string(self):
        return to_hex_string([getattr(self, fd.name) for fd in fields(self)])


@dataclass
class FilmicRGBParams:
    grey_point_source: float = 18.45
    black_point_source: float = -7.75
    white_point_source: float = 4.400000095367432
    reconstruct_threshold: float = 3.0
    reconstruct_feather: float = 3.0
    reconstruct_bloom_vs_details: float = 100.0
    reconstruct_grey_vs_color: float = 100.0
    reconstruct_structure_vs_texture: float = 0.0
    security_factor: float = 0.0
    grey_point_target: float = 18.45
    black_point_target: float = 0.01517634
    white_point_target: float = 100.0
    output_power: float = 3.75882887840271
    latitude: float = 50.0
    contrast: float = 1.1
    saturation: float = 0.0
    balance: float = 0.0
    noise_level: float = 0.2

    # enum dt_iop_filmicrgb_methods_type_t
    preserve_color: int = 3  # "preserve chrominance"
    # enum dt_iop_filmicrgb_colorscience_type_t
    # DT_FILMIC_COLORSCIENCE_V3 = 2
    version: int = 2  # "color science"
    auto_hardness: bool = True  # "auto adjust hardness"
    custom_grey: bool = False  # "use custom middle-gray values"
    high_quality_reconstruction: int = 1  # "iterations of high-quality reconstruction"
    # enum dt_iop_filmic_noise_distribution_t
    # DT_NOISE_GAUSSIAN = 1
    noise_distribution: int = 1  # "type of noise"

    # dt_iop_filmicrgb_curve_type_t
    # DT_FILMIC_CURVE_RATIONAL = 2
    shadows: int = 2  #  "contrast in shadows"
    highlights: int = 2  #  "contrast in highlights"

    compensate_icc_black: bool = False  # "compensate output ICC profile black point"

    # enum dt_iop_filmicrgb_spline_version_type_t
    # DT_FILMIC_SPLINE_VERSION_V3 = 2
    spline_version: int = 2  # "spline handling"

    def to_hex_string(self):
        return to_hex_string([getattr(self, fd.name) for fd in fields(self)])


# dt_iop_highlights_params_t
@dataclass
class HighlightsParams:
    # In C, they are enums:
    # CLIP = 0,
    # LCH = 1,
    # INPAINT = 2 ("reconstruct color").
    mode: int = 0

    # In C, the comments say they're unused.
    blendL: float = 1.0
    blendC: float = 0.0
    blendH: float = 0.0

    # Clipping threshold.
    clip: float = 1.0

    def to_hex_string(self):
        return to_hex_string([getattr(self, fd.name) for fd in fields(self)])


@dataclass
class SharpenParams:
    radius: float = 2.0
    amount: float = 0.5
    threshold: float = 0.5

    def to_hex_string(self):
        return to_hex_string([getattr(self, fd.name) for fd in fields(self)])


@dataclass
class TemperatureParams:
    red: float = 1.420689582824707
    green: float = 1.0
    blue: float = 2.0
    g2: float = math.nan

    def to_hex_string(self):
        return to_hex_string([getattr(self, fd.name) for fd in fields(self)])


# Pipeline order is as follows, * means it can be skipped and therefore has a bool param.
# 0 rawprepare
# 1 temperature      *
# 2 highlights       *
# 3 demosaic
# 4 flip             *
# 5 exposure         *
# 6 colorin
# 7 sharpen          *
# 8 colorbalancergb  *
# 9 filmicrgb        *
# 10 colorout
def get_pipe_xmp(raw_prepare_params=RawPrepareParams(),
                 temperature_params=TemperatureParams(),
                 highlights_params=HighlightsParams(),
                 exposure_params=ExposureParams(),
                 sharpen_params=SharpenParams(),
                 colorbalancergb_params=ColorBalanceRGBParams(),
                 filmicrgb_params=FilmicRGBParams()):
    def zineo(x):
        return 0 if x is None else 1

    # Terrible hack - just disabling a stage doesn't let you set params to "",
    # it must be a valid hex string (because it checks version numbers).
    def to_hex(x, default_value):
        return default_value.to_hex_string() if x is None else x.to_hex_string(
        )

    return _FMT_STR.format(
        raw_prepare_params=to_hex(raw_prepare_params, RawPrepareParams()),
        enable_temperature=zineo(temperature_params),
        temperature_params=to_hex(temperature_params, TemperatureParams()),
        enable_highlights=zineo(highlights_params),
        highlights_params=to_hex(highlights_params, HighlightsParams()),
        enable_exposure=zineo(exposure_params),
        exposure_params=to_hex(exposure_params, ExposureParams()),
        enable_sharpen=zineo(sharpen_params),
        sharpen_params=to_hex(sharpen_params, SharpenParams()),
        enable_colorbalancergb=zineo(colorbalancergb_params),
        colorbalancergb_params=to_hex(colorbalancergb_params,
                                      ColorBalanceRGBParams()),
        enable_filmicrgb=zineo(filmicrgb_params),
        filmicrgb_params=to_hex(filmicrgb_params, FilmicRGBParams()))


def render(src_dng_path, dst_path, pipe_stage_flags):
    with tempfile.NamedTemporaryFile(mode="w+t", suffix=".xmp",
                                     delete=False) as f:
        f.write(get_pipe_xmp(**pipe_stage_flags))
        xmp_path = f.name
    args = [
        _DARKTABLE_CLI, src_dng_path, xmp_path, dst_path, "--core",
        "--disable-opencl", "-d", "perf"
    ]
    print('Running:\n', ' '.join(args), '\n')
    subprocess.run(args)


def render_stages(src_dng_path, dst_dir):
    os.makedirs(dst_dir, exist_ok=True)

    # Make a list of parameter dictionaries, *disabling* pipeline stages from
    # back to front.
    #
    # Set a stage's params to None to disable it. Otherwise, pass it a
    # <stage>Params instance.

    # Everything enabled.
    params_dicts = [{}]
    # Disable final stage, everything else uses default params.
    params_dicts.append(dict(params_dicts[-1], filmicrgb_params=None)
                        )  # TODO(etseng): Remove this stage by turning it off.
    # Disable penultimate stage, everything else uses default params.
    params_dicts.append(dict(params_dicts[-1], colorbalancergb_params=None))
    # ...
    params_dicts.append(dict(params_dicts[-1], sharpen_params=None))
    params_dicts.append(dict(params_dicts[-1], exposure_params=None))
    params_dicts.append(dict(params_dicts[-1], highlights_params=None))
    params_dicts.append(dict(params_dicts[-1], temperature_params=None))

    for i in range(len(params_dicts)):
        params_dict = params_dicts[i]
        dst_path = os.path.join(dst_dir, f"{i:03}.tif")
        render(src_dng_path, dst_path, params_dict)


#print('blend_params: ', BlendParams().to_hex_string())
#
#print('colorbalancergb_params: ', ColorBalanceRGBParams().to_hex_string())
#
#print('exposure_blend_params: ',
#      make_exposure_filmicrgb_blend_params().to_hex_string())
#
#print('highlights_blend_params: ',
#      make_highlights_blend_params().to_hex_string())
#
#print('sharpen_blend_params: ', make_sharpen_blend_params().to_hex_string())
#
#exposure_params = ExposureParams(black=-0.000244140625,
#                                 exposure=0.5,
#                                 compensate_exposure_bias=True)
#print('exposure_params: ', exposure_params.to_hex_string())
#
#print('filmic_rgb_params: ', FilmicRGBParams().to_hex_string())
#
#print('temp_params: ', TemperatureParams().to_hex_string())
#
#print('sharpen_params: ', SharpenParams().to_hex_string())
#
#print('highlights_params: ', HighlightsParams().to_hex_string())

#render_stages("/Users/jiawen/Downloads/IMG_20200827_100935.dng",
#              "/tmp/darktable_stages_IMG_20200827_100935")
