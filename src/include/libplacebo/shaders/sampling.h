/*
 * This file is part of libplacebo.
 *
 * libplacebo is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * libplacebo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with libplacebo.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBPLACEBO_SHADERS_SAMPLING_H_
#define LIBPLACEBO_SHADERS_SAMPLING_H_

// Sampling operations. These shaders perform some form of sampling operation
// from a given pl_tex. In order to use these, the pl_shader *must* have been
// created using the same `ra` as the originating `pl_tex`. Otherwise, this
// is undefined behavior. They require nothing (PL_SHADER_SIG_NONE) and return
// a color (PL_SHADER_SIG_COLOR).

#include <libplacebo/filters.h>
#include <libplacebo/shaders.h>

// Common parameters for sampling operations
struct pl_sample_src {
    const struct pl_tex *tex; // texture to sample
    struct pl_rect2df rect;   // sub-rect to sample from (optional)
    int components;           // number of components to sample (optional)
    int new_w, new_h;         // dimensions of the resulting output (optional)
    float scale;              // factor to multiply into sampled signal (optional)
};

struct pl_deband_params {
    // The number of debanding steps to perform per sample. Each step reduces a
    // bit more banding, but takes time to compute. Note that the strength of
    // each step falls off very quickly, so high numbers (>4) are practically
    // useless. Defaults to 1.
    int iterations;

    // The debanding filter's cut-off threshold. Higher numbers increase the
    // debanding strength dramatically, but progressively diminish image
    // details. Defaults to 4.0.
    float threshold;

    // The debanding filter's initial radius. The radius increases linearly
    // for each iteration. A higher radius will find more gradients, but a
    // lower radius will smooth more aggressively. Defaults to 16.0.
    float radius;

    // Add some extra noise to the image. This significantly helps cover up
    // remaining quantization artifacts. Higher numbers add more noise.
    // Note: When debanding HDR sources, even a small amount of grain can
    // result in a very big change to the brightness level. It's recommended to
    // either scale this value down or disable it entirely for HDR.
    //
    // Defaults to 6.0, which is very mild.
    float grain;
};

extern const struct pl_deband_params pl_deband_default_params;

// Debands a given texture and returns the sampled color in `vec4 color`. If
// `params` is left as NULL, it defaults to &pl_deband_default_params. Note
// that `tex->params.sample_mode` must be PL_TEX_SAMPLE_LINEAR. When the given
// `pl_sample_src` implies scaling, this effectively performs bilinear sampling.
//
// Note: This can also be used as a pure grain function, by setting the number
// of iterations to 0.
void pl_shader_deband(struct pl_shader *sh, const struct pl_sample_src *src,
                      const struct pl_deband_params *params);

// Performs direct / native texture sampling. This uses whatever built-in GPU
// sampling is built into the GPU and specified using src->params.sample_mode.
//
// Note: This is generally very low quality and should be avoided if possible,
// for both upscaling and downscaling. The only exception to this rule of thumb
// is exact 2x downscaling with PL_TEX_SAMPLE_LINEAR, as well as integer
// upscaling with PL_TEX_SAMPLE_NEAREST.
bool pl_shader_sample_direct(struct pl_shader *sh, const struct pl_sample_src *src);

// Performs hardware-accelerated / efficient bicubic sampling. This is more
// efficient than using the generalized sampling routines and
// pl_filter_function_bicubic. Requires the source texture to be set up with
// sample_mode PL_TEX_SAMPLE_LINEAR. Only works well when upscaling - avoid
// for downscaling.
bool pl_shader_sample_bicubic(struct pl_shader *sh, const struct pl_sample_src *src);

struct pl_sample_filter_params {
    // The filter to use for sampling.
    struct pl_filter_config filter;
    // The precision of the LUT. Defaults to 64 if unspecified.
    int lut_entries;
    // See `pl_filter_params.cutoff`. Defaults to 0.001 if unspecified. Only
    // relevant for polar filters.
    float cutoff;
    // Antiringing strength. A value of 0.0 disables antiringing, and a value
    // of 1.0 enables full-strength antiringing. Defaults to 0.0 if
    // unspecified. Only relevant for separated/orthogonal filters.
    float antiring;
    // Disable the use of compute shaders (e.g. if rendering to non-storable tex)
    bool no_compute;
    // Disable the use of filter widening / anti-aliasing (for downscaling)
    bool no_widening;

    // This shader object is used to store the LUT, and will be recreated
    // if necessary. To avoid thrashing the resource, users should avoid trying
    // to re-use the same LUT for different filter configurations or scaling
    // ratios. Must be set to a valid pointer, and the target NULL-initialized.
    struct pl_shader_obj **lut;
};

// Performs polar sampling. This internally chooses between an optimized compute
// shader, and various fragment shaders, depending on the supported GLSL version
// and GPU features. Returns whether or not it was successful.
//
// Note: `params->filter.polar` must be true to use this function.
bool pl_shader_sample_polar(struct pl_shader *sh,
                            const struct pl_sample_src *src,
                            const struct pl_sample_filter_params *params);

enum {
    PL_SEP_VERT = 0,
    PL_SEP_HORIZ,
    PL_SEP_PASSES
};

// Performs orthogonal (1D) sampling. Using this twice in a row (once vertical
// and once horizontal) effectively performs a 2D upscale. This is lower
// quality than polar sampling, but significantly faster, and therefore the
// recommended default. Returns whether or not it was successful.
//
// 0 <= pass < PL_SEP_PASSES indicates which component of the transformation to
// apply. PL_SEP_VERT only applies the vertical component, and PL_SEP_HORIZ
// only the horizontal. The non-relevant component of the `src->rect` is ignored
// entirely.
//
// Note: Due to internal limitations, this may currently only be used on 2D
// textures - even though the basic principle would work for 1D and 3D textures
// as well.
bool pl_shader_sample_ortho(struct pl_shader *sh, int pass,
                            const struct pl_sample_src *src,
                            const struct pl_sample_filter_params *params);

#endif // LIBPLACEBO_SHADERS_SAMPLING_H_
