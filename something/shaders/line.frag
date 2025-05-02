#version 330 core
out vec4 FragColor;

in vec3 lineColor; // Receive color from vertex shader

void main()
{
    FragColor = vec4(lineColor, 1.0f); // Output the received color
}