/**
 * A structure holding the value of our uniforms
 */
struct MyUniforms {
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
    modelMatrix: mat4x4f,
    color: vec4f,
    cameraWorldPosition: vec3f,
    time: f32,
};

/**
 * A structure with fields labeled with vertex attribute locations can be used
 * as input to the entry point of a shader.
 */
struct VertexInput {
    @location(0) position: vec3f,
    @location(1) normal: vec3f,
    @location(2) color: vec3f,
    @location(3) uv: vec2f,
};

/**
 * A structure with fields labeled with builtins and locations can also be used
 * as *output* of the vertex shader, which is also the input of the fragment
 * shader.
 */
struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) color: vec3f,
    @location(1) normal: vec3f,
    @location(2) uv: vec2f,
    @location(3) viewDirection: vec3f,
}

/**
 * A structure holding the lighting settings
 */
struct LightingUniforms {
    directions: array<vec4f, 2>,
    colors: array<vec4f, 2>,
}

const pi = 3.14159265359;

@group(0) @binding(0) 
var<uniform> uMyUniforms: MyUniforms;
@group(0) @binding(1) 
var baseColorTexture: texture_2d<f32>;
@group(0) @binding(2)
var textureSampler: sampler;
@group(0) @binding(3)
var<uniform> uLighting: LightingUniforms;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
	var out: VertexOutput;

    let worldPosition = uMyUniforms.modelMatrix * vec4f(in.position, 1.0);
    out.position = uMyUniforms.projectionMatrix * uMyUniforms.viewMatrix * worldPosition;

    let cameraWorldPosition = uMyUniforms.cameraWorldPosition;
    out.viewDirection = cameraWorldPosition - worldPosition.xyz;

    out.normal = (uMyUniforms.modelMatrix * vec4f(in.normal, 0.0)).xyz;
    out.uv = in.uv;
	out.color = in.color;
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    // Sample texture
    let baseColor = textureSample(baseColorTexture, textureSampler, in.uv).rgb;
    let kd = 1.0; // strength of diffuse effect
    let ks = 0.5; // strength of specular effect

    var color = vec3f(0.0);
    let normal = normalize(in.normal);
    for (var i: i32 = 0; i < 2; i++) {
        // Get normalized light direction
        let direction = normalize(uLighting.directions[i].xyz);

        // Diffuse lighting
        let lightColor = uLighting.colors[i].rgb;
        let diffuse = max(0.0, dot(direction, normal)) * lightColor;

        // Specular lighting
        let R = reflect(direction, normal);
        let V = normalize(in.viewDirection);
        let RoV = max(0.0, dot(R, V));
        let hardness = 16.0;
        let specular = vec3f(pow(RoV, hardness));

        color += baseColor * kd * diffuse + ks * specular;
    }

    // apply gamma correction
    let corrected_color = pow(color, vec3f(2.2));
    return vec4f(corrected_color, 1.0);
}