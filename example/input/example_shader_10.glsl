#version 310 es
#extension GL_EXT_texture_buffer : require

// end extensions

precision mediump float;
precision highp int;

layout(std140) uniform pb0
{
    vec4 Padding0[149];
    float PaddingB0_0;
    highp float View_PreExposure;
    float PaddingF2392_0;
    float PaddingF2392_1;
    vec4 Padding2392[7];
    float PaddingB2392_0;
    float PaddingB2392_1;
    highp float View_GameTime;
} View;

layout(std140) uniform pb1
{
    vec4 Padding0[98];
    float PaddingB0_0;
    float PaddingB0_1;
    uint MobileBasePass_bManualEyeAdaption;
    highp float MobileBasePass_DefaultEyeExposure;
} MobileBasePass;

#define Material_ScalarExpressions5 (pc2_h[10].xyzw)
#define Material_ScalarExpressions4 (pc2_h[9].xyzw)
#define Material_ScalarExpressions2 (pc2_h[8].xyzw)
#define Material_ScalarExpressions3 (pc2_h[7].xyzw)
#define Material_VectorExpressions17 (pc2_h[6].xyzw)
#define Material_VectorExpressions13 (pc2_h[5].xyzw)
#define Material_VectorExpressions16 (pc2_h[4].xyzw)
#define Material_VectorExpressions5 (pc2_h[3].xyzw)
#define Material_VectorExpressions6 (pc2_h[2].xyzw)
#define Material_VectorExpressions8 (pc2_h[1].xyzw)
#define Material_VectorExpressions11 (pc2_h[0].xyzw)
uniform highp vec4 pc2_h[11];


#define _Globals_CustomRenderingFlags (floatBitsToInt(pu_h[0].x))
uniform highp vec4 pu_h[1];


uniform highp samplerBuffer ps0;
uniform mediump sampler2D ps1;
uniform mediump sampler2D ps2;

layout(location = 0) in vec4 in_var_TEXCOORD10;
layout(location = 1) in vec4 in_var_TEXCOORD11;
layout(location = 2) in highp vec4 in_var_TEXCOORD0[1];
layout(location = 3) in highp vec4 in_var_COLOR1;
layout(location = 4) flat in highp vec4 in_var_COLOR2;
layout(location = 5) in highp vec3 in_var_PARTICLE_POSITION;
layout(location = 6) in highp vec4 in_var_TEXCOORD8;
layout(location = 0) out vec4 out_var_SV_Target0;
layout(location = 1) out vec4 out_var_SV_Target1;
layout(location = 2) out vec4 out_var_SV_Target2;
layout(location = 3) out highp float out_var_SV_Target3;

float _121;
vec4 _122;

void main()
{
    highp vec3 _148 = step(abs(Material_VectorExpressions11.xyz), vec3(0.0));
    highp vec3 _154 = vec3(View.View_GameTime, View.View_GameTime, _121);
    vec4 _165 = texture(ps1, ((in_var_TEXCOORD0[0].xy * Material_VectorExpressions8.xy) + (mod(_154, vec3(1.0) / (((vec3(1.0) - _148) * Material_VectorExpressions11.xyz) + _148)) * Material_VectorExpressions11.xyz).xy) + vec2(in_var_COLOR2.zy), Material_ScalarExpressions3.z);
    float _46 = _165.y;
    vec3 _47 = mix(Material_VectorExpressions6.xyz, Material_VectorExpressions5.xyz, vec3(Material_ScalarExpressions2.z)) * _46;
    highp vec3 _192 = step(abs(Material_VectorExpressions16.xyz), vec3(0.0));
    vec4 _39 = vec4(max(((in_var_COLOR1.xyz * mix(_47, vec3(dot(_47, vec3(0.300048828125, 0.58984375, 0.1099853515625))), vec3(Material_ScalarExpressions3.w))) * Material_ScalarExpressions4.x) / vec3((MobileBasePass.MobileBasePass_bManualEyeAdaption != 0u) ? MobileBasePass.MobileBasePass_DefaultEyeExposure : texelFetch(ps0, int(0u)).x), vec3(0.0)) * clamp(((((((texture(ps2, ((in_var_TEXCOORD0[0].xy * Material_VectorExpressions13.xy) + (mod(_154, vec3(1.0) / (((vec3(1.0) - _192) * Material_VectorExpressions16.xyz) + _192)) * Material_VectorExpressions16.xyz).xy) + Material_VectorExpressions17.xy).xyz * ((in_var_COLOR1.w * Material_ScalarExpressions4.y) * _46)) * Material_ScalarExpressions5.z) * 1.0) * 1.0) * 1.0) * 1.0).x, 0.0, 1.0), 0.0);
    highp vec3 _211 = _39.xyz * View.View_PreExposure;
    vec4 _68 = _122;
    _68.x = float(_Globals_CustomRenderingFlags) * 0.100000001490116119384765625;
    out_var_SV_Target0 = vec4(_211.x, _211.y, _211.z, _39.w);
    out_var_SV_Target2 = _68;
    out_var_SV_Target3 = 0.0;
}

    