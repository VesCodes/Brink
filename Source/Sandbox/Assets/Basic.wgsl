@group(0) @binding(0) var<uniform> mvp: mat4x4f;

struct VsInput
{
	@location(0) position: vec3f,
	@location(1) jointIndex: vec4u,
	@location(2) jointWeight: vec4f,
}

struct VsOutput
{
	@builtin(position) position: vec4f,
	@location(0) worldPosition: vec3f,
	@location(1) jointColor: vec3f,
}

fn GetJointColor(i: u32) -> vec3f
{
    let c = vec3u((i * 42) % 256u, (i * 69) % 256, (i * 17) % 256);
    return vec3f(c) / 255.0;
}

@vertex fn VsMain(input: VsInput) -> VsOutput
{
	var output: VsOutput;

	output.position = mvp * vec4f(input.position, 1);
	output.worldPosition = input.position;

	output.jointColor =
		GetJointColor(input.jointIndex.x) * input.jointWeight.x +
		GetJointColor(input.jointIndex.y) * input.jointWeight.y +
		GetJointColor(input.jointIndex.z) * input.jointWeight.z +
		GetJointColor(input.jointIndex.w) * input.jointWeight.w;

	return output;
}

@fragment fn PsMain(input: VsOutput) -> @location(0) vec4f
{
	let dx = dpdx(input.worldPosition);
	let dy = dpdy(input.worldPosition);
	let normal = normalize(cross(dx, dy));

    let lightDir = normalize(vec3f(0.0, -2.0, -1.0));

	let ambient = 0.25;
    let ambientColor = input.jointColor;

    let diffuse = max(dot(normal, lightDir), 0.0);
    let diffuseColor = vec3f(0.5, 0.35, 0.4);

    let color = ambient * ambientColor + diffuse * diffuseColor;

	return vec4f(color, 1.0);
}
