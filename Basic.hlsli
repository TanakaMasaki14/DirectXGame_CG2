cbuffer ConstBufferDataMaterial : register(b0)
{
    // �F(RGBA)
    float4 color;
};

// ���_�V�F�[�_�[�̏o�͍\����
struct VSOutput
{
    float4 svpos : SV_POSITION; //�V�X�e���p���_���W
    float3 normal : NORMAL; //�@���x�N�g��
    float2 uv : TEXCOORD; //uv�l
};

cbuffer ConstBufferDataTransform : register(b1)
{
    matrix mat;//�R�c�ϊ��s��
};