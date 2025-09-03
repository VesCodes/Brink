@group(0) @binding(0) var<uniform> mvp: mat4x4f;
@group(0) @binding(1) var<storage> boneTransforms: array<mat4x4f>;

struct VsInput
{
	@location(0) position: vec3f,
	@location(1) boneIndices: vec4u,
	@location(2) boneWeights: vec4f,
}

struct VsOutput
{
	@builtin(position) position: vec4f,
	@location(0) worldPosition: vec3f,
	@location(1) boneColor: vec3f,
}

fn GetBoneColor(i: u32) -> vec3f
{
    let c = vec3u((i * 42) % 256u, (i * 69) % 256, (i * 17) % 256);
    return vec3f(c) / 255.0;
}

@vertex fn VsMain(input: VsInput) -> VsOutput
{
	var output: VsOutput;

	let boneTransform =
		boneTransforms[input.boneIndices[0]] * input.boneWeights[0] +
		boneTransforms[input.boneIndices[1]] * input.boneWeights[1] +
		boneTransforms[input.boneIndices[2]] * input.boneWeights[2] +
		boneTransforms[input.boneIndices[3]] * input.boneWeights[3];

	output.position = mvp * boneTransform * vec4f(input.position, 1);
	output.worldPosition = input.position;

	output.boneColor =
		GetBoneColor(input.boneIndices.x) * input.boneWeights.x +
		GetBoneColor(input.boneIndices.y) * input.boneWeights.y +
		GetBoneColor(input.boneIndices.z) * input.boneWeights.z +
		GetBoneColor(input.boneIndices.w) * input.boneWeights.w;

	return output;
}

@fragment fn PsMain(input: VsOutput) -> @location(0) vec4f
{
	let dx = dpdx(input.worldPosition);
	let dy = dpdy(input.worldPosition);
	let normal = normalize(cross(dx, dy));

    let lightDir = normalize(vec3f(0.0, -2.0, -1.0));

	let ambient = 0.25;
    let ambientColor = input.boneColor;

    let diffuse = max(dot(normal, lightDir), 0.0);
    let diffuseColor = vec3f(0.5, 0.35, 0.4);

    let color = ambient * ambientColor + diffuse * diffuseColor;

	return vec4f(color, 1.0);
}
