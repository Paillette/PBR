#version 430

layout(binding = 0) uniform sampler2D u_Texture;
layout(binding = 1) uniform sampler2D u_blurTexture;
uniform sampler2D u_Normal;

varying vec2 v_UV;

void main(void)
{
const float gamma = 2.2;

	vec3 hdrColor = texture2D(u_Texture, v_UV).rgb;
	vec3 bloomColor = texture2D(u_blurTexture, v_UV).rgb;
	hdrColor += bloomColor;

	vec3 color = vec3(1.0) - exp(-hdrColor * 2.0);
	vec3 result = pow(color, vec3(1.0 / 2.2));

	gl_FragColor = texture2D(u_Texture, v_UV);

	//gl_FragColor = vec4(hdrColor, 1.0);

}