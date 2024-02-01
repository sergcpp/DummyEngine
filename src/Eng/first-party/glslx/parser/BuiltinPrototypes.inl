R"(
// Angle and Trigonometry Functions
genFType radians(genFType degrees) [[builtin]];         genFType degrees(genFType radians) [[builtin]];
genFType sin(genFType angle) [[builtin]];               genFType cos(genFType angle) [[builtin]];
genFType tan(genFType angle) [[builtin]];
genFType asin(genFType x) [[builtin]];                  genFType acos(genFType x) [[builtin]];
genFType atan(genFType y, genFType x) [[builtin]];      genFType atan(genFType y_over_x) [[builtin]];
genFType sinh(genFType x) [[builtin]];                  genFType cosh(genFType x) [[builtin]];
genFType tanh(genFType x) [[builtin]];
genFType asinh(genFType x) [[builtin]];                 genFType acosh(genFType x) [[builtin]];
genFType atanh(genFType x) [[builtin]];

// Exponential Functions
genFType pow(genFType x, genFType y) [[builtin]];
genFType exp(genFType x) [[builtin]];                   genFType log(genFType x) [[builtin]];
genFType exp2(genFType x) [[builtin]];                  genFType log2(genFType x) [[builtin]];
genFType sqrt(genFType x) [[builtin]];                  genDType sqrt(genDType x) [[builtin]];
genFType inversesqrt(genFType x) [[builtin]];           genDType inversesqrt(genDType x) [[builtin]];

// Common Functions
genFType abs(genFType x) [[builtin]];                   genIType abs(genIType x) [[builtin]];                   genDType abs(genDType x) [[builtin]];
genFType sign(genFType x) [[builtin]];                  genIType sign(genIType x) [[builtin]];                  genDType sign(genDType x) [[builtin]];
genFType floor(genFType x) [[builtin]];                 genDType floor(genDType x) [[builtin]];
genFType trunc(genFType x) [[builtin]];                 genDType trunc(genDType x) [[builtin]];
genFType round(genFType x) [[builtin]];                 genDType round(genDType x) [[builtin]];
genFType roundEven(genFType x) [[builtin]];             genDType roundEven(genDType x) [[builtin]];
genFType ceil(genFType x) [[builtin]];                  genDType ceil(genDType x) [[builtin]];
genFType fract(genFType x) [[builtin]];                 genDType fract(genDType x) [[builtin]];
genFType mod(genFType x, float y) [[builtin]];          genFType mod(genFType x, genFType y) [[builtin]];
genDType mod(genDType x, double y) [[builtin]];         genDType mod(genDType x, genDType y) [[builtin]];
genFType modf(genFType x, out genFType i) [[builtin]];  genDType modf(genDType x, out genDType i) [[builtin]];
genFType min(genFType x, genFType y) [[builtin]];       genFType min(genFType x, float y) [[builtin]];
genDType min(genDType x, genDType y) [[builtin]];       genDType min(genDType x, double y) [[builtin]];
genIType min(genIType x, genIType y) [[builtin]];       genIType min(genIType x, int y) [[builtin]];
genUType min(genUType x, genUType y) [[builtin]];       genUType min(genUType x, uint y) [[builtin]];
genFType max(genFType x, genFType y) [[builtin]];       genFType max(genFType x, float y) [[builtin]];
genDType max(genDType x, genDType y) [[builtin]];       genDType max(genDType x, double y) [[builtin]];
genIType max(genIType x, genIType y) [[builtin]];       genIType max(genIType x, int y) [[builtin]];
genUType max(genUType x, genUType y) [[builtin]];       genUType max(genUType x, uint y) [[builtin]];
genFType clamp(genFType x, genFType minVal, genFType maxVal) [[builtin]];   genFType clamp(genFType x, float minVal, float maxVal) [[builtin]];
genDType clamp(genDType x, genDType minVal, genDType maxVal) [[builtin]];   genDType clamp(genDType x, double minVal, double maxVal) [[builtin]];
genIType clamp(genIType x, genIType minVal, genIType maxVal) [[builtin]];   genIType clamp(genIType x, int minVal, int maxVal) [[builtin]];
genUType clamp(genUType x, genUType minVal, genUType maxVal) [[builtin]];   genUType clamp(genUType x, uint minVal, uint maxVal) [[builtin]];
genFType mix(genFType x, genFType y, genFType a) [[builtin]];               genFType mix(genFType x, genFType y, float a) [[builtin]];
genDType mix(genDType x, genDType y, genDType a) [[builtin]];               genDType mix(genDType x, genDType y, double a) [[builtin]];
genFType mix(genFType x, genFType y, genBType a) [[builtin]];               genDType mix(genDType x, genDType y, genBType a) [[builtin]];
genIType mix(genIType x, genIType y, genBType a) [[builtin]];               genUType mix(genUType x, genUType y, genBType a) [[builtin]];
genBType mix(genBType x, genBType y, genBType a) [[builtin]];
genFType step(genFType edge, genFType x) [[builtin]];                       genFType step(float edge, genFType x) [[builtin]];
genDType step(genDType edge, genDType x) [[builtin]];                       genDType step(double edge, genDType x) [[builtin]];
genFType smoothstep(genFType edge0, genFType edge1, genFType x) [[builtin]]; genFType smoothstep(float edge0, float edge1, genFType x) [[builtin]];
genDType smoothstep(genDType edge0, genDType edge1, genDType x) [[builtin]]; genDType smoothstep(double edge0, double edge1, genDType x) [[builtin]];
genBType isnan(genFType x) [[builtin]];                                     genBType isnan(genDType x) [[builtin]];
genBType isinf(genFType x) [[builtin]];                                     genBType isinf(genDType x) [[builtin]];
genIType floatBitsToInt(highp genFType value) [[builtin]];                  genUType floatBitsToUint(highp genFType value) [[builtin]];
genFType intBitsToFloat(highp genIType value) [[builtin]];                  genFType uintBitsToFloat(highp genUType value) [[builtin]];
genFType fma(genFType a, genFType b, genFType c) [[builtin]];               genDType fma(genDType a, genDType b, genDType c) [[builtin]];
genFType frexp(highp genFType x, out highp genIType exp) [[builtin]];       genDType frexp(genDType x, out genIType exp) [[builtin]];
genFType ldexp(highp genFType x, highp genIType exp) [[builtin]];           genDType ldexp(genDType x, genIType exp) [[builtin]];

// Floating-Point Pack and Unpack Functions
highp uint packUnorm2x16(vec2 v) [[builtin]];                               highp uint packSnorm2x16(vec2 v) [[builtin]];
uint packUnorm4x8(vec4 v) [[builtin]];                                      uint packSnorm4x8(vec4 v) [[builtin]];
vec2 unpackUnorm2x16(highp uint p) [[builtin]];                             vec2 unpackSnorm2x16(highp uint p) [[builtin]];
vec4 unpackUnorm4x8(highp uint p) [[builtin]];                              vec4 unpackSnorm4x8(highp uint p) [[builtin]];
uint packHalf2x16( vec2 v) [[builtin]];                                     vec2 unpackHalf2x16( uint v) [[builtin]];
double packDouble2x32(uvec2 v) [[builtin]];                                 uvec2 unpackDouble2x32(double v) [[builtin]];

// Geometric Functions
float length(genFType x) [[builtin]];                                       double length(genDType x) [[builtin]];
float distance(genFType p0, genFType p1) [[builtin]];                       double distance(genDType p0, genDType p1) [[builtin]];
float dot(genFType x, genFType y) [[builtin]];                              double dot(genDType x, genDType y) [[builtin]];
vec3 cross(vec3 x, vec3 y) [[builtin]];                                     dvec3 cross(dvec3 x, dvec3 y) [[builtin]];
genFType normalize(genFType x) [[builtin]];                                 genDType normalize(genDType x) [[builtin]];
genFType faceforward(genFType N, genFType I, genFType Nref) [[builtin]];    genDType faceforward(genDType N, genDType I, genDType Nref) [[builtin]];
genFType reflect(genFType I, genFType N) [[builtin]];                       genDType reflect(genDType I, genDType N) [[builtin]];
genFType refract(genFType I, genFType N, float eta) [[builtin]];            genDType refract(genDType I, genDType N, double eta) [[builtin]];

// Matrix Functions
mat2 matrixCompMult(mat2 x, mat2 y) [[builtin]];       mat3 matrixCompMult(mat3 x, mat3 y) [[builtin]];       mat4 matrixCompMult(mat4 x, mat4 y) [[builtin]];
mat2x3 matrixCompMult(mat2x3 x, mat2x3 y) [[builtin]]; mat2x4 matrixCompMult(mat2x4 x, mat2x4 y) [[builtin]]; mat3x2 matrixCompMult(mat3x2 x, mat3x2 y) [[builtin]];
mat3x4 matrixCompMult(mat3x4 x, mat3x4 y) [[builtin]]; mat4x2 matrixCompMult(mat4x2 x, mat4x2 y) [[builtin]]; mat4x3 matrixCompMult(mat4x3 x, mat4x3 y) [[builtin]];
mat2 outerProduct(vec2 c, vec2 r) [[builtin]];     mat3 outerProduct(vec3 c, vec3 r) [[builtin]];   mat4 outerProduct(vec4 c, vec4 r) [[builtin]];
mat2x3 outerProduct(vec3 c, vec2 r) [[builtin]];   mat3x2 outerProduct(vec2 c, vec3 r) [[builtin]]; mat2x4 outerProduct(vec4 c, vec2 r) [[builtin]];
mat4x2 outerProduct(vec2 c, vec4 r) [[builtin]];   mat3x4 outerProduct(vec4 c, vec3 r) [[builtin]]; mat4x3 outerProduct(vec3 c, vec4 r) [[builtin]];
mat2 transpose(mat2 m) [[builtin]];                mat3 transpose(mat3 m) [[builtin]];              mat4 transpose(mat4 m) [[builtin]];
mat2x3 transpose(mat3x2 m) [[builtin]];            mat3x2 transpose(mat2x3 m) [[builtin]];          mat2x4 transpose(mat4x2 m) [[builtin]];
mat4x2 transpose(mat2x4 m) [[builtin]];            mat3x4 transpose(mat4x3 m) [[builtin]];          mat4x3 transpose(mat3x4 m) [[builtin]];
float determinant(mat2 m) [[builtin]];             float determinant(mat3 m) [[builtin]];           float determinant(mat4 m) [[builtin]];
mat2 inverse(mat2 m) [[builtin]];                  mat3 inverse(mat3 m) [[builtin]];                mat4 inverse(mat4 m) [[builtin]];
)"
    R"(
// Vector Relational Functions
bvec lessThan(vec x, vec y) [[builtin]];                bvec lessThan(ivec x, ivec y) [[builtin]];              bvec lessThan(uvec x, uvec y) [[builtin]];
bvec lessThanEqual(vec x, vec y) [[builtin]];           bvec lessThanEqual(ivec x, ivec y) [[builtin]];         bvec lessThanEqual(uvec x, uvec y) [[builtin]];
bvec greaterThan(vec x, vec y) [[builtin]];             bvec greaterThan(ivec x, ivec y) [[builtin]];           bvec greaterThan(uvec x, uvec y) [[builtin]];
bvec greaterThanEqual(vec x, vec y) [[builtin]];        bvec greaterThanEqual(ivec x, ivec y) [[builtin]];      bvec greaterThanEqual(uvec x, uvec y) [[builtin]];
bvec equal(vec x, vec y) [[builtin]];                   bvec equal(ivec x, ivec y) [[builtin]];                 bvec equal(uvec x, uvec y) [[builtin]];
bvec equal(bvec x, bvec y) [[builtin]];
bvec notEqual(vec x, vec y) [[builtin]];                bvec notEqual(ivec x, ivec y) [[builtin]];              bvec notEqual(uvec x, uvec y) [[builtin]];
bvec notEqual(bvec x, bvec y) [[builtin]];
bool any(bvec x) [[builtin]];                           bool all(bvec x) [[builtin]];                           bvec not(bvec x) [[builtin]];

// Integer Functions
genUType uaddCarry(highp genUType x, highp genUType y, out lowp genUType carry) [[builtin]];
genUType usubBorrow(highp genUType x, highp genUType y, out lowp genUType borrow) [[builtin]];

void umulExtended(highp genUType x, highp genUType y, out highp genUType msb, out highp genUType lsb) [[builtin]];
void imulExtended(highp genIType x, highp genIType y, out highp genIType msb, out highp genIType lsb) [[builtin]];

genIType bitfieldExtract(genIType value, int offset, int bits) [[builtin]];
genUType bitfieldExtract(genUType value, int offset, int bits) [[builtin]];

genIType bitfieldInsert(genIType base, genIType insert, int offset, int bits) [[builtin]];
genUType bitfieldInsert(genUType base, genUType insert, int offset, int bits) [[builtin]];

genIType bitfieldReverse(highp genIType value) [[builtin]];
genUType bitfieldReverse(highp genUType value) [[builtin]];

genIType bitCount(genIType value) [[builtin]];          genIType bitCount(genUType value) [[builtin]];
genIType findLSB(genIType value) [[builtin]];           genIType findLSB(genUType value) [[builtin]];
genIType findMSB(highp genIType value) [[builtin]];     genIType findMSB(highp genUType value) [[builtin]];

// Texture Functions
int textureSize(gsampler1D s, int lod) [[builtin]];     ivec2 textureSize(gsampler2D s, int lod) [[builtin]];   ivec3 textureSize(gsampler3D s, int lod) [[builtin]];
int textureSize(sampler1DShadow s, int lod) [[builtin]];
ivec2 textureSize(sampler2DShadow s, int lod) [[builtin]];
ivec2 textureSize(samplerCubeShadow s, int lod) [[builtin]];
ivec2 textureSize(gsamplerCube s, int lod) [[builtin]];
ivec3 textureSize(gsamplerCubeArray s, int lod) [[builtin]];
ivec3 textureSize(samplerCubeArrayShadow s, int lod) [[builtin]];
ivec2 textureSize(gsampler2DRect s) [[builtin]];
ivec2 textureSize(sampler2DRectShadow s) [[builtin]];
ivec2 textureSize(sampler1DArrayShadow s, int lod) [[builtin]];
ivec2 textureSize(gsampler1DArray s, int lod) [[builtin]];      ivec3 textureSize(gsampler2DArray s, int lod) [[builtin]];
ivec3 textureSize(sampler2DArrayShadow s, int lod) [[builtin]];
int textureSize(gsamplerBuffer s) [[builtin]];
ivec2 textureSize(gsampler2DMS s) [[builtin]];
ivec3 textureSize(gsampler2DMSArray s) [[builtin]];

vec2 textureQueryLod(gsampler1D s, float P) [[builtin]];                vec2 textureQueryLod(gsampler2D s, vec2 P) [[builtin]];     vec2 textureQueryLod(gsampler3D s, vec3 P) [[builtin]];
vec2 textureQueryLod(gsamplerCube s, vec3 P) [[builtin]];
vec2 textureQueryLod(gsampler1DArray s, float P) [[builtin]];           vec2 textureQueryLod(gsampler2DArray s, vec2 P) [[builtin]];
vec2 textureQueryLod(gsamplerCubeArray s, vec3 P) [[builtin]];
vec2 textureQueryLod(sampler1DShadow s, float P) [[builtin]];           vec2 textureQueryLod(sampler2DShadow s, vec2 P) [[builtin]];
vec2 textureQueryLod(samplerCubeShadow s, vec3 P) [[builtin]];
vec2 textureQueryLod(sampler1DArrayShadow s, float P) [[builtin]];      vec2 textureQueryLod(sampler2DArrayShadow s, vec2 P) [[builtin]];
vec2 textureQueryLod(samplerCubeArrayShadow s, vec3 P) [[builtin]];

int textureQueryLevels(gsampler1D s) [[builtin]];                   int textureQueryLevels(gsampler2D s) [[builtin]];               int textureQueryLevels(gsampler3D s) [[builtin]];
int textureQueryLevels(gsamplerCube s) [[builtin]];
int textureQueryLevels(gsampler1DArray s) [[builtin]];              int textureQueryLevels(gsampler2DArray s) [[builtin]];
int textureQueryLevels(gsamplerCubeArray s) [[builtin]];
int textureQueryLevels(sampler1DShadow s) [[builtin]];              int textureQueryLevels(sampler2DShadow s) [[builtin]];
int textureQueryLevels(samplerCubeShadow s) [[builtin]];
int textureQueryLevels(sampler1DArrayShadow s) [[builtin]];         int textureQueryLevels(sampler2DArrayShadow s) [[builtin]];
int textureQueryLevels(samplerCubeArrayShadow s) [[builtin]];

int textureSamples(gsampler2DMS s) [[builtin]];
int textureSamples(gsampler2DMSArray s) [[builtin]];

// Texel Lookup Functions
gvec4 texture(gsampler1D s, float P) [[builtin]];                   gvec4 texture(gsampler1D s, float P, float bias) [[builtin]];
gvec4 texture(gsampler2D s, vec2 P) [[builtin]];                    gvec4 texture(gsampler2D s, vec2 P, float bias) [[builtin]];
gvec4 texture(gsampler3D s, vec3 P) [[builtin]];                    gvec4 texture(gsampler3D s, vec3 P, float bias) [[builtin]];
gvec4 texture(gsamplerCube s, vec3 P) [[builtin]];                  gvec4 texture(gsamplerCube s, vec3 P, float bias) [[builtin]];
float texture(sampler1DShadow s, vec3 P) [[builtin]];               float texture(sampler1DShadow s, vec3 P, float bias) [[builtin]];
float texture(sampler2DShadow s, vec3 P) [[builtin]];               float texture(sampler2DShadow s, vec3 P, float bias) [[builtin]];
float texture(samplerCubeShadow s, vec4 P) [[builtin]];             float texture(samplerCubeShadow s, vec4 P, float bias) [[builtin]];
gvec4 texture(gsampler2DArray s, vec3 P) [[builtin]];               gvec4 texture(gsampler2DArray s, vec3 P, float bias) [[builtin]];
gvec4 texture(gsamplerCubeArray s, vec4 P) [[builtin]];             gvec4 texture(gsamplerCubeArray s, vec4 P, float bias) [[builtin]];
gvec4 texture(gsampler1DArray s, vec2 P) [[builtin]];               gvec4 texture(gsampler1DArray s, vec2 P, float bias) [[builtin]];
float texture(sampler1DArrayShadow s, vec3 P) [[builtin]];          float texture(sampler1DArrayShadow s, vec3 P, float bias) [[builtin]];
float texture(sampler2DArrayShadow s, vec4 P) [[builtin]];
gvec4 texture(gsampler2DRect s, vec2 P) [[builtin]];
float texture(sampler2DRectShadow s, vec3 P) [[builtin]];
float texture(samplerCubeArrayShadow s, vec4 P, float compare) [[builtin]];

gvec4 textureProj(gsampler1D s, vec2 P) [[builtin]];                gvec4 textureProj(gsampler1D s, vec2 P, float bias) [[builtin]];
gvec4 textureProj(gsampler1D s, vec4 P) [[builtin]];                gvec4 textureProj(gsampler1D s, vec4 P, float bias) [[builtin]];
gvec4 textureProj(gsampler2D s, vec3 P) [[builtin]];                gvec4 textureProj(gsampler2D s, vec3 P, float bias) [[builtin]];
gvec4 textureProj(gsampler2D s, vec4 P) [[builtin]];                gvec4 textureProj(gsampler2D s, vec4 P, float bias) [[builtin]];
gvec4 textureProj(gsampler3D s, vec4 P) [[builtin]];                gvec4 textureProj(gsampler3D s, vec4 P, float bias) [[builtin]];
float textureProj(sampler1DShadow s, vec4 P) [[builtin]];           float textureProj(sampler1DShadow s, vec4 P, float bias) [[builtin]];
float textureProj(sampler2DShadow s, vec4 P) [[builtin]];           float textureProj(sampler2DShadow s, vec4 P, float bias) [[builtin]];
gvec4 textureProj(gsampler2DRect s, vec3 P) [[builtin]];
gvec4 textureProj(gsampler2DRect s, vec4 P) [[builtin]];
float textureProj(sampler2DRectShadow s, vec4 P) [[builtin]];

gvec4 textureProj(gsampler1D s, vec2 P) [[builtin]];                gvec4 textureProj(gsampler1D s, vec2 P, float bias) [[builtin]];
gvec4 textureProj(gsampler1D s, vec4 P) [[builtin]];                gvec4 textureProj(gsampler1D s, vec4 P, float bias) [[builtin]];
gvec4 textureProj(gsampler2D s, vec3 P) [[builtin]];                gvec4 textureProj(gsampler2D s, vec3 P, float bias) [[builtin]];
gvec4 textureProj(gsampler2D s, vec4 P) [[builtin]];                gvec4 textureProj(gsampler2D s, vec4 P, float bias) [[builtin]];
gvec4 textureProj(gsampler3D s, vec4 P) [[builtin]];                gvec4 textureProj(gsampler3D s, vec4 P, float bias) [[builtin]];
float textureProj(sampler1DShadow s, vec4 P) [[builtin]];           float textureProj(sampler1DShadow s, vec4 P, float bias) [[builtin]];
float textureProj(sampler2DShadow s, vec4 P) [[builtin]];           float textureProj(sampler2DShadow s, vec4 P, float bias) [[builtin]];
gvec4 textureProj(gsampler2DRect s, vec3 P) [[builtin]];
gvec4 textureProj(gsampler2DRect s, vec4 P) [[builtin]];
float textureProj(sampler2DRectShadow s, vec4 P) [[builtin]];

gvec4 textureLod(gsampler1D s, float P, float lod) [[builtin]];
gvec4 textureLod(gsampler2D s, vec2 P, float lod) [[builtin]];
gvec4 textureLod(gsampler3D s, vec3 P, float lod) [[builtin]];
gvec4 textureLod(gsamplerCube s, vec3 P, float lod) [[builtin]];
float textureLod(sampler1DShadow s, vec3 P, float lod) [[builtin]];
float textureLod(sampler2DShadow s, vec3 P, float lod) [[builtin]];
gvec4 textureLod(gsampler1DArray s, vec2 P, float lod) [[builtin]];
float textureLod(sampler1DArrayShadow s, vec3 P, float lod) [[builtin]];
gvec4 textureLod(gsampler2DArray s, vec3 P, float lod) [[builtin]];
gvec4 textureLod(gsamplerCubeArray s, vec4 P, float lod) [[builtin]];

gvec4 textureOffset(gsampler1D s, float P, int offset) [[builtin]];
gvec4 textureOffset(gsampler1D s, float P, int offset, float bias) [[builtin]];
gvec4 textureOffset(gsampler2D s, vec2 P, ivec2 offset) [[builtin]];
gvec4 textureOffset(gsampler2D s, vec2 P, ivec2 offset, float bias) [[builtin]];
gvec4 textureOffset(gsampler3D s, vec3 P, ivec3 offset) [[builtin]];
gvec4 textureOffset(gsampler3D s, vec3 P, ivec3 offset, float bias) [[builtin]];
float textureOffset(sampler2DShadow s, vec3 P, ivec2 offset) [[builtin]];
float textureOffset(sampler2DShadow s, vec3 P, ivec2 offset, float bias) [[builtin]];
gvec4 textureOffset(gsampler2DRect s, vec2 P, ivec2 offset) [[builtin]];
float textureOffset(sampler2DRectShadow s, vec3 P, ivec2 offset) [[builtin]];
float textureOffset(sampler1DShadow s, vec3 P, int offset) [[builtin]];
float textureOffset(sampler1DShadow s, vec3 P, int offset, float bias) [[builtin]];
gvec4 textureOffset(gsampler1DArray s, vec2 P, int offset) [[builtin]];
gvec4 textureOffset(gsampler1DArray s, vec2 P, int offset, float bias) [[builtin]];
gvec4 textureOffset(gsampler2DArray s, vec3 P, ivec2 offset) [[builtin]];
gvec4 textureOffset(gsampler2DArray s, vec3 P, ivec2 offset, float bias) [[builtin]];
float textureOffset(sampler1DArrayShadow s, vec3 P, int offset) [[builtin]];
float textureOffset(sampler1DArrayShadow s, vec3 P, int offset, float bias) [[builtin]];
float textureOffset(sampler2DArrayShadow s, vec4 P, ivec2 offset) [[builtin]];

gvec4 texelFetch(gsampler1D s, int P, int lod) [[builtin]];
gvec4 texelFetch(gsampler2D s, ivec2 P, int lod) [[builtin]];
gvec4 texelFetch(gsampler3D s, ivec3 P, int lod)  [[builtin]];
gvec4 texelFetch(gsampler2DRect s, ivec2 P) [[builtin]];
gvec4 texelFetch(gsampler1DArray s, ivec2 P, int lod) [[builtin]];
gvec4 texelFetch(gsampler2DArray s, ivec3 P, int lod) [[builtin]];
gvec4 texelFetch(gsamplerBuffer s, int P) [[builtin]];
gvec4 texelFetch(gsampler2DMS s, ivec2 P, int sample) [[builtin]];
gvec4 texelFetch(gsampler2DMSArray s, ivec3 P, int sample) [[builtin]];

gvec4 texelFetchOffset(gsampler1D s, int P, int lod, int offset) [[builtin]];
gvec4 texelFetchOffset(gsampler2D s, ivec2 P, int lod, ivec2 offset) [[builtin]];
gvec4 texelFetchOffset(gsampler3D s, ivec3 P, int lod, ivec3 offset) [[builtin]];
gvec4 texelFetchOffset(gsampler2DRect s, ivec2 P, ivec2 offset) [[builtin]];
gvec4 texelFetchOffset(gsampler1DArray s, ivec2 P, int lod, int offset) [[builtin]];
gvec4 texelFetchOffset(gsampler2DArray s, ivec3 P, int lod, ivec2 offset) [[builtin]];

gvec4 textureProjOffset(gsampler1D s, vec2 P, int offset) [[builtin]];
gvec4 textureProjOffset(gsampler1D s, vec2 P, int offset, float bias) [[builtin]];
gvec4 textureProjOffset(gsampler1D s, vec4 P, int offset) [[builtin]];
gvec4 textureProjOffset(gsampler1D s, vec4 P, int offset, float bias) [[builtin]];
gvec4 textureProjOffset(gsampler2D s, vec3 P, ivec2 offset) [[builtin]];
gvec4 textureProjOffset(gsampler2D s, vec3 P, ivec2 offset, float bias) [[builtin]];
gvec4 textureProjOffset(gsampler2D s, vec4 P, ivec2 offset) [[builtin]];
gvec4 textureProjOffset(gsampler2D s, vec4 P, ivec2 offset, float bias) [[builtin]];
gvec4 textureProjOffset(gsampler3D s, vec4 P, ivec3 offset) [[builtin]];
gvec4 textureProjOffset(gsampler3D s, vec4 P, ivec3 offset, float bias) [[builtin]];
gvec4 textureProjOffset(gsampler2DRect s, vec3 P, ivec2 offset) [[builtin]];
gvec4 textureProjOffset(gsampler2DRect s, vec4 P, ivec2 offset) [[builtin]];
float textureProjOffset(sampler2DRectShadow s, vec4 P, ivec2 offset) [[builtin]];
float textureProjOffset(sampler1DShadow s, vec4 P, int offset) [[builtin]];
float textureProjOffset(sampler1DShadow s, vec4 P, int offset, float bias) [[builtin]];
float textureProjOffset(sampler2DShadow s, vec4 P, ivec2 offset) [[builtin]];
float textureProjOffset(sampler2DShadow s, vec4 P, ivec2 offset, float bias) [[builtin]];

gvec4 textureLodOffset(gsampler1D s, float P, float lod, int offset) [[builtin]];
gvec4 textureLodOffset(gsampler2D s, vec2 P, float lod, ivec2 offset) [[builtin]];
gvec4 textureLodOffset(gsampler3D s, vec3 P, float lod, ivec3 offset) [[builtin]];
float textureLodOffset(sampler1DShadow s, vec3 P, float lod, int offset) [[builtin]];
float textureLodOffset(sampler2DShadow s, vec3 P, float lod, ivec2 offset) [[builtin]];
gvec4 textureLodOffset(gsampler1DArray s, vec2 P, float lod, int offset) [[builtin]];
gvec4 textureLodOffset(gsampler2DArray s, vec3 P, float lod, ivec2 offset) [[builtin]];
float textureLodOffset(sampler1DArrayShadow s, vec3 P, float lod, int offset) [[builtin]];

gvec4 textureProjLod(gsampler1D s, vec2 P, float lod) [[builtin]];
gvec4 textureProjLod(gsampler1D s, vec4 P, float lod) [[builtin]];
gvec4 textureProjLod(gsampler2D s, vec3 P, float lod) [[builtin]];
gvec4 textureProjLod(gsampler2D s, vec4 P, float lod) [[builtin]];
gvec4 textureProjLod(gsampler3D s, vec4 P, float lod) [[builtin]];
float textureProjLod(sampler1DShadow s, vec4 P, float lod) [[builtin]];
float textureProjLod(sampler2DShadow s, vec4 P, float lod) [[builtin]];

gvec4 textureProjLodOffset(gsampler1D s, vec2 P, float lod, int offset) [[builtin]];
gvec4 textureProjLodOffset(gsampler1D s, vec4 P, float lod, int offset) [[builtin]];
gvec4 textureProjLodOffset(gsampler2D s, vec3 P, float lod, ivec2 offset) [[builtin]];
gvec4 textureProjLodOffset(gsampler2D s, vec4 P, float lod, ivec2 offset) [[builtin]];
gvec4 textureProjLodOffset(gsampler3D s, vec4 P, float lod, ivec3 offset) [[builtin]];
float textureProjLodOffset(sampler1DShadow s, vec4 P, float lod, int offset) [[builtin]];
float textureProjLodOffset(sampler2DShadow s, vec4 P, float lod, ivec2 offset) [[builtin]];
)"
    R"(
gvec4 textureGrad(gsampler1D s, float _P, float dPdx, float dPdy) [[builtin]];
gvec4 textureGrad(gsampler2D s, vec2 P, vec2 dPdx, vec2 dPdy) [[builtin]];
gvec4 textureGrad(gsampler3D s, vec3 P, vec3 dPdx, vec3 dPdy) [[builtin]];
gvec4 textureGrad(gsamplerCube s, vec3 P, vec3 dPdx, vec3 dPdy) [[builtin]];
gvec4 textureGrad(gsampler2DRect s, vec2 P, vec2 dPdx, vec2 dPdy) [[builtin]];
float textureGrad(sampler2DRectShadow s, vec3 P, vec2 dPdx, vec2 dPdy) [[builtin]];
float textureGrad(sampler1DShadow s, vec3 P, float dPdx, float dPdy) [[builtin]];
gvec4 textureGrad(gsampler1DArray s, vec2 P, float dPdx, float dPdy) [[builtin]];
gvec4 textureGrad(gsampler2DArray s, vec3 P, vec2 dPdx, vec2 dPdy) [[builtin]];
float textureGrad(sampler1DArrayShadow s, vec3 P, float dPdx, float dPdy) [[builtin]];
float textureGrad(sampler2DShadow s, vec3 P, vec2 dPdx, vec2 dPdy) [[builtin]];
float textureGrad(samplerCubeShadow s, vec4 P, vec3 dPdx, vec3 dPdy) [[builtin]];
float textureGrad(sampler2DArrayShadow s, vec4 P, vec2 dPdx, vec2 dPdy) [[builtin]];
gvec4 textureGrad(gsamplerCubeArray s, vec4 P, vec3 dPdx, vec3 dPdy) [[builtin]];

gvec4 textureGradOffset(gsampler1D s, float P, float dPdx, float dPdy, int offset) [[builtin]];
gvec4 textureGradOffset(gsampler2D s, vec2 P, vec2 dPdx, vec2 dPdy, ivec2 offset) [[builtin]];
gvec4 textureGradOffset(gsampler3D s, vec3 P, vec3 dPdx, vec3 dPdy, ivec3 offset) [[builtin]];
gvec4 textureGradOffset(gsampler2DRect s, vec2 P, vec2 dPdx, vec2 dPdy, ivec2 offset) [[builtin]];
float textureGradOffset(sampler2DRectShadow s, vec3 P, vec2 dPdx, vec2 dPdy, ivec2 offset) [[builtin]];
float textureGradOffset(sampler1DShadow s, vec3 P, float dPdx, float dPdy, int offset) [[builtin]];
float textureGradOffset(sampler2DShadow s, vec3 P, vec2 dPdx, vec2 dPdy, ivec2 offset) [[builtin]];
gvec4 textureGradOffset(gsampler2DArray s, vec3 P, vec2 dPdx, vec2 dPdy, ivec2 offset) [[builtin]];
gvec4 textureGradOffset(gsampler1DArray s, vec2 P, float dPdx, float dPdy, int offset) [[builtin]];
float textureGradOffset(sampler1DArrayShadow s, vec3 P, float dPdx, float dPdy, int offset) [[builtin]];
float textureGradOffset(sampler2DArrayShadow s, vec4 P, vec2 dPdx, vec2 dPdy, ivec2 offset) [[builtin]];

gvec4 textureProjGrad(gsampler1D s, vec2 P, float dPdx, float dPdy) [[builtin]];
gvec4 textureProjGrad(gsampler1D s, vec4 P, float dPdx, float dPdy) [[builtin]];
gvec4 textureProjGrad(gsampler2D s, vec3 P, vec2 dPdx, vec2 dPdy) [[builtin]];
gvec4 textureProjGrad(gsampler2D s, vec4 P, vec2 dPdx, vec2 dPdy) [[builtin]];
gvec4 textureProjGrad(gsampler3D s, vec4 P, vec3 dPdx, vec3 dPdy) [[builtin]];
gvec4 textureProjGrad(gsampler2DRect s, vec3 P, vec2 dPdx, vec2 dPdy) [[builtin]];
gvec4 textureProjGrad(gsampler2DRect s, vec4 P, vec2 dPdx, vec2 dPdy) [[builtin]];
float textureProjGrad(sampler2DRectShadow s, vec4 P, vec2 dPdx, vec2 dPdy) [[builtin]];
float textureProjGrad(sampler1DShadow s, vec4 P, float dPdx, float dPdy) [[builtin]];
float textureProjGrad(sampler2DShadow s, vec4 P, vec2 dPdx, vec2 dPdy) [[builtin]];

gvec4 textureProjGradOffset(gsampler1D s, vec2 P, float dPdx, float dPdy, int offset) [[builtin]];
gvec4 textureProjGradOffset(gsampler1D s, vec4 P, float dPdx, float dPdy, int offset) [[builtin]];
gvec4 textureProjGradOffset(gsampler2D s, vec3 P, vec2 dPdx, vec2 dPdy, ivec2 offset) [[builtin]];
gvec4 textureProjGradOffset(gsampler2D s, vec4 P, vec2 dPdx, vec2 dPdy, ivec2 offset) [[builtin]];
gvec4 textureProjGradOffset(gsampler3D s, vec4 P, vec3 dPdx, vec3 dPdy, ivec3 offset) [[builtin]];
gvec4 textureProjGradOffset(gsampler2DRect s, vec3 P, vec2 dPdx, vec2 dPdy, ivec2 offset) [[builtin]];
gvec4 textureProjGradOffset(gsampler2DRect s, vec4 P, vec2 dPdx, vec2 dPdy, ivec2 offset) [[builtin]];
float textureProjGradOffset(sampler2DRectShadow s, vec4 P, vec2 dPdx, vec2 dPdy, ivec2 offset) [[builtin]];
float textureProjGradOffset(sampler1DShadow s, vec4 P, float dPdx, float dPdy, int offset) [[builtin]];
float textureProjGradOffset(sampler2DShadow s, vec4 P, vec2 dPdx, vec2 dPdy, ivec2 offset) [[builtin]];

// Texture Gather Functions
gvec4 textureGather(gsampler2D s, vec2 P) [[builtin]];
gvec4 textureGather(gsampler2D s, vec2 P, int comp) [[builtin]];
gvec4 textureGather(gsampler2DArray s, vec3 P) [[builtin]];
gvec4 textureGather(gsampler2DArray s, vec3 P, int comp) [[builtin]];
gvec4 textureGather(gsamplerCube s, vec3 P) [[builtin]];
gvec4 textureGather(gsamplerCube s, vec3 P, int comp) [[builtin]];
gvec4 textureGather(gsamplerCubeArray s, vec4 P) [[builtin]];
gvec4 textureGather(gsamplerCubeArray s, vec4 P, int comp) [[builtin]];
gvec4 textureGather(gsampler2DRect s, vec2 P) [[builtin]];
gvec4 textureGather(gsampler2DRect s, vec2 P, int comp) [[builtin]];
vec4 textureGather(sampler2DShadow s, vec2 P, float refZ) [[builtin]];
vec4 textureGather(sampler2DArrayShadow s, vec3 P, float refZ) [[builtin]];
vec4 textureGather(samplerCubeShadow s, vec3 P, float refZ) [[builtin]];
vec4 textureGather(samplerCubeArrayShadow s, vec4 P, float refZ) [[builtin]];
vec4 textureGather(sampler2DRectShadow s, vec2 P, float refZ) [[builtin]];

gvec4 textureGatherOffset(gsampler2D s, vec2 P, ivec2 offset) [[builtin]];
gvec4 textureGatherOffset(gsampler2D s, vec2 P, ivec2 offset, int comp) [[builtin]];
gvec4 textureGatherOffset(gsampler2DArray s, vec3 P, ivec2 offset) [[builtin]];
gvec4 textureGatherOffset(gsampler2DArray s, vec3 P, ivec2 offset, int comp) [[builtin]];
vec4 textureGatherOffset(sampler2DShadow s, vec2 P, float refZ, ivec2 offset) [[builtin]];
vec4 textureGatherOffset(sampler2DArrayShadow s, vec3 P, float refZ, ivec2 offset) [[builtin]];
gvec4 textureGatherOffset(gsampler2DRect s, vec2 P, ivec2 offset) [[builtin]];
gvec4 textureGatherOffset(gsampler2DRect s, vec2 P, ivec2 offset, int comp) [[builtin]];
vec4 textureGatherOffset(sampler2DRectShadow s, vec2 P, float refZ, ivec2 offset) [[builtin]];

gvec4 textureGatherOffsets(gsampler2D s, vec2 P, ivec2 offsets[4]) [[builtin]];
gvec4 textureGatherOffsets(gsampler2D s, vec2 P, ivec2 offsets[4], int comp) [[builtin]];
gvec4 textureGatherOffsets(gsampler2DArray s, vec3 P, ivec2 offsets[4]) [[builtin]];
gvec4 textureGatherOffsets(gsampler2DArray s, vec3 P, ivec2 offsets[4], int comp) [[builtin]];
vec4 textureGatherOffsets(sampler2DShadow s, vec2 P, float refZ, ivec2 offsets[4]) [[builtin]];
vec4 textureGatherOffsets(sampler2DArrayShadow s, vec3 P, float refZ, ivec2 offsets[4]) [[builtin]];
gvec4 textureGatherOffsets(gsampler2DRect s, vec2 P, ivec2 offsets[4]) [[builtin]];
gvec4 textureGatherOffsets(gsampler2DRect s, vec2 P, ivec2 offsets[4], int comp) [[builtin]];
vec4 textureGatherOffsets(sampler2DRectShadow s, vec2 P, float refZ, ivec2 offsets[4]) [[builtin]];

// Atomic Memory Functions
uint atomicAdd(inout uint mem, uint data) [[builtin]];
int atomicAdd(inout int mem, int data) [[builtin]];
uint atomicMin(inout uint mem, uint data) [[builtin]];
int atomicMin(inout int mem, int data) [[builtin]];
uint atomicMax(inout uint mem, uint data) [[builtin]];
int atomicMax(inout int mem, int data) [[builtin]];
uint atomicAnd(inout uint mem, uint data) [[builtin]];
int atomicAnd(inout int mem, int data) [[builtin]];
uint atomicOr(inout uint mem, uint data) [[builtin]];
int atomicOr(inout int mem, int data) [[builtin]];
uint atomicXor(inout uint mem, uint data) [[builtin]];
int atomicXor(inout int mem, int data) [[builtin]];
uint atomicExchange(inout uint mem, uint data) [[builtin]];
int atomicExchange(inout int mem, int data) [[builtin]];
uint atomicCompSwap(inout uint mem, uint compare, uint data) [[builtin]];
int atomicCompSwap(inout int mem, int compare, int data) [[builtin]];

// Image Functions
int imageSize(readonly writeonly gimage1D image) [[builtin]];
ivec2 imageSize(readonly writeonly gimage2D image) [[builtin]];
ivec3 imageSize(readonly writeonly gimage3D image) [[builtin]];
ivec2 imageSize(readonly writeonly gimageCube image) [[builtin]];
ivec3 imageSize(readonly writeonly gimageCubeArray image) [[builtin]];
ivec3 imageSize(readonly writeonly gimage2DArray image) [[builtin]];
ivec2 imageSize(readonly writeonly gimage2DRect image) [[builtin]];
ivec2 imageSize(readonly writeonly gimage1DArray image) [[builtin]];
ivec2 imageSize(readonly writeonly gimage2DMS image) [[builtin]];
ivec3 imageSize(readonly writeonly gimage2DMSArray image) [[builtin]];
int imageSize(readonly writeonly gimageBuffer image) [[builtin]];

int imageSamples(readonly writeonly gimage2DMS image) [[builtin]];
int imageSamples(readonly writeonly gimage2DMSArray image) [[builtin]];

gvec4 imageLoad(gimage1D image, int P) [[builtin]];
gvec4 imageLoad(gimage2D image, ivec2 P) [[builtin]];
gvec4 imageLoad(gimage3D image, ivec3 P) [[builtin]];
gvec4 imageLoad(gimage2DRect image, ivec2 P) [[builtin]];
gvec4 imageLoad(gimageCube image, ivec3 P) [[builtin]];
gvec4 imageLoad(gimageBuffer image, int P) [[builtin]];
gvec4 imageLoad(gimage1DArray image, ivec2 P) [[builtin]];
gvec4 imageLoad(gimage2DArray image, ivec3 P) [[builtin]];
gvec4 imageLoad(gimageCubeArray image, ivec3 P) [[builtin]];
gvec4 imageLoad(gimage2DMS image, ivec2 P, int sample) [[builtin]];
gvec4 imageLoad(gimage2DMSArray image, ivec3 P, int sample) [[builtin]];

void imageStore(gimage1D image, int P, gvec4 data) [[builtin]];
void imageStore(gimage2D image, ivec2 P, gvec4 data) [[builtin]];
void imageStore(gimage3D image, ivec3 P, gvec4 data) [[builtin]];
void imageStore(gimage2DRect image, ivec2 P, gvec4 data) [[builtin]];
void imageStore(gimageCube image, ivec3 P, gvec4 data) [[builtin]];
void imageStore(gimageBuffer image, int P, gvec4 data) [[builtin]];
void imageStore(gimage1DArray image, ivec2 P, gvec4 data) [[builtin]];
void imageStore(gimage2DArray image, ivec3 P, gvec4 data) [[builtin]];
void imageStore(gimageCubeArray image, ivec3 P, gvec4 data) [[builtin]];
void imageStore(gimage2DMS image, ivec2 P, int sample, gvec4 data) [[builtin]];
void imageStore(gimage2DMSArray image, ivec3 P, int sample, gvec4 data) [[builtin]];

uint imageAtomicAdd(gimage1D image, int P, uint data) [[builtin]];
uint imageAtomicAdd(gimage2D image, ivec2 P, uint data) [[builtin]];
uint imageAtomicAdd(gimage3D image, ivec3 P, uint data) [[builtin]];
uint imageAtomicAdd(gimage2DRect image, ivec2 P, uint data) [[builtin]];
uint imageAtomicAdd(gimageCube image, ivec3 P, uint data) [[builtin]];
uint imageAtomicAdd(gimageBuffer image, int P, uint data) [[builtin]];
uint imageAtomicAdd(gimage1DArray image, ivec2 P, uint data) [[builtin]];
uint imageAtomicAdd(gimage2DArray image, ivec3 P, uint data) [[builtin]];
uint imageAtomicAdd(gimageCubeArray image, ivec3 P, uint data) [[builtin]];
uint imageAtomicAdd(gimage2DMS image, ivec2 P, int sample, uint data) [[builtin]];
uint imageAtomicAdd(gimage2DMSArray image, ivec3 P, int sample, uint data) [[builtin]];
int imageAtomicAdd(gimage1D image, int P, int data) [[builtin]];
int imageAtomicAdd(gimage2D image, ivec2 P, int data) [[builtin]];
int imageAtomicAdd(gimage3D image, ivec3 P, int data) [[builtin]];
int imageAtomicAdd(gimage2DRect image, ivec2 P, int data) [[builtin]];
int imageAtomicAdd(gimageCube image, ivec3 P, int data) [[builtin]];
int imageAtomicAdd(gimageBuffer image, int P, int data) [[builtin]];
int imageAtomicAdd(gimage1DArray image, ivec2 P, int data) [[builtin]];
int imageAtomicAdd(gimage2DArray image, ivec3 P, int data) [[builtin]];
int imageAtomicAdd(gimageCubeArray image, ivec3 P, int data) [[builtin]];
int imageAtomicAdd(gimage2DMS image, ivec2 P, int sample, int data) [[builtin]];
int imageAtomicAdd(gimage2DMSArray image, ivec3 P, int sample, int data) [[builtin]];

uint imageAtomicMin(gimage1D image, int P, uint data) [[builtin]];
uint imageAtomicMin(gimage2D image, ivec2 P, uint data) [[builtin]];
uint imageAtomicMin(gimage3D image, ivec3 P, uint data) [[builtin]];
uint imageAtomicMin(gimage2DRect image, ivec2 P, uint data) [[builtin]];
uint imageAtomicMin(gimageCube image, ivec3 P, uint data) [[builtin]];
uint imageAtomicMin(gimageBuffer image, int P, uint data) [[builtin]];
uint imageAtomicMin(gimage1DArray image, ivec2 P, uint data) [[builtin]];
uint imageAtomicMin(gimage2DArray image, ivec3 P, uint data) [[builtin]];
uint imageAtomicMin(gimageCubeArray image, ivec3 P, uint data) [[builtin]];
uint imageAtomicMin(gimage2DMS image, ivec2 P, int sample, uint data) [[builtin]];
uint imageAtomicMin(gimage2DMSArray image, ivec3 P, int sample, uint data) [[builtin]];
int imageAtomicMin(gimage1D image, int P, int data) [[builtin]];
int imageAtomicMin(gimage2D image, ivec2 P, int data) [[builtin]];
int imageAtomicMin(gimage3D image, ivec3 P, int data) [[builtin]];
int imageAtomicMin(gimage2DRect image, ivec2 P, int data) [[builtin]];
int imageAtomicMin(gimageCube image, ivec3 P, int data) [[builtin]];
int imageAtomicMin(gimageBuffer image, int P, int data) [[builtin]];
int imageAtomicMin(gimage1DArray image, ivec2 P, int data) [[builtin]];
int imageAtomicMin(gimage2DArray image, ivec3 P, int data) [[builtin]];
int imageAtomicMin(gimageCubeArray image, ivec3 P, int data) [[builtin]];
int imageAtomicMin(gimage2DMS image, ivec2 P, int sample, int data) [[builtin]];
int imageAtomicMin(gimage2DMSArray image, ivec3 P, int sample, int data) [[builtin]];

uint imageAtomicMax(gimage1D image, int P, uint data) [[builtin]];
uint imageAtomicMax(gimage2D image, ivec2 P, uint data) [[builtin]];
uint imageAtomicMax(gimage3D image, ivec3 P, uint data) [[builtin]];
uint imageAtomicMax(gimage2DRect image, ivec2 P, uint data) [[builtin]];
uint imageAtomicMax(gimageCube image, ivec3 P, uint data) [[builtin]];
uint imageAtomicMax(gimageBuffer image, int P, uint data) [[builtin]];
uint imageAtomicMax(gimage1DArray image, ivec2 P, uint data) [[builtin]];
uint imageAtomicMax(gimage2DArray image, ivec3 P, uint data) [[builtin]];
uint imageAtomicMax(gimageCubeArray image, ivec3 P, uint data) [[builtin]];
uint imageAtomicMax(gimage2DMS image, ivec2 P, int sample, uint data) [[builtin]];
uint imageAtomicMax(gimage2DMSArray image, ivec3 P, int sample, uint data) [[builtin]];
int imageAtomicMax(gimage1D image, int P, int data) [[builtin]];
int imageAtomicMax(gimage2D image, ivec2 P, int data) [[builtin]];
int imageAtomicMax(gimage3D image, ivec3 P, int data) [[builtin]];
int imageAtomicMax(gimage2DRect image, ivec2 P, int data) [[builtin]];
int imageAtomicMax(gimageCube image, ivec3 P, int data) [[builtin]];
int imageAtomicMax(gimageBuffer image, int P, int data) [[builtin]];
int imageAtomicMax(gimage1DArray image, ivec2 P, int data) [[builtin]];
int imageAtomicMax(gimage2DArray image, ivec3 P, int data) [[builtin]];
int imageAtomicMax(gimageCubeArray image, ivec3 P, int data) [[builtin]];
int imageAtomicMax(gimage2DMS image, ivec2 P, int sample, int data) [[builtin]];
int imageAtomicMax(gimage2DMSArray image, ivec3 P, int sample, int data) [[builtin]];
)"
    R"(
uint imageAtomicAnd(gimage1D image, int P, uint data) [[builtin]];
uint imageAtomicAnd(gimage2D image, ivec2 P, uint data) [[builtin]];
uint imageAtomicAnd(gimage3D image, ivec3 P, uint data) [[builtin]];
uint imageAtomicAnd(gimage2DRect image, ivec2 P, uint data) [[builtin]];
uint imageAtomicAnd(gimageCube image, ivec3 P, uint data) [[builtin]];
uint imageAtomicAnd(gimageBuffer image, int P, uint data) [[builtin]];
uint imageAtomicAnd(gimage1DArray image, ivec2 P, uint data) [[builtin]];
uint imageAtomicAnd(gimage2DArray image, ivec3 P, uint data) [[builtin]];
uint imageAtomicAnd(gimageCubeArray image, ivec3 P, uint data) [[builtin]];
uint imageAtomicAnd(gimage2DMS image, ivec2 P, int sample, uint data) [[builtin]];
uint imageAtomicAnd(gimage2DMSArray image, ivec3 P, int sample, uint data) [[builtin]];
int imageAtomicAnd(gimage1D image, int P, int data) [[builtin]];
int imageAtomicAnd(gimage2D image, ivec2 P, int data) [[builtin]];
int imageAtomicAnd(gimage3D image, ivec3 P, int data) [[builtin]];
int imageAtomicAnd(gimage2DRect image, ivec2 P, int data) [[builtin]];
int imageAtomicAnd(gimageCube image, ivec3 P, int data) [[builtin]];
int imageAtomicAnd(gimageBuffer image, int P, int data) [[builtin]];
int imageAtomicAnd(gimage1DArray image, ivec2 P, int data) [[builtin]];
int imageAtomicAnd(gimage2DArray image, ivec3 P, int data) [[builtin]];
int imageAtomicAnd(gimageCubeArray image, ivec3 P, int data) [[builtin]];
int imageAtomicAnd(gimage2DMS image, ivec2 P, int sample, int data) [[builtin]];
int imageAtomicAnd(gimage2DMSArray image, ivec3 P, int sample, int data) [[builtin]];

uint imageAtomicOr(gimage1D image, int P, uint data) [[builtin]];
uint imageAtomicOr(gimage2D image, ivec2 P, uint data) [[builtin]];
uint imageAtomicOr(gimage3D image, ivec3 P, uint data) [[builtin]];
uint imageAtomicOr(gimage2DRect image, ivec2 P, uint data) [[builtin]];
uint imageAtomicOr(gimageCube image, ivec3 P, uint data) [[builtin]];
uint imageAtomicOr(gimageBuffer image, int P, uint data) [[builtin]];
uint imageAtomicOr(gimage1DArray image, ivec2 P, uint data) [[builtin]];
uint imageAtomicOr(gimage2DArray image, ivec3 P, uint data) [[builtin]];
uint imageAtomicOr(gimageCubeArray image, ivec3 P, uint data) [[builtin]];
uint imageAtomicOr(gimage2DMS image, ivec2 P, int sample, uint data) [[builtin]];
uint imageAtomicOr(gimage2DMSArray image, ivec3 P, int sample, uint data) [[builtin]];
int imageAtomicOr(gimage1D image, int P, int data) [[builtin]];
int imageAtomicOr(gimage2D image, ivec2 P, int data) [[builtin]];
int imageAtomicOr(gimage3D image, ivec3 P, int data) [[builtin]];
int imageAtomicOr(gimage2DRect image, ivec2 P, int data) [[builtin]];
int imageAtomicOr(gimageCube image, ivec3 P, int data) [[builtin]];
int imageAtomicOr(gimageBuffer image, int P, int data) [[builtin]];
int imageAtomicOr(gimage1DArray image, ivec2 P, int data) [[builtin]];
int imageAtomicOr(gimage2DArray image, ivec3 P, int data) [[builtin]];
int imageAtomicOr(gimageCubeArray image, ivec3 P, int data) [[builtin]];
int imageAtomicOr(gimage2DMS image, ivec2 P, int sample, int data) [[builtin]];
int imageAtomicOr(gimage2DMSArray image, ivec3 P, int sample, int data) [[builtin]];

uint imageAtomicXor(gimage1D image, int P, uint data) [[builtin]];
uint imageAtomicXor(gimage2D image, ivec2 P, uint data) [[builtin]];
uint imageAtomicXor(gimage3D image, ivec3 P, uint data) [[builtin]];
uint imageAtomicXor(gimage2DRect image, ivec2 P, uint data) [[builtin]];
uint imageAtomicXor(gimageCube image, ivec3 P, uint data) [[builtin]];
uint imageAtomicXor(gimageBuffer image, int P, uint data) [[builtin]];
uint imageAtomicXor(gimage1DArray image, ivec2 P, uint data) [[builtin]];
uint imageAtomicXor(gimage2DArray image, ivec3 P, uint data) [[builtin]];
uint imageAtomicXor(gimageCubeArray image, ivec3 P, uint data) [[builtin]];
uint imageAtomicXor(gimage2DMS image, ivec2 P, int sample, uint data) [[builtin]];
uint imageAtomicXor(gimage2DMSArray image, ivec3 P, int sample, uint data) [[builtin]];
int imageAtomicXor(gimage1D image, int P, int data) [[builtin]];
int imageAtomicXor(gimage2D image, ivec2 P, int data) [[builtin]];
int imageAtomicXor(gimage3D image, ivec3 P, int data) [[builtin]];
int imageAtomicXor(gimage2DRect image, ivec2 P, int data) [[builtin]];
int imageAtomicXor(gimageCube image, ivec3 P, int data) [[builtin]];
int imageAtomicXor(gimageBuffer image, int P, int data) [[builtin]];
int imageAtomicXor(gimage1DArray image, ivec2 P, int data) [[builtin]];
int imageAtomicXor(gimage2DArray image, ivec3 P, int data) [[builtin]];
int imageAtomicXor(gimageCubeArray image, ivec3 P, int data) [[builtin]];
int imageAtomicXor(gimage2DMS image, ivec2 P, int sample, int data) [[builtin]];
int imageAtomicXor(gimage2DMSArray image, ivec3 P, int sample, int data) [[builtin]];

uint imageAtomicExchange(gimage1D image, int P, uint data) [[builtin]];
uint imageAtomicExchange(gimage2D image, ivec2 P, uint data) [[builtin]];
uint imageAtomicExchange(gimage3D image, ivec3 P, uint data) [[builtin]];
uint imageAtomicExchange(gimage2DRect image, ivec2 P, uint data) [[builtin]];
uint imageAtomicExchange(gimageCube image, ivec3 P, uint data) [[builtin]];
uint imageAtomicExchange(gimageBuffer image, int P, uint data) [[builtin]];
uint imageAtomicExchange(gimage1DArray image, ivec2 P, uint data) [[builtin]];
uint imageAtomicExchange(gimage2DArray image, ivec3 P, uint data) [[builtin]];
uint imageAtomicExchange(gimageCubeArray image, ivec3 P, uint data) [[builtin]];
uint imageAtomicExchange(gimage2DMS image, ivec2 P, int sample, uint data) [[builtin]];
uint imageAtomicExchange(gimage2DMSArray image, ivec3 P, int sample, uint data) [[builtin]];
int imageAtomicExchange(gimage1D image, int P, int data) [[builtin]];
int imageAtomicExchange(gimage2D image, ivec2 P, int data) [[builtin]];
int imageAtomicExchange(gimage3D image, ivec3 P, int data) [[builtin]];
int imageAtomicExchange(gimage2DRect image, ivec2 P, int data) [[builtin]];
int imageAtomicExchange(gimageCube image, ivec3 P, int data) [[builtin]];
int imageAtomicExchange(gimageBuffer image, int P, int data) [[builtin]];
int imageAtomicExchange(gimage1DArray image, ivec2 P, int data) [[builtin]];
int imageAtomicExchange(gimage2DArray image, ivec3 P, int data) [[builtin]];
int imageAtomicExchange(gimageCubeArray image, ivec3 P, int data) [[builtin]];
int imageAtomicExchange(gimage2DMS image, ivec2 P, int sample, int data) [[builtin]];
int imageAtomicExchange(gimage2DMSArray image, ivec3 P, int sample, int data) [[builtin]];
int imageAtomicExchange(gimage1D image, int P, float data) [[builtin]];
int imageAtomicExchange(gimage2D image, ivec2 P, float data) [[builtin]];
int imageAtomicExchange(gimage3D image, ivec3 P, float data) [[builtin]];
int imageAtomicExchange(gimage2DRect image, ivec2 P, float data) [[builtin]];
int imageAtomicExchange(gimageCube image, ivec3 P, float data) [[builtin]];
int imageAtomicExchange(gimageBuffer image, int P, float data) [[builtin]];
int imageAtomicExchange(gimage1DArray image, ivec2 P, float data) [[builtin]];
int imageAtomicExchange(gimage2DArray image, ivec3 P, float data) [[builtin]];
int imageAtomicExchange(gimageCubeArray image, ivec3 P, float data) [[builtin]];
int imageAtomicExchange(gimage2DMS image, ivec2 P, int sample, float data) [[builtin]];
int imageAtomicExchange(gimage2DMSArray image, ivec3 P, int sample, float data) [[builtin]];

uint imageAtomicCompSwap(gimage1D image, int P, uint compare, uint data) [[builtin]];
uint imageAtomicCompSwap(gimage2D image, ivec2 P, uint compare, uint data) [[builtin]];
uint imageAtomicCompSwap(gimage3D image, ivec3 P, uint compare, uint data) [[builtin]];
uint imageAtomicCompSwap(gimage2DRect image, ivec2 P, uint compare, uint data) [[builtin]];
uint imageAtomicCompSwap(gimageCube image, ivec3 P, uint compare, uint data) [[builtin]];
uint imageAtomicCompSwap(gimageBuffer image, int P, uint compare, uint data) [[builtin]];
uint imageAtomicCompSwap(gimage1DArray image, ivec2 P, uint compare, uint data) [[builtin]];
uint imageAtomicCompSwap(gimage2DArray image, ivec3 P, uint compare, uint data) [[builtin]];
uint imageAtomicCompSwap(gimageCubeArray image, ivec3 P, uint compare, uint data) [[builtin]];
uint imageAtomicCompSwap(gimage2DMS image, ivec2 P, int sample, uint compare, uint data) [[builtin]];
uint imageAtomicCompSwap(gimage2DMSArray image, ivec3 P, int sample, uint compare, uint data) [[builtin]];
int imageAtomicCompSwap(gimage1D image, int P, int compare, int data) [[builtin]];
int imageAtomicCompSwap(gimage2D image, ivec2 P, int compare, int data) [[builtin]];
int imageAtomicCompSwap(gimage3D image, ivec3 P, int compare, int data) [[builtin]];
int imageAtomicCompSwap(gimage2DRect image, ivec2 P, int compare, int data) [[builtin]];
int imageAtomicCompSwap(gimageCube image, ivec3 P, int compare, int data) [[builtin]];
int imageAtomicCompSwap(gimageBuffer image, int P, int compare, int data) [[builtin]];
int imageAtomicCompSwap(gimage1DArray image, ivec2 P, int compare, int data) [[builtin]];
int imageAtomicCompSwap(gimage2DArray image, ivec3 P, int compare, int data) [[builtin]];
int imageAtomicCompSwap(gimageCubeArray image, ivec3 P, int compare, int data) [[builtin]];
int imageAtomicCompSwap(gimage2DMS image, ivec2 P, int sample, int compare, int data) [[builtin]];
int imageAtomicCompSwap(gimage2DMSArray image, ivec3 P, int sample, int compare, int data) [[builtin]];

// Geometry Shader Functions
void EmitStreamVertex(int stream) [[builtin]];
void EndStreamPrimitive(int stream) [[builtin]];
void EmitVertex() [[builtin]];
void EndPrimitive() [[builtin]];

// Derivative Functions
genFType dFdx(genFType p) [[builtin]];
genFType dFdy(genFType p) [[builtin]];
genFType dFdxFine(genFType p) [[builtin]];
genFType dFdyFine(genFType p) [[builtin]];
genFType dFdxCoarse(genFType p) [[builtin]];
genFType dFdyCoarse(genFType p) [[builtin]];
genFType fwidth(genFType p) [[builtin]];
genFType fwidthFine(genFType p) [[builtin]];
genFType fwidthCoarse(genFType p) [[builtin]];

// Interpolation Functions
float interpolateAtCentroid(float interpolant) [[builtin]];
vec2 interpolateAtCentroid(vec2 interpolant) [[builtin]];
vec3 interpolateAtCentroid(vec3 interpolant) [[builtin]];
vec4 interpolateAtCentroid(vec4 interpolant) [[builtin]];
float interpolateAtSample(float interpolant, int s) [[builtin]];
vec2 interpolateAtSample(vec2 interpolant, int s) [[builtin]];
vec3 interpolateAtSample(vec3 interpolant, int s) [[builtin]];
vec4 interpolateAtSample(vec4 interpolant, int s) [[builtin]];
float interpolateAtOffset(float interpolant, vec2 offset) [[builtin]];
vec2 interpolateAtOffset(vec2 interpolant, vec2 offset) [[builtin]];
vec3 interpolateAtOffset(vec3 interpolant, vec2 offset) [[builtin]];
vec4 interpolateAtOffset(vec4 interpolant, vec2 offset) [[builtin]];

// Shader Invocation Control Functions
void barrier() [[builtin]];

// Shader Memory Control Functions
void memoryBarrier() [[builtin]];
void memoryBarrierAtomicCounter() [[builtin]];
void memoryBarrierBuffer() [[builtin]];
void memoryBarrierShared() [[builtin]];
void memoryBarrierImage() [[builtin]];
void groupMemoryBarrier() [[builtin]];

// Subpass-Input Functions
gvec4 subpassLoad(gsubpassInput subpass) [[builtin]];
gvec4 subpassLoad(gsubpassInputMS subpass, int s) [[builtin]];

// Shader Invocation Group Functions
bool anyInvocation(bool value) [[builtin]];
bool allInvocations(bool value) [[builtin]];
bool allInvocationsEqual(bool value) [[builtin]];
)"


