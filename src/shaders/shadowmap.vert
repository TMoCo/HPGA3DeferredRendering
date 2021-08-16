#version 450
#extension GL_ARB_separate_shader_objects : enable

//
// Vertex shader for deferred rendering shadow map generation stage 
// 

// uniform
layout(binding = 0, std140) uniform UniformBufferObject {
	mat4 depthMVP; // light's view and projection
} ubo;

// inputs specified in the vertex buffer attributes
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec2 inTexCoord;

out gl_PerVertex { 
	vec4 gl_Position; 
};

void main() {
	gl_Position = ubo.depthMVP * vec4(inPosition, 1.0f);
}
