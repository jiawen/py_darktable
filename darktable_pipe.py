import math
import os
import struct
import subprocess
import tempfile
from dataclasses import dataclass, field

# Point me to darktable-cli for 3.8.
_DARKTABLE_CLI = "/Applications/darktable.app/Contents/MacOS/darktable-cli"

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
      darktable:params="080000000800000008000000080000004000400040004000ff030000"
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
      darktable:params="28d9b53f0000803f000000400000c07f"
      darktable:multi_name=""
      darktable:multi_priority="0"
      darktable:blendop_version="11"
      darktable:blendop_params="gz14eJxjYIAACQYYOOHEgAZY0QVwggZ7CB6pfNoAAE8gGQg="/>
     <rdf:li
      darktable:num="6"
      darktable:operation="highlights"
      darktable:enabled="{enable_highlights}"
      darktable:modversion="2"
      darktable:params="000000000000803f00000000000000000000803f"
      darktable:multi_name=""
      darktable:multi_priority="0"
      darktable:blendop_version="11"
      darktable:blendop_params="gz13eJxjYGBgYARiCQYYOOHEgAZY0QVwggZ7CB6pfNoAAFDAGQk="/>
     <rdf:li
      darktable:num="7"
      darktable:operation="sharpen"
      darktable:enabled="{enable_sharpen}"
      darktable:modversion="1"
      darktable:params="000000400000003f0000003f"
      darktable:multi_name=""
      darktable:multi_priority="0"
      darktable:blendop_version="11"
      darktable:blendop_params="gz13eJxjYGBgYAJiCQYYOOHEgAZY0QVwggZ7CB6pfNoAAFJgGQo="/>
     <rdf:li
      darktable:num="8"
      darktable:operation="filmicrgb"
      darktable:enabled="{enable_filmicrgb}"
      darktable:modversion="5"
      darktable:params="gz02eJybNXOyIwPDjwNnz/Q4MDA4QPEJJwiGgFlANfrLKmxAYssnFADlPZyA6u1h8mfP+NgxA2kmIGaEijFCMRMUM0BpAGItFNk="
      darktable:multi_name=""
      darktable:multi_priority="0"
      darktable:blendop_version="11"
      darktable:blendop_params="gz10eJxjYGBgYAFiCQYYOOHEgAZY0QVwggZ7CB6pfOygYtaVAyCMi08IAAB/xiOk"/>
     <rdf:li
      darktable:num="9"
      darktable:operation="exposure"
      darktable:enabled="{enable_exposure}"
      darktable:modversion="6"
      darktable:params="00000000000080b90000003f00004842000080c001000000"
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
    </rdf:Seq>
   </darktable:history>
  </rdf:Description>
 </rdf:RDF>
</x:xmpmeta>
'''


# Pipeline order is as follows, * means it can be skipped and therefore has a bool param.
# 0 rawprepare
# 1 temperature  *
# 2 highlights   *
# 3 demosaic
# 4 flip         *
# 5 exposure     *
# 6 colorin
# 7 sharpen      *
# 8 filmicrgb    *
# 9 colorout
def get_pipe_xmp(enable_temperature=1,
                 enable_highlights=1,
                 enable_exposure=1,
                 enable_sharpen=1,
                 enable_filmicrgb=1):
    return FMT_STR.format(enable_temperature=enable_temperature,
                          enable_highlights=enable_highlights,
                          enable_exposure=enable_exposure,
                          enable_sharpen=enable_sharpen,
                          enable_filmicrgb=enable_filmicrgb)


def render(src_dng_path, dst_path, pipe_stage_flags):
    with tempfile.NamedTemporaryFile(mode="w+t", suffix=".xmp",
                                     delete=False) as f:
        f.write(get_pipe_xmp(**pipe_stage_flags))
        xmp_path = f.name
    args = [
        _DARKTABLE_CLI, src_dng_path, xmp_path, dst_path, "--core", "-d",
        "perf"
    ]
    print('Running:\n', ' '.join(args), '\n')
    subprocess.run(args)


def render_stages(src_dng_path, dst_dir):
    os.makedirs(dst_dir, exist_ok=True)

    # Make a list of parameter dictionaries, *disabling* pipeline stages from back
    # to front.

    # Everything enabled.
    params_dicts = [{}]
    # Disable final stage.
    params_dicts.append(dict(params_dicts[-1], enable_filmicrgb=0))
    # Disable penultimate stage.
    params_dicts.append(dict(params_dicts[-1], enable_sharpen=0))
    # ...
    params_dicts.append(dict(params_dicts[-1], enable_exposure=0))
    params_dicts.append(dict(params_dicts[-1], enable_highlights=0))
    params_dicts.append(dict(params_dicts[-1], enable_temperature=0))

    for i in range(len(params_dicts)):
        params_dict = params_dicts[i]
        dst_path = os.path.join(dst_dir, f"{i:03}.tif")
        render(src_dng_path, dst_path, params_dict)


# render_stages("IMG_20200827_100935.dng", "/tmp/darktable_stages_IMG_20200827_100935")


def to_hex_string(x):
    if type(x) == bytes:
        # Convert each byte into two hex characters and concat into a string.
        return ''.join([format(byte, '02x') for byte in x])
    if type(x) == float:
        return to_hex_string(struct.pack('f', x))
    elif type(x) == int:
        return to_hex_string(struct.pack('i', x))
    elif type(x) == list:
        return ''.join([to_hex_string(y) for y in x])
    else:
        raise ValueError("Unsupported type: %s" % type(x))


# filmic rgb: 224 bytes
# -> 28 floats

# default:
# 00000000 00000000 18000000 00000000 0000c842 00000000 00000000 00000000 00000000 05000000 00000000 00000000 00000000 00000000 00000000 00000000
# 00000000 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f
# 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f
# 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f
# 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f
# 0000803f 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000
# 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000

# highlights:
# 00000000 01000000 18000000 00000000 0000c842 00000000 00000000 00000000 00000000 05000000 00000000 00000000 00000000 00000000 00000000 00000000
# 00000000 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f
# 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f
# 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f
# 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f
# 0000803f 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000
# 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000

# exposure and filmic_rgb:
#            0        1        2        3        4        5        6        7        8        9        A        B        C        D        E        F
# 0   00000000 04000000 18000000 00000000 0000c842 00000000 00000000 00000000 00000000 05000000 00000000 00000000 00000000 00000000 00000000 00000000
# 16  00000000 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f
# 32  0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f
# 48  0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f
# 64  0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f
# 80  0000803f 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 789ad4c0 789ad4c0 00000000 00000000 789ad4c0 789ad4c0 00000000
# 96  00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000

# sharpen:
# 00000000 02000000 18000000 00000000 0000c842 00000000 00000000 00000000 00000000 05000000 00000000 00000000 00000000 00000000 00000000 00000000
# 00000000 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f
# 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f
# 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f
# 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f 0000803f 00000000 00000000 0000803f
# 0000803f 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000
# 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000


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
    # uint32_t reserved[3];

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
        return to_hex_string([
            self.mask_mode,
            self.blend_cst,
            self.blend_mode,
            self.blend_parameter,
            self.opacity,
            self.mask_combine,
            self.mask_id,
            self.blendif,
            self.feathering_radius,
            self.feathering_guide,
            self.blur_radius,
            self.contrast,
            self.brightness,
            self.details,
            [0] * 3,  # reserved
            self.blendif_parameters,
            self.blendif_boost_factors,
            self.raster_mask_source,
            self.raster_mask_instance,
            self.raster_mask_id,
            # Treat bool as an int here so that it's packed as 4 bytes.
            int(self.raster_mask_invert)
        ])


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
        return to_hex_string([
            self.mode,
            self.black,
            self.exposure,
            self.deflicker_percentile,
            self.deflicker_target_level,
            # Treat bool as an int here so that it's packed as 4 bytes.
            int(self.compensate_exposure_bias)
        ])


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
        return to_hex_string(
            [self.mode, self.blendL, self.blendC, self.blendH, self.clip])


@dataclass
class SharpenParams:
    radius: float = 2.0
    amount: float = 0.5
    threshold: float = 0.5

    def to_hex_string(self):
        return to_hex_string([self.radius, self.amount, self.threshold])


@dataclass
class TemperatureParams:
    red: float
    green: float
    blue: float
    g2: float

    def to_hex_string(self):
        return to_hex_string([self.red, self.green, self.blue, self.g2])


print('blend_params: ', BlendParams().to_hex_string())

print('exposure_blend_params: ',
      make_exposure_filmicrgb_blend_params().to_hex_string())

print('highlights_blend_params: ',
      make_highlights_blend_params().to_hex_string())

print('sharpen_blend_params: ', make_sharpen_blend_params().to_hex_string())

exposure_params = ExposureParams(black=-0.000244140625,
                                 exposure=0.5,
                                 compensate_exposure_bias=True)
print('exposure_params: ', exposure_params.to_hex_string())

temp_params = TemperatureParams(1.420689582824707, 1.0, 2.0, math.nan)
print('temp_params: ', temp_params.to_hex_string())

print('sharpen_params: ', SharpenParams().to_hex_string())

print('highlights_params: ', HighlightsParams().to_hex_string())
