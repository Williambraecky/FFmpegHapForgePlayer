#version 330 core

out vec4 colorOut;

in vec2 uv;

uniform sampler2D cocgsy_src;

void main()
{
    colorOut = texture(cocgsy_src, uv);
}
