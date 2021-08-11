#version 450
#extension GL_ARB_separate_shader_objects : enable

//
// Simple forward rendering fragment shader stage
// 

// Textures
layout(binding = 1) uniform sampler2D albedoSampler;
layout(binding = 2) uniform sampler2D metallicRoughnessSampler;

// Input from previous stage
layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

// Output
layout(location = 0) out vec4 outColor;

void main() {
    // ambient
    float ambientStrength = 0.1;
    vec3 ambient = vec3(0.5f, 0.5f, 0.5f);
  	
    // diffuse 
    vec3 lightDir = vec3(0.0f, 1.0f, 0.0f);
    float diff    = max(dot(fragNormal, lightDir), 0.0);
    vec3 diffuse  = diff * vec3(0.5f, 0.5f, 0.5f);
    
    // specular
    float specularStrength = 0.5;
    vec3 viewDir    = normalize(vec3(0.0f, 0.0f, 3.0f) - fragPos);
    vec3 reflectDir = reflect(-lightDir, fragNormal);  
    float spec      = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular   = specularStrength * spec * vec3(0.5f, 0.5f, 0.5f);
        
    vec3 result = (ambient + diffuse + specular) * texture(albedoSampler, fragTexCoord).rgb;
    outColor = vec4(result, 1.0);
}