#version 450
#extension GL_ARB_separate_shader_objects : enable

//
// Fragment shader for deferred rendering skybox
// 

// uniform
layout(binding = 1) uniform samplerCube skybox;

// input from previous stage is vertex position
layout(location = 0) in vec3 inTexCoord;

// output frag colour
layout (location = 0) out vec4 outPosition;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec4 outAlbedo;

void main() {
	outPosition = vec4(inTexCoord, 1.0f);
	outNormal = vec4(0.0f);
	outAlbedo = vec4(inTexCoord, 1.0f);
	outAlbedo = texture(skybox, inTexCoord);
}
