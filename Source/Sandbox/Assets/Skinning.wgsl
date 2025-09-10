struct SkinningConstants
{
	positionsOffset : u32,
	boneInfluencesOffset : u32,
	boneTransformsOffset : u32,
	skinnedPositionsOffset : u32,
	vertexCount : u32,
}

struct BoneInfluence
{
	indices: vec4u,
	weights: vec4f,
}

@group(0) @binding(0) var<storage> constants: SkinningConstants;
@group(0) @binding(1) var<storage> positions: array<f32>;
@group(0) @binding(2) var<storage> boneInfluences: array<BoneInfluence>;
@group(0) @binding(3) var<storage> boneTransforms: array<mat4x4f>;
@group(0) @binding(4) var<storage, read_write> skinnedPositions: array<f32>;

@compute @workgroup_size(64)
fn CsMain(@builtin(global_invocation_id) threadId: vec3u)
{
	let idx = threadId.x;
	if (idx < constants.vertexCount)
	{
		let positionsOffset = (constants.positionsOffset + idx) * 3;
		let skinnedPositionsOffset = (constants.skinnedPositionsOffset + idx) * 3;

		let boneInfluence = boneInfluences[constants.boneInfluencesOffset + idx];
		let boneTransform = boneTransforms[constants.boneTransformsOffset + boneInfluence.indices.x] * boneInfluence.weights.x
						  + boneTransforms[constants.boneTransformsOffset + boneInfluence.indices.y] * boneInfluence.weights.y
						  + boneTransforms[constants.boneTransformsOffset + boneInfluence.indices.z] * boneInfluence.weights.z
						  + boneTransforms[constants.boneTransformsOffset + boneInfluence.indices.w] * boneInfluence.weights.w;

		let position = vec4f(positions[positionsOffset + 0], positions[positionsOffset + 1], positions[positionsOffset + 2], 1.0);
		let skinnedPosition = boneTransform * position;

		skinnedPositions[skinnedPositionsOffset + 0] = skinnedPosition.x;
		skinnedPositions[skinnedPositionsOffset + 1] = skinnedPosition.y;
		skinnedPositions[skinnedPositionsOffset + 2] = skinnedPosition.z;
	}
}
