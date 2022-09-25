#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

struct Vertex {
    vec3 inPosition;
    vec3 inNormal;
    vec2 inTexCoord;
};

struct Addresses {
    uint64_t vertexAddress;
    uint64_t indexAddress;
};

hitAttributeEXT vec2 attribs;

layout(buffer_reference, scalar) buffer Vertices {Vertex v[]; };
layout(buffer_reference, scalar) buffer Indices {ivec3 i[]; };

layout(location = 0) rayPayloadInEXT hitpayload{ vec3 hitValue; } prd;
layout(location = 1) rayPayloadEXT bool isShadowed;
layout(location = 2) rayPayloadEXT hitpayload{ vec3 hitValue; float len; } relfect;

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(set = 1, binding = 0) uniform matrices {
    mat4 viewProj;
    mat4 viewInverse;
    mat4 projInverse;
} ubo;
layout(set = 1, binding = 1, scalar) buffer addresses {Addresses a[];} addr;
layout(set = 2, binding = 0) uniform samplerCube Cubemap;
layout(set = 3, binding = 0) uniform sampler3D ltc1;
layout(set = 3, binding = 1) uniform sampler3D ltc2;
layout(set = 3, binding = 2) uniform sampler3D ltc3;

layout(push_constant) uniform constants {
    vec3 position;
    float intensity;
} light;

vec3 ownColor = vec3(0.8, 0.8, 0.8);
float e = 2.71828, pi = 3.1415926535897932384626433832795;

float cos_theta(const vec3 w)       { return w.z; }
float cos_2_theta(const vec3 w)     { return w.z*w.z; }
float sin_2_theta(const vec3 w)     { return max(0., 1. - cos_2_theta(w)); }
float sin_theta(const vec3 w)       { return sqrt(sin_2_theta(w)); }
float tan_theta(const vec3 w)       { return sin_theta(w) / cos_theta(w); }
float cos_phi(const vec3 w)         { return (sin_theta(w) == 0.) ? 1. : clamp(w.x / sin_theta(w), -1., 1.); }
float sin_phi(const vec3 w)         { return (sin_theta(w) == 0.) ? 0. : clamp(w.y / sin_theta(w), -1., 1.); }
float cos_2_phi(const vec3 w)       { return cos_phi(w) * cos_phi(w); }
float sin_2_phi(const vec3 w)       { return sin_phi(w) * sin_phi(w); }

float random( vec2 p ) {
    vec2 K1 = vec2(
        23.14069263277926, // e^pi
         2.665144142690225 // 2^sqrt(2)
    );
    return fract( cos( dot(p,K1) ) * 12345.6789 );
}

vec3 getPerpendicularVector(vec3 u)
{
    vec3 a = abs(u);
    uint xm = ((a.x - a.y)<0 && (a.x - a.z)<0) ? 1 : 0;
    uint ym = (a.y - a.z)<0 ? (1 ^ xm) : 0;
    uint zm = 1 ^ (xm | ym);
    return cross(u, vec3(xm, ym, zm));
}

vec3 schlickFresnel(float LdotH, float roughness) {
    float ior = 0.18104 ; // indice of refraction for gold
    vec3 f0 = vec3(pow(ior - 1, 2) / pow(ior + 1, 2));
    f0 = mix(f0, ownColor, roughness);
    return f0 + (vec3(1.0) - f0) * pow(1.0 - LdotH, 5);
}

vec3 cookTorrance(vec3 L, vec3 E, vec3 N, float metalicness) {
    vec3 H = normalize(L + E);
    float cosAlpha = dot(N, H);
    float cosTheta = dot(E, H);

    // D
    float eExponent = pow(tan(acos(cosAlpha)) / metalicness, 2) * -1;
    float D = (1.0 / (pow(metalicness, 2) * pow(cosAlpha, 4))) * pow(e, eExponent);

    // G
    float G = min(min(1.0, (2.0 * cosAlpha * dot(N, E) / cosTheta)), (2.0 * cosAlpha * dot(N, L) / cosTheta));
    G = clamp(G, 0, 1);

    // F using Schlick approx
    vec3 fresnel = schlickFresnel(cosTheta, metalicness);

    return (fresnel / pi) * (D * G / (dot(N, L) * dot(N, E)));
}

float ggxNormalDistribution(float NdotH, float roughness) {
    float a2 = roughness * roughness;
    float d = ((NdotH * a2 - NdotH) * NdotH + 1);
    if (d == 0) {
        d = 0.0000000001;
    }
    return a2 / (d * d * pi);
}

float ggxP22Anisotropic(float x, float y, float ax, float ay) {
    float x2 = x * x;
    float y2 = y * y;
    float ax2 = ax * ax;
    float ay2 = ay * ay;
    float denom = 1.0 + (x2 / ax2) + (y2 / ay2);
    float denom2 = denom * denom;
    return 1.0 / (pi * ax * ay * denom2);
}

float ggxNDFanisotropic(vec3 omegaH, float ax, float ay) {
    float slopeX = -(omegaH.x / omegaH.z);
    float slopeY = -(omegaH.y / omegaH.z);
    float cos_theta = cos_theta(omegaH);
    float cos_4_theta = cos_theta * cos_theta * cos_theta * cos_theta;
    float ggxP22 = ggxP22Anisotropic(slopeX, slopeY, ax, ay);
    return ggxP22 / cos_4_theta;
}

float schlickMaskingTerm(float NdotL, float NdotE, float roughness) {
    float k = roughness * roughness / 2;
    float g_v = NdotE / (NdotE * (1 - k) + k);
    float g_l = NdotL / (NdotL * (1 - k) + k);
    return g_v * g_l;
}

vec3 getGGXmicrofacet(uint seed, float roughness, vec3 N) {
    /*vec2 randVal = vec2(nextRand(seed), nextRand(seed));

    vec3 B = getPerpendicularVector(N);
    vec3 T = cross(B, N);

    float a2 = roughness * roughness;
    float cosThetaH = sqrt(max(0.0f, (1.0 - randVal.x) / ((a2 - 1.0) * randVal.x + 1)));
    float sinThetaH = sqrt(max(0.0f, 1.0 - cosThetaH * cosThetaH));
    float phiH = randVal.y * pi * 2.0;

    return T * (sinThetaH * cos(phiH)) +
           B * (sinThetaH * sin(phiH)) +
           N * cosThetaH;
           */
           return vec3(0.0);
}

vec3 ggx(uint seed, vec3 L, vec3 E, vec3 N, float roughness) {
    vec3 H = normalize(N + E);
    float NdotH = clamp(dot(N, H), 0.0, 1.0);
    float NdotL = clamp(dot(N, L), 0.0, 1.0);
    float NdotE = clamp(dot(N, E), 0.0, 1.0);
    float LdotH = clamp(dot(L, H), 0.0, 1.0);

    float D = ggxNormalDistribution(NdotH, roughness);
    float G = schlickMaskingTerm(NdotL, NdotE, roughness);
    vec3 F = schlickFresnel(LdotH, 0.6 /*metalicness*/);

    return D * G * F / (4 * NdotE);
}

mat3 orthonormalBasis(vec3 N) {
    vec3 f, r;
    if (N.z < -0.999999) {
        f = vec3(0, -1, 0);
        r = vec3(-1, 0, 0);
    } else {
        float a = 1.0 / (1.0 + N.z);
        float b = -N.x * N.y * a;
        f = normalize(vec3(1.0 - N.x * N.x * a, b, -N.x));
        r = normalize(vec3(b, 1 - N.y * N.y * a, -N.y));
    }
    return mat3(f, r, N);
}

float lambdaGGXanisotropic(vec3 omega, float ax, float ay) {
    float cos_phi = cos_phi(omega);
    float sin_phi = sin_phi(omega);
    float alpha_o = sqrt(cos_phi * cos_phi * ax * ax + sin_phi * sin_phi * ay * ay);
    float a = 1.0 / (alpha_o * tan_theta(omega));
    return 0.5 * (-1.0 + sqrt(1.0 + 1.0 / (a*a)));
}

vec3 anisotropicGGX(vec3 L, vec3 E, vec3 N, float roughnessX, float roughnessY) {
    mat3 TBN = orthonormalBasis(N);
    mat3 TBN_t = transpose(TBN);
    vec3 wo = normalize(TBN_t * E);
    vec3 wi = normalize(TBN_t * L);
    vec3 wg = normalize(TBN_t * N);
    vec3 wh = normalize(wo + wi);

    float wi_dot_wh = clamp(dot(wi, wh), 0.0, 1.0);
    float wg_dot_wi = clamp(cos_theta(wi), 0.0, 1.0);

    float ax = roughnessX * roughnessY;
    float ay = roughnessY * roughnessY;

    vec3 F = schlickFresnel(wi_dot_wh, 0.6 /*metalicness*/);
    float lambda_wo = lambdaGGXanisotropic(wo, ax, ay);
    float lambda_wi = lambdaGGXanisotropic(wi, ax, ay);
    float D = ggxNDFanisotropic(wh, ax, ay);
    float G = 1.0 / (1.0 + lambda_wo + lambda_wi);

    return wg_dot_wi * vec3(D * G /* F*/) / (4.0 * cos_theta(wi) * cos_theta(wo));
}


mat3 Lerp(mat3 a, mat3 b, float u)
{
    return a + u * (b - a);
}

mat3 FetchData_Tex3D(vec3 P)
{
    const vec3 m0 = texture(ltc1, P).rgb;
    const vec3 m1 = texture(ltc2, P).rgb;
    const vec3 m2 = texture(ltc3, P).rgb;

    return transpose(mat3(m0, m1, m2));
}

mat3 LtcMatrix_Tex3D(vec4 P)
{
    const float ws = P.w * 7.0f;
    const float ws_f = floor(ws       );
    const float ws_c = min(floor(ws + 1.0f), 7.0f);
    const float w = fract(ws);

    const float x = (P.x * 7.0 + 0.5) / 8.0;
    const float y = (P.y * 7.0 + 0.5) / 8.0;
    const float z1 = ((P.z * 7.0 + 8.0 * ws_f + 0.5) / 64.0);
    const float z2 = ((P.z * 7.0 + 8.0 * ws_c + 0.5) / 64.0);

    const mat3 m1 = FetchData_Tex3D(vec3(x, y, z1));
    const mat3 m2 = FetchData_Tex3D(vec3(x, y, z2));

    return Lerp(m1, m2, w);
}

void LtcMatrix(vec3 wo, mat3 TBN, float alphaX, float alphaY, out mat3 ltcMatrix) {
    float thetaO = acos(wo.z);
    bool flipConfig = alphaY > alphaX;
    float phiO_tmp = atan(wo.y, wo.x);
    float phiO_tmp2 = flipConfig ? pi / 2.0 - phiO_tmp : phiO_tmp;
    float phiO = phiO_tmp2 >= 0.0 ? phiO_tmp2 : phiO_tmp2 + 2.0 * pi;
    float u0 = ((flipConfig ? alphaY : alphaX) - 1e-3) / (1.0 - 1e-3);
    float u1 = (flipConfig ? alphaX / alphaY : alphaY / alphaX);
    float u2 = thetaO / pi * 2;

    if (phiO < pi * 0.5) {
        float u3 = phiO / (pi * 0.5);
        vec4 u = vec4(u3 / 1.0, u2, u1, u0);

        ltcMatrix = LtcMatrix_Tex3D(u);
    } else if (phiO < pi) {
        float u3 = (phiO - pi) / (pi * 0.5);
        const vec4 u = vec4(u3 / 1.0f, u2, u1, u0);
        const mat3 flip = mat3(-1.0, 0.0, 0.0,
                                0.0, 1.0, 0.0,
                                0.0, 0.0, 1.0);

        ltcMatrix = flip * LtcMatrix_Tex3D(u);
    } else if (phiO < pi * 1.5) {
        const float u3 = (phiO - pi) / (pi * 0.5f);
        const vec4 u = vec4(u3 / 1.0f, u2, u1, u0);
        const mat3 flip = mat3(-1.0, 0.0, 0.0,
                                0.0, -1.0, 0.0,
                                0.0, 0.0, 1.0);

        ltcMatrix = flip * LtcMatrix_Tex3D(u);
    } else if (phiO < pi * 2.0) {
        const float u3 = (2.0f * pi - phiO) / (pi * 0.5f);
        const vec4 u = vec4(u3 / 1.0f, u2, u1, u0);
        const mat3 flip = mat3(1.0, 0.0, 0.0,
                               0.0, -1.0, 0.0,
                               0.0, 0.0, 1.0);

        ltcMatrix = flip * LtcMatrix_Tex3D(u);
    }
}

void main() {
float alphaX = 0.05;
float alphaY = 0.5;
    Addresses  address = addr.a[gl_InstanceCustomIndexEXT];
    Indices    indices     = Indices(address.indexAddress);
    Vertices   vertices    = Vertices(address.vertexAddress);

    ivec3 ind = indices.i[gl_PrimitiveID];

    Vertex v0 = vertices.v[ind.x];
    Vertex v1 = vertices.v[ind.y];
    Vertex v2 = vertices.v[ind.z];

    const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

    const vec3 pos      = v0.inPosition * barycentrics.x + v1.inPosition * barycentrics.y + v2.inPosition * barycentrics.z;
    const vec3 worldPos = vec3(gl_ObjectToWorldEXT * vec4(pos, 1.0));

    const vec3 nrm      = v0.inNormal * barycentrics.x + v1.inNormal * barycentrics.y + v2.inNormal * barycentrics.z;
    const vec3 worldNrm = normalize(vec3(nrm * gl_WorldToObjectEXT));

    vec3 ambientColor = vec3(0.1, 0.1, 0.1);
    
    vec3 lDir = light.position - worldPos;
    float lightDistance = length(lDir);
    vec3 L = normalize(lDir);

    vec3 outColor = vec3(0.0);

    float specularShare = 0.9;

    outColor = ambientColor * ownColor;
    float attenuation = 1;

    if(dot(worldNrm, L) > 0)
    {
        // lambertian factor
        float lambertian = dot(worldNrm, L);
        float lightIntensity = light.intensity / (lightDistance * lightDistance);
        outColor += ownColor * lambertian * (1.0 - specularShare);
        float tMin   = 0.001;
        float tMax   = length(light.position - worldPos);
        vec3  origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
        vec3  rayDir = L;
        uint  flags =
            gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
        isShadowed = true;
        traceRayEXT(topLevelAS,  // acceleration structure
                flags,       // rayFlags
                0xFF,        // cullMask
                0,           // sbtRecordOffset
                0,           // sbtRecordStride
                1,           // missIndex
                origin,      // ray origin
                tMin,        // ray min range
                rayDir,      // ray direction
                tMax,        // ray max range
                1            // payload (location = 1)
        );

        if (isShadowed) {
            attenuation = 0.7;
        } else {
            //outColor += ggx(69420, L, gl_WorldRayDirectionEXT, worldNrm, specularShare) * specularShare;
            //outColor += anisotropicGGX(L, -gl_WorldRayDirectionEXT, worldNrm, alphaX, alphaY) * specularShare;
            //outColor += cookTorrance(L, gl_WorldRayDirectionEXT, worldNrm, specularShare) * specularShare;
        }
        outColor *= lightIntensity;
    } else {
        attenuation = 0.4;
    }

    
    float tMin   = 0.001;
    float tMax   = 100;
    vec3  origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    uint  flags =
        gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
    relfect.len = gl_HitTEXT * 2;

    vec2 randSeed = gl_WorldRayDirectionEXT.zy;
    randSeed.y = random(randSeed);
    randSeed.x = random(randSeed);
    bool useLtc = true;
    uint samples = useLtc ? 200 : 200;
    for (uint i = 0; i < samples; ++i) {
        float rand1 = randSeed.x = random(randSeed);
        float rand2 = randSeed.y = random(randSeed);

        mat3 TBN = orthonormalBasis(worldNrm);
        mat3 TBN_t = transpose(TBN);
        vec3 wo = normalize(TBN_t * -gl_WorldRayDirectionEXT);
        vec3 wg = normalize(TBN_t * worldNrm);

        if(useLtc) {
            mat3 mLtc;
            LtcMatrix(TBN * wo, TBN_t, alphaX, alphaY, mLtc);
            //mLtc = transpose(mLtc);

            float r = sqrt(rand1);
            float x = r * cos(2.0 * pi * rand2);
            float y = r * sin(2.0 * pi * rand2);
            float z = sqrt(1.0 - rand1);
            vec3 wiStd = vec3(x, y, z);
            vec3 wi = mLtc * wiStd;
            vec3 wiNormalised = normalize(mLtc * wiStd);
            vec3 wiWorld = normalize(TBN * wiNormalised);
            outColor += textureLod(Cubemap, wiWorld.xzy, 0).xyz * wi / wiNormalised;
        } else {
            vec3 vh = normalize(vec3(alphaX * wo.x, alphaY * wo.y, wo.z));
            float lensq = vh.x * vh.x + vh.y * vh.y;
            vec3 T1 = lensq > 0 ? vec3(-vh.y, vh.x, 0) * inversesqrt(lensq) : vec3(1.0, 0.0, 0.0);
            vec3 T2 = cross(vh, T1);
            float r = sqrt(rand1);
            float phi = 2 * pi * rand2;
            float t1 = r * cos(phi);
            float t2 = r * sin(phi);
            float s = 0.5 * (1.0 + vh.z);
            t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;
            vec3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * vh;

            vec3 wm = normalize(vec3(alphaX * Nh.x, alphaY * Nh.y, max(0.0, Nh.z)));

            vec3 wi = normalize(2.0 * dot(wm, wo) * wm - wo);
            vec3 wh = normalize(wo + wi);
            vec3 wiWorld = normalize(TBN * wi);
        
            float lambda_wo = lambdaGGXanisotropic(wo, alphaX, alphaY);
            float lambda_wi = lambdaGGXanisotropic(wi, alphaX, alphaY);
            float D = ggxNDFanisotropic(wh, alphaX, alphaY);
            float G = 1.0 / (1.0 + lambda_wo + lambda_wi);

            float pdf = D * dot(wm, wg);
            if (pdf > 0.0) {
                relfect.hitValue = vec3(0.0);
                traceRayEXT(topLevelAS,  // acceleration structure
                        flags,       // rayFlags
                        0xFF,        // cullMask
                        0,           // sbtRecordOffset
                        0,           // sbtRecordStride
                        2,           // missIndex
                        origin,      // ray origin
                        tMin,        // ray min range
                        wiWorld,     // ray direction
                        tMax,        // ray max range
                        2            // payload (location = 2)
                );
                outColor += relfect.hitValue * anisotropicGGX(wiWorld, -gl_WorldRayDirectionEXT, worldNrm, alphaX, alphaY) / samples;
            }
        }
    }

    /*
    { // reflection
        float tMin   = 0.001;
        float tMax   = 100;
        vec3  origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
        vec3  rayDir = reflect(gl_WorldRayDirectionEXT * gl_HitTEXT, worldNrm);
        uint  flags =
            gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
        relfect.len = gl_HitTEXT * 2;
        traceRayEXT(topLevelAS,  // acceleration structure
                flags,       // rayFlags
                0xFF,        // cullMask
                0,           // sbtRecordOffset
                0,           // sbtRecordStride
                2,           // missIndex
                origin,      // ray origin
                tMin,        // ray min range
                rayDir,      // ray direction
                tMax,        // ray max range
                2            // payload (location = 2)
        );
        //outColor += relfect.hitValue * ggx(69420, rayDir, gl_WorldRayDirectionEXT, worldNrm, specularShare) * specularShare * 8;
        // outColor += relfect.hitValue * anisotropicGGX(rayDir, -gl_WorldRayDirectionEXT, worldNrm, alphaX, alphaY) * specularShare;
        //outColor += relfect.hitValue * cookTorrance(rayDir, gl_WorldRayDirectionEXT, worldNrm, specularShare) * specularShare;
        //outColor = cookTorrance(-rayDir, gl_WorldRayDirectionEXT, worldNrm, specularShare) * 20;
    }
    */

    prd.hitValue = outColor * attenuation;
}
