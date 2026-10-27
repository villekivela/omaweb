#version 440

// The CRT glass a Scene can declare, as one pass over the Scene's small
// picture, drawn as the website's glass composites it (website/scene.js): a
// rolling refresh band and a flicker in the picture, its grade toward a second
// hue, its pixels shown square, lifted and clipped, the bloom of a smaller
// copy over it, the beam's scanlines, the halation screened over them, a
// phosphor mask multiplied over that and a vignette. A vector Scene has no
// raster: no scanlines, mask, band or lift. The amounts are the Scene's `crt`
// block, which the website's glass draws by too, so the road reads the same
// through either glass.

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
    // The picture and the bloom are brightened by `lift`, the beam's and the
    // mask's lifts together, and clipped, before the scanlines and the mask
    // darken them.
    float lift;
    // One beam to each `scanPeriod` logical pixels, one of the Scene's rows,
    // `beamWidth` of the row wide, falling off by `beamFalloff`, and the gap
    // between beams `scanShade` darker.
    float scanPeriod;
    float scanShade;
    float beamWidth;
    float beamFalloff;
    // The mask's stripes leave their own channel and dim the others to
    // `maskDim`.
    float maskDim;
    // The halation is the picture cut at its threshold (halation.frag),
    // blurred by 3 by 3 bilinear taps `haloStep` apart and screened
    // `haloStrength` of it over the picture.
    float haloStrength;
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

// The Scene's picture as the website's glass draws into it before it is
// shown: the band, the flicker and the grade, clipped as a canvas clips it.
vec3 drawnInto(vec3 colour, float band, float down)
{
    colour = mix(colour, bandColour.rgb, band);
    colour *= 1.0 - flicker;
    // The bright parts take the second hue at their own brightness, and more
    // of them lower in the frame; the dark ones keep the Scene's colours.
    float level = luma(colour);
    vec3 tinted = gradeHue.rgb * (level / max(luma(gradeHue.rgb), 0.05));
    float amount = max(
        smoothstep(gradeHighlights.x, gradeHighlights.y, level) * gradeHighlights.z * down,
        smoothstep(gradeMidtones.x, gradeMidtones.y, level) * gradeMidtones.z * down * down);
    return clamp(mix(colour, tinted, amount), 0.0, 1.0);
}

void main()
{
    vec2 uv = qt_TexCoord0;
    vec2 inFrame = (uv - frame.xy) / frame.zw;

    // The band is drawn into the picture, so it lands on whole rows of it.
    float row = floor(uv.y * sceneSize.y) + 0.5;
    float band = max(0.0, 1.0 - abs(row - bandY) / bandReach) * bandStrength * raster;
    float down = clamp((inFrame.y - gradeTop) / gradeDepth, 0.0, 1.0);

    // The picture and its bloom, each lifted and clipped as a filter clips
    // it, the bloom laid over the picture.
    vec3 picture = clamp(drawnInto(texture(source, uv).rgb, band, down) * lift, 0.0, 1.0);
    vec3 blur = clamp(drawnInto(texture(bloom, uv).rgb, band, down) * lift, 0.0, 1.0);
    vec3 colour = mix(picture, blur, bloomMix);

    if (raster > 0.5) {
        // Each of the Scene's rows is one pass of the beam, centred in the
        // row, darkest between rows.
        float across = abs(fract(uv.y * glassHeight / scanPeriod) - 0.5) * 2.0;
        float beam = exp(-pow(across / beamWidth, 2.0) * beamFalloff);
        colour *= mix(1.0 - scanShade, 1.0, beam);
    }

    // Light from the bright parts scattered in the faceplate: the cut copy,
    // spread smoothly and blurred by a binomial 3 by 3, whose spread is the
    // website's Gaussian blur.
    vec3 glow = vec3(0.0);
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
            glow += texture(halo, uv + vec2(float(x), float(y)) * haloStep).rgb
                * ((x == 0 ? 2.0 : 1.0) * (y == 0 ? 2.0 : 1.0));
    glow /= 16.0;
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
        colour *= grille;
    }

    float reach = length((inFrame - 0.5) * 2.0) / sqrt(2.0);
    colour *= 1.0 - clamp((reach - vignetteClear) / (1.0 - vignetteClear), 0.0, 1.0)
        * vignetteShade;

    fragColor = vec4(colour, 1.0) * qt_Opacity;
}
