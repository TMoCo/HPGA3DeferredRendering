#version 450

layout (binding = 1) uniform sampler2D samplerPosition;
layout (binding = 2) uniform sampler2D samplerNormal;
layout (binding = 3) uniform sampler2D samplerAlbedo;

struct Light {
	vec4 position;
	vec3 color;
	float radius;	
};

layout(binding = 4, std140) uniform UniformBufferObject {
	vec4 viewPos;
	Light[4] lights;
} ubo;

layout (location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

const vec3 ambient = vec3(0.2f, 0.2f, 0.2f);

void main() 
{   
	// extract data from textures
	vec3 fragPos = texture(samplerPosition, inUV).rgb; // no need to convert to world space thanks to image format
	vec4 normal = texture(samplerNormal, inUV);
	vec4 albedo = texture(samplerAlbedo, inUV);
	float depth = texture(samplerPosition, inUV).a;

	vec3 fragcolor = albedo.rgb * ambient;

	switch(int(ubo.viewPos.w)) {
		case 0: {
			// encode if object illuminated with normal w component
			if (normal.w == 0.0f) {
				fragcolor = albedo.rgb;
				break;
			}

			for (int i = 0; i < 4; i++) {
				// vec to light
				vec3 toLight = ubo.lights[i].position.xyz - fragPos;
				float distToLight = length(toLight);

				if (distToLight < ubo.lights[i].radius) {
					// direction to frag from viewer
					vec3 viewToFrag = normalize(ubo.viewPos.xyz - fragPos);

					float attenuation = ubo.lights[i].radius / (pow(distToLight, 2.0f) + 1.0f);

					// diffuse
					float normalDotToLight = max(0.0f, dot(normal.xyz, toLight));
					vec3 diffuse = ubo.lights[i].color * albedo.rgb * normalDotToLight * attenuation;

					// specular
					vec3 r = reflect(-toLight, normal.xyz);
					float normalDotReflect = max(0.0f, dot(r, viewToFrag));
					vec3 specular = ubo.lights[i].color * albedo.a * pow(normalDotReflect, 16.0f) * attenuation;

					fragcolor += diffuse + specular;
				}
			}
			break;
		}
		case 1: 
			fragcolor = fragPos;
			break;
		case 2:
			fragcolor = normal.xyz;
			break;
		case 3:
			fragcolor = albedo.rgb;
			break;
		case 4:
			fragcolor = vec3(depth);
			break;
	}
	outColor = vec4(fragcolor, 1.0f);
}