#version 450
#extension GL_ARB_separate_shader_objects : enable

//
// Vertex shader for deferred rendering offscreen stage 
// 

// uniform
layout(binding = 0, std140) uniform UniformBufferObject {
    mat4 modl;
    mat4 view;
	mat4 proj;
	mat4 norm;
} ubo;

// inputs specified in the vertex buffer attributes
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec2 inTexCoord;

// outputs
layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec4 fragTangent;
layout(location = 3) out vec2 fragTexCoord;

void main() {
	// position
	vec4 tmpPos = ubo.modl * vec4(inPosition, 1.0f);
	fragPos      = tmpPos.xyz;

	vec4 pos = ubo.proj * ubo.view * tmpPos;
	gl_Position = pos;
	// normal
	mat3 normal  = mat3(ubo.norm);
    fragNormal   = normal * inNormal;
	// tangent
	fragTangent  = inTangent;
	// texture uv
    fragTexCoord = inTexCoord;
}