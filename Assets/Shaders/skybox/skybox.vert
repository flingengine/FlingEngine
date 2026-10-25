#version 450

// Normalized device coordinates of this pixel, for the fragment shader to make a view ray
layout (location = 0) out vec2 outNDC;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{
	// One triangle that covers the full screen, made from the vertex index with no vertex buffer
	// @see https://www.saschawillems.de/blog/2016/08/13/vulkan-tutorial-on-rendering-a-fullscreen-quad-without-buffers/
	vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	outNDC = uv * 2.0 - 1.0;
	gl_Position = vec4(outNDC, 1.0, 1.0);
}
