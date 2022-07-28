// マテリアル
cbuffer ConstBufferDataMaterial : register(b0)
{
    // 色(RGBA)
    float4 color;
};

// 頂点シェーダーの出力構造体
// （頂点シェーダーからピクセルシェーダーへのやり取りに使用
struct VSOutput
{
    float4 svpos : SV_POSITION; //システム用頂点座標
    float3 normal: NORMAL; //法線ベクトル
    float2 uv : TEXCOORD; //uv値
};

//３Ｄ変換行列する）
cbuffer ConstBufferDataTransform : register(b1)
{
    matrix mat;//３Ｄ変換行列
};