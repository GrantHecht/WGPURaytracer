/**
 * A structure holding the value of our uniforms
 */
struct MyUniforms {
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
    modelMatrix: mat4x4f,
    color: vec4f,
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
}

const pi = 3.14159265359;

@group(0) @binding(0) 
var<uniform> uMyUniforms: MyUniforms;
@group(0) @binding(1) 
var gradientTexture: texture_2d<f32>;
@group(0) @binding(2)
var textureSampler: sampler;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
	var out: VertexOutput;
    out.position = uMyUniforms.projectionMatrix * uMyUniforms.viewMatrix * uMyUniforms.modelMatrix * vec4f(in.position, 1.0);
    out.normal = normalize((uMyUniforms.modelMatrix * vec4f(in.normal, 0.0)).xyz);
    out.uv = in.uv;
	out.color = in.color;
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    //let texelCoords = vec2i(in.uv * vec2f(textureDimensions(gradientTexture)));
    //let color = textureLoad(gradientTexture, texelCoords, 0).rgb;
    let color = textureSample(gradientTexture, textureSampler, in.uv).rgb;

    // apply gamma correction
    let linear_color = pow(color, vec3f(2.2));
    return vec4f(linear_color, 1.0);
}