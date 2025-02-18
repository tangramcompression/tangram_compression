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

#define Material_ScalarExpressions4 (pc2_h[7].xyzw)
#define Material_ScalarExpressions3 (pc2_h[6].xyzw)
#define Material_ScalarExpressions2 (pc2_h[5].xyzw)
#define Material_VectorExpressions12 (pc2_h[4].xyzw)
#define Material_VectorExpressions8 (pc2_h[3].xyzw)
#define Material_VectorExpressions11 (pc2_h[2].xyzw)
#define Material_VectorExpressions5 (pc2_h[1].xyzw)
#define Material_VectorExpressions6 (pc2_h[0].xyzw)
uniform highp vec4 pc2_h[8];


#define _Globals_CustomRenderingFlags (floatBitsToInt(pu_h[0].x))
uniform highp vec4 pu_h[1];


uniform highp samplerBuffer ps0;
uniform mediump sampler2D ps1;
uniform mediump sampler2D ps2;

layout(location = 0) in highp vec4 in_var_TEXCOORD0;
layout(location = 1) in highp vec4 in_var_PARTICLE_SUBUVS;
layout(location = 2) in highp vec3 in_var_TEXCOORD4;
layout(location = 3) in highp vec4 in_var_TEXCOORD5;
layout(location = 4) in highp vec4 in_var_PARTICLE_POSITION;
layout(location = 5) in highp vec4 in_var_TANGENTX;
layout(location = 6) in highp vec4 in_var_TANGENTZ;
layout(location = 7) in highp vec4 in_var_TEXCOORD8;
layout(location = 0) out vec4 out_var_SV_Target0;
layout(location = 1) out vec4 out_var_SV_Target1;
layout(location = 2) out vec4 out_var_SV_Target2;

float _112;
vec4 _113;

void main()
{
    vec4 _43 = mix(texture(ps1, in_var_PARTICLE_SUBUVS.xy), texture(ps1, in_var_PARTICLE_SUBUVS.zw), vec4(in_var_TEXCOORD4.x));
    float _44 = _43.x;
    vec3 _45 = mix(Material_VectorExpressions6.xyz, Material_VectorExpressions5.xyz, vec3(Material_ScalarExpressions2.z)) * _44;
    highp vec3 _168 = step(abs(Material_VectorExpressions11.xyz), vec3(0.0));
    vec4 _39 = vec4(max(((in_var_TEXCOORD5.xyz * mix(_45, vec3(dot(_45, vec3(0.300048828125, 0.58984375, 0.1099853515625))), vec3(Material_ScalarExpressions2.w))) * Material_ScalarExpressions3.x) / vec3((MobileBasePass.MobileBasePass_bManualEyeAdaption != 0u) ? MobileBasePass.MobileBasePass_DefaultEyeExposure : texelFetch(ps0, int(0u)).x), vec3(0.0)) * clamp((((((texture(ps2, ((vec2(in_var_TEXCOORD0.x, in_var_TEXCOORD0.y) * Material_VectorExpressions8.xy) + (mod(vec3(View.View_GameTime, View.View_GameTime, _112), vec3(1.0) / (((vec3(1.0) - _168) * Material_VectorExpressions11.xyz) + _168)) * Material_VectorExpressions11.xyz).xy) + Material_VectorExpressions12.xy).xyz * ((in_var_TEXCOORD5.w * Material_ScalarExpressions3.y) * _44)) * Material_ScalarExpressions4.z) * 1.0) * 1.0) * 1.0).x, 0.0, 1.0), 0.0);
    highp vec3 _190 = _39.xyz * View.View_PreExposure;
    vec4 _64 = _113;
    _64.x = float(_Globals_CustomRenderingFlags) * 0.100000001490116119384765625;
    out_var_SV_Target0 = vec4(_190.x, _190.y, _190.z, _39.w);
    out_var_SV_Target2 = _64;
}

    