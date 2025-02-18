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

layout(location = 0) in vec4 in_var_TEXCOORD11;
layout(location = 1) in highp vec4 in_var_TEXCOORD0;
layout(location = 0) out vec4 out_var_SV_Target0;
layout(location = 1) out vec4 out_var_SV_Target1;
layout(location = 2) out vec4 out_var_SV_Target2;
layout(location = 3) out highp float out_var_SV_Target3;

float _108;
vec4 _109;

void main()
{
    highp vec3 _133 = step(abs(Material_VectorExpressions11.xyz), vec3(0.0));
    highp vec3 _139 = vec3(View.View_GameTime, View.View_GameTime, _108);
    vec4 _148 = texture(ps1, (vec2(in_var_TEXCOORD0.x, in_var_TEXCOORD0.y) * Material_VectorExpressions8.xy) + (mod(_139, vec3(1.0) / (((vec3(1.0) - _133) * Material_VectorExpressions11.xyz) + _133)) * Material_VectorExpressions11.xyz).xy, Material_ScalarExpressions3.z);
    float _39 = _148.y;
    vec3 _40 = mix(Material_VectorExpressions6.xyz, Material_VectorExpressions5.xyz, vec3(Material_ScalarExpressions2.z)) * _39;
    highp vec3 _174 = step(abs(Material_VectorExpressions16.xyz), vec3(0.0));
    vec4 _34 = vec4(max((mix(_40, vec3(dot(_40, vec3(0.300048828125, 0.58984375, 0.1099853515625))), vec3(Material_ScalarExpressions3.w)) * Material_ScalarExpressions4.x) / vec3((MobileBasePass.MobileBasePass_bManualEyeAdaption != 0u) ? MobileBasePass.MobileBasePass_DefaultEyeExposure : texelFetch(ps0, int(0u)).x), vec3(0.0)) * clamp(((((((texture(ps2, ((vec2(in_var_TEXCOORD0.x, in_var_TEXCOORD0.y) * Material_VectorExpressions13.xy) + (mod(_139, vec3(1.0) / (((vec3(1.0) - _174) * Material_VectorExpressions16.xyz) + _174)) * Material_VectorExpressions16.xyz).xy) + Material_VectorExpressions17.xy).xyz * (Material_ScalarExpressions4.y * _39)) * Material_ScalarExpressions5.z) * 1.0) * 1.0) * 1.0) * 1.0).x, 0.0, 1.0), 0.0);
    highp vec3 _193 = _34.xyz * View.View_PreExposure;
    vec4 _58 = _109;
    _58.x = float(_Globals_CustomRenderingFlags) * 0.100000001490116119384765625;
    out_var_SV_Target0 = vec4(_193.x, _193.y, _193.z, _34.w);
    out_var_SV_Target2 = _58;
    out_var_SV_Target3 = 0.0;
}

    