#version 450
#extension GL_ARB_separate_shader_objects : enable

//
// Vertex shader for deferred rendering skybox
// 

// uniform
layout(binding = 0, std140) uniform UniformBufferObject {
	mat4 projection;
    mat4 view;
} ubo;

// inputs specified in the vertex buffer attributes
layout(location = 0) in vec3 inPosition;

// outputs
layout(location = 0) out vec3 fragTexCoord;

void main() {
	fragTexCoord = inPosition;
	vec4 pos = ubo.projection * ubo.view * vec4(inPosition, 1.0f);
	gl_Position = pos.xyww; // replace the z component with w to guarantee always at max distance when in NDCS
}