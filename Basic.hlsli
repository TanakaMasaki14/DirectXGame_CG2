cbuffer ConstBufferDataMaterial : register(b0)
{
    // 色(RGBA)
    float4 color;
};

// 頂点シェーダーの出力構造体
struct VSOutput
{
    float4 svpos : SV_POSITION; //システム用頂点座標
    float3 normal : NORMAL; //法線ベクトル
    float2 uv : TEXCOORD; //uv値
};

cbuffer ConstBufferDataTransform : register(b1)
{
    matrix mat;//３Ｄ変換行列
};