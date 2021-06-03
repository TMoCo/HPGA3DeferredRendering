#version 450

layout (binding = 1) uniform sampler2D samplerposition;
layout (binding = 2) uniform sampler2D samplerNormal;
layout (binding = 3) uniform sampler2D samplerAlbedo;

layout (location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

void main() 
{   
	outColor = vec4(inUV, 1.0f, 1.0f);	
}