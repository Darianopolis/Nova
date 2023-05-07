#version 460

layout(location = 0) in vec3 inColor;

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = vec4(inColor, inColor.r);
}