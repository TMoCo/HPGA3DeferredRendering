#version 450
#extension GL_ARB_separate_shader_objects : enable

//
// fragment shader for deferred rendering shadow map generation stage 
// 

// Write to the depth image
layout(location = 0) out float fragDepth;

void main() {
	fragDepth = gl_FragCoord.z;
}