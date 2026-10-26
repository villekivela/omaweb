#version 440

// The CRT glass a Scene can declare, as one pass over the Scene's small
// picture: its pixels shown square, a rolling refresh band, a flicker, the
// bloom of a smaller copy, a grade toward a second hue, the beam's
// scanlines, the halation, a phosphor mask and a vignette. A vector Scene
// has no raster: no scanlines, mask or band. The amounts are the Scene's
// `crt` block, which the website's glass draws by too, so the road reads the
// same through either glass.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    // The Scene's picture in its own pixels.
    vec2 sceneSize;
    // The glass's height in logical pixels.
    float glassHeight;
    // The part of the glass the reader sees, as x, y, width and height from 0
    // to 1, which the vignette is centred in.
    vec4 frame;
    // The refresh band's middle and half height, in the Scene's pixels.
    float bandY;
    float bandReach;
    // How much the band lightens at its middle and the flicker darkens.
    float bandStrength;
    float flicker;
    vec4 bandColour;
    // How much of the bloom shows over the picture.
    float bloomMix;
    // 1 on a raster Scene, 0 on a vector one.
    float raster;
    // One beam to each `scanPeriod` logical pixels, one of the Scene's rows.
    // Its width is `beamNarrowest` to `beamWidest` of the row as the row is
    // dark to bright, it falls off by `beamFalloff`, the gap between beams is
    // `scanShade` darker, and the whole is lifted by `beamLift`.
    float scanPeriod;
    float scanShade;
    float beamNarrowest;
    float beamWidest;
    float beamFalloff;
    float beamLift;
    // The mask's stripes leave their own channel and dim the others to
    // `maskDim`, and the whole is lifted by `maskLift`.
    float maskDim;
    float maskLift;
    // The halation counts what is brighter than `haloThreshold` in the mipmap
    // level `haloLod`, sampled 3 by 3 `haloStep` apart, and screens
    // `haloStrength` of it over the picture.
    float haloThreshold;
    float haloStrength;
    float haloLod;
    vec2 haloStep;
    // The vignette is clear to `vignetteClear` of the way from the frame's
    // middle to its corners, and `vignetteShade` darker at them.
    float vignetteClear;
    float vignetteShade;
    // The grade, as grade.frag draws it over the plain picture: toward
    // `gradeHue`, from `gradeTop` of the way down the frame and full
    // `gradeDepth` further, the highlights and the midtones each from the
    // brightness x to y by up to z.
    vec4 gradeHue;
    float gradeTop;
    float gradeDepth;
    vec3 gradeHighlights;
    vec3 gradeMidtones;
};

layout(binding = 1) uniform sampler2D source;
layout(binding = 2) uniform sampler2D bloom;
layout(binding = 3) uniform sampler2D halo;

float luma(vec3 colour)
{
    return dot(colour, vec3(0.2126, 0.7152, 0.0722));
}

void main()
{
    vec2 uv = qt_TexCoord0;
    vec3 colour = texture(source, uv).rgb;

    // The band is drawn into the picture, so it lands on whole rows of it.
    float row = floor(uv.y * sceneSize.y) + 0.5;
    float band = max(0.0, 1.0 - abs(row - bandY) / bandReach) * bandStrength * raster;
    colour = mix(colour, bandColour.rgb, band);

    colour = mix(colour, texture(bloom, uv).rgb, bloomMix);
    colour *= 1.0 - flicker;

    // The bright parts take the second hue at their own brightness, and more
    // of them lower in the frame; the dark ones keep the Scene's colours.
    vec2 inFrame = (uv - frame.xy) / frame.zw;
    float level = luma(colour);
    vec3 tinted = gradeHue.rgb * (level / max(luma(gradeHue.rgb), 0.05));
    float down = clamp((inFrame.y - gradeTop) / gradeDepth, 0.0, 1.0);
    float amount = max(
        smoothstep(gradeHighlights.x, gradeHighlights.y, level) * gradeHighlights.z * down,
        smoothstep(gradeMidtones.x, gradeMidtones.y, level) * gradeMidtones.z * down * down);
    colour = mix(colour, tinted, amount);

    if (raster > 0.5) {
        // Each of the Scene's rows is one pass of the beam, centred in the
        // row: a bright row swells to fill the gap below it, a dark one thins
        // and leaves it dark.
        float across = abs(fract(uv.y * glassHeight / scanPeriod) - 0.5) * 2.0;
        float width = mix(beamNarrowest, beamWidest, sqrt(clamp(luma(colour), 0.0, 1.0)));
        float beam = exp(-pow(across / width, 2.0) * beamFalloff);
        colour *= mix(1.0 - scanShade, 1.0, beam) * beamLift;
    }

    // Light from the bright parts scattered in the faceplate: a wide, faint
    // glow from an averaged copy, of what is brighter than the threshold only.
    vec3 glow = vec3(0.0);
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
            glow += textureLod(halo, uv + vec2(float(x), float(y)) * haloStep, haloLod).rgb;
    glow = max(glow / 9.0 - haloThreshold, 0.0) / (1.0 - haloThreshold);
    colour = 1.0 - (1.0 - colour) * (1.0 - glow * haloStrength);

    if (raster > 0.5) {
        // An aperture grille: one red, one green and one blue stripe in each
        // three device pixels, whatever the Scene's pitch, as a tube's mask.
        float stripe = mod(floor(gl_FragCoord.x), 3.0);
        vec3 grille = vec3(maskDim);
        if (stripe < 0.5)
            grille.r = 1.0;
        else if (stripe < 1.5)
            grille.g = 1.0;
        else
            grille.b = 1.0;
        colour *= grille * maskLift;
    }

    float reach = length((inFrame - 0.5) * 2.0) / sqrt(2.0);
    colour *= 1.0 - clamp((reach - vignetteClear) / (1.0 - vignetteClear), 0.0, 1.0)
        * vignetteShade;

    fragColor = vec4(colour, 1.0) * qt_Opacity;
}
