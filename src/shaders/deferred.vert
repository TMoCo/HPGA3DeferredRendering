#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) out vec2 outUV;

// shader taken from https://github.com/SaschaWillems/Vulkan/blob/master/data/shaders/glsl/deferred/deferred.vert

void main() 
{
	// magic bit operations to get the UV coordinates of a triangle (such that the whole viewport fits in it)
	// based on vertex id
	outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2); 
	gl_Position = vec4(outUV * 2.0f - 1.0f, 0.0f, 1.0f);
}