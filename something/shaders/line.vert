#version 330 core
layout (location = 0) in vec3 aPos;   // Vertex position
layout (location = 1) in vec3 aColor; // Vertex color

out vec3 lineColor; // Pass color to fragment shader

uniform mat4 model;      // Model matrix (for positioning/scaling the line)
uniform mat4 view;       // View matrix
uniform mat4 projection; // Projection matrix

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    lineColor = aColor; // Pass the color along
}