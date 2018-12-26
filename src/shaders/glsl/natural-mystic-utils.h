// -*- glsl -*-
#if !defined(NATURAL_MYSTIC_UTILS_H_INCLUDED)
#define NATURAL_MYSTIC_UTILS_H_INCLUDED 1

#include "natural-mystic-config.h"

/* Desaturate a color in linear RGB color space. The parameter
 * "degree" should be in [0,1] where 0 being no desaturation and 1
 * being full desaturation (completely gray).
 */
vec3 desaturate(vec3 color, float degree) {
    float luma = dot(color, vec3(0.22, 0.707, 0.071));
    return mix(color, vec3(luma), degree);
}

/* Convert linear RGB to HSV. */
vec4 rgb2hsv(vec4 rgb) {
    float rgbMax = max(rgb.r, max(rgb.g, rgb.b));
    float rgbMin = min(rgb.r, min(rgb.g, rgb.b));
    float h      = 0.0;
    float s      = 0.0;
    float v      = rgbMax;
    float delta  = rgbMax - rgbMin;

    if (delta != 0.0) {
        if (rgbMax == rgb.r) {
            // Between yellow and magenta.
            h = (rgb.g - rgb.b) / delta;
        }
        else if (rgbMax == rgb.b) {
            // Between cyan and yellow;
            h = 2.0 + (rgb.b - rgb.r) / delta;
        }
        else {
            // Between magenta and syan.
            h = 4.0 + (rgb.r - rgb.g) / delta;
        }
    }
    h *= 60.0; // degree
    h  = h < 0.0 ? h + 360.0 : h;

    if (rgbMax != 0.0) {
        s = delta / rgbMax;
    }

    return vec4(h, s, v, rgb.a);
}

/* Generate a random scalar based on some 2D vector. See
 * https://thebookofshaders.com/13/
 */
highp float random(highp vec2 st) {
    st += 128.0; // The function has a bad characteristic near (0, 0).
    return fract(
        sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}

/* Generate a perlin noise based on some 2D vector. Based on Morgan
 * McGuire @morgan3d https://www.shadertoy.com/view/4dS3Wd
 */
highp float perlinNoise(highp vec2 st) {
    highp vec2 i = floor(st);
    highp vec2 f = fract(st);

    // Four corners in 2D of a tile.
    highp float a = random(i);
    highp float b = random(i + vec2(1.0, 0.0));
    highp float c = random(i + vec2(0.0, 1.0));
    highp float d = random(i + vec2(1.0, 1.0));

    highp vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(a, b, u.x) +
        (c - a)* u.y * (1.0 - u.x) +
        (d - b) * u.x * u.y;
}

/* Generate an fBM noise based on some 2D vector. See
 * https://thebookofshaders.com/13/
 */
highp float fBM(int octaves, highp vec2 st) {
    // Initial values
    highp float value = 0.0;
    highp float amplitude = 0.5;

    // Loop of octaves
    for (int i = 0; i < octaves; i++) {
        value += amplitude * perlinNoise(st);
        st *= 2.0;
        amplitude *= 0.5;
    }
    return value;
}

/* Apply the ambient light on the original fragment "frag". Without
 * this filter, objects getting no light will be rendered in complete
 * darkness, which isn't how the reality works.
 */
vec3 applyAmbientLight(vec3 frag) {
    // Objects that are already bright should not be affected, hence
    // the "0.5 -".
    vec3 level = max(vec3(0.1), 0.5 - frag) * 1.2 + 1.0;
    return frag * level;
}

/* Calculate and apply a shadow on the original fragment "frag". The
 * argument "torchLevel" should be the torch light level [0, 1],
 * "sunLevel" should be the terrain-dependent sunlight level [0, 1],
 * and "daylight" should be the time-dependent daylight level.
 */
vec3 applyShadow(vec3 frag, float torchLevel, float sunLevel, float daylight, float baseDensity) {
    const vec3 shadowColor = vec3(0.0);
    const float minShadow = 0.0;  // [0, 1]
    const float maxShadow = 0.45; // [0, 1]
    const float torchLightCutOff = 0.1; // [0, 1]

    /* The less the sunlight level is, the darker the fragment will
     * be. */
    float density = mix(maxShadow, minShadow,
                        smoothstep(0.865, 0.875, sunLevel));

    if (density > 0.0) {
        /* The existence of torch light should negate the effect of
         * shadows. The higher the torch light level is, the less the
         * shadow affects the color. However, we also don't want
         * torches to completely drive away the shadow, since the
         * intensity of the torch light wouldn't be comparable to that
         * of sunlight.
         */
        density *= mix(1.0, torchLightCutOff,
                       smoothstep(0.5, 1.0, torchLevel));

        /* NOTE: Lack of daylight should also negate the effect of
         * shadows, because sadly the light map passed by the upstream
         * doesn't take torches into account. But we can't, because if
         * we reduce "density" depending on "daylight", caves will get
         * brighter at night. We could possibly overcome the issue by
         * completely disabling the vanilla lighting (i.e. diffuse *=
         * texture2D(TEXTURE_1, uv1)) and replacing it with an HDR
         * lighting.
         */
        return mix(frag, shadowColor, baseDensity * density);
    }
    else {
        return frag;
    }
}

/* Calculate the torch light flickering factor based on the in-game
 * time.
 */
float torchLightFlicker(highp float time) {
    /* The flicker is the sum of several sine waves with varying
     * frequently, phase, and amplitude. The reason for the final
     * "/ 10.0" is to avoid underflows.
     *
     * Invariant: -1 <= flicker <= 1 (or Bad Things will happen)
     */
    highp float flicker =
        ( sin(time * 11.0      ) * 0.35 + // fast wave
          sin(time *  3.0 + 0.3) * 0.7    // slow wave
        ) / 10.0;

    /* Workaround for MCPE-39749: the uniform TIME might not be a
     * number. See https://bugs.mojang.com/browse/MCPE-39749
     */
    flicker = clamp(flicker, -1.0, 1.0);
    flicker = isnan(flicker) || isinf(flicker) ? 0.0 : flicker;
    return flicker;
}

/* Calculate and apply the torch color on the original fragment
 * "frag". The argument "torchLevel" should be the torch light level
 * [0, 1], "sunLevel" should be the terrain-dependent sunlight level
 * [0, 1], and "daylight" should be the time-dependent daylight
 * level. The "time" is the in-game time, used for the flickering
 * effect.
 */
vec3 applyTorchColor(vec3 frag, float torchLevel, float sunLevel, float daylight, highp float time) {
    const vec3 torchColor = vec3(0.8, 0.3, -0.2);
    const float torchDecay = 0.55; // [0, 1]
    const float baseIntensity = 1.0; // [0, 1]
    const float sunlightCutOff = 0.1; // [0, 1]

    /* The sunlight should prevent torches from affecting the color,
     * but we also have to take the daylight level into account.
     */
    float intensity = max(0.0, torchLevel - torchDecay) * baseIntensity;
    if (intensity > 0.0) {
        intensity *= mix(1.0, sunlightCutOff, smoothstep(0.65, 0.875, sunLevel * daylight));
#if defined(ENABLE_TORCH_FLICKER)
        intensity *= torchLightFlicker(time) + 1.0;
#endif
        return frag + torchColor * intensity;
    }
    else {
        return frag;
    }
}

/* Apply the sunlight on a fragment "frag" based on the time-dependent
 * daylight level "daylight" [0,1]. The argument "sunLevel" should be
 * the terrain-dependent sunlight level [0,1]. The sunlight is
 * yellow-ish red.
 */
vec3 applySunlight(vec3 frag, float sunLevel, float daylight) {
    const vec3 sunColor = vec3(0.7, 0.3, 0.0);

    float intensity = (0.5 - abs(0.5 - daylight)) * sunLevel;
    if (intensity > 0.0) {
        float sunset = dot(frag, vec3(1.0)) / 1.5;
        return mix(frag, sunset * sunColor, intensity);
    }
    else {
        return frag;
    }
}

/* Apply the skylight on a fragment "frag" based on the time-dependent
 * daylight level "daylight" [0,1]. The argument "sunLevel" should be
 * the terrain-dependent sunlight level [0,1]. The skylight is
 * blue-ish white.
 */
vec3 applySkylight(vec3 frag, float sunLevel, float daylight) {
    const vec3 skyColor = vec3(0.8, 0.8, 1.0);
    const float skyLevel = 0.07;

    float intensity = sunLevel * skyLevel * daylight;

    return frag + skyColor * intensity;
}

/* Apply the moonlight on a fragment "frag" based on the
 * time-dependent daylight level "daylight" [0,1]. The argument
 * "torchLevel" should be the torch light level [0,1], and the
 * argument "sunLevel" should be the terrain-dependent sunlight level
 * [0,1]. The moonlight is purple-ish white.
 */
vec3 applyMoonlight(vec3 frag, float torchLevel, float sunLevel, float daylight) {
    const vec3 moonColor = vec3(0.5, 0.8, 0.95);
    const float moonLevel = 0.7;

    /* The reason why we take into account the torch light level is
     * that the intensity of the torch light is far higher than that
     * of moonlight.
     */
    float factor = sunLevel * (1.0 - torchLevel) * moonLevel * (1.0 - daylight);
    if (factor > 0.0) {
        return mix(frag, desaturate(frag, 0.8) * moonColor, factor);
    }
    else {
        return frag;
    }
}

/* Compute the fog color based on a base fog color, and a camera
 * distance. The resulting color should be mixed with the light color
 * using the alpha of the result. [Currently unused]
 */
vec4 computeFogColor(vec3 baseFog, float dist) {
    // See: http://in2gpu.com/2014/07/22/create-fog-shader/
    const float density = 0.01;

    float fogFactor = 1.0 / exp(pow(dist * density, 2.0));
    fogFactor = clamp(fogFactor, 0.0, 1.0);

    return vec4(baseFog, 1.0 - fogFactor);
}

/* Apply Uncharted 2 tone mapping to the original fragment "frag".
 * See: http://filmicworlds.com/blog/filmic-tonemapping-operators/
 */
vec3 uncharted2ToneMap_(vec3 x) {
    const float A = 0.15;
    const float B = 0.50;
    const float C = 0.10;
    const float D = 0.20;
    const float E = 0.02;
    const float F = 0.30;

    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}
vec3 uncharted2ToneMap(vec3 frag, float exposureBias) {
    const float W = 11.2;

    vec3 curr = uncharted2ToneMap_(exposureBias * frag);
    vec3 whiteScale = 1.0 / uncharted2ToneMap_(vec3(W, W, W));
    vec3 color = curr * whiteScale;

    return color;
}

/* Apply ACES filmic tone mapping to the original fragment "x".
 * See:
 * https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
 */
vec3 acesFilmicToneMap(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;

    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

/* Apply a contrast filter to the original fragment "frag".
 */
vec3 contrastFilter(vec3 frag, float contrast) {
    return (frag - 0.5) * max(contrast, 0.0) + 0.5;
}

/* Apply an HDR exposure filter to the original LDR fragment
 * "frag". The resulting image will be HDR, and need to be tone-mapped
 * back to LDR at the last stage. */
vec3 hdrExposure(vec3 frag, float overExposure, float underExposure) {
    vec3 overExposed   = frag / overExposure;
    vec3 normalExposed = frag;
    vec3 underExposed = frag * underExposure;

    return mix(overExposed, underExposed, normalExposed);
}

#endif /* NATURAL_MYSTIC_UTILS_H_INCLUDED */
