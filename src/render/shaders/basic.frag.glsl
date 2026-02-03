#version 450

// Inputs from vertex shader
layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec4 fragColor;

// Uniforms
layout(binding = 0) uniform UniformData {
    mat4 model;
    mat4 view;
    mat4 projection;
    vec4 cameraPosition;
    vec4 lightDirection;
    vec4 lightColor;
    vec4 ambientColor;
} ubo;

// Texture sampler
layout(binding = 1) uniform sampler2D texSampler;

// Output
layout(location = 0) out vec4 outColor;

void main() {
    // Sample texture
    vec4 texColor = texture(texSampler, fragTexCoord);

    // Basic lighting
    vec3 normal = normalize(fragNormal);
    vec3 lightDir = normalize(-ubo.lightDirection.xyz);

    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * ubo.lightColor.rgb;

    // Ambient
    vec3 ambient = ubo.ambientColor.rgb;

    // Combine
    vec3 lighting = ambient + diffuse;
    vec3 result = lighting * texColor.rgb * fragColor.rgb;

    outColor = vec4(result, texColor.a * fragColor.a);
}
