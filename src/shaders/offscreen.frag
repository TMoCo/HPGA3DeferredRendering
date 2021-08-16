#version 450

//
// fragment shader for deferred rendering offscreen stage 
//

// textures
layout (binding = 1) uniform sampler2D albedoSampler;
layout (binding = 2) uniform sampler2D metallicRoughnessSampler;

// input from previous stage
// outputs
layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec4 fragTangent;
layout(location = 3) in vec2 fragTexCoord;

layout (location = 0) out vec4 outPosition;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec4 outAlbedo;

const float far = 20.0f;
const float near = 0.1;

// compute linear depth
// https://learnopengl.com/Advanced-OpenGL/Depth-testing
float linearize_Z(float z , float zNear , float zFar){
	return (2 * zNear * zFar) / (zFar + zNear - (z * 2.0f  - 1.0f) * (zFar -zNear)) ;
}

void main() 
{
	// output to the gbuffer's color attachments
	outPosition = vec4(fragPos, linearize_Z(gl_FragCoord.z, near, far) / far);
	outNormal   = vec4(fragNormal, 1.0f);
	outAlbedo   = texture(albedoSampler, fragTexCoord);

	// Calculate normal in tangent space
	// https://github.com/KhronosGroup/glTF/blob/master/specification/2.0/README.md#tangent-space-definition
	vec3 N = fragNormal;
	vec3 T = fragTangent.xyz;
	vec3 B = cross(N, T) * fragTangent.w; // bitangent
	mat3 TBN = mat3(T, B, N);

	// from normal map if provided
	// vec3 outNormal = TBN * normalize(texture(samplerNormalMap, inUV).xyz * 2.0 - vec3(1.0));
}