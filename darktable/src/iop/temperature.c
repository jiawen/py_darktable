/*
    This file is part of darktable,
    Copyright (C) 2009-2021 darktable developers.

    darktable is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    darktable is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with darktable.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if defined(__SSE__)
#include <xmmintrin.h>
#endif
#include <assert.h>
#include <lcms2.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "bauhaus/bauhaus.h"
#include "common/colorspaces_inline_conversions.h"
#include "common/darktable.h"
#include "common/opencl.h"
#include "control/control.h"
#include "control/conf.h"
#include "develop/develop.h"
#include "develop/imageop_gui.h"
#include "develop/imageop_math.h"
#include "develop/tiling.h"
#include "dtgtk/expander.h"
#include "external/wb_presets.c"
#include "gui/accelerators.h"
#include "gui/gtk.h"
#include "gui/color_picker_proxy.h"
#include "iop/iop_api.h"

// for Kelvin temperature and bogus WB
#include "common/colorspaces.h"
#include "external/cie_colorimetric_tables.c"

#include "iop/dump_tmp.h"

DT_MODULE_INTROSPECTION(3, dt_iop_temperature_params_t)

#define INITIALBLACKBODYTEMPERATURE 4000

#define DT_IOP_LOWEST_TEMPERATURE 1901
#define DT_IOP_HIGHEST_TEMPERATURE 25000

#define DT_IOP_LOWEST_TINT 0.135
#define DT_IOP_HIGHEST_TINT 2.326

#define DT_IOP_NUM_OF_STD_TEMP_PRESETS 4

// If you reorder presets combo, change this consts
#define DT_IOP_TEMP_AS_SHOT 0
#define DT_IOP_TEMP_SPOT 1
#define DT_IOP_TEMP_USER 2
#define DT_IOP_TEMP_D65 3

static void gui_sliders_update(struct dt_iop_module_t *self);

typedef struct dt_iop_temperature_params_t
{
  float red;    // $MIN: 0.0 $MAX: 8.0
  float green;  // $MIN: 0.0 $MAX: 8.0
  float blue;   // $MIN: 0.0 $MAX: 8.0
  float g2;     // $MIN: 0.0 $MAX: 8.0 $DESCRIPTION: "emerald"
} dt_iop_temperature_params_t;

typedef struct dt_iop_temperature_gui_data_t
{
  GtkWidget *scale_k, *scale_tint, *coeff_widgets, *scale_r, *scale_g, *scale_b, *scale_g2;
  GtkWidget *presets;
  GtkWidget *finetune;
  GtkWidget *buttonbar;
  GtkWidget *colorpicker;
  GtkWidget *btn_asshot; //As Shot
  GtkWidget *btn_user;
  GtkWidget *btn_d65;
  GtkWidget *coeffs_expander;
  GtkWidget *coeffs_toggle;
  GtkWidget *temp_label;
  GtkWidget *balance_label;
  int preset_cnt;
  int preset_num[54];
  double daylight_wb[4];
  double as_shot_wb[4];
  double mod_coeff[4];
  float mod_temp, mod_tint;
  double XYZ_to_CAM[4][3], CAM_to_XYZ[3][4];
  int colored_sliders;
  int blackbody_is_confusing;
  int expand_coeffs;
  gboolean button_bar_visible;
} dt_iop_temperature_gui_data_t;

typedef struct dt_iop_temperature_data_t
{
  float coeffs[4];
} dt_iop_temperature_data_t;

typedef struct dt_iop_temperature_global_data_t
{
  int kernel_whitebalance_4f;
  int kernel_whitebalance_1f;
  int kernel_whitebalance_1f_xtrans;
} dt_iop_temperature_global_data_t;

typedef struct dt_iop_temperature_preset_data_t
{
  int no_ft_pos;
  int min_ft_pos;
  int max_ft_pos;
} dt_iop_temperature_preset_data_t;

int legacy_params(dt_iop_module_t *self, const void *const old_params, const int old_version,
                  void *new_params, const int new_version)
{
  if(old_version == 2 && new_version == 3)
  {
    typedef struct dt_iop_temperature_params_v2_t
    {
      float temp_out;
      float coeffs[3];
    } dt_iop_temperature_params_v2_t;

    dt_iop_temperature_params_v2_t *o = (dt_iop_temperature_params_v2_t *)old_params;
    dt_iop_temperature_params_t *n = (dt_iop_temperature_params_t *)new_params;

    n->red = o->coeffs[0];
    n->green = o->coeffs[1];
    n->blue = o->coeffs[2];
    n->g2 = NAN;

    return 0;
  }
  return 1;
}

static inline void _temp_params_from_array(dt_iop_temperature_params_t *p, const double a[4])
{
  p->red = a[0]; p->green = a[1]; p->blue = a[2]; p->g2 = a[3];
}

static inline void _temp_array_from_params(double a[4], const dt_iop_temperature_params_t *p)
{
  a[0] = p->red; a[1] = p->green; a[2] = p->blue; a[3] = p->g2;
}

static int ignore_missing_wb(dt_image_t *img)
{
  // Ignore files that end with "-hdr.dng" since these are broken files we
  // generated without any proper WB tagged
  if(g_str_has_suffix(img->filename,"-hdr.dng"))
    return TRUE;

  static const char *const ignored_cameras[] = {
    "Canon PowerShot A610",
    "Canon PowerShot S3 IS",
    "Canon PowerShot A620",
    "Canon PowerShot A720 IS",
    "Canon PowerShot A630",
    "Canon PowerShot A640",
    "Canon PowerShot A650",
    "Canon PowerShot SX110 IS",
    "Mamiya ZD",
    "Canon EOS D2000C",
    "Kodak EOS DCS 1",
    "Kodak DCS560C",
    "Kodak DCS460D",
    "Nikon E5700",
    "Sony DSC-F828",
    "GITUP GIT2",
  };

  for(int i=0; i < sizeof(ignored_cameras)/sizeof(ignored_cameras[1]); i++)
    if(!strcmp(img->camera_makermodel, ignored_cameras[i]))
      return TRUE;

  return FALSE;
}


const char *name()
{
  return C_("modulename", "white balance");
}

const char *description(struct dt_iop_module_t *self)
{
  return dt_iop_set_description(self, _("scale raw RGB channels to balance white and help demosaicing"),
                                      _("corrective"),
                                      _("linear, raw, scene-referred"),
                                      _("linear, raw"),
                                      _("linear, raw, scene-referred"));
}

int default_group()
{
  return IOP_GROUP_BASIC | IOP_GROUP_GRADING;
}

int flags()
{
  return IOP_FLAGS_ALLOW_TILING | IOP_FLAGS_ONE_INSTANCE | IOP_FLAGS_UNSAFE_COPY;
}

int default_colorspace(dt_iop_module_t *self, dt_dev_pixelpipe_t *pipe, dt_dev_pixelpipe_iop_t *piece)
{
  // This module may work in RAW or RGB (e.g. for TIFF files) depending on the input
  // The module does not change the color space between the input and output, therefore implement it here
  if(piece && piece->dsc_in.cst != iop_cs_RAW)
    return iop_cs_rgb;
  return iop_cs_RAW;
}

/*
 * Spectral power distribution functions
 * https://en.wikipedia.org/wiki/Spectral_power_distribution
 */
typedef double((*spd)(unsigned long int wavelength, double TempK));

/*
 * Bruce Lindbloom, "Spectral Power Distribution of a Blackbody Radiator"
 * http://www.brucelindbloom.com/Eqn_Blackbody.html
 */
static double spd_blackbody(unsigned long int wavelength, double TempK)
{
  // convert wavelength from nm to m
  const long double lambda = (double)wavelength * 1e-9;

/*
 * these 2 constants were computed using following Sage code:
 *
 * (from http://physics.nist.gov/cgi-bin/cuu/Value?h)
 * h = 6.62606957 * 10^-34 # Planck
 * c= 299792458 # speed of light in vacuum
 * k = 1.3806488 * 10^-23 # Boltzmann
 *
 * c_1 = 2 * pi * h * c^2
 * c_2 = h * c / k
 *
 * print 'c_1 = ', c_1, ' ~= ', RealField(128)(c_1)
 * print 'c_2 = ', c_2, ' ~= ', RealField(128)(c_2)
 */

#define c1 3.7417715246641281639549488324352159753e-16L
#define c2 0.014387769599838156481252937624049081933L

  return (double)(c1 / (powl(lambda, 5) * (expl(c2 / (lambda * TempK)) - 1.0L)));

#undef c2
#undef c1
}

/*
 * Bruce Lindbloom, "Spectral Power Distribution of a CIE D-Illuminant"
 * http://www.brucelindbloom.com/Eqn_DIlluminant.html
 * and https://en.wikipedia.org/wiki/Standard_illuminant#Illuminant_series_D
 */
static double spd_daylight(unsigned long int wavelength, double TempK)
{
  cmsCIExyY WhitePoint = { 0.3127, 0.3290, 1.0 };

  /*
   * Bruce Lindbloom, "TempK to xy"
   * http://www.brucelindbloom.com/Eqn_T_to_xy.html
   */
  cmsWhitePointFromTemp(&WhitePoint, TempK);

  const double M = (0.0241 + 0.2562 * WhitePoint.x - 0.7341 * WhitePoint.y),
               m1 = (-1.3515 - 1.7703 * WhitePoint.x + 5.9114 * WhitePoint.y) / M,
               m2 = (0.0300 - 31.4424 * WhitePoint.x + 30.0717 * WhitePoint.y) / M;

  const unsigned long int j
      = ((wavelength - cie_daylight_components[0].wavelength)
         / (cie_daylight_components[1].wavelength - cie_daylight_components[0].wavelength));

  return (cie_daylight_components[j].S[0] + m1 * cie_daylight_components[j].S[1]
          + m2 * cie_daylight_components[j].S[2]);
}

/*
 * Bruce Lindbloom, "Computing XYZ From Spectral Data (Emissive Case)"
 * http://www.brucelindbloom.com/Eqn_Spect_to_XYZ.html
 */
static cmsCIEXYZ spectrum_to_XYZ(double TempK, spd I)
{
  cmsCIEXYZ Source = {.X = 0.0, .Y = 0.0, .Z = 0.0 };

  /*
   * Color matching functions
   * https://en.wikipedia.org/wiki/CIE_1931_color_space#Color_matching_functions
   */
  for(size_t i = 0; i < cie_1931_std_colorimetric_observer_count; i++)
  {
    const unsigned long int lambda = cie_1931_std_colorimetric_observer[0].wavelength
                                     + (cie_1931_std_colorimetric_observer[1].wavelength
                                        - cie_1931_std_colorimetric_observer[0].wavelength) * i;
    const double P = I(lambda, TempK);
    Source.X += P * cie_1931_std_colorimetric_observer[i].xyz.X;
    Source.Y += P * cie_1931_std_colorimetric_observer[i].xyz.Y;
    Source.Z += P * cie_1931_std_colorimetric_observer[i].xyz.Z;
  }

  // normalize so that each component is in [0.0, 1.0] range
  const double _max = fmax(fmax(Source.X, Source.Y), Source.Z);
  Source.X /= _max;
  Source.Y /= _max;
  Source.Z /= _max;

  return Source;
}

// TODO: temperature and tint cannot be disjoined! (here it assumes no tint)
static cmsCIEXYZ temperature_to_XYZ(double TempK)
{
  if(TempK < DT_IOP_LOWEST_TEMPERATURE) TempK = DT_IOP_LOWEST_TEMPERATURE;
  if(TempK > DT_IOP_HIGHEST_TEMPERATURE) TempK = DT_IOP_HIGHEST_TEMPERATURE;

  if(TempK < INITIALBLACKBODYTEMPERATURE)
  {
    // if temperature is less than 4000K we use blackbody,
    // because there will be no Daylight reference below 4000K...
    return spectrum_to_XYZ(TempK, spd_blackbody);
  }
  else
  {
    return spectrum_to_XYZ(TempK, spd_daylight);
  }
}

static cmsCIEXYZ temperature_tint_to_XYZ(double TempK, double tint)
{
  cmsCIEXYZ xyz = temperature_to_XYZ(TempK);

  xyz.Y /= tint; // TODO: This is baaad!

  return xyz;
}

// binary search inversion
static void XYZ_to_temperature(cmsCIEXYZ XYZ, float *TempK, float *tint)
{
  double maxtemp = DT_IOP_HIGHEST_TEMPERATURE, mintemp = DT_IOP_LOWEST_TEMPERATURE;
  cmsCIEXYZ _xyz;

  for(*TempK = (maxtemp + mintemp) / 2.0; (maxtemp - mintemp) > 1.0; *TempK = (maxtemp + mintemp) / 2.0)
  {
    _xyz = temperature_to_XYZ(*TempK);
    if(_xyz.Z / _xyz.X > XYZ.Z / XYZ.X)
      maxtemp = *TempK;
    else
      mintemp = *TempK;
  }

  *tint = (_xyz.Y / _xyz.X) / (XYZ.Y / XYZ.X); // TODO: Fix this to move orthogonally to planckian locus


  if(*TempK < DT_IOP_LOWEST_TEMPERATURE) *TempK = DT_IOP_LOWEST_TEMPERATURE;
  if(*TempK > DT_IOP_HIGHEST_TEMPERATURE) *TempK = DT_IOP_HIGHEST_TEMPERATURE;
  if(*tint < DT_IOP_LOWEST_TINT) *tint = DT_IOP_LOWEST_TINT;
  if(*tint > DT_IOP_HIGHEST_TINT) *tint = DT_IOP_HIGHEST_TINT;
}

static void xyz2mul(dt_iop_module_t *self, cmsCIEXYZ xyz, double mul[4])
{
  dt_iop_temperature_gui_data_t *g = (dt_iop_temperature_gui_data_t *)self->gui_data;

  double XYZ[3] = { xyz.X, xyz.Y, xyz.Z };

  double CAM[4];
  for(int k = 0; k < 4; k++)
  {
    CAM[k] = 0.0;
    for(int i = 0; i < 3; i++)
    {
      CAM[k] += g->XYZ_to_CAM[k][i] * XYZ[i];
    }
  }

  for(int k = 0; k < 4; k++) mul[k] = 1.0 / CAM[k];
}

static void temp2mul(dt_iop_module_t *self, double TempK, double tint, double mul[4])
{
  cmsCIEXYZ xyz = temperature_to_XYZ(TempK);

  xyz.Y /= tint; // TODO: This is baaad!
  /**
   * TODO:
   * problem here is that tint as it is is just a nasty hack modyfying Y component
   * and therefore changing RGB coefficients in wrong way,
   * because modifying only Y in that way doesn’t move XYZ point orthogonally
   * to planckian locus. That means it actually changes temperature and thus it lies!
   */

  xyz2mul(self, xyz, mul);
}

static cmsCIEXYZ mul2xyz(dt_iop_module_t *self, const dt_iop_temperature_params_t *p)
{
  dt_iop_temperature_gui_data_t *g = (dt_iop_temperature_gui_data_t *)self->gui_data;

  double CAM[4];
  _temp_array_from_params(CAM, p);
  for(int k = 0; k < 4; k++) CAM[k] = CAM[k] > 0.0f ? 1.0 / CAM[k] : 0.0f;

  double XYZ[3];
  for(int k = 0; k < 3; k++)
  {
    XYZ[k] = 0.0;
    for(int i = 0; i < 4; i++)
    {
      XYZ[k] += g->CAM_to_XYZ[k][i] * CAM[i];
    }
  }

  return (cmsCIEXYZ){ XYZ[0], XYZ[1], XYZ[2] };
}

static void mul2temp(dt_iop_module_t *self, dt_iop_temperature_params_t *p, float *TempK, float *tint)
{
  XYZ_to_temperature(mul2xyz(self, p), TempK, tint);
}

/*
 * interpolate values from p1 and p2 into out.
 */
static void dt_wb_preset_interpolate(const wb_data *const p1, // the smaller tuning
                                     const wb_data *const p2, // the larger tuning (can't be == p1)
                                     wb_data *out)            // has tuning initialized
{
  const double t = CLAMP((double)(out->tuning - p1->tuning) / (double)(p2->tuning - p1->tuning), 0.0, 1.0);
  for(int k = 0; k < 3; k++)
  {
    out->channel[k] = 1.0 / (((1.0 - t) / p1->channel[k]) + (t / p2->channel[k]));
  }
}

#ifdef _OPENMP
#pragma omp declare simd aligned(inp,outp)
#endif
static inline void scaled_copy_4wide(float *const outp, const float *const inp, const float *const coeffs)
{
  // this needs to be in a separate function to make GCC8 vectorize it at -O2 as well as -O3
  for_four_channels(c, aligned(inp, coeffs, outp))
    outp[c] = inp[c] * coeffs[c];
}

void process(struct dt_iop_module_t *self, dt_dev_pixelpipe_iop_t *piece, const void *const ivoid,
             void *const ovoid, const dt_iop_roi_t *const roi_in, const dt_iop_roi_t *const roi_out)
{
  const uint32_t filters = piece->pipe->dsc.filters;
  const uint8_t(*const xtrans)[6] = (const uint8_t(*const)[6])piece->pipe->dsc.xtrans;
  const dt_iop_temperature_data_t *const d = (dt_iop_temperature_data_t *)piece->data;

  const float *const in = (const float *const)ivoid;
  float *const out = (float *const)ovoid;
  const float *const d_coeffs = d->coeffs;

  fprintf(stderr, "ELEPHANT: [TEMPERATURE]. ch = %d, bpc = %d, filters = %d\n", piece->colors, piece->bpc, filters);
  dump_tmp(in, roi_in, piece->colors, "/tmp/temperature_bayer_in.tmp");

  if(filters == 9u)
  { // xtrans float mosaiced
#ifdef _OPENMP
#pragma omp parallel for default(none) \
    dt_omp_firstprivate(d_coeffs, in, out, roi_out, xtrans) \
    schedule(static)
#endif
    for(int j = 0; j < roi_out->height; j++)
    {
      const float DT_ALIGNED_PIXEL coeffs[3][4] =
      {
        { d_coeffs[FCxtrans(j, 0, roi_out, xtrans)], d_coeffs[FCxtrans(j, 1, roi_out, xtrans)],
          d_coeffs[FCxtrans(j, 2, roi_out, xtrans)], d_coeffs[FCxtrans(j, 3, roi_out, xtrans)] },
        { d_coeffs[FCxtrans(j, 4, roi_out, xtrans)], d_coeffs[FCxtrans(j, 5, roi_out, xtrans)],
          d_coeffs[FCxtrans(j, 6, roi_out, xtrans)], d_coeffs[FCxtrans(j, 7, roi_out, xtrans)] },
        { d_coeffs[FCxtrans(j, 8, roi_out, xtrans)], d_coeffs[FCxtrans(j, 9, roi_out, xtrans)],
          d_coeffs[FCxtrans(j, 10, roi_out, xtrans)], d_coeffs[FCxtrans(j, 11, roi_out, xtrans)] },
      };
      // process sensels four at a time
      //(note that attempting to ensure alignment for this main loop actually slowed things down marginally)
      int i = 0;
      for(int coeff = 0; i + 4 < roi_out->width; i += 4, coeff = (coeff+1)%3)
      {
        const size_t p = (size_t)j * roi_out->width + i;
        for_four_channels(c) // in and out are NOT aligned when width is not a multiple of 4
          out[p+c] = in[p+c] * coeffs[coeff][c];
      }
      // process the leftover sensels
      for(; i < roi_out->width; i++)
      {
        const size_t p = (size_t)j * roi_out->width + i;
        out[p] = in[p] * d_coeffs[FCxtrans(j, i, roi_out, xtrans)];
      }
    }
  }
  else if(filters)
  { // bayer float mosaiced
    fprintf(stderr, "ELEPHANT: [TEMPERATURE] Bayer float mosaiced.\n");
    const int width = roi_out->width;
#ifdef _OPENMP
#pragma omp parallel for default(none) \
    dt_omp_firstprivate(d_coeffs, filters, in, out, roi_out, width)       \
    schedule(static)
#endif
    for(int j = 0; j < roi_out->height; j++)
    {
      int i = 0;
      const int alignment = ((4 - (j * width & (4 - 1))) & (4 - 1));
      const int offset_j = j + roi_out->y;

      // process the unaligned sensels at the start of the row (when width is not a multiple of 4)
      for( ; i < alignment; i++)
      {
        const size_t p = (size_t)j * width + i;
        out[p] = in[p] * d_coeffs[FC(offset_j, i + roi_out->x, filters)];
      }
      const dt_aligned_pixel_t coeffs = { d_coeffs[FC(offset_j, i + roi_out->x, filters)],
                                          d_coeffs[FC(offset_j, i + roi_out->x + 1,filters)],
                                          d_coeffs[FC(offset_j, i + roi_out->x + 2, filters)],
                                          d_coeffs[FC(offset_j, i + roi_out->x + 3, filters)] };
      // process sensels four at a time
      for(; i < (width & ~3); i += 4)
      {
        const size_t p = (size_t)j * width + i;
        scaled_copy_4wide(out + p,in + p, coeffs);
      }
      // process the leftover sensels
      for(i = width & ~3; i < width; i++)
      {
        const size_t p = (size_t)j * width + i;
        out[p] = in[p] * d_coeffs[FC(j + roi_out->y, i + roi_out->x, filters)];
      }
    }
  }
  else
  { // non-mosaiced
    const size_t ch = piece->colors;

#ifdef _OPENMP
#pragma omp parallel for SIMD() default(none) \
    dt_omp_firstprivate(ch, d, in, out, roi_out) \
    schedule(static) \
    collapse(2)
#endif
    for(size_t k = 0; k < ch * roi_out->width * roi_out->height; k += ch)
    {
      for(ptrdiff_t c = 0; c < 3; c++)
      {
        const size_t p = k + c;
        out[p] = in[p] * d->coeffs[c];
      }
    }

    if(piece->pipe->mask_display & DT_DEV_PIXELPIPE_DISPLAY_MASK)
      dt_iop_alpha_copy(ivoid, ovoid, roi_out->width, roi_out->height);
  }

  piece->pipe->dsc.temperature.enabled = 1;
  for(int k = 0; k < 4; k++)
  {
    piece->pipe->dsc.temperature.coeffs[k] = d->coeffs[k];
    piece->pipe->dsc.processed_maximum[k] = d->coeffs[k] * piece->pipe->dsc.processed_maximum[k];
    self->dev->proxy.wb_coeffs[k] = d->coeffs[k];
  }
  dump_tmp(out, roi_out, piece->colors, "/tmp/temperature_bayer_out.tmp");
}

#if defined(__SSE__)
void process_sse2(struct dt_iop_module_t *self, dt_dev_pixelpipe_iop_t *piece, const void *const ivoid,
                  void *const ovoid, const dt_iop_roi_t *const roi_in, const dt_iop_roi_t *const roi_out)
{
  const uint32_t filters = piece->pipe->dsc.filters;
  dt_iop_temperature_data_t *d = (dt_iop_temperature_data_t *)piece->data;
  if(filters)
  { // xtrans float mosaiced or bayer float mosaiced
    // plain C version is same speed for Bayer and actually a bit faster for Xtrans, so use it instead
    process(self,piece,ivoid,ovoid,roi_in,roi_out);
    return;
  }
  else
  { // non-mosaiced
    const size_t ch = piece->colors;

    const __m128 coeffs = _mm_set_ps(1.0f, d->coeffs[2], d->coeffs[1], d->coeffs[0]);

#ifdef _OPENMP
#pragma omp parallel for default(none) \
    dt_omp_firstprivate(ch, coeffs, ivoid, ovoid, roi_out) \
    shared(d) \
    schedule(static)
#endif
    for(int k = 0; k < roi_out->height; k++)
    {
      const float *in = ((float *)ivoid) + ch * k * roi_out->width;
      float *out = ((float *)ovoid) + ch * k * roi_out->width;
      for(int j = 0; j < roi_out->width; j++, in += ch, out += ch)
      {
        const __m128 input = _mm_load_ps(in);
        const __m128 multiplied = _mm_mul_ps(input, coeffs);
        _mm_stream_ps(out, multiplied);
      }
    }
    _mm_sfence();

    if(piece->pipe->mask_display & DT_DEV_PIXELPIPE_DISPLAY_MASK)
      dt_iop_alpha_copy(ivoid, ovoid, roi_out->width, roi_out->height);
  }

  piece->pipe->dsc.temperature.enabled = 1;
  for(int k = 0; k < 4; k++)
  {
    piece->pipe->dsc.temperature.coeffs[k] = d->coeffs[k];
    piece->pipe->dsc.processed_maximum[k] = d->coeffs[k] * piece->pipe->dsc.processed_maximum[k];
    self->dev->proxy.wb_coeffs[k] = d->coeffs[k];
  }
}
#endif

#ifdef HAVE_OPENCL
int process_cl(struct dt_iop_module_t *self, dt_dev_pixelpipe_iop_t *piece, cl_mem dev_in, cl_mem dev_out,
               const dt_iop_roi_t *const roi_in, const dt_iop_roi_t *const roi_out)
{
  dt_iop_temperature_data_t *d = (dt_iop_temperature_data_t *)piece->data;
  dt_iop_temperature_global_data_t *gd = (dt_iop_temperature_global_data_t *)self->global_data;

  const int devid = piece->pipe->devid;
  const uint32_t filters = piece->pipe->dsc.filters;
  cl_mem dev_coeffs = NULL;
  cl_mem dev_xtrans = NULL;
  cl_int err = -999;
  int kernel = -1;

  if(filters == 9u)
  {
    kernel = gd->kernel_whitebalance_1f_xtrans;
  }
  else if(filters)
  {
    kernel = gd->kernel_whitebalance_1f;
  }
  else
  {
    kernel = gd->kernel_whitebalance_4f;
  }

  if(filters == 9u)
  {
    dev_xtrans
        = dt_opencl_copy_host_to_device_constant(devid, sizeof(piece->pipe->dsc.xtrans), piece->pipe->dsc.xtrans);
    if(dev_xtrans == NULL) goto error;
  }

  dev_coeffs = dt_opencl_copy_host_to_device_constant(devid, sizeof(float) * 3, d->coeffs);
  if(dev_coeffs == NULL) goto error;

  const int width = roi_in->width;
  const int height = roi_in->height;

  size_t sizes[] = { ROUNDUPWD(width), ROUNDUPHT(height), 1 };
  dt_opencl_set_kernel_arg(devid, kernel, 0, sizeof(cl_mem), (void *)&dev_in);
  dt_opencl_set_kernel_arg(devid, kernel, 1, sizeof(cl_mem), (void *)&dev_out);
  dt_opencl_set_kernel_arg(devid, kernel, 2, sizeof(int), (void *)&width);
  dt_opencl_set_kernel_arg(devid, kernel, 3, sizeof(int), (void *)&height);
  dt_opencl_set_kernel_arg(devid, kernel, 4, sizeof(cl_mem), (void *)&dev_coeffs);
  dt_opencl_set_kernel_arg(devid, kernel, 5, sizeof(uint32_t), (void *)&filters);
  dt_opencl_set_kernel_arg(devid, kernel, 6, sizeof(uint32_t), (void *)&roi_out->x);
  dt_opencl_set_kernel_arg(devid, kernel, 7, sizeof(uint32_t), (void *)&roi_out->y);
  dt_opencl_set_kernel_arg(devid, kernel, 8, sizeof(cl_mem), (void *)&dev_xtrans);
  err = dt_opencl_enqueue_kernel_2d(devid, kernel, sizes);
  if(err != CL_SUCCESS) goto error;

  dt_opencl_release_mem_object(dev_coeffs);
  dt_opencl_release_mem_object(dev_xtrans);

  piece->pipe->dsc.temperature.enabled = 1;
  for(int k = 0; k < 4; k++)
  {
    piece->pipe->dsc.temperature.coeffs[k] = d->coeffs[k];
    piece->pipe->dsc.processed_maximum[k] = d->coeffs[k] * piece->pipe->dsc.processed_maximum[k];
    self->dev->proxy.wb_coeffs[k] = d->coeffs[k];
  }
  return TRUE;

error:
  dt_opencl_release_mem_object(dev_coeffs);
  dt_opencl_release_mem_object(dev_xtrans);
  dt_print(DT_DEBUG_OPENCL, "[opencl_white_balance] couldn't enqueue kernel! %d\n", err);
  return FALSE;
}
#endif

void commit_params(struct dt_iop_module_t *self, dt_iop_params_t *p1, dt_dev_pixelpipe_t *pipe,
                   dt_dev_pixelpipe_iop_t *piece)
{
  dt_iop_temperature_params_t *p = (dt_iop_temperature_params_t *)p1;
  dt_iop_temperature_data_t *d = (dt_iop_temperature_data_t *)piece->data;
  dt_iop_temperature_gui_data_t *g = (dt_iop_temperature_gui_data_t *)self->gui_data;

  if(self->hide_enable_button)
  {
    piece->enabled = 0;
    return;
  }

  d->coeffs[0] = p->red;
  d->coeffs[1] = p->green;
  d->coeffs[2] = p->blue;
  d->coeffs[3] = p->g2;

  // 4Bayer images not implemented in OpenCL yet
  if(self->dev->image_storage.flags & DT_IMAGE_4BAYER) piece->process_cl_ready = 0;

  if(g)
  {
    // advertise on the pipe if coeffs are D65 for validity check
    gboolean is_D65 = TRUE;
    for(int c = 0; c < 3; c++)
      if(d->coeffs[c] != (float)g->daylight_wb[c]) is_D65 = FALSE;

    self->dev->proxy.wb_is_D65 = is_D65;
  }
}

void init_pipe(struct dt_iop_module_t *self, dt_dev_pixelpipe_t *pipe, dt_dev_pixelpipe_iop_t *piece)
{
  piece->data = malloc(sizeof(dt_iop_temperature_data_t));
}

void cleanup_pipe(struct dt_iop_module_t *self, dt_dev_pixelpipe_t *pipe, dt_dev_pixelpipe_iop_t *piece)
{
  free(piece->data);
  piece->data = NULL;
}

int generate_preset_combo(struct dt_iop_module_t *self)
{
  dt_iop_temperature_gui_data_t *g = (dt_iop_temperature_gui_data_t *)self->gui_data;
  int presets_found = 0;

  const char *wb_name = NULL;
  if(!dt_image_is_ldr(&self->dev->image_storage))
    for(int i = 0; i < wb_preset_count; i++)
    {
      if(presets_found >= 50) break;
      if(!strcmp(wb_preset[i].make, self->dev->image_storage.camera_maker)
         && !strcmp(wb_preset[i].model, self->dev->image_storage.camera_model))
      {
        if(!wb_name) // This is first found preset for maker/model. add section.
        {
          char *section = g_strdup_printf("%s %s", self->dev->image_storage.camera_maker, self->dev->image_storage.camera_model);
          dt_bauhaus_combobox_add_section(g->presets, section);
          g_free(section);
          g->preset_cnt++;
        }
        if(!wb_name || strcmp(wb_name, wb_preset[i].name))
        {
          // new preset found
          dt_iop_temperature_preset_data_t *preset = malloc(sizeof(dt_iop_temperature_preset_data_t));
          wb_name = wb_preset[i].name;
          preset->no_ft_pos = i;
          preset->max_ft_pos = i;
          preset->min_ft_pos = i;
          if(wb_preset[i].tuning != 0)
          {
            // finetuning found.
            // min finetuning is always first, since wb_preset is ordered.
            int ft_pos = i;
            int last_ft = wb_preset[i].tuning;
            preset->min_ft_pos = ft_pos++;
            while (strcmp(wb_name, wb_preset[ft_pos].name) == 0)
            {
              if(wb_preset[ft_pos].tuning == 0)
              {
                preset->no_ft_pos = ft_pos;
              }
              if(wb_preset[ft_pos].tuning > last_ft)
              {
                preset->max_ft_pos = ft_pos;
                last_ft = wb_preset[ft_pos].tuning;
              }
              ft_pos++;
            }

          }
          dt_bauhaus_combobox_add_full(g->presets, _(wb_preset[i].name), DT_BAUHAUS_COMBOBOX_ALIGN_RIGHT, preset, free, TRUE);
          g->preset_num[g->preset_cnt] = i;
          g->preset_cnt++;
          presets_found++;
        }
      }
    }

  return presets_found;
}

void color_finetuning_slider(struct dt_iop_module_t *self)
{
  dt_iop_temperature_gui_data_t *g = (dt_iop_temperature_gui_data_t *)self->gui_data;

  dt_bauhaus_slider_clear_stops(g->finetune);
  dt_bauhaus_slider_set_feedback(g->finetune, !g->colored_sliders);

  if(!g->colored_sliders) return;

  dt_iop_temperature_preset_data_t *preset = dt_bauhaus_combobox_get_data(g->presets);
  if(preset != NULL)
  {
    //we can do realistic/exaggerated.

    double min_tune[3] = {0.0};
    double no_tune[3] = {0.0};
    double max_tune[3] = {0.0};
    if(!g->blackbody_is_confusing)
    {
      //realistic
      const double neutral[3] = {
          1 / wb_preset[preset->no_ft_pos].channel[0],
          1 / wb_preset[preset->no_ft_pos].channel[1],
          1 / wb_preset[preset->no_ft_pos].channel[2],
      };
      for(int ch=0; ch<3; ch++)
      {
        min_tune[ch] = neutral[ch] * wb_preset[preset->min_ft_pos].channel[ch];
        no_tune[ch]  = neutral[ch] * wb_preset[preset->no_ft_pos].channel[ch];
        max_tune[ch] = neutral[ch] * wb_preset[preset->max_ft_pos].channel[ch];
      }

      const float maxsRGBmin_tune = fmaxf(fmaxf(min_tune[0], min_tune[1]), min_tune[2]);
      const float maxsRGBmax_tune = fmaxf(fmaxf(max_tune[0], max_tune[1]), max_tune[2]);

      for(int ch=0; ch<3; ch++)
      {
        min_tune[ch] = min_tune[ch] / maxsRGBmin_tune;
        no_tune[ch]  = 1.0;
        max_tune[ch] = max_tune[ch] / maxsRGBmax_tune;
      }
    }
    else
    {
      //exaggerated

      for(int ch=0; ch<3; ch++)
      {
        min_tune[ch] = 0.5;
        no_tune[ch]  = 0.9;
        max_tune[ch] = 0.5;
      }

      if(wb_preset[preset->min_ft_pos].channel[0] < wb_preset[preset->max_ft_pos].channel[0])
      {
        // from blue to red
        min_tune[0] = 0.1;
        min_tune[2] = 0.9;
        max_tune[0] = 0.9;
        max_tune[2] = 0.1;
      }
      else
      {
        //from red to blue
        min_tune[0] = 0.9;
        min_tune[2] = 0.1;
        max_tune[0] = 0.1;
        max_tune[2] = 0.9;
      }
    }

    dt_bauhaus_slider_set_stop(g->finetune, 0.0, min_tune[0], min_tune[1], min_tune[2]);
    dt_bauhaus_slider_set_stop(g->finetune, 0.5, no_tune[0],  no_tune[1],  no_tune[2]);
    dt_bauhaus_slider_set_stop(g->finetune, 1.0, max_tune[0], max_tune[1], max_tune[2]);
  }
  if(gtk_widget_get_visible(GTK_WIDGET(g->finetune)))
  {
    gtk_widget_queue_draw(GTK_WIDGET(g->finetune));
  }
}

void color_rgb_sliders(struct dt_iop_module_t *self)
{
  dt_iop_temperature_gui_data_t *g = (dt_iop_temperature_gui_data_t *)self->gui_data;

  const gboolean color_rgb = g->colored_sliders &&
                             !(self->dev->image_storage.flags & DT_IMAGE_4BAYER);

  dt_bauhaus_slider_clear_stops(g->scale_r);
  dt_bauhaus_slider_clear_stops(g->scale_g);
  dt_bauhaus_slider_clear_stops(g->scale_b);
  dt_bauhaus_slider_clear_stops(g->scale_g2);
  dt_bauhaus_slider_set_feedback(g->scale_r, !color_rgb);
  dt_bauhaus_slider_set_feedback(g->scale_g, !color_rgb);
  dt_bauhaus_slider_set_feedback(g->scale_b, !color_rgb);
  dt_bauhaus_slider_set_feedback(g->scale_g2, !color_rgb);

  if(!color_rgb) return;

  // there are 3 ways to do colored sliders: naive (independent 0->1), smart(er) (dependent 0->1) and real (coeff)

  if(FALSE)
  {
  //naive:
    dt_bauhaus_slider_set_stop(g->scale_r, 0.0, 0.0, 0.0, 0.0);
    dt_bauhaus_slider_set_stop(g->scale_r, 1.0, 1.0, 0.0, 0.0);

    dt_bauhaus_slider_set_stop(g->scale_g, 0.0, 0.0, 0.0, 0.0);
    dt_bauhaus_slider_set_stop(g->scale_g, 1.0, 0.0, 1.0, 0.0);

    dt_bauhaus_slider_set_stop(g->scale_b, 0.0, 0.0, 0.0, 0.0);
    dt_bauhaus_slider_set_stop(g->scale_b, 1.0, 0.0, 0.0, 1.0);

    dt_bauhaus_slider_set_stop(g->scale_g2, 0.0, 0.0, 0.0, 0.0);
    dt_bauhaus_slider_set_stop(g->scale_g2, 1.0, 0.0, 1.0, 0.0);
  }
  if(!g->blackbody_is_confusing)
  {
    //smart(er) than naive
    const float rchan = dt_bauhaus_slider_get(g->scale_r) / dt_bauhaus_slider_get_hard_max(g->scale_r);
    const float gchan = dt_bauhaus_slider_get(g->scale_g) / dt_bauhaus_slider_get_hard_max(g->scale_g);
    const float bchan = dt_bauhaus_slider_get(g->scale_b) / dt_bauhaus_slider_get_hard_max(g->scale_b);

    dt_bauhaus_slider_set_stop(g->scale_r, 0.0, 0.0, gchan, bchan);
    dt_bauhaus_slider_set_stop(g->scale_r, 1.0, 1.0, gchan, bchan);

    dt_bauhaus_slider_set_stop(g->scale_g, 0.0, rchan, 0.0, bchan);
    dt_bauhaus_slider_set_stop(g->scale_g, 1.0, rchan, 1.0, bchan);

    dt_bauhaus_slider_set_stop(g->scale_b, 0.0, rchan, gchan, 0.0);
    dt_bauhaus_slider_set_stop(g->scale_b, 1.0, rchan, gchan, 1.0);
  }
  else
  {
    //real (ish)
    //we consider daylight wb to be "reference white"
    const double white[3] = {
      1.0/g->daylight_wb[0],
      1.0/g->daylight_wb[1],
      1.0/g->daylight_wb[2],
    };

    const float rchanmul = dt_bauhaus_slider_get(g->scale_r);
    const float rchanmulmax = dt_bauhaus_slider_get_hard_max(g->scale_r);
    const float gchanmul = dt_bauhaus_slider_get(g->scale_g);
    const float gchanmulmax = dt_bauhaus_slider_get_hard_max(g->scale_g);
    const float bchanmul = dt_bauhaus_slider_get(g->scale_b);
    const float bchanmulmax = dt_bauhaus_slider_get_hard_max(g->scale_g);

    dt_bauhaus_slider_set_stop(g->scale_r, 0.0, white[0]*0.0, white[1]*gchanmul, white[2]*bchanmul);
    dt_bauhaus_slider_set_stop(g->scale_r, g->daylight_wb[0]/rchanmulmax, white[0]*g->daylight_wb[0], white[1]*gchanmul, white[2]*bchanmul);
    dt_bauhaus_slider_set_stop(g->scale_r, 1.0, white[0]*1.0, white[1]*(gchanmul/gchanmulmax), white[2]*(bchanmul/bchanmulmax));

    dt_bauhaus_slider_set_stop(g->scale_g, 0.0, white[0]*rchanmul, white[1]*0.0, white[2]*bchanmul);
    dt_bauhaus_slider_set_stop(g->scale_g, g->daylight_wb[1]/bchanmulmax, white[0]*rchanmul, white[1]*g->daylight_wb[1], white[2]*bchanmul);
    dt_bauhaus_slider_set_stop(g->scale_g, 1.0, white[0]*(rchanmul/rchanmulmax), white[1]*1.0, white[2]*(bchanmul/bchanmulmax));

    dt_bauhaus_slider_set_stop(g->scale_b, 0.0, white[0]*rchanmul, white[1]*gchanmul, white[2]*0.0);
    dt_bauhaus_slider_set_stop(g->scale_b, g->daylight_wb[2]/bchanmulmax, white[0]*rchanmul, white[1]*gchanmul, white[2]*g->daylight_wb[2]);
    dt_bauhaus_slider_set_stop(g->scale_b, 1.0, white[0]*(rchanmul/rchanmulmax), white[1]*(gchanmul/gchanmulmax), white[2]*1.0);
  }

  if(gtk_widget_get_visible(GTK_WIDGET(g->scale_r)))
  {
    gtk_widget_queue_draw(GTK_WIDGET(g->scale_r));
    gtk_widget_queue_draw(GTK_WIDGET(g->scale_g));
    gtk_widget_queue_draw(GTK_WIDGET(g->scale_b));
  }
}

void color_temptint_sliders(struct dt_iop_module_t *self)
{
  dt_iop_temperature_gui_data_t *g = (dt_iop_temperature_gui_data_t *)self->gui_data;

  dt_bauhaus_slider_clear_stops(g->scale_k);
  dt_bauhaus_slider_clear_stops(g->scale_tint);
  dt_bauhaus_slider_set_feedback(g->scale_k, !g->colored_sliders);
  dt_bauhaus_slider_set_feedback(g->scale_tint, !g->colored_sliders);

  if(!g->colored_sliders) return;

  const double temp_step = (double)(DT_IOP_HIGHEST_TEMPERATURE - DT_IOP_LOWEST_TEMPERATURE) / (DT_BAUHAUS_SLIDER_MAX_STOPS - 1.0);
  const double tint_step = (double)(DT_IOP_HIGHEST_TINT - DT_IOP_LOWEST_TINT) / (DT_BAUHAUS_SLIDER_MAX_STOPS - 1.0);
  const int blackbody_is_confusing = g->blackbody_is_confusing;

  const float cur_temp = dt_bauhaus_slider_get(g->scale_k);
  const float cur_tint = dt_bauhaus_slider_get(g->scale_tint);

  //we consider daylight wb to be "reference white"
  const double dayligh_white[3] = {
    1.0/g->daylight_wb[0],
    1.0/g->daylight_wb[1],
    1.0/g->daylight_wb[2],
  };

  double cur_coeffs[4] = {0.0};
  temp2mul(self, cur_temp, 1.0, cur_coeffs);
  const double cur_white[3] = {
    1.0/cur_coeffs[0],
    1.0/cur_coeffs[1],
    1.0/cur_coeffs[2],
  };

  if(blackbody_is_confusing)
  {
    // show effect of adjustment on temp/tint sliders
    for(int i = 0; i < DT_BAUHAUS_SLIDER_MAX_STOPS; i++)
    {
      const float stop = i / (DT_BAUHAUS_SLIDER_MAX_STOPS - 1.0);
      const double K = DT_IOP_LOWEST_TEMPERATURE + i * temp_step;
      const double tint = DT_IOP_LOWEST_TINT + i * tint_step;

      double coeffs_K[4];
      double coeffs_tint[4];
      temp2mul(self, K, cur_tint, coeffs_K);
      temp2mul(self, cur_temp, tint, coeffs_tint);
      coeffs_K[0] /= coeffs_K[1];
      coeffs_K[2] /= coeffs_K[1];
      coeffs_K[3] /= coeffs_K[1];
      coeffs_K[1] = 1.0;
      coeffs_tint[0] /= coeffs_tint[1];
      coeffs_tint[2] /= coeffs_tint[1];
      coeffs_tint[3] /= coeffs_tint[1];
      coeffs_tint[1] = 1.0;

      dt_aligned_pixel_t sRGB_K = { dayligh_white[0]*coeffs_K[0], dayligh_white[1]*coeffs_K[1],
                                    dayligh_white[2]*coeffs_K[2] };
      dt_aligned_pixel_t sRGB_tint = {cur_white[0]*coeffs_tint[0], cur_white[1]*coeffs_tint[1],
                                      cur_white[2]*coeffs_tint[2]};
      const float maxsRGB_K = fmaxf(fmaxf(sRGB_K[0], sRGB_K[1]), sRGB_K[2]);
      const float maxsRGB_tint = fmaxf(fmaxf(sRGB_tint[0], sRGB_tint[1]),sRGB_tint[2]);

      if(maxsRGB_K > 1.f)
      {
        for(int ch=0; ch<3; ch++)
        {
          sRGB_K[ch] = fmaxf(sRGB_K[ch] / maxsRGB_K, 0.f);
        }
      }
      if(maxsRGB_tint > 1.f)
      {
        for(int ch=0; ch<3; ch++)
        {
          sRGB_tint[ch] = fmaxf(sRGB_tint[ch] / maxsRGB_tint, 0.f);
        }
      }
      dt_bauhaus_slider_set_stop(g->scale_k, stop, sRGB_K[0], sRGB_K[1], sRGB_K[2]);
      dt_bauhaus_slider_set_stop(g->scale_tint, stop, sRGB_tint[0], sRGB_tint[1], sRGB_tint[2]);
    }
  }
  else
  {
    // reflect actual black body colors for the temperature slider
    for(int i = 0; i < DT_BAUHAUS_SLIDER_MAX_STOPS; i++)
    {
      const float stop = i / (DT_BAUHAUS_SLIDER_MAX_STOPS - 1.0);
      const double K = DT_IOP_LOWEST_TEMPERATURE + i * temp_step;
      const double tint = DT_IOP_LOWEST_TINT + i * tint_step;

      const cmsCIEXYZ cmsXYZ_temp = temperature_tint_to_XYZ(K,cur_tint);
      const cmsCIEXYZ cmsXYZ_tint = temperature_tint_to_XYZ(cur_temp, tint);
      dt_aligned_pixel_t XYZ_temp = {cmsXYZ_temp.X, cmsXYZ_temp.Y, cmsXYZ_temp.Z};
      dt_aligned_pixel_t XYZ_tint = {cmsXYZ_tint.X, cmsXYZ_tint.Y, cmsXYZ_tint.Z};
      dt_aligned_pixel_t sRGB_temp;
      dt_aligned_pixel_t sRGB_tint;

      dt_XYZ_to_Rec709_D65(XYZ_temp, sRGB_temp);
      dt_XYZ_to_Rec709_D65(XYZ_tint, sRGB_tint);

      const float maxsRGB_temp = fmaxf(fmaxf(sRGB_temp[0], sRGB_temp[1]), sRGB_temp[2]);
      const float maxsRGB_tint = fmaxf(fmaxf(sRGB_tint[0], sRGB_tint[1]), sRGB_tint[2]);

      if(maxsRGB_temp > 1.f)
      {
        for(int ch=0; ch<3; ch++)
        {
          sRGB_temp[ch] = fmaxf(sRGB_temp[ch] / maxsRGB_temp, 0.f);
        }
      }

      if(maxsRGB_tint > 1.f)
      {
        for(int ch=0; ch<3; ch++)
        {
          sRGB_tint[ch] = fmaxf(sRGB_tint[ch] / maxsRGB_tint, 0.f);
        }
      }

      dt_bauhaus_slider_set_stop(g->scale_k, stop, sRGB_temp[0], sRGB_temp[1], sRGB_temp[2]);
      dt_bauhaus_slider_set_stop(g->scale_tint, stop, sRGB_tint[0], sRGB_tint[1], sRGB_tint[2]);
    }
  }

  if(gtk_widget_get_visible(GTK_WIDGET(g->scale_k)))
  {
    gtk_widget_queue_draw(GTK_WIDGET(g->scale_k));
    gtk_widget_queue_draw(GTK_WIDGET(g->scale_tint));
  }
}

static void _display_wb_error(struct dt_iop_module_t *self)
{
  // this module instance is doing chromatic adaptation
  dt_iop_temperature_gui_data_t *g = (dt_iop_temperature_gui_data_t *)self->gui_data;
  if(g == NULL) return;

  ++darktable.gui->reset;

  if(self->dev->proxy.chroma_adaptation != NULL && !self->dev->proxy.wb_is_D65)
  {
    // our second biggest problem : another module is doing CAT elsewhere in the pipe
    dt_iop_set_module_trouble_message(self, _("white balance applied twice"),
                                      _("the color calibration module is enabled,\n"
                                        "and performing chromatic adaptation.\n"
                                        "set the white balance here to camera reference (D65)\n"
                                        "or disable chromatic adaptation in color calibration."),
                                      "double application of white balance");
  }
  else
  {
    // no longer in trouble
    dt_iop_set_module_trouble_message(self, NULL, NULL, NULL);
  }

  --darktable.gui->reset;
}


void gui_focus(struct dt_iop_module_t *self, gboolean in)
{
  _display_wb_error(self);
}


void gui_update(struct dt_iop_module_t *self)
{
  dt_iop_temperature_gui_data_t *g = (dt_iop_temperature_gui_data_t *)self->gui_data;
  dt_iop_temperature_params_t *p = (dt_iop_temperature_params_t *)self->params;

  if(self->hide_enable_button) return;

  dt_iop_color_picker_reset(self, TRUE);

  float tempK, tint;
  mul2temp(self, p, &tempK, &tint);

  dt_bauhaus_slider_set(g->scale_k, tempK);
  dt_bauhaus_slider_set(g->scale_tint, tint);
  dt_bauhaus_slider_set(g->scale_r, p->red);
  dt_bauhaus_slider_set(g->scale_g, p->green);
  dt_bauhaus_slider_set(g->scale_b, p->blue);
  dt_bauhaus_slider_set(g->scale_g2, p->g2);

  dt_bauhaus_combobox_set(g->presets, -1);
  dt_bauhaus_slider_set(g->finetune, 0);

  gboolean show_finetune = FALSE;

  gboolean found = FALSE;

  // is this a "as shot" white balance?
  if(p->red == g->as_shot_wb[0] && p->green == g->as_shot_wb[1] && p->blue == g->as_shot_wb[2])
  {
    dt_bauhaus_combobox_set(g->presets, DT_IOP_TEMP_AS_SHOT);
    found = TRUE;
  }
  else
  {
    // is this a "D65 white balance"?
    if((p->red == (float)g->daylight_wb[0]) &&
       (p->green == (float)g->daylight_wb[1]) &&
       (p->blue == (float)g->daylight_wb[2]))
    {
      dt_bauhaus_combobox_set(g->presets, DT_IOP_TEMP_D65);
      found = TRUE;
    }
  }

  if(!found)
  {
    // look through all added presets
    for(int j = DT_IOP_NUM_OF_STD_TEMP_PRESETS; !found && (j < g->preset_cnt); j++)
    {
      // look through all variants of this preset, with different tuning
      for(int i = g->preset_num[j];
          !found && (i < wb_preset_count) &&
          !strcmp(wb_preset[i].make, self->dev->image_storage.camera_maker) &&
          !strcmp(wb_preset[i].model, self->dev->image_storage.camera_model) &&
          !strcmp(wb_preset[i].name, wb_preset[g->preset_num[j]].name);
          i++)
      {
        if(p->red == (float)wb_preset[i].channel[0] &&
           p->green == (float)wb_preset[i].channel[1] &&
           p->blue == (float)wb_preset[i].channel[2])
        {
          // got exact match!
          dt_bauhaus_combobox_set(g->presets, j);
          dt_iop_temperature_preset_data_t *preset = dt_bauhaus_combobox_get_data(g->presets);
          if(preset != NULL)
          {
            show_finetune = preset->min_ft_pos != preset->max_ft_pos;
            if(show_finetune)
            {
              dt_bauhaus_slider_set_hard_min(g->finetune, wb_preset[preset->min_ft_pos].tuning);
              dt_bauhaus_slider_set_hard_max(g->finetune, wb_preset[preset->max_ft_pos].tuning);
              dt_bauhaus_slider_set_default(g->finetune, wb_preset[preset->no_ft_pos].tuning);
            }
          }

          dt_bauhaus_slider_set(g->finetune, wb_preset[i].tuning);
          found = TRUE;
          break;
        }
      }
    }

    if(!found)
    {
      // ok, we haven't found exact match, maybe this was interpolated?

      // look through all added presets
      for(int j = DT_IOP_NUM_OF_STD_TEMP_PRESETS; !found && (j < g->preset_cnt); j++)
      {
        // look through all variants of this preset, with different tuning
        int i = g->preset_num[j] + 1;
        while(!found && (i < wb_preset_count) && !strcmp(wb_preset[i].make, self->dev->image_storage.camera_maker)
              && !strcmp(wb_preset[i].model, self->dev->image_storage.camera_maker)
              && !strcmp(wb_preset[i].name, wb_preset[g->preset_num[j]].name))
        {
          // let's find gaps
          if(wb_preset[i - 1].tuning + 1 == wb_preset[i].tuning)
          {
            i++;
            continue;
          }

          // we have a gap!

          // we do not know what finetuning value was set, we need to bruteforce to find it
          for(int tune = wb_preset[i - 1].tuning + 1; !found && (tune < wb_preset[i].tuning); tune++)
          {
            wb_data interpolated = {.tuning = tune };
            dt_wb_preset_interpolate(&wb_preset[i - 1], &wb_preset[i], &interpolated);

            if(p->red == (float)interpolated.channel[0] &&
               p->green == (float)interpolated.channel[1] &&
               p->blue == (float)interpolated.channel[2])
            {
              // got exact match!

              dt_bauhaus_combobox_set(g->presets, j);
              dt_iop_temperature_preset_data_t *preset = dt_bauhaus_combobox_get_data(g->presets);
              if(preset != NULL)
              {
                show_finetune = preset->min_ft_pos != preset->max_ft_pos;
                if(show_finetune)
                {
                  dt_bauhaus_slider_set_hard_min(g->finetune, wb_preset[preset->min_ft_pos].tuning);
                  dt_bauhaus_slider_set_hard_max(g->finetune, wb_preset[preset->max_ft_pos].tuning);
                  dt_bauhaus_slider_set_default(g->finetune, wb_preset[preset->no_ft_pos].tuning);
                }
              }
              dt_bauhaus_slider_set(g->finetune, tune);
              found = TRUE;
              break;
            }
          }
          i++;
        }
      }
    }
    if (!found) //since we haven't got a match - it's user-set
    {
      dt_bauhaus_combobox_set(g->presets, DT_IOP_TEMP_USER);
    }
  }

  if (!found || isnan(g->mod_temp)) // reset or initialize user-defined
  {
    g->mod_temp = tempK;
    g->mod_tint = tint;
    _temp_array_from_params(g->mod_coeff, p);
  }

  const gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g->coeffs_toggle));
  dtgtk_togglebutton_set_paint(DTGTK_TOGGLEBUTTON(g->coeffs_toggle), dtgtk_cairo_paint_solid_arrow,
                               CPF_STYLE_BOX | (active?CPF_DIRECTION_DOWN:CPF_DIRECTION_LEFT), NULL);
  gtk_widget_set_visible(GTK_WIDGET(g->finetune), show_finetune);
  gtk_widget_set_visible(g->buttonbar, g->button_bar_visible);
  dtgtk_expander_set_expanded(DTGTK_EXPANDER(g->coeffs_expander), active);

  const int preset = dt_bauhaus_combobox_get(g->presets);

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g->btn_asshot), preset == DT_IOP_TEMP_AS_SHOT);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g->btn_user), preset == DT_IOP_TEMP_USER);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g->btn_d65), preset == DT_IOP_TEMP_D65);

  color_temptint_sliders(self);
  color_rgb_sliders(self);
  color_finetuning_slider(self);

  _display_wb_error(self);

  gtk_widget_queue_draw(self->widget);
}

static int calculate_bogus_daylight_wb(dt_iop_module_t *module, double bwb[4])
{
  if(!dt_image_is_matrix_correction_supported(&module->dev->image_storage))
  {
    bwb[0] = 1.0;
    bwb[2] = 1.0;
    bwb[1] = 1.0;
    bwb[3] = 1.0;

    return 0;
  }

  double mul[4];
  if(dt_colorspaces_conversion_matrices_rgb(module->dev->image_storage.camera_makermodel, NULL, NULL, module->dev->image_storage.d65_color_matrix, mul))
  {
    // normalize green:
    bwb[0] = mul[0] / mul[1];
    bwb[2] = mul[2] / mul[1];
    bwb[1] = 1.0;
    bwb[3] = mul[3] / mul[1];

    return 0;
  }

  return 1;
}

static void prepare_matrices(dt_iop_module_t *module)
{
  dt_iop_temperature_gui_data_t *g = (dt_iop_temperature_gui_data_t *)module->gui_data;

  // sRGB D65
  const double RGB_to_XYZ[3][4] = { { 0.4124564, 0.3575761, 0.1804375, 0 },
                                    { 0.2126729, 0.7151522, 0.0721750, 0 },
                                    { 0.0193339, 0.1191920, 0.9503041, 0 } };

  // sRGB D65
  const double XYZ_to_RGB[4][3] = { { 3.2404542, -1.5371385, -0.4985314 },
                                    { -0.9692660, 1.8760108, 0.0415560 },
                                    { 0.0556434, -0.2040259, 1.0572252 },
                                    { 0, 0, 0 } };

  if(!dt_image_is_raw(&module->dev->image_storage))
  {
    // let's just assume for now(TM) that if it is not raw, it is sRGB
    memcpy(g->XYZ_to_CAM, XYZ_to_RGB, sizeof(g->XYZ_to_CAM));
    memcpy(g->CAM_to_XYZ, RGB_to_XYZ, sizeof(g->CAM_to_XYZ));
    return;
  }

  char *camera = module->dev->image_storage.camera_makermodel;
  if (!dt_colorspaces_conversion_matrices_xyz(camera, module->dev->image_storage.d65_color_matrix,
                                                      g->XYZ_to_CAM, g->CAM_to_XYZ))
  {
    fprintf(stderr, "[temperature] `%s' color matrix not found for image\n", camera);
    dt_control_log(_("`%s' color matrix not found for image"), camera);
  }
}

static void find_coeffs(dt_iop_module_t *module, double coeffs[4])
{
  const dt_image_t *img = &module->dev->image_storage;

  // the raw should provide wb coeffs:
  int ok = 1;
  // Only check the first three values, the fourth is usually NAN for RGB
  const int num_coeffs = (img->flags & DT_IMAGE_4BAYER) ? 4 : 3;
  for(int k = 0; ok && k < num_coeffs; k++)
  {
    if(!isnormal(img->wb_coeffs[k]) || img->wb_coeffs[k] == 0.0f) ok = 0;
  }
  if(ok)
  {
    for(int k = 0; k < 4; k++) coeffs[k] = img->wb_coeffs[k];
    return;
  }

  if(!ignore_missing_wb(&(module->dev->image_storage)))
  {
    //  only display this if we have a sample, otherwise it is better to keep
    //  on screen the more important message about missing sample and the way
    //  to contribute.
    if(!img->camera_missing_sample)
      dt_control_log(_("failed to read camera white balance information from `%s'!"),
                     img->filename);
    fprintf(stderr, "[temperature] failed to read camera white balance information from `%s'!\n",
            img->filename);
  }

  double bwb[4];
  if(!calculate_bogus_daylight_wb(module, bwb))
  {
    // found camera matrix and used it to calculate bogus daylight wb
    for(int c = 0; c < 4; c++) coeffs[c] = bwb[c];
    return;
  }

  // no cam matrix??? try presets:
  for(int i = 0; i < wb_preset_count; i++)
  {
    if(!strcmp(wb_preset[i].make, img->camera_maker)
       && !strcmp(wb_preset[i].model, img->camera_model))
    {
      // just take the first preset we find for this camera
      for(int k = 0; k < 3; k++) coeffs[k] = wb_preset[i].channel[k];
      return;
    }
  }

  // did not find preset either?
  // final security net: hardcoded default that fits most cams.
  coeffs[0] = 2.0;
  coeffs[1] = 1.0;
  coeffs[2] = 1.5;
  coeffs[3] = 1.0;
}

void reload_defaults(dt_iop_module_t *module)
{
  dt_iop_temperature_params_t *d = module->default_params;
  *d = (dt_iop_temperature_params_t){ 1.0, 1.0, 1.0, 1.0 };

  // we might be called from presets update infrastructure => there is no image
  if(!module->dev || module->dev->image_storage.id == -1) return;

  const gboolean is_raw = dt_image_is_matrix_correction_supported(&module->dev->image_storage);
  const gboolean is_modern =
    dt_conf_is_equal("plugins/darkroom/chromatic-adaptation", "modern");

  module->default_enabled = 0;
  module->hide_enable_button = 0;

  // White balance module doesn't need to be enabled for monochrome raws (like
  // for leica monochrom cameras). prepare_matrices is a noop as well, as there
  // isn't a color matrix, so we can skip that as well.
  if(dt_image_is_monochrome(&(module->dev->image_storage)))
  {
    module->hide_enable_button = 1;
  }
  else
  {
    if(module->gui_data) prepare_matrices(module);

    /* check if file is raw / hdr */
    if(is_raw)
    {
      // raw images need wb:
      module->default_enabled = 1;

      // if workflow = modern, only set WB coeffs equivalent to D65 illuminant
      // full chromatic adaptation is deferred to channelmixerrgb
      double coeffs[4] = { 0 };
      if(is_modern && !calculate_bogus_daylight_wb(module, coeffs))
      {
        d->red = coeffs[0]/coeffs[1];
        d->blue = coeffs[2]/coeffs[1];
        d->g2 = coeffs[3]/coeffs[1];
        d->green = 1.0f;
      }
      else
      {
        // do best to find starting coeffs
        find_coeffs(module, coeffs);
        d->red = coeffs[0]/coeffs[1];
        d->blue = coeffs[2]/coeffs[1];
        d->g2 = coeffs[3]/coeffs[1];
        d->green = 1.0f;
      }
    }
  }

  // remember daylight wb used for temperature/tint conversion,
  // assuming it corresponds to CIE daylight (D65)
  dt_iop_temperature_gui_data_t *g = (dt_iop_temperature_gui_data_t *)module->gui_data;
  if(g)
  {
    gtk_stack_set_visible_child_name(GTK_STACK(module->widget), module->hide_enable_button ? "disabled" : "enabled");

    dt_bauhaus_slider_set_default(g->scale_r, d->red);
    dt_bauhaus_slider_set_default(g->scale_g, d->green);
    dt_bauhaus_slider_set_default(g->scale_b, d->blue);
    dt_bauhaus_slider_set_default(g->scale_g2, d->g2);

    // to have at least something and definitely not crash
    _temp_array_from_params(g->daylight_wb, d);

    if(!calculate_bogus_daylight_wb(module, g->daylight_wb))
    {
      // found camera matrix and used it to calculate bogus daylight wb
    }
    else
    {
      // if we didn't find anything for daylight wb, look for a wb preset with appropriate name.
      // we're normalizing that to be D65
      for(int i = 0; i < wb_preset_count; i++)
      {
        if(!strcmp(wb_preset[i].make, module->dev->image_storage.camera_maker)
           && !strcmp(wb_preset[i].model, module->dev->image_storage.camera_model)
           && (!strcmp(wb_preset[i].name, Daylight) || !strcmp(wb_preset[i].name, DirectSunlight))
           && wb_preset[i].tuning == 0)
        {

          for(int k = 0; k < 4; k++) g->daylight_wb[k] = wb_preset[i].channel[k];
          break;
        }
      }

    }

    // Store EXIF WB coeffs
    if(is_raw)
      find_coeffs(module, g->as_shot_wb);
    else
      g->as_shot_wb[0] = g->as_shot_wb[1] = g->as_shot_wb[2] = g->as_shot_wb[3] = 1.f;

    g->as_shot_wb[0] /= g->as_shot_wb[1];
    g->as_shot_wb[2] /= g->as_shot_wb[1];
    g->as_shot_wb[3] /= g->as_shot_wb[1];
    g->as_shot_wb[1] = 1.0;

    float TempK, tint;
    mul2temp(module, d, &TempK, &tint);

    dt_bauhaus_slider_set_default(g->scale_k, TempK);
    dt_bauhaus_slider_set_default(g->scale_tint, tint);

    dt_bauhaus_combobox_clear(g->presets);

    dt_bauhaus_combobox_add(g->presets, C_("white balance", "as shot")); // old "camera". reason for change: all other RAW development tools use "As Shot" or "shot"
    dt_bauhaus_combobox_add(g->presets, C_("white balance", "from image area")); // old "spot", reason: describes exactly what'll happen
    dt_bauhaus_combobox_add(g->presets, C_("white balance", "user modified"));
    dt_bauhaus_combobox_add(g->presets, C_("white balance", "camera reference")); // old "camera neutral", reason: better matches intent

    g->preset_cnt = DT_IOP_NUM_OF_STD_TEMP_PRESETS;
    memset(g->preset_num, 0, sizeof(g->preset_num));

    generate_preset_combo(module);

    gui_sliders_update(module);
  }
}

void init_global(dt_iop_module_so_t *module)
{
  const int program = 2; // basic.cl, from programs.conf
  dt_iop_temperature_global_data_t *gd
      = (dt_iop_temperature_global_data_t *)malloc(sizeof(dt_iop_temperature_global_data_t));
  module->data = gd;
  gd->kernel_whitebalance_4f = dt_opencl_create_kernel(program, "whitebalance_4f");
  gd->kernel_whitebalance_1f = dt_opencl_create_kernel(program, "whitebalance_1f");
  gd->kernel_whitebalance_1f_xtrans = dt_opencl_create_kernel(program, "whitebalance_1f_xtrans");
}

void cleanup_global(dt_iop_module_so_t *module)
{
  dt_iop_temperature_global_data_t *gd = (dt_iop_temperature_global_data_t *)module->data;
  dt_opencl_free_kernel(gd->kernel_whitebalance_4f);
  dt_opencl_free_kernel(gd->kernel_whitebalance_1f);
  dt_opencl_free_kernel(gd->kernel_whitebalance_1f_xtrans);
  free(module->data);
  module->data = NULL;
}

static void temp_tint_callback(GtkWidget *slider, dt_iop_module_t *self)
{
  if(darktable.gui->reset) return;

  dt_iop_temperature_gui_data_t *g = (dt_iop_temperature_gui_data_t *)self->gui_data;

  dt_iop_color_picker_reset(self, TRUE);

  g->mod_temp = dt_bauhaus_slider_get(g->scale_k);
  g->mod_tint = dt_bauhaus_slider_get(g->scale_tint);

  temp2mul(self, g->mod_temp, g->mod_tint, g->mod_coeff);

  // normalize
  g->mod_coeff[0] /= g->mod_coeff[1];
  g->mod_coeff[2] /= g->mod_coeff[1];
  g->mod_coeff[3] /= g->mod_coeff[1];
  g->mod_coeff[1] = 1.0;

  dt_bauhaus_combobox_set(g->presets, DT_IOP_TEMP_USER);
}

void gui_changed(dt_iop_module_t *self, GtkWidget *w, void *previous)
{
  dt_iop_temperature_gui_data_t *g = (dt_iop_temperature_gui_data_t *)self->gui_data;
  dt_iop_temperature_params_t *p = (dt_iop_temperature_params_t *)self->params;

  _temp_array_from_params(g->mod_coeff, p);

  mul2temp(self, p, &g->mod_temp, &g->mod_tint);

  dt_bauhaus_combobox_set(g->presets, DT_IOP_TEMP_USER);

  _display_wb_error(self);
}

static gboolean btn_toggled(GtkWidget *togglebutton, GdkEventButton *event, dt_iop_module_t *self)
{
  if(darktable.gui->reset) return TRUE;

  dt_iop_temperature_gui_data_t *g = (dt_iop_temperature_gui_data_t*)self->gui_data;

  int preset = togglebutton == g->btn_asshot ? DT_IOP_TEMP_AS_SHOT :
               togglebutton == g->btn_d65 ? DT_IOP_TEMP_D65 :
               togglebutton == g->btn_user ? DT_IOP_TEMP_USER : 0;

  if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(togglebutton)))
  {
    if(dt_bauhaus_combobox_get(g->presets) != preset)
      dt_bauhaus_combobox_set(g->presets, preset);
  }
  else if(dt_bauhaus_combobox_get(g->presets) == preset)
  {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(togglebutton), TRUE);
  }

  return TRUE;
}

static void preset_tune_callback(GtkWidget *widget, dt_iop_module_t *self)
{
  if(darktable.gui->reset) return;

  dt_iop_temperature_gui_data_t *g = (dt_iop_temperature_gui_data_t *)self->gui_data;
  dt_iop_temperature_params_t *p = (dt_iop_temperature_params_t *)self->params;

  const int pos = dt_bauhaus_combobox_get(g->presets);
  const int tune = dt_bauhaus_slider_get(g->finetune);

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g->btn_asshot), pos == DT_IOP_TEMP_AS_SHOT);
  if(pos != DT_IOP_TEMP_SPOT) dt_iop_color_picker_reset(self, TRUE);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g->btn_user), pos == DT_IOP_TEMP_USER);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g->btn_d65), pos == DT_IOP_TEMP_D65);

  gboolean show_finetune = FALSE;

  switch(pos)
  {
    case -1: // just un-setting.
      return;
    case DT_IOP_TEMP_AS_SHOT: // as shot wb
      _temp_params_from_array(p, g->as_shot_wb);
      break;
    case DT_IOP_TEMP_SPOT: // from image area wb, expose callback will set p->rgbg2.
      if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g->colorpicker)))
      {
        gboolean ret_val;
        g_signal_emit_by_name(G_OBJECT(g->colorpicker), "button-press-event", NULL, &ret_val);
      }
      break;
    case DT_IOP_TEMP_USER: // directly changing one of the coeff sliders also changes the mod_coeff so it can be read here
      _temp_params_from_array(p, g->mod_coeff);
      break;
    case DT_IOP_TEMP_D65: // camera reference d65
      _temp_params_from_array(p, g->daylight_wb);
      break;
    default: // camera WB presets
    {
      gboolean found = FALSE;
      dt_iop_temperature_preset_data_t *preset = dt_bauhaus_combobox_get_data(g->presets);
      // look through all variants of this preset, with different tuning
      for(int i = preset->min_ft_pos;
          (i < (preset->max_ft_pos + 1)) // we can limit search spread thanks to knowing where to look!
          && !strcmp(wb_preset[i].make, self->dev->image_storage.camera_maker)
          && !strcmp(wb_preset[i].model, self->dev->image_storage.camera_model)
          && !strcmp(wb_preset[i].name, wb_preset[preset->no_ft_pos].name);
          i++)
      {
        if(wb_preset[i].tuning == tune)
        {
          // got exact match!
          _temp_params_from_array(p, wb_preset[i].channel);
          found = TRUE;
          break;
        }
      }

      if(!found)
      {
        // ok, we haven't found exact match, need to interpolate

        // let's find 2 most closest tunings with needed_tuning in-between
        int min_id = INT_MIN, max_id = INT_MIN;

        // look through all variants of this preset, with different tuning, starting from second entry (if
        // any)
        int i = preset->min_ft_pos + 1;
        while((i < preset->max_ft_pos+1) && !strcmp(wb_preset[i].make, self->dev->image_storage.camera_maker)
              && !strcmp(wb_preset[i].model, self->dev->image_storage.camera_model)
              && !strcmp(wb_preset[i].name, wb_preset[preset->no_ft_pos].name))
        {
          if(wb_preset[i - 1].tuning < tune && wb_preset[i].tuning > tune)
          {
            min_id = i - 1;
            max_id = i;
            break;
          }

          i++;
        }

        // have we found enough good data?
        if(min_id == INT_MIN || max_id == INT_MIN || min_id == max_id) break; // hysteresis

        found = TRUE;
        wb_data interpolated = {.tuning = tune };
        dt_wb_preset_interpolate(&wb_preset[min_id], &wb_preset[max_id], &interpolated);
        _temp_params_from_array(p, interpolated.channel);
      }

      show_finetune = preset->min_ft_pos != preset->max_ft_pos;
      if(show_finetune)
      {
        ++darktable.gui->reset;
        dt_bauhaus_slider_set_hard_min(g->finetune, wb_preset[preset->min_ft_pos].tuning);
        dt_bauhaus_slider_set_hard_max(g->finetune, wb_preset[preset->max_ft_pos].tuning);
        dt_bauhaus_slider_set_default(g->finetune, wb_preset[preset->no_ft_pos].tuning);
        --darktable.gui->reset;
      }
    }
    break;
  }

  gtk_widget_set_visible(GTK_WIDGET(g->finetune), show_finetune);

  if(self->off) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(self->off), 1);

  float TempK, tint;

  if(pos == DT_IOP_TEMP_USER)
  {
    TempK = g->mod_temp;
    tint = g->mod_tint;
  }
  else
  {
    mul2temp(self, p, &TempK, &tint);
  }

  ++darktable.gui->reset;
  dt_bauhaus_slider_set(g->scale_k, TempK);
  dt_bauhaus_slider_set(g->scale_tint, tint);
  dt_bauhaus_slider_set(g->scale_r, p->red);
  dt_bauhaus_slider_set(g->scale_g, p->green);
  dt_bauhaus_slider_set(g->scale_b, p->blue);
  dt_bauhaus_slider_set(g->scale_g2, p->g2);
  --darktable.gui->reset;

  color_temptint_sliders(self);
  color_rgb_sliders(self);
  color_finetuning_slider(self);

  dt_dev_add_history_item(darktable.develop, self, TRUE);
}

void color_picker_apply(dt_iop_module_t *self, GtkWidget *picker, dt_dev_pixelpipe_iop_t *piece)
{
  if(darktable.gui->reset) return;

  dt_iop_temperature_gui_data_t *g = (dt_iop_temperature_gui_data_t *)self->gui_data;
  dt_iop_temperature_params_t *p = (dt_iop_temperature_params_t *)self->params;

  // capture gui color picked event.
  if(self->picked_color_max[0] < self->picked_color_min[0]) return;
  const float *grayrgb = self->picked_color;

  // normalize green:
  p->green = grayrgb[1] > 0.001f ? 1.0f / grayrgb[1] : 1.0f;
  p->red  = fmaxf(0.0f, fminf(8.0f, (grayrgb[0] > 0.001f ? 1.0f / grayrgb[0] : 1.0f) / p->green));
  p->blue = fmaxf(0.0f, fminf(8.0f, (grayrgb[2] > 0.001f ? 1.0f / grayrgb[2] : 1.0f) / p->green));
  p->g2   = fmaxf(0.0f, fminf(8.0f, (grayrgb[3] > 0.001f ? 1.0f / grayrgb[3] : 1.0f) / p->green));
  p->green = 1.0;

  dt_bauhaus_combobox_set(g->presets, DT_IOP_TEMP_SPOT);
}

static void _coeffs_button_changed(GtkDarktableToggleButton *widget, gpointer user_data)
{
  dt_iop_module_t *self = (dt_iop_module_t *)user_data;
  dt_iop_temperature_gui_data_t *g = (dt_iop_temperature_gui_data_t *)self->gui_data;
  const gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g->coeffs_toggle));
  dtgtk_expander_set_expanded(DTGTK_EXPANDER(g->coeffs_expander), active);
  dtgtk_togglebutton_set_paint(DTGTK_TOGGLEBUTTON(g->coeffs_toggle), dtgtk_cairo_paint_solid_arrow,
                               CPF_STYLE_BOX | (active?CPF_DIRECTION_DOWN:CPF_DIRECTION_LEFT), NULL);
  g->expand_coeffs = active;
  dt_conf_set_bool("plugins/darkroom/temperature/expand_coefficients", active);
}

static void _coeffs_expander_click(GtkWidget *widget, GdkEventButton *e, gpointer user_data)
{
  if(e->type == GDK_2BUTTON_PRESS || e->type == GDK_3BUTTON_PRESS) return;

  dt_iop_module_t *self = (dt_iop_module_t *)user_data;
  dt_iop_temperature_gui_data_t *g = (dt_iop_temperature_gui_data_t *)self->gui_data;

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g->coeffs_toggle), !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g->coeffs_toggle)));
}

static void gui_sliders_update(struct dt_iop_module_t *self)
{
  const dt_image_t *img = &self->dev->image_storage;
  dt_iop_temperature_gui_data_t *g = (dt_iop_temperature_gui_data_t *)self->gui_data;

  if(FILTERS_ARE_CYGM(img->buf_dsc.filters))
  {
    dt_bauhaus_widget_set_label(g->scale_r, NULL, N_("green"));
    gtk_widget_set_tooltip_text(g->scale_r, _("green channel coefficient"));
    dt_bauhaus_widget_set_label(g->scale_g, NULL, N_("magenta"));
    gtk_widget_set_tooltip_text(g->scale_g, _("magenta channel coefficient"));
    dt_bauhaus_widget_set_label(g->scale_b, NULL, N_("cyan"));
    gtk_widget_set_tooltip_text(g->scale_b, _("cyan channel coefficient"));
    dt_bauhaus_widget_set_label(g->scale_g2, NULL, N_("yellow"));
    gtk_widget_set_tooltip_text(g->scale_g2, _("yellow channel coefficient"));

    gtk_box_reorder_child(GTK_BOX(g->coeff_widgets), g->scale_b, 0);
    gtk_box_reorder_child(GTK_BOX(g->coeff_widgets), g->scale_g2, 1);
    gtk_box_reorder_child(GTK_BOX(g->coeff_widgets), g->scale_g, 2);
    gtk_box_reorder_child(GTK_BOX(g->coeff_widgets), g->scale_r, 3);
  }
  else
  {
    dt_bauhaus_widget_set_label(g->scale_r, NULL, N_("red"));
    gtk_widget_set_tooltip_text(g->scale_r, _("red channel coefficient"));
    dt_bauhaus_widget_set_label(g->scale_g, NULL, N_("green"));
    gtk_widget_set_tooltip_text(g->scale_g, _("green channel coefficient"));
    dt_bauhaus_widget_set_label(g->scale_b, NULL, N_("blue"));
    gtk_widget_set_tooltip_text(g->scale_b, _("blue channel coefficient"));
    dt_bauhaus_widget_set_label(g->scale_g2, NULL, N_("emerald"));
    gtk_widget_set_tooltip_text(g->scale_g2, _("emerald channel coefficient"));

    gtk_box_reorder_child(GTK_BOX(g->coeff_widgets), g->scale_r, 0);
    gtk_box_reorder_child(GTK_BOX(g->coeff_widgets), g->scale_g, 1);
    gtk_box_reorder_child(GTK_BOX(g->coeff_widgets), g->scale_b, 2);
    gtk_box_reorder_child(GTK_BOX(g->coeff_widgets), g->scale_g2, 3);
  }

  gtk_widget_set_visible(GTK_WIDGET(g->scale_g2), (img->flags & DT_IMAGE_4BAYER));
}

static void temp_label_click(GtkWidget *label, GdkEventButton *event, gpointer user_data)
{
  dt_iop_module_t *self = (dt_iop_module_t *)user_data;
  dt_iop_temperature_gui_data_t *g = (dt_iop_temperature_gui_data_t *)self->gui_data;

  gchar *old_config = dt_conf_get_string("plugins/darkroom/temperature/colored_sliders");

  if(!g_strcmp0(old_config, "no color"))
  {
    dt_conf_set_string("plugins/darkroom/temperature/colored_sliders", "illuminant color");
    g->colored_sliders = TRUE;
    g->blackbody_is_confusing = FALSE;
  }
  else if (!g_strcmp0(old_config, "illuminant color"))
  {
    dt_conf_set_string("plugins/darkroom/temperature/colored_sliders", "effect emulation");
    g->colored_sliders = TRUE;
    g->blackbody_is_confusing = TRUE;
  }
  else
  {
    dt_conf_set_string("plugins/darkroom/temperature/colored_sliders", "no color");
    g->colored_sliders = FALSE;
    g->blackbody_is_confusing = FALSE;
  }

  g_free(old_config);

  color_temptint_sliders(self);
  color_rgb_sliders(self);
  color_finetuning_slider(self);
}

static void _preference_changed(gpointer instance, gpointer user_data)
{
  dt_iop_module_t *self = (dt_iop_module_t *)user_data;
  dt_iop_temperature_gui_data_t *g = (dt_iop_temperature_gui_data_t *)self->gui_data;

  const char *config = dt_conf_get_string_const("plugins/darkroom/temperature/colored_sliders");
  g->colored_sliders = g_strcmp0(config, "no color"); // true if config != "no color"
  g->blackbody_is_confusing = g->colored_sliders && g_strcmp0(config, "illuminant color"); // true if config != "illuminant color"

  g->button_bar_visible = dt_conf_get_bool("plugins/darkroom/temperature/button_bar");
  gtk_widget_set_visible(g->buttonbar, g->button_bar_visible);

  color_temptint_sliders(self);
  color_rgb_sliders(self);
  color_finetuning_slider(self);
}

static void _develop_ui_pipe_finished_callback(gpointer instance, gpointer user_data)
{
  dt_iop_module_t *self = (dt_iop_module_t *)user_data;
  _display_wb_error(self);
}


void gui_init(struct dt_iop_module_t *self)
{
  dt_iop_temperature_gui_data_t *g = IOP_GUI_ALLOC(temperature);

  DT_DEBUG_CONTROL_SIGNAL_CONNECT(darktable.signals, DT_SIGNAL_DEVELOP_UI_PIPE_FINISHED,
                            G_CALLBACK(_develop_ui_pipe_finished_callback), self);

  const char *config = dt_conf_get_string_const("plugins/darkroom/temperature/colored_sliders");
  g->colored_sliders = g_strcmp0(config, "no color"); // true if config != "no color"
  g->blackbody_is_confusing = g->colored_sliders && g_strcmp0(config, "illuminant color"); // true if config != "illuminant color"
  g->expand_coeffs = dt_conf_get_bool("plugins/darkroom/temperature/expand_coefficients");

  const int feedback = g->colored_sliders ? 0 : 1;
  g->button_bar_visible = dt_conf_get_bool("plugins/darkroom/temperature/button_bar");

  GtkBox *box_enabled = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, DT_BAUHAUS_SPACE));

  g->mod_temp = NAN;
  for(int k = 0; k < 4; k++)
  {
    g->daylight_wb[k] = 1.0;
    g->as_shot_wb[k] = 1.f;
  }

  GtkWidget *temp_label_box = gtk_event_box_new();
  g->temp_label = dt_ui_section_label_new(_("scene illuminant temp"));
  gtk_widget_set_tooltip_text(g->temp_label, _("click to cycle color mode on sliders"));
  GtkStyleContext *context = gtk_widget_get_style_context(GTK_WIDGET(g->temp_label));
  gtk_style_context_add_class(context, "section_label_top");
  gtk_container_add(GTK_CONTAINER(temp_label_box), g->temp_label);

  g_signal_connect(G_OBJECT(temp_label_box), "button-release-event", G_CALLBACK(temp_label_click), self);

  gtk_box_pack_start(box_enabled, temp_label_box, TRUE, TRUE, 0);

  //Match UI order: temp first, then tint (like every other app ever)
  g->scale_k = dt_bauhaus_slider_new_with_range_and_feedback(self, DT_IOP_LOWEST_TEMPERATURE, DT_IOP_HIGHEST_TEMPERATURE,
                                                             10., 5000.0, 0, feedback);
  dt_bauhaus_slider_set_format(g->scale_k, "%.0f K");
  dt_bauhaus_widget_set_label(g->scale_k, NULL, N_("temperature"));
  gtk_widget_set_tooltip_text(g->scale_k, _("color temperature (in Kelvin)"));
  gtk_box_pack_start(box_enabled, g->scale_k, TRUE, TRUE, 0);

  g->scale_tint = dt_bauhaus_slider_new_with_range_and_feedback(self, DT_IOP_LOWEST_TINT, DT_IOP_HIGHEST_TINT,
                                                                .01, 1.0, 3, feedback);
  dt_bauhaus_widget_set_label(g->scale_tint, NULL, N_("tint"));
  gtk_widget_set_tooltip_text(g->scale_tint, _("color tint of the image, from magenta (value < 1) to green (value > 1)"));
  gtk_box_pack_start(box_enabled, g->scale_tint, TRUE, TRUE, 0);

  // collapsible section for coeffs that are generally not to be used
  GtkWidget *destdisp_head = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, DT_BAUHAUS_SPACE);
  GtkWidget *header_evb = gtk_event_box_new();
  GtkWidget *destdisp = dt_ui_section_label_new(_("channel coefficients"));
  context = gtk_widget_get_style_context(destdisp_head);
  gtk_style_context_add_class(context, "section-expander");
  gtk_container_add(GTK_CONTAINER(header_evb), destdisp);

  g->coeffs_toggle = dtgtk_togglebutton_new(dtgtk_cairo_paint_solid_arrow, CPF_STYLE_BOX | CPF_DIRECTION_LEFT, NULL);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g->coeffs_toggle), g->expand_coeffs);
  gtk_widget_set_name(GTK_WIDGET(g->coeffs_toggle), "control-button");

  g->coeff_widgets = self->widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, DT_BAUHAUS_SPACE);
  gtk_box_pack_start(GTK_BOX(destdisp_head), header_evb, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(destdisp_head), g->coeffs_toggle, FALSE, FALSE, 0);

  g->coeffs_expander = dtgtk_expander_new(destdisp_head, g->coeff_widgets);
  dtgtk_expander_set_expanded(DTGTK_EXPANDER(g->coeffs_expander), TRUE);
  gtk_box_pack_end(box_enabled, g->coeffs_expander, FALSE, FALSE, 0);

  g_signal_connect(G_OBJECT(g->coeffs_toggle), "toggled", G_CALLBACK(_coeffs_button_changed),  (gpointer)self);

  g_signal_connect(G_OBJECT(header_evb), "button-release-event", G_CALLBACK(_coeffs_expander_click),
                   (gpointer)self);

  g->scale_r = dt_bauhaus_slider_from_params(self, N_("red"));
  g->scale_g = dt_bauhaus_slider_from_params(self, N_("green"));
  g->scale_b = dt_bauhaus_slider_from_params(self, N_("blue"));
  g->scale_g2 = dt_bauhaus_slider_from_params(self, "g2");
  dt_bauhaus_slider_set_digits(g->scale_r, 3);
  dt_bauhaus_slider_set_digits(g->scale_g, 3);
  dt_bauhaus_slider_set_digits(g->scale_b, 3);
  dt_bauhaus_slider_set_digits(g->scale_g2, 3);
  dt_bauhaus_slider_set_step(g->scale_r, 0.05);
  dt_bauhaus_slider_set_step(g->scale_g, 0.05);
  dt_bauhaus_slider_set_step(g->scale_b, 0.05);
  dt_bauhaus_slider_set_step(g->scale_g2, 0.05);

  gtk_widget_set_no_show_all(g->scale_g2, TRUE);

  g->balance_label = dt_ui_section_label_new(_("white balance settings"));
  gtk_box_pack_start(box_enabled, g->balance_label, TRUE, TRUE, 0);

  g->btn_asshot = dt_iop_togglebutton_new(self, N_("settings"), N_("as shot"), NULL,
                                          G_CALLBACK(btn_toggled), FALSE, 0, 0,
                                          dtgtk_cairo_paint_camera, NULL);
  gtk_widget_set_tooltip_text(g->btn_asshot, _("set white balance to as shot"));

  // create color picker to be able to send its signal when spot selected,
  // this module may expect data in RAW or RGB, setting the color picker CST to iop_cs_NONE will make the color
  // picker to depend on the number of color channels of the pixels. It is done like this as we may not know the
  // actual kind of data we are using in the GUI (it is part of the pipeline).
  g->colorpicker = dt_color_picker_new_with_cst(self, DT_COLOR_PICKER_AREA, NULL, iop_cs_NONE);
  dt_action_define_iop(self, N_("settings"), N_("from image area"), g->colorpicker, &dt_action_def_toggle);
  dtgtk_togglebutton_set_paint(DTGTK_TOGGLEBUTTON(g->colorpicker), dtgtk_cairo_paint_colorpicker, CPF_STYLE_FLAT, NULL);
  gtk_widget_set_tooltip_text(g->colorpicker, _("set white balance to detected from area"));

  g->btn_user = dt_iop_togglebutton_new(self, N_("settings"), N_("user modified"), NULL,
                                        G_CALLBACK(btn_toggled), FALSE, 0, 0,
                                        dtgtk_cairo_paint_masks_drawn, NULL);
  gtk_widget_set_tooltip_text(g->btn_user, _("set white balance to user modified"));


  g->btn_d65 = dt_iop_togglebutton_new(self, N_("settings"), N_("camera reference"), NULL,
                                       G_CALLBACK(btn_toggled), FALSE, 0, 0,
                                       dtgtk_cairo_paint_bulb, NULL);
  gtk_widget_set_tooltip_text(g->btn_d65, _("set white balance to camera reference point\nin most cases it should be D65"));

  g->buttonbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_end(GTK_BOX(g->buttonbar), g->btn_d65, TRUE, TRUE, 0);
  gtk_box_pack_end(GTK_BOX(g->buttonbar), g->btn_user, TRUE, TRUE, 0);
  gtk_box_pack_end(GTK_BOX(g->buttonbar), g->colorpicker, TRUE, TRUE, 0);
  gtk_box_pack_end(GTK_BOX(g->buttonbar), g->btn_asshot, TRUE, TRUE, 0);
  gtk_box_pack_start(box_enabled, g->buttonbar, TRUE, TRUE, 0);

  g->presets = dt_bauhaus_combobox_new(self);
  dt_bauhaus_widget_set_label(g->presets, N_("settings"), N_("settings")); // relabel to settings to remove confusion between module presets and white balance settings
  gtk_widget_set_tooltip_text(g->presets, _("choose white balance setting"));
  gtk_box_pack_start(box_enabled, g->presets, TRUE, TRUE, 0);

  g->finetune = dt_bauhaus_slider_new_with_range_and_feedback(self, -9.0, 9.0, 1.0, 0.0, 0, feedback);
  dt_bauhaus_widget_set_label(g->finetune, NULL, N_("finetune"));
  dt_bauhaus_slider_set_format(g->finetune, _("%.0f mired"));
  gtk_widget_set_tooltip_text(g->finetune, _("fine tune camera's white balance setting"));
  gtk_box_pack_start(box_enabled, g->finetune, TRUE, TRUE, 0);

  g_signal_connect(G_OBJECT(g->scale_k), "value-changed", G_CALLBACK(temp_tint_callback), self);
  g_signal_connect(G_OBJECT(g->scale_tint), "value-changed", G_CALLBACK(temp_tint_callback), self);

  g_signal_connect(G_OBJECT(g->presets), "value-changed", G_CALLBACK(preset_tune_callback), self);
  g_signal_connect(G_OBJECT(g->finetune), "value-changed", G_CALLBACK(preset_tune_callback), self);

  // update the gui when the preferences changed (i.e. colored sliders stuff)
  DT_DEBUG_CONTROL_SIGNAL_CONNECT(darktable.signals, DT_SIGNAL_PREFERENCES_CHANGE,
                                  G_CALLBACK(_preference_changed), (gpointer)self);

  // start building top level widget
  self->widget = gtk_stack_new();
  gtk_stack_set_homogeneous(GTK_STACK(self->widget), FALSE);

  GtkWidget *label_disabled = gtk_label_new(_("white balance disabled for camera"));
  gtk_widget_set_halign(label_disabled, GTK_ALIGN_START);
  gtk_label_set_ellipsize(GTK_LABEL(label_disabled), PANGO_ELLIPSIZE_END);

  gtk_stack_add_named(GTK_STACK(self->widget), GTK_WIDGET(box_enabled), "enabled");
  gtk_stack_add_named(GTK_STACK(self->widget), label_disabled, "disabled");
}

void gui_cleanup(struct dt_iop_module_t *self)
{
  self->request_color_pick = DT_REQUEST_COLORPICK_OFF;
  DT_DEBUG_CONTROL_SIGNAL_DISCONNECT(darktable.signals, G_CALLBACK(_preference_changed), self);
  DT_DEBUG_CONTROL_SIGNAL_DISCONNECT(darktable.signals,
                                     G_CALLBACK(_develop_ui_pipe_finished_callback), self);

  IOP_GUI_FREE;
}

void gui_reset(struct dt_iop_module_t *self)
{
  dt_iop_temperature_gui_data_t *g = (dt_iop_temperature_gui_data_t *)self->gui_data;

  const int preset = dt_bauhaus_combobox_get(g->presets);
  dt_iop_color_picker_reset(self, TRUE);

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g->btn_asshot), preset == DT_IOP_TEMP_AS_SHOT);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g->btn_user), preset == DT_IOP_TEMP_USER);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g->btn_d65), preset == DT_IOP_TEMP_D65);

  dtgtk_expander_set_expanded(DTGTK_EXPANDER(g->coeffs_expander), g->expand_coeffs);
  dtgtk_togglebutton_set_paint(DTGTK_TOGGLEBUTTON(g->coeffs_toggle), dtgtk_cairo_paint_solid_arrow,
                               CPF_STYLE_BOX | (g->expand_coeffs?CPF_DIRECTION_DOWN:CPF_DIRECTION_LEFT), NULL);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g->coeffs_toggle), g->expand_coeffs);

  color_finetuning_slider(self);
  color_rgb_sliders(self);
  color_temptint_sliders(self);
  _display_wb_error(self);
}

// modelines: These editor modelines have been set for all relevant files by tools/update_modelines.sh
// vim: shiftwidth=2 expandtab tabstop=2 cindent
// kate: tab-indents: off; indent-width 2; replace-tabs on; indent-mode cstyle; remove-trailing-spaces modified;
