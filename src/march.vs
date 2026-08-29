#version 330 core

layout (location = 0) in vec3 position;

uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;

out vec3 localpos;

void main()
{
	localpos = position + vec3(0.5);
	gl_Position = proj * view * model * vec4(position, 1.0);
}
