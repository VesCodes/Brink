struct SkinningConstants
{
	positionsOffset : u32,
	boneIndicesOffset : u32,
	boneWeightsOffset : u32,
	boneTransformsOffset : u32,
	skinnedPositionsOffset : u32,
	vertexCount : u32,
}

@group(0) @binding(0) var<storage> constants: SkinningConstants;
@group(0) @binding(1) var<storage, read_write> meshBuffer: array<u32>;

fn LoadVec3_f32(byteOffset: u32, idx: u32) -> vec3<f32>
{
	let offset = (byteOffset + idx * 12u) / 4u;

	return vec3<f32>(
		bitcast<f32>(meshBuffer[offset]),
		bitcast<f32>(meshBuffer[offset + 1u]),
		bitcast<f32>(meshBuffer[offset + 2u]),
	);
}

fn LoadVec4_u32(byteOffset: u32, idx: u32) -> vec4<u32>
{
	let offset = (byteOffset + idx * 16u) / 4u;

	return vec4<u32>(
		meshBuffer[offset],
		meshBuffer[offset + 1u],
		meshBuffer[offset + 2u],
		meshBuffer[offset + 3u],
	);
}

fn LoadVec4_f32(byteOffset: u32, idx: u32) -> vec4<f32>
{
	let offset = (byteOffset + idx * 16u) / 4u;

	return vec4<f32>(
		bitcast<f32>(meshBuffer[offset]),
		bitcast<f32>(meshBuffer[offset + 1u]),
		bitcast<f32>(meshBuffer[offset + 2u]),
		bitcast<f32>(meshBuffer[offset + 3u]),
	);
}

fn LoadMat4_f32(byteOffset: u32, idx: u32) -> mat4x4<f32>
{
	let offset = (byteOffset + idx * 64u) / 4u;

	return mat4x4<f32>(
		vec4<f32>(
			bitcast<f32>(meshBuffer[offset + 0u]),
			bitcast<f32>(meshBuffer[offset + 1u]),
			bitcast<f32>(meshBuffer[offset + 2u]),
			bitcast<f32>(meshBuffer[offset + 3u]),
		),
		vec4<f32>(
			bitcast<f32>(meshBuffer[offset + 4u]),
			bitcast<f32>(meshBuffer[offset + 5u]),
			bitcast<f32>(meshBuffer[offset + 6u]),
			bitcast<f32>(meshBuffer[offset + 7u]),
		),
		vec4<f32>(
			bitcast<f32>(meshBuffer[offset + 8u]),
			bitcast<f32>(meshBuffer[offset + 9u]),
			bitcast<f32>(meshBuffer[offset + 10u]),
			bitcast<f32>(meshBuffer[offset + 11u]),
		),
		vec4<f32>(
			bitcast<f32>(meshBuffer[offset + 12u]),
			bitcast<f32>(meshBuffer[offset + 13u]),
			bitcast<f32>(meshBuffer[offset + 14u]),
			bitcast<f32>(meshBuffer[offset + 15u]),
		),
	);
}

fn StoreVec3_f32(byteOffset: u32, idx: u32, value: vec3<f32>)
{
	let offset = (byteOffset + idx * 12u) / 4u;

	meshBuffer[offset] = bitcast<u32>(value.x);
	meshBuffer[offset + 1u] = bitcast<u32>(value.y);
	meshBuffer[offset + 2u] = bitcast<u32>(value.z);
}

@compute @workgroup_size(64)
fn CsMain(@builtin(global_invocation_id) threadId: vec3u)
{
	let idx = threadId.x;
	if (idx < constants.vertexCount)
	{
		let position = vec4f(LoadVec3_f32(constants.positionsOffset, idx), 1.0);
		var skinnedPosition = vec3f(0.0);

		let boneIndices = LoadVec4_u32(constants.boneIndicesOffset, idx);
		let boneWeights = LoadVec4_f32(constants.boneWeightsOffset, idx);

		for (var i = 0; i < 4; i++)
		{
			if (boneWeights[i] > 0.0)
			{
				let boneTransform = LoadMat4_f32(constants.boneTransformsOffset, boneIndices[i]);
				skinnedPosition += (boneTransform * position).xyz * boneWeights[i];
			}
		}

		StoreVec3_f32(constants.skinnedPositionsOffset, idx, skinnedPosition);
	}
}
