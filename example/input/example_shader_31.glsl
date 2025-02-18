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

float _131;
vec4 _132;

void main()
{
    highp vec4 _146 = vec4(_131, _131, gl_FragCoord.z, 1.0) * (1.0 / gl_FragCoord.w);
    highp vec3 _164 = step(abs(Material_VectorExpressions11.xyz), vec3(0.0));
    highp vec3 _170 = vec3(View.View_GameTime, View.View_GameTime, _131);
    vec4 _181 = texture(ps1, ((in_var_TEXCOORD0[0].xy * Material_VectorExpressions8.xy) + (mod(_170, vec3(1.0) / (((vec3(1.0) - _164) * Material_VectorExpressions11.xyz) + _164)) * Material_VectorExpressions11.xyz).xy) + vec2(in_var_COLOR2.zy), Material_ScalarExpressions3.z);
    vec3 _52 = mix(Material_VectorExpressions6.xyz, Material_VectorExpressions5.xyz, vec3(Material_ScalarExpressions2.z)) * vec4(_181.xyz, _131).xyz;
    vec3 _56 = mix(_52, vec3(dot(_52, vec3(0.300048828125, 0.58984375, 0.1099853515625))), vec3(Material_ScalarExpressions3.w));
    highp float _225;
    do
    {
        if (View.View_ViewToClip[3u].w < 1.0)
        {
            _225 = _146.w;
            break;
        }
        else
        {
            highp float _209 = _146.z;
            _225 = ((_209 * View.View_InvDeviceZToWorldZTransform.x) + View.View_InvDeviceZToWorldZTransform.y) + (1.0 / ((_209 * View.View_InvDeviceZToWorldZTransform.z) - View.View_InvDeviceZToWorldZTransform.w));
            break;
        }
    } while(false);
    highp vec3 _255 = step(abs(Material_VectorExpressions16.xyz), vec3(0.0));
    vec4 _269 = texture(ps2, ((in_var_TEXCOORD0[0].xy * Material_VectorExpressions13.xy) + (mod(_170, vec3(1.0) / (((vec3(1.0) - _255) * Material_VectorExpressions16.xyz) + _255)) * Material_VectorExpressions16.xyz).xy) + Material_VectorExpressions17.xy);
    float _72 = clamp(Material_ScalarExpressions6.y * ((((in_var_COLOR1.w * Material_ScalarExpressions4.y) * dot(_56, vec3(0.300048828125, 0.58984375, 0.1099853515625))) * min(max(((((_Globals_gl_LastFragDepthARM * View.View_InvDeviceZToWorldZTransform.x) + View.View_InvDeviceZToWorldZTransform.y) + (1.0 / ((_Globals_gl_LastFragDepthARM * View.View_InvDeviceZToWorldZTransform.z) - View.View_InvDeviceZToWorldZTransform.w))) - min(_225, Material_ScalarExpressions5.x)) / Material_ScalarExpressions4.w, 0.0), 1.0)) * _269.w), 0.0, 1.0);
    if ((_72 - 0.00100040435791015625) < 0.0)
    {
        discard;
    }
    vec4 _40 = vec4(max(((in_var_COLOR1.xyz * _56) * Material_ScalarExpressions4.x) / vec3((MobileBasePass.MobileBasePass_bManualEyeAdaption != 0u) ? MobileBasePass.MobileBasePass_DefaultEyeExposure : texelFetch(ps0, int(0u)).x), vec3(0.0)) * 1.0, _72);
    highp vec3 _277 = _40.xyz * View.View_PreExposure;
    vec4 _73 = _132;
    _73.x = float(_Globals_CustomRenderingFlags) * 0.100000001490116119384765625;
    out_var_SV_Target0 = vec4(_277.x, _277.y, _277.z, _40.w);
    out_var_SV_Target2 = _73;
    out_var_SV_Target3 = 0.0;
}

    