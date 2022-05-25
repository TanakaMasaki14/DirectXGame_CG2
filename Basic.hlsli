cbuffer ConstBufferDataMaterial : register(b0)
{
	float4 color; // �F(RGBA)
};

struct VSOutput
{
	float4 svpos : SV_POSITION;
	float uv : TEXCOORD;
};