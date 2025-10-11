struct VS_IN
{
    float3  Position        : POSITION0;
    float4  BoneWeights     : BLENDWEIGHT;
    float4  BoneIndices     : BLENDINDICES;
    float3  Normal          : NORMAL;
    float2  Tex0            : TEXCOORD0;
};

struct VS_OUT
{
    float4 Position         : POSITION;
    float2 Tex0             : TEXCOORD0;
    float3 Diffuse          : COLOR0;
};

struct PS_INPUT
{
    float3 Diffuse : COLOR0;
    float2 Texture : TEXCOORD0;
};


// Must match the #defines in xenon_sample.cpp
float4x3 ObjToWorld : register(c0);
float4x3 WorldToView : register(c4);
float4x4 ViewToClip : register(c8);
float3 DirFromLight : register(c12);
float4 LightColour : register(c13);
float4 AmbientColour : register(c14);

#define TRILINEAR_SAMPLER sampler_state { MipFilter = LINEAR; MinFilter = LINEAR; MagFilter = LINEAR; AddressU = WRAP; AddressV = WRAP; }
sampler2D  diffuse_texture     : register(s0) = TRILINEAR_SAMPLER;

// Fetch a bone matrix from the vertex stream
float4x3 VFetchBoneMatrix( int BoneIndex )
{
    float4 Result1;
    float4 Result2;
    float4 Result3;
    asm
    {
        vfetch Result1, BoneIndex, position1;
        vfetch Result2, BoneIndex, position2;
        vfetch Result3, BoneIndex, position3;
    };

    float4x3 Result;
    Result._11_21_31_41 = Result1;
    Result._12_22_32_42 = Result2;
    Result._13_23_33_43 = Result3;
    return Result;
}

VS_OUT SkinVSVertexFetch( VS_IN In )
{
    VS_OUT Out;

    float BoneWeights[4] = (float[4])In.BoneWeights;
    int BoneIndices[4]   = (int[4])In.BoneIndices;

    float4 InPos     = float4( In.Position, 1 );
    float3 WorldPos    = 0;
    float3 WorldNormal = 0;

    for( int i = 0; i < 4; ++i )
    {
        float4x3 BoneMatrix = VFetchBoneMatrix( BoneIndices[i] );
        WorldPos    += BoneWeights[i] * mul( InPos, BoneMatrix );
        WorldNormal += BoneWeights[i] * mul( In.Normal, BoneMatrix );
    }

    float3 ViewPos = mul(float4(WorldPos, 1), WorldToView);
    float4 ClipPos = mul(float4(ViewPos, 1), ViewToClip);
    Out.Position   = ClipPos;

    Out.Tex0    = In.Tex0;
    Out.Diffuse = LightColour * dot(float4(normalize(WorldNormal),0), DirFromLight) + AmbientColour;

    return Out;
}

float4 PShader ( PS_INPUT In ) : COLOR
{
    float3 DiffuseSamp = tex2D( diffuse_texture, In.Texture );
    DiffuseSamp *= In.Diffuse;

    return float4(DiffuseSamp, 1);
}

