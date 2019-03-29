#ifdef GL_ES
    precision mediump float;
#endif

/*
UNIFORMS
    s_texture : 0
    texcoord_offset : 1
*/

uniform sampler2D s_texture;
uniform vec2 texcoord_offset;


varying vec2 aVertexUVs_;


float FxaaLuma(vec4 rgba) {
        //return rgba.w;
        //return 1.0;
        vec3 luma = vec3(0.299, 0.587, 0.114);
        return dot(rgba.rgb, luma);//clamp(dot(rgba.rgb, luma), 0.0, 1.0);
}

vec4 FxaaPixelShader0(vec2 pos,
                      sampler2D tex,
                      vec2 texcoord_offset) {
  //float FXAA_SPAN_MAX = 8.0;
  //float FXAA_REDUCE_MUL = 1.0/8.0;
  //float FXAA_REDUCE_MIN = (1.0/128.0);
  
  
  float FXAA_SPAN_MAX = 8.0;
  float FXAA_REDUCE_MUL = 1.0/8.0;
  float FXAA_REDUCE_MIN = (1.0/512.0);

  vec3 rgbNW = texture2D(tex, pos + (vec2(-1.0, -1.0) * texcoord_offset)).xyz;
  vec3 rgbNE = texture2D(tex, pos + (vec2(+1.0, -1.0) * texcoord_offset)).xyz;
  vec3 rgbSW = texture2D(tex, pos + (vec2(-1.0, +1.0) * texcoord_offset)).xyz;
  vec3 rgbSE = texture2D(tex, pos + (vec2(+1.0, +1.0) * texcoord_offset)).xyz;
  vec3 rgbM  = texture2D(tex, pos).xyz;
    
  vec3 luma = vec3(0.299, 0.587, 0.114);
  float lumaNW = dot(rgbNW, luma);
  float lumaNE = dot(rgbNE, luma);
  float lumaSW = dot(rgbSW, luma);
  float lumaSE = dot(rgbSE, luma);
  float lumaM  = dot( rgbM, luma);
    
  float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
  float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

  vec2 dir;
  dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
  dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));
    
  float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);
    
  float rcpDirMin = 1.0/(min(abs(dir.x), abs(dir.y)) + dirReduce);
  
  dir = min(vec2(FXAA_SPAN_MAX,  FXAA_SPAN_MAX), 
        max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX), dir * rcpDirMin)) * texcoord_offset;
        
  vec3 rgbA = (1.0/2.0) * (
              texture2D(tex, pos + dir * (1.0/3.0 - 0.5)).xyz +
              texture2D(tex, pos + dir * (2.0/3.0 - 0.5)).xyz);
  vec3 rgbB = rgbA * (1.0/2.0) + (1.0/4.0) * (
              texture2D(tex, pos + dir * (0.0/3.0 - 0.5)).xyz +
              texture2D(tex, pos + dir * (3.0/3.0 - 0.5)).xyz);
  float lumaB = dot(rgbB, luma);
  
  if((lumaB < lumaMin) || (lumaB > lumaMax)){
    return vec4(rgbA, 1.0);
  } else {
    return vec4(rgbB, 1.0);
  }
}

vec4 FxaaPixelShader(
        vec2 pos,
        sampler2D tex,
        vec2 fxaaQualityRcpFrame,
        float fxaaQualitySubpix,
        float fxaaQualityEdgeThreshold,
        float fxaaQualityEdgeThresholdMin
) {
        vec2 posM;
        posM.x = pos.x;
        posM.y = pos.y;
        vec4 rgbyM = texture2D(tex, posM);
        rgbyM.w = FxaaLuma(rgbyM);
        
        float lumaS = FxaaLuma(texture2D(tex, posM + (vec2(0.0, 1.0) * fxaaQualityRcpFrame)));
        float lumaE = FxaaLuma(texture2D(tex, posM + (vec2(1.0, 0.0) * fxaaQualityRcpFrame)));
        float lumaN = FxaaLuma(texture2D(tex, posM + (vec2(0.0, -1.0) * fxaaQualityRcpFrame)));
        float lumaW = FxaaLuma(texture2D(tex, posM + (vec2(-1.0, 0.0) * fxaaQualityRcpFrame)));
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
        
        
        float lumaNW = FxaaLuma(texture2D(tex, posM + (vec2(-1.0, -1.0) * fxaaQualityRcpFrame)));
        float lumaSE = FxaaLuma(texture2D(tex, posM + (vec2(1.0, 1.0) * fxaaQualityRcpFrame)));
        float lumaNE = FxaaLuma(texture2D(tex, posM + (vec2(1.0, -1.0) * fxaaQualityRcpFrame)));
        float lumaSW = FxaaLuma(texture2D(tex, posM + (vec2(-1.0, 1.0) * fxaaQualityRcpFrame)));
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
        float lumaEndN = FxaaLuma(texture2D(tex, posN));
        float subpixE = subpixC * subpixC;
        float lumaEndP = FxaaLuma(texture2D(tex, posP));
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
                if(!doneN) lumaEndN = FxaaLuma(texture2D(tex, posN.xy));
                if(!doneP) lumaEndP = FxaaLuma(texture2D(tex, posP.xy));
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
                        if(!doneN) lumaEndN = FxaaLuma(texture2D(tex, posN.xy));
                        if(!doneP) lumaEndP = FxaaLuma(texture2D(tex, posP.xy));
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
                                if(!doneN) lumaEndN = FxaaLuma(texture2D(tex, posN.xy));
                                if(!doneP) lumaEndP = FxaaLuma(texture2D(tex, posP.xy));
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
                                        if(!doneN) lumaEndN = FxaaLuma(texture2D(tex, posN.xy));
                                        if(!doneP) lumaEndP = FxaaLuma(texture2D(tex, posP.xy));
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

        return vec4(texture2D(tex, posM).xyz, rgbyM.w);
}

void main() {
#if POSTPROCESS_QUALITY >= 1
    gl_FragColor = FxaaPixelShader(
                aVertexUVs_,
                s_texture,
                texcoord_offset,
                0.25,
                0.125,//0.063,//0.166,
                0.0312//0.0625
        );
#else
    gl_FragColor = FxaaPixelShader0(aVertexUVs_, s_texture, texcoord_offset);
#endif
}
