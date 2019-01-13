// -*- glsl -*-
#if !defined(NATURAL_MYSTIC_WATER_H_INCLUDED)
#define NATURAL_MYSTIC_WATER_H_INCLUDED 1

/* Overview of our water system:
 *
 * In the vertex shader we compute geometric undulations of a base
 * mesh map with Gerstner waves, as well as their partial derivatives
 * to compute the surface normal. The opacity of the water is also
 * computed in the vertex shader based on the view angle. The
 * resulting normal is then passed to the fragment shader.
 *
 * In the fragment shader we generate smaller waves and sum up with
 * the geometric normal, to get the final normal. We use the normal to
 * compute the intensity of the specular light.
 *
 * See also: https://developer.nvidia.com/gpugems/GPUGems/gpugems_ch01.html
 */

highp vec3 gerstnerWave(
    highp vec3 wPos, highp float time, out highp vec3 normal,
    float Q, float numWaves, float Ai, vec2 Di, float Li, float Si) {

    const float wFactor = 9.80665 * 2.0 * 3.14159;

    float wi    = sqrt(wFactor / Li);
    float Qi    = Q / (wi * Ai * numWaves);
    float phi_i = Si * 2.0 / Li;

    highp float phase = wi * dot(Di, wPos.xz) + phi_i * time;
    wPos.xz += Qi * Ai * Di * cos(phase);
    wPos.y  +=      Ai      * sin(phase);

    return wPos;
}

vec2 deg2dir(float deg) {
    float rad = radians(deg);
    return vec2(cos(rad), sin(rad));
}

highp vec3 geometricWaterWave(highp vec3 wPos, highp float time, out highp vec3 normal) {
    /* The Gerstner wave function is:
     *
     *              [ x + Σ(Q_i A_i * D_i.x * cos(w_i D_i · (x, y) + φ_i t)), ]
     * P(x, y, t) = | y + Σ(Q_i A_i * D_i.y * cos(w_i D_i · (x, y) + φ_i t)), |
     *              [     Σ(    A_i         * sin(w_i D_i · (x, y) + φ_i t))  ]
     *
     * where
     *
     *   Q_i: The steepness of the i-th wave. Q_i = 0 means that the
     *        wave i is plainly a sine, while Q_i = 1/(w_i A_i) gives
     *        a sharp crest. To ease setting the parameters, we leave
     *        the specification of Q as a steepness parameter [0, 1]
     *        and use Q_i = Q/(w_i A_i * numWaves).
     *
     *   A_i: The amplitude of the i-th wave.
     *
     *   D_i: The horizontal vector of the i-th wave, perpendicular to
     *        the wave front along which the crest travels.
     *
     *   w_i: The frequency of the i-th wave. For wavelength L_i, w_i
     *        = sqrt(g * 2π/L_i) where g is the gravitational
     *        constant 9.80665.
     *
     *   φ_i: The phase constant of the i-th wave. For speed S_i,
     *        φ_i = S_i * 2/L_i.
     *
     * And the surface normal will be:
     *
     *              [    -Σ(D_i.x * w_i A_i * cos(w_i D_i · (x, y) + φ_i t)), ]
     * N(x, y, t) = |    -Σ(D_i.y * w_i A_i * cos(w_i D_i · (x, y) + φ_i t)), |
     *              [ 1 - Σ(Q_i   * w_i A_i * sin(w_i D_i · (x, y) + φ_i t))  ]
     */
    const float Q        = 0.4;
    const float numWaves = float(4);

    normal = vec3(0.0, 1.0, 0.0);
    wPos   = gerstnerWave(wPos, time, normal, Q, numWaves, 0.07, deg2dir( 90.0), 16.0, 6.0);
    wPos   = gerstnerWave(wPos, time, normal, Q, numWaves, 0.07, deg2dir(260.0), 15.0, 6.0);
    wPos   = gerstnerWave(wPos, time, normal, Q, numWaves, 0.03, deg2dir( 50.0),  8.0, 8.0);
    wPos   = gerstnerWave(wPos, time, normal, Q, numWaves, 0.03, deg2dir(200.0),  7.0, 8.0);

    return wPos;
}

#endif /* !defined(NATURAL_MYSTIC_WATER_H_INCLUDED) */
