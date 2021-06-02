#version 450
#extension GL_ARB_separate_shader_objects : enable

//
// Uniforms
//

//
// Textures
//

layout(binding = 1) uniform sampler2D textureSamplers[2];

//
// Input from previous stage
//

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

//
// Output
//

layout(location = 0) out vec4 outColor;

void main() {

    /*
	// do something
	vec4 albedo = texture(texSampler, fragTexCoord).rgba;

    // view direction, assumes eye is at the origin (which is the case)
    vec3 viewDir = normalize(-fragPos);
    // light direction from fragment to light
    vec3 lightDir = normalize(ubo.lightPos.xyz - fragPos);
    // reflect direction, reflection of the light direction by the fragment normal
    vec3 reflectDir = reflect(-lightDir, fragNormal);


    // diffuse (lambertian)
    float diff = max(dot(lightDir, fragNormal), 0.0f);
    vec3 diffuse = chrome.diffuse * diff;

    // specular 
    float spec = pow(max(dot(viewDir, reflectDir), 0.0f), chrome.exponent);
    vec3 specular = chrome.specular * spec;
    */

    // vector multiplication is element wise <3
    outColor = vec4( texture(textureSamplers[0], fragTexCoord).rgb, 1.0f);
}