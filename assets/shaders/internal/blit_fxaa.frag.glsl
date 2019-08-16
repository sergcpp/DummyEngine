#version 310 es

#if defined(GL_ES) || defined(VULKAN)
	precision highp int;
	precision mediump float;
#endif

#include "_fs_common.glsl"

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
*/

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};
 
layout(binding = REN_BASE0_TEX_SLOT) uniform sampler2D s_texture;

#if defined(VULKAN)
layout(push_constant) uniform PushConstants {
    layout(offset = 16) vec2 texcoord_offset;
};
#else
layout(location = 12) uniform vec2 texcoord_offset;
#endif

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 aVertexUVs_;
#else
in vec2 aVertexUVs_;
#endif

layout(location = 0) out vec4 outColor;

float FxaaLuma(vec4 rgba) {
        const vec3 luma = vec3(0.299, 0.587, 0.114);
        return dot(rgba.rgb, luma);
}

vec4 FxaaPixelShader(vec2 pos,
                     sampler2D tex,
                     vec2 fxaaQualityRcpFrame,
                     float fxaaQualitySubpix,
                     float fxaaQualityEdgeThreshold,
                     float fxaaQualityEdgeThresholdMin) {
        vec2 posM;
        posM.x = pos.x;
        posM.y = pos.y;
        vec4 rgbyM = texture(tex, posM);
        rgbyM.w = FxaaLuma(rgbyM);
        
        float lumaS = FxaaLuma(texture(tex, posM + (vec2(0.0, 1.0) * fxaaQualityRcpFrame)));
        float lumaE = FxaaLuma(texture(tex, posM + (vec2(1.0, 0.0) * fxaaQualityRcpFrame)));
        float lumaN = FxaaLuma(texture(tex, posM + (vec2(0.0, -1.0) * fxaaQualityRcpFrame)));
        float lumaW = FxaaLuma(texture(tex, posM + (vec2(-1.0, 0.0) * fxaaQualityRcpFrame)));
        float maxSM = max(lumaS, rgbyM.w);
        float minSM = min(lumaS, rgbyM.w);
        float maxESM = max(lumaE, maxSM);
        float minESM = min(lumaE, minSM);
        float maxWN = max(lumaN, lumaW);
        float minWN = min(lumaN, lumaW);
        float rangeMax = max(maxWN, maxESM);
        float rangeMin = min(minWN, minESM);
        float rangeMaxScaled = rangeMax * fxaaQualityEdgeThreshold;
        float range = rangeMax - rangeMin;
        float rangeMaxClamped = max(fxaaQualityEdgeThresholdMin, rangeMaxScaled);

        bool earlyExit = range < rangeMaxClamped;
        if(earlyExit)
                return rgbyM;
        
        
        float lumaNW = FxaaLuma(texture(tex, posM + (vec2(-1.0, -1.0) * fxaaQualityRcpFrame)));
        float lumaSE = FxaaLuma(texture(tex, posM + (vec2(1.0, 1.0) * fxaaQualityRcpFrame)));
        float lumaNE = FxaaLuma(texture(tex, posM + (vec2(1.0, -1.0) * fxaaQualityRcpFrame)));
        float lumaSW = FxaaLuma(texture(tex, posM + (vec2(-1.0, 1.0) * fxaaQualityRcpFrame)));
        float lumaNS = lumaN + lumaS;
        float lumaWE = lumaW + lumaE;
        float subpixRcpRange = 1.0/range;
        float subpixNSWE = lumaNS + lumaWE;
        float edgeHorz1 = (-2.0 * rgbyM.w) + lumaNS;
        float edgeVert1 = (-2.0 * rgbyM.w) + lumaWE;
        float lumaNESE = lumaNE + lumaSE;
        float lumaNWNE = lumaNW + lumaNE;
        float edgeHorz2 = (-2.0 * lumaE) + lumaNESE;
        float edgeVert2 = (-2.0 * lumaN) + lumaNWNE;
        float lumaNWSW = lumaNW + lumaSW;
        float lumaSWSE = lumaSW + lumaSE;
        float edgeHorz4 = (abs(edgeHorz1) * 2.0) + abs(edgeHorz2);
        float edgeVert4 = (abs(edgeVert1) * 2.0) + abs(edgeVert2);
        float edgeHorz3 = (-2.0 * lumaW) + lumaNWSW;
        float edgeVert3 = (-2.0 * lumaS) + lumaSWSE;
        float edgeHorz = abs(edgeHorz3) + edgeHorz4;
        float edgeVert = abs(edgeVert3) + edgeVert4;
        float subpixNWSWNESE = lumaNWSW + lumaNESE;
        float lengthSign = fxaaQualityRcpFrame.x;
        bool horzSpan = edgeHorz >= edgeVert;
        float subpixA = subpixNSWE * 2.0 + subpixNWSWNESE;
        if(!horzSpan) lumaN = lumaW;
        if(!horzSpan) lumaS = lumaE;
        if(horzSpan) lengthSign = fxaaQualityRcpFrame.y;
        float subpixB = (subpixA * (1.0/12.0)) - rgbyM.w;
        float gradientN = lumaN - rgbyM.w;
        float gradientS = lumaS - rgbyM.w;
        float lumaNN = lumaN + rgbyM.w;
        float lumaSS = lumaS + rgbyM.w;
        bool pairN = abs(gradientN) >= abs(gradientS);
        float gradient = max(abs(gradientN), abs(gradientS));
        if(pairN) lengthSign = -lengthSign;
        float subpixC = clamp(abs(subpixB) * subpixRcpRange, 0.0, 1.0);
        vec2 posB;
        posB.x = posM.x;
        posB.y = posM.y;
        vec2 offNP;
        offNP.x = (!horzSpan) ? 0.0 : fxaaQualityRcpFrame.x;
        offNP.y = ( horzSpan) ? 0.0 : fxaaQualityRcpFrame.y;
        if(!horzSpan) posB.x += lengthSign * 0.5;
        if( horzSpan) posB.y += lengthSign * 0.5;
        vec2 posN;
        posN.x = posB.x - offNP.x * 1.0;
        posN.y = posB.y - offNP.y * 1.0;
        vec2 posP;
        posP.x = posB.x + offNP.x * 1.0;
        posP.y = posB.y + offNP.y * 1.0;
        float subpixD = ((-2.0)*subpixC) + 3.0;
        float lumaEndN = FxaaLuma(texture(tex, posN));
        float subpixE = subpixC * subpixC;
        float lumaEndP = FxaaLuma(texture(tex, posP));
        if(!pairN) lumaNN = lumaSS;
        float gradientScaled = gradient * 1.0/4.0;
        float lumaMM = rgbyM.w - lumaNN * 0.5;
        float subpixF = subpixD * subpixE;
        bool lumaMLTZero = lumaMM < 0.0;
        lumaEndN -= lumaNN * 0.5;
        lumaEndP -= lumaNN * 0.5;
        bool doneN = abs(lumaEndN) >= gradientScaled;
        bool doneP = abs(lumaEndP) >= gradientScaled;
        if(!doneN) posN.x -= offNP.x * 1.5;
        if(!doneN) posN.y -= offNP.y * 1.5;
        bool doneNP = (!doneN) || (!doneP);
        if(!doneP) posP.x += offNP.x * 1.5;
        if(!doneP) posP.y += offNP.y * 1.5;
        if(doneNP) {
                if(!doneN) lumaEndN = FxaaLuma(texture(tex, posN.xy));
                if(!doneP) lumaEndP = FxaaLuma(texture(tex, posP.xy));
                if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
                if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
                doneN = abs(lumaEndN) >= gradientScaled;
                doneP = abs(lumaEndP) >= gradientScaled;
                if(!doneN) posN.x -= offNP.x * 2.0;
                if(!doneN) posN.y -= offNP.y * 2.0;
                doneNP = (!doneN) || (!doneP);
                if(!doneP) posP.x += offNP.x * 2.0;
                if(!doneP) posP.y += offNP.y * 2.0;
                if(doneNP) {
                        if(!doneN) lumaEndN = FxaaLuma(texture(tex, posN.xy));
                        if(!doneP) lumaEndP = FxaaLuma(texture(tex, posP.xy));
                        if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
                        if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
                        doneN = abs(lumaEndN) >= gradientScaled;
                        doneP = abs(lumaEndP) >= gradientScaled;
                        if(!doneN) posN.x -= offNP.x * 2.0;
                        if(!doneN) posN.y -= offNP.y * 2.0;
                        doneNP = (!doneN) || (!doneP);
                        if(!doneP) posP.x += offNP.x * 2.0;
                        if(!doneP) posP.y += offNP.y * 2.0;
                        if(doneNP) {
                                if(!doneN) lumaEndN = FxaaLuma(texture(tex, posN.xy));
                                if(!doneP) lumaEndP = FxaaLuma(texture(tex, posP.xy));
                                if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
                                if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
                                doneN = abs(lumaEndN) >= gradientScaled;
                                doneP = abs(lumaEndP) >= gradientScaled;
                                if(!doneN) posN.x -= offNP.x * 4.0;
                                if(!doneN) posN.y -= offNP.y * 4.0;
                                doneNP = (!doneN) || (!doneP);
                                if(!doneP) posP.x += offNP.x * 4.0;
                                if(!doneP) posP.y += offNP.y * 4.0;
                                if(doneNP) {
                                        if(!doneN) lumaEndN = FxaaLuma(texture(tex, posN.xy));
                                        if(!doneP) lumaEndP = FxaaLuma(texture(tex, posP.xy));
                                        if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
                                        if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
                                        doneN = abs(lumaEndN) >= gradientScaled;
                                        doneP = abs(lumaEndP) >= gradientScaled;
                                        if(!doneN) posN.x -= offNP.x * 12.0;
                                        if(!doneN) posN.y -= offNP.y * 12.0;
                                        doneNP = (!doneN) || (!doneP);
                                        if(!doneP) posP.x += offNP.x * 12.0;
                                        if(!doneP) posP.y += offNP.y * 12.0;
                                }
                        }
                }
        }

        float dstN = posM.x - posN.x;
        float dstP = posP.x - posM.x;
        if(!horzSpan) dstN = posM.y - posN.y;
        if(!horzSpan) dstP = posP.y - posM.y;

        bool goodSpanN = (lumaEndN < 0.0) != lumaMLTZero;
        float spanLength = (dstP + dstN);
        bool goodSpanP = (lumaEndP < 0.0) != lumaMLTZero;
        float spanLengthRcp = 1.0/spanLength;

        bool directionN = dstN < dstP;
        float dst = min(dstN, dstP);
        bool goodSpan = directionN ? goodSpanN : goodSpanP;
        float subpixG = subpixF * subpixF;
        float pixelOffset = (dst * (-spanLengthRcp)) + 0.5;
        float subpixH = subpixG * fxaaQualitySubpix;

        float pixelOffsetGood = goodSpan ? pixelOffset : 0.0;
        float pixelOffsetSubpix = max(pixelOffsetGood, subpixH);
        if(!horzSpan) posM.x += pixelOffsetSubpix * lengthSign;
        if( horzSpan) posM.y += pixelOffsetSubpix * lengthSign;

        return vec4(texture(tex, posM).xyz, rgbyM.w);
}

void main() {
    outColor = FxaaPixelShader(aVertexUVs_ * shrd_data.uResAndFRes.xy / shrd_data.uResAndFRes.zw, s_texture, texcoord_offset, 0.75, 0.125, 0.0625);
    outColor.a = 1.0;
}
