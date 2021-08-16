#version 450

layout (binding = 1) uniform sampler2D samplerPosition;
layout (binding = 2) uniform sampler2D samplerNormal;
layout (binding = 3) uniform sampler2D samplerAlbedo;
layout (binding = 5) uniform sampler2D samplerShadowMap;

struct Light {
	vec4 position;
	vec3 color;
	float radius;	
};

layout(binding = 4, std140) uniform UniformBufferObject {
	vec4 viewPos;
	mat4 depthMVP;
	mat4 cameraMVP;
	Light[1] lights;
} ubo;

layout (location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

const vec3 ambient = vec3(0.2f, 0.2f, 0.2f);

#define NEAR 0.1f
#define FAR 40.0f

// compute linear depth, taken from:
// https://learnopengl.com/Advanced-OpenGL/Depth-testing
// (formula has been factorised)
float linZ (float z, float n, float f) {
	return n / (f - f*z + n*z);
}

// given a scene coordinate transformed by a light's MVP matrix, perform perspective division to get the NDC coordinates
// from the light's view point 
float computeShadow(vec4 shadowCoord) {
	vec3 shadowNDC = shadowCoord.xyz / shadowCoord.w; // perspective division
	shadowNDC.xy = shadowNDC.xy * 0.5f + 0.5f; // mapping from [-1,1] to [0,1] for sampling the shadow map
	return shadowNDC.z > texture(samplerShadowMap, shadowNDC.xy).r ? 1.0f : 0.0f; // if fragment depth greater than depth to occluder, then fragment is in shadow
}

void main() 
{   
	// values from gbuffer attachments
	vec4 fragPos = vec4(texture(samplerPosition, inUV).rgb, 1.0f); // no need to convert to world space thanks to image format
	vec4 normal = texture(samplerNormal, inUV); 
	vec4 albedo = texture(samplerAlbedo, inUV);

	// is fragment skybox? encoded in normal's w component
	if (normal.w == 0.0f && ubo.viewPos.w != 5 ) {
		outColor = vec4(albedo.rgb, 1.0f);
		return;
	}

	vec4 cameraCoord = ubo.cameraMVP * vec4(fragPos.xyz, 1.0f); // fragment position in camera space
	vec4 shadowCoord = ubo.depthMVP * vec4(fragPos.xyz, 1.0f); // fragment position in light's space

	switch(int(ubo.viewPos.w)) {
		// scene composition
		case 0: {
			vec3 fragcolor = albedo.rgb * ambient;

			for (int i = 0; i < 1; i++) {
				// vec to light
				vec3 toLight = ubo.lights[i].position.xyz - fragPos.xyz;
				float distToLight = length(toLight);

				if (distToLight < ubo.lights[i].radius) {
					// direction to frag from viewer
					vec3 viewToFrag = normalize(ubo.viewPos.xyz - fragPos.xyz);

					float attenuation = ubo.lights[i].radius / (pow(distToLight, 2.0f) + 5.0f);

					// diffuse
					float normalDotToLight = max(0.0f, dot(normal.xyz, toLight));
					vec3 diffuse = ubo.lights[i].color * albedo.rgb * normalDotToLight * attenuation;

					// specular
					vec3 r = reflect(-toLight, normal.xyz);
					float normalDotReflect = max(0.0f, dot(r, viewToFrag));
					vec3 specular = ubo.lights[i].color * albedo.a * pow(normalDotReflect, 3.0f) * attenuation;

					fragcolor += (1.0f - computeShadow(shadowCoord)) * (diffuse + specular);
				}
			}
			outColor = vec4(fragcolor, 1.0f);
			break;
		}
		// position
		case 1: 
			outColor = vec4(fragPos.xyz, 1.0f);
			break;
		// normal
		case 2:
			outColor = vec4(normal.xyz, 1.0f);
			break;
		// albedo&
		case 3:
			outColor = vec4(albedo.rgb, 1.0f);
			break;
		// depth
		case 4:
			// encoded scene depth in alpha channel of position texture in previous render pass
			outColor = vec4(vec3(texture(samplerPosition, inUV).a), 1.0f);
			break;
		// shadowmap value
		case 5:
			outColor = vec4(linZ(texture(samplerShadowMap, inUV).r, NEAR, FAR));
			break;
		// position projected in light space
		case 6:
			outColor = vec4(shadowCoord.xy, 0.0f, 1.0f); // display the NDC coordinates for light
			break;
		// position projected in camera space
		case 7:
			outColor = vec4(cameraCoord.xy, 0.0f, 1.0f); // display the NDC coordinates for camera
			break;
		// depth in light space
		case 8:
			// color the scene according to the shadow map's depth, apply perspective division for correct values
			outColor = vec4(vec3(linZ(shadowCoord.z / shadowCoord.w , NEAR, FAR)), 1.0f); 
			break;
	}
}