#version 310 es
#extension GL_EXT_texture_buffer : require


#ifdef GL_ARM_shader_framebuffer_fetch_depth_stencil
#extension GL_ARM_shader_framebuffer_fetch_depth_stencil : enable
#define _Globals_gl_LastFragDepthARM gl_LastFragDepthARM
#else
#define _Globals_gl_LastFragDepthARM 0.0
#endif

#ifdef GL_QCOM_shader_framebuffer_fetch_rate
#extension GL_QCOM_shader_framebuffer_fetch_rate : enable
#define GL_QCOM_shader_framebuffer_fetch_rate 1
#endif

// end extensions

precision mediump float;
precision highp int;

layout(std140) uniform pb0
{
    vec4 Padding0[28];
    highp mat4 View_ViewToClip;
    vec4 Padding512[40];
    highp vec4 View_InvDeviceZToWorldZTransform;
    vec4 Padding1168[76];
    float PaddingB1168_0;
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

#define Material_ScalarExpressions5 (pc2_h[11].xyzw)
#define Material_ScalarExpressions4 (pc2_h[10].xyzw)
#define Material_ScalarExpressions6 (pc2_h[9].xyzw)
#define Material_ScalarExpressions2 (pc2_h[8].xyzw)
#define Material_ScalarExpressions3 (pc2_h[7].xyzw)
#define Material_VectorExpressions17 (pc2_h[6].xyzw)
#define Material_VectorExpressions13 (pc2_h[5].xyzw)
#define Material_VectorExpressions16 (pc2_h[4].xyzw)
#define Material_VectorExpressions5 (pc2_h[3].xyzw)
#define Material_VectorExpressions6 (pc2_h[2].xyzw)
#define Material_VectorExpressions8 (pc2_h[1].xyzw)
#define Material_VectorExpressions11 (pc2_h[0].xyzw)
uniform highp vec4 pc2_h[12];


#define _Globals_CustomRenderingFlags (floatBitsToInt(pu_h[0].x))
uniform highp vec4 pu_h[1];


uniform highp samplerBuffer ps0;
uniform mediump sampler2D ps1;
uniform mediump sampler2D ps2;

layout(location = 0) in highp float in_var_TEXCOORD0;
layout(location = 1) flat in highp vec4 in_var_TEXCOORD1;
layout(location = 2) in highp vec4 in_var_TEXCOORD2;
layout(location = 3) in highp vec4 in_var_TEXCOORD3;
layout(location = 4) in highp vec4 in_var_PARTICLE_POSITION;
layout(location = 5) in highp vec4 in_var_TEXCOORD8;
layout(location = 0) out vec4 out_var_SV_Target0;
layout(location = 1) out vec4 out_var_SV_Target1;
layout(location = 2) out vec4 out_var_SV_Target2;
layout(location = 3) out highp float out_var_SV_Target3;

float _129;
vec4 _130;

void main()
{
    highp vec4 _143 = vec4(_129, _129, gl_FragCoord.z, 1.0) * (1.0 / gl_FragCoord.w);
    highp vec3 _161 = step(abs(Material_VectorExpressions11.xyz), vec3(0.0));
    highp vec3 _167 = vec3(View.View_GameTime, View.View_GameTime, _129);
    vec4 _178 = texture(ps1, ((vec2(in_var_TEXCOORD3.x, in_var_TEXCOORD3.y) * Material_VectorExpressions8.xy) + (mod(_167, vec3(1.0) / (((vec3(1.0) - _161) * Material_VectorExpressions11.xyz) + _161)) * Material_VectorExpressions11.xyz).xy) + vec2(in_var_TEXCOORD1.zy), Material_ScalarExpressions3.z);
    vec3 _51 = mix(Material_VectorExpressions6.xyz, Material_VectorExpressions5.xyz, vec3(Material_ScalarExpressions2.z)) * vec4(_178.xyz, _129).xyz;
    vec3 _55 = mix(_51, vec3(dot(_51, vec3(0.300048828125, 0.58984375, 0.1099853515625))), vec3(Material_ScalarExpressions3.w));
    highp float _222;
    do
    {
        if (View.View_ViewToClip[3u].w < 1.0)
        {
            _222 = _143.w;
            break;
        }
        else
        {
            highp float _206 = _143.z;
            _222 = ((_206 * View.View_InvDeviceZToWorldZTransform.x) + View.View_InvDeviceZToWorldZTransform.y) + (1.0 / ((_206 * View.View_InvDeviceZToWorldZTransform.z) - View.View_InvDeviceZToWorldZTransform.w));
            break;
        }
    } while(false);
    highp vec3 _252 = step(abs(Material_VectorExpressions16.xyz), vec3(0.0));
    vec4 _266 = texture(ps2, ((vec2(in_var_TEXCOORD3.x, in_var_TEXCOORD3.y) * Material_VectorExpressions13.xy) + (mod(_167, vec3(1.0) / (((vec3(1.0) - _252) * Material_VectorExpressions16.xyz) + _252)) * Material_VectorExpressions16.xyz).xy) + Material_VectorExpressions17.xy);
    float _71 = clamp(Material_ScalarExpressions6.y * ((((in_var_TEXCOORD2.w * Material_ScalarExpressions4.y) * dot(_55, vec3(0.300048828125, 0.58984375, 0.1099853515625))) * min(max(((((_Globals_gl_LastFragDepthARM * View.View_InvDeviceZToWorldZTransform.x) + View.View_InvDeviceZToWorldZTransform.y) + (1.0 / ((_Globals_gl_LastFragDepthARM * View.View_InvDeviceZToWorldZTransform.z) - View.View_InvDeviceZToWorldZTransform.w))) - min(_222, Material_ScalarExpressions5.x)) / Material_ScalarExpressions4.w, 0.0), 1.0)) * _266.w), 0.0, 1.0);
    if ((_71 - 0.00100040435791015625) < 0.0)
    {
        discard;
    }
    vec4 _39 = vec4(max(((in_var_TEXCOORD2.xyz * _55) * Material_ScalarExpressions4.x) / vec3((MobileBasePass.MobileBasePass_bManualEyeAdaption != 0u) ? MobileBasePass.MobileBasePass_DefaultEyeExposure : texelFetch(ps0, int(0u)).x), vec3(0.0)) * 1.0, _71);
    highp vec3 _274 = _39.xyz * View.View_PreExposure;
    vec4 _72 = _130;
    _72.x = float(_Globals_CustomRenderingFlags) * 0.100000001490116119384765625;
    out_var_SV_Target0 = vec4(_274.x, _274.y, _274.z, _39.w);
    out_var_SV_Target2 = _72;
    out_var_SV_Target3 = 0.0;
}

    