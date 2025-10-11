attribute vec3 MyPosition;
attribute ivec4 BoneWeights;
attribute ivec4 BoneIndices;
attribute vec3 MyNormal;

uniform mat4 BoneMatrices[56];

uniform mat4 u_viewMtx;
uniform mat4 u_projMtx;

uniform vec4 DirFromLight;
uniform vec4 LightColour;
uniform vec4 AmbientLightColour;

varying vec4 diffuse_color;

void main()
{
    vec4 summedPos = vec4(0, 0, 0, 0);
    vec3 summedNorm = vec3(0, 0, 0);

    // We're going to assume that we can transform the normal by the matrix, which is not
    // technically correct unless we're positive there's not a scaling factor involved.
    for (int i = 0; i < 4; ++i)
    {
        mat4 BoneMatrix = BoneMatrices[int(BoneIndices[i])];
        summedPos  += (BoneMatrix * vec4(MyPosition, 1)) * BoneWeights[i];
        summedNorm += vec3((BoneMatrix * vec4(MyNormal, 0.0)) * BoneWeights[i]);
    }

    gl_Position    = u_projMtx * u_viewMtx * summedPos;
    diffuse_color  = LightColour * dot(normalize(summedNorm), vec3(DirFromLight)) + AmbientLightColour;
}
