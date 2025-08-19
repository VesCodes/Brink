@group(0) @binding(0) var<uniform> mvp: mat4x4f;

struct VsInput
{
	@location(0) position: vec3f,
	@location(1) boneIndex: vec4u,
	@location(2) boneWeight: vec4f,
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

	output.position = mvp * vec4f(input.position, 1);
	output.worldPosition = input.position;

	output.boneColor =
		GetBoneColor(input.boneIndex.x) * input.boneWeight.x +
		GetBoneColor(input.boneIndex.y) * input.boneWeight.y +
		GetBoneColor(input.boneIndex.z) * input.boneWeight.z +
		GetBoneColor(input.boneIndex.w) * input.boneWeight.w;

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
