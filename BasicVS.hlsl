#include "Basic.hlsli"

VSOutput main(float4 pos : POSITION, float uv : TEXCOORD)
{
	VSOutput output;
	output.svpos;
	output.uv = uv;
	return output;
}
