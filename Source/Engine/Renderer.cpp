#include "Renderer.h"

#include "Core/Math.h"
#include "Core/Memory.h"
#include "Core/Platform.h"
#include "Core/Pool.h"

#include "Graphics.h"

namespace Bk
{
	struct MeshConstants
	{
		Mat4f mvp;
	};

	struct SkinningConstants
	{
		uint32 positionsOffset;
		uint32 boneInfluencesOffset;
		uint32 boneTransformsOffset;
		uint32 skinnedPositionsOffset;
		uint32 vertexCount;
	};

	struct BoneInfluence
	{
		Vec4u indices;
		Vec4f weights;
	};

	struct Mesh
	{
		TSpan<MeshSection> sections;

		uint32 positionsOffset;
		uint32 indicesOffset;
		uint32 vertexCount;

		uint32 skinningPositionsOffset;
		uint32 skinningBoneInfluencesOffset;
		uint32 skinningBoneTransformsOffset;
	};

	struct
	{
		Arena arena;

		TPool<Mesh> meshes;

		uint32 meshConstantsBuffer;

		// #TODO: Single mesh storage buffer for everything?

		size_t meshPositionsBufferOffset;
		size_t meshPositionsBufferSize;
		uint32 meshPositionsBuffer;

		size_t meshIndicesBufferOffset;
		size_t meshIndicesBufferSize;
		uint32 meshIndicesBuffer;

		uint32 meshBindingGroup;
		uint32 meshPipeline;

		uint32 skinningConstantsBuffer;

		size_t skinningPositionsBufferOffset;
		size_t skinningPositionsBufferSize;
		uint32 skinningPositionsBuffer;

		size_t skinningBoneInfluencesBufferOffset;
		size_t skinningBoneInfluencesBufferSize;
		uint32 skinningBoneInfluencesBuffer;

		size_t skinningBoneTransformsBufferOffset;
		size_t skinningBoneTransformsBufferSize;
		uint32 skinningBoneTransformsBuffer;

		uint32 skinningBindingGroup;
		uint32 skinningPipeline;
	} renderer;

	String LoadShader(Arena& arena, String filePath)
	{
		String result = String::Empty;

		if (FileHandle fileHandle = OpenFile(filePath, FileAccess::Read))
		{
			TSpan<uint8> fileData = Push(arena, GetFileSize(fileHandle));
			if (ReadFile(fileHandle, fileData) == fileData.length)
			{
				result = String(reinterpret_cast<char*>(fileData.data), fileData.length);
			}

			CloseFile(fileHandle);
		}

		return result;
	}

	void InitializeRenderer()
	{
		ArenaScope scratch = GetScratchArena();

		Allocate(renderer.arena, renderer.meshes, 512);

		renderer.meshConstantsBuffer = CreateBuffer({
			.name = "Mesh Constants",
			.type = GfxBufferType::Uniform,
			.access = GfxBufferAccess::GpuOnly,
			.size = sizeof(MeshConstants),
		});

		renderer.meshPositionsBufferOffset = 0;
		renderer.meshPositionsBufferSize = BK_MEGABYTES(4);
		renderer.meshPositionsBuffer = CreateBuffer({
			.name = "Mesh Positions",
			.type = GfxBufferType::Vertex | GfxBufferType::Storage,
			.access = GfxBufferAccess::GpuOnly,
			.size = renderer.meshPositionsBufferSize,
		});

		renderer.meshIndicesBufferOffset = 0;
		renderer.meshIndicesBufferSize = BK_MEGABYTES(4);
		renderer.meshIndicesBuffer = CreateBuffer({
			.name = "Mesh Indices",
			.type = GfxBufferType::Index,
			.access = GfxBufferAccess::GpuOnly,
			.size = renderer.meshIndicesBufferSize,
		});

		uint32 meshBindingLayout = CreateBindingLayout({
			.name = "Mesh Layout",
			.bindings = {
				{ .type = GfxBindingType::UniformBuffer, .stage = GfxBindingStage::Vertex },
			},
		});

		renderer.meshBindingGroup = CreateBindingGroup({
			.name = "Mesh Group",
			.bindingLayout = meshBindingLayout,
			.bindings = {
				{ .buffer = renderer.meshConstantsBuffer },
			},
		});

		// #TODO: Engine assets
		String meshShaderCode = LoadShader(scratch.arena, "Assets/Mesh.wgsl");
		renderer.meshPipeline = CreateRenderPipeline({
			.name = "Mesh Pipeline",
			.vertexShader = {
				.code = meshShaderCode,
				.buffers = {
					{
						.stride = 12,
						.attributes = {
							{ .offset = 0, .format = GfxVertexFormat::Float32x3 },
						},
					},
				},
			},
			.pixelShader = {
				.code = meshShaderCode,
			},
			.bindingLayouts = {
				meshBindingLayout,
			},
		});

		// ---

		renderer.skinningConstantsBuffer = CreateBuffer({
			.name = "Skinning Constants",
			.type = GfxBufferType::Storage,
			.access = GfxBufferAccess::GpuOnly,
			.size = AlignUp(sizeof(SkinningConstants), 256) * renderer.meshes.capacity,
		});

		renderer.skinningPositionsBufferOffset = 0;
		renderer.skinningPositionsBufferSize = BK_MEGABYTES(4);
		renderer.skinningPositionsBuffer = CreateBuffer({
			.name = "Skinning Positions",
			.type = GfxBufferType::Storage,
			.access = GfxBufferAccess::GpuOnly,
			.size = renderer.skinningPositionsBufferSize,
		});

		renderer.skinningBoneInfluencesBufferOffset = 0;
		renderer.skinningBoneInfluencesBufferSize = BK_MEGABYTES(4);
		renderer.skinningBoneInfluencesBuffer = CreateBuffer({
			.name = "Skinning Bone Influences",
			.type = GfxBufferType::Storage,
			.access = GfxBufferAccess::GpuOnly,
			.size = renderer.skinningBoneInfluencesBufferSize,
		});

		renderer.skinningBoneTransformsBufferOffset = 0;
		renderer.skinningBoneTransformsBufferSize = BK_MEGABYTES(4);
		renderer.skinningBoneTransformsBuffer = CreateBuffer({
			.name = "Skinning Bone Transforms",
			.type = GfxBufferType::Storage,
			.access = GfxBufferAccess::GpuOnly,
			.size = renderer.skinningBoneTransformsBufferSize,
		});

		uint32 skinningBindingLayout = CreateBindingLayout({
			.name = "Skinning Layout",
			.bindings = {
				{ .type = GfxBindingType::DynamicReadOnlyStorageBuffer, .stage = GfxBindingStage::Compute },
				{ .type = GfxBindingType::ReadOnlyStorageBuffer, .stage = GfxBindingStage::Compute },
				{ .type = GfxBindingType::ReadOnlyStorageBuffer, .stage = GfxBindingStage::Compute },
				{ .type = GfxBindingType::ReadOnlyStorageBuffer, .stage = GfxBindingStage::Compute },
				{ .type = GfxBindingType::StorageBuffer, .stage = GfxBindingStage::Compute },
			},
		});

		renderer.skinningBindingGroup = CreateBindingGroup({
			.name = "Skinning Group",
			.bindingLayout = skinningBindingLayout,
			.bindings = {
				{ .buffer = renderer.skinningConstantsBuffer, .size = AlignUp(sizeof(SkinningConstants), 256) },
				{ .buffer = renderer.skinningPositionsBuffer },
				{ .buffer = renderer.skinningBoneInfluencesBuffer },
				{ .buffer = renderer.skinningBoneTransformsBuffer },
				{ .buffer = renderer.meshPositionsBuffer },
			},
		});

		// #TODO: Engine assets
		String skinningShaderCode = LoadShader(scratch.arena, "Assets/Skinning.wgsl");
		renderer.skinningPipeline = CreateComputePipeline({
			.name = "Skinning Pipeline",
			.computeShader = {
				.code = skinningShaderCode,
			},
			.bindingLayouts = {
				skinningBindingLayout,
			},
		});
	}

	uint32 CreateMesh(const MeshDesc& desc)
	{
		ArenaScope scratch = GetScratchArena();

		uint32 handle = 0;
		if (Mesh* mesh = AcquireSlot(renderer.meshes, &handle))
		{
			mesh->sections = Copy(renderer.arena, desc.sections);
			mesh->vertexCount = desc.positions.length * 3;

			TSpan<uint8> meshPositions = AsBytes(desc.positions);
			TSpan<uint8> meshIndices = AsBytes(desc.indices);

			BK_ASSERT(renderer.meshPositionsBufferOffset + meshPositions.length <= renderer.meshPositionsBufferSize);
			mesh->positionsOffset = renderer.meshPositionsBufferOffset / sizeof(Vec3f);
			renderer.meshPositionsBufferOffset += WriteBuffer(renderer.meshPositionsBuffer, meshPositions, renderer.meshPositionsBufferOffset);

			BK_ASSERT(renderer.meshIndicesBufferOffset + meshIndices.length <= renderer.meshIndicesBufferSize);
			mesh->indicesOffset = renderer.meshIndicesBufferOffset / sizeof(uint16);
			renderer.meshIndicesBufferOffset += WriteBuffer(renderer.meshIndicesBuffer, meshIndices, renderer.meshIndicesBufferOffset);

			mesh->skinningPositionsOffset = UINT32_MAX;
			mesh->skinningBoneInfluencesOffset = UINT32_MAX;
			mesh->skinningBoneTransformsOffset = UINT32_MAX;

			if (desc.boneIndices.length != 0)
			{
				BK_ASSERT(desc.boneIndices.length == desc.boneWeights.length);

				BK_ASSERT(renderer.skinningPositionsBufferOffset + meshPositions.length <= renderer.skinningPositionsBufferSize);
				mesh->skinningPositionsOffset = renderer.skinningPositionsBufferOffset / sizeof(Vec3f);
				renderer.skinningPositionsBufferOffset += WriteBuffer(renderer.skinningPositionsBuffer, meshPositions, renderer.skinningPositionsBufferOffset);

				TSpan<BoneInfluence> boneInfluences = Push<BoneInfluence>(scratch.arena, desc.boneIndices.length / 4);
				for (size_t boneIdx = 0; boneIdx < boneInfluences.length; ++boneIdx)
				{
					BoneInfluence& boneInfluence = boneInfluences[boneIdx];

					// #TODO: Pack indices down to 16b each once validated
					boneInfluence.indices[0] = desc.boneIndices[boneIdx * 4 + 0];
					boneInfluence.indices[1] = desc.boneIndices[boneIdx * 4 + 1];
					boneInfluence.indices[2] = desc.boneIndices[boneIdx * 4 + 2];
					boneInfluence.indices[3] = desc.boneIndices[boneIdx * 4 + 3];

					boneInfluence.weights[0] = desc.boneWeights[boneIdx * 4 + 0];
					boneInfluence.weights[1] = desc.boneWeights[boneIdx * 4 + 1];
					boneInfluence.weights[2] = desc.boneWeights[boneIdx * 4 + 2];
					boneInfluence.weights[3] = desc.boneWeights[boneIdx * 4 + 3];
				}

				TSpan<uint8> boneInfluencesU8 = AsBytes(boneInfluences);

				BK_ASSERT(renderer.skinningBoneInfluencesBufferOffset + boneInfluencesU8.length <= renderer.skinningBoneInfluencesBufferSize);
				mesh->skinningBoneInfluencesOffset = renderer.skinningBoneInfluencesBufferOffset / sizeof(BoneInfluence);
				renderer.skinningBoneInfluencesBufferOffset += WriteBuffer(renderer.skinningBoneInfluencesBuffer, boneInfluencesU8, renderer.skinningBoneInfluencesBufferOffset);
			}
		}

		return handle;
	}

	void DestroyMesh(uint32 handle)
	{
		// #TODO: Buffer fragmentation; mesh sections allocation
		// Some sort of allocation manager should suffice; e.g. https://github.com/sebbbi/OffsetAllocator
		ReleaseSlot(renderer.meshes, handle);
	}

	// #TODO: Batch skin/draw mesh passes; rudimentary scene management?

	void SkinMesh(uint32 handle, TSpan<Mat4f> boneTransforms)
	{
		Mesh* mesh = GetSlot(renderer.meshes, handle);
		if (!mesh)
		{
			return;
		}

		TSpan<uint8> skinningBoneTransforms = AsBytes(boneTransforms);
		if (mesh->skinningBoneTransformsOffset == UINT32_MAX)
		{
			BK_ASSERT(renderer.skinningBoneTransformsBufferOffset + skinningBoneTransforms.length <= renderer.skinningBoneTransformsBufferSize);
			mesh->skinningBoneTransformsOffset = renderer.skinningBoneTransformsBufferOffset / sizeof(Mat4f);
			renderer.skinningBoneTransformsBufferOffset += skinningBoneTransforms.length;
		}

		WriteBuffer(renderer.skinningBoneTransformsBuffer, skinningBoneTransforms, mesh->skinningBoneTransformsOffset * sizeof(Mat4f));

		SkinningConstants skinningConstants;
		skinningConstants.positionsOffset = mesh->skinningPositionsOffset;
		skinningConstants.boneInfluencesOffset = mesh->skinningBoneInfluencesOffset;
		skinningConstants.boneTransformsOffset = mesh->skinningBoneTransformsOffset;
		skinningConstants.skinnedPositionsOffset = mesh->positionsOffset;
		skinningConstants.vertexCount = mesh->vertexCount;

		// #TODO: Could be per-pass dispatch index
		uint32 meshIdx = mesh - renderer.meshes.slots;
		uint32 skinningConstantsOffset = meshIdx * AlignUp(sizeof(SkinningConstants), 256);
		WriteBuffer(renderer.skinningConstantsBuffer, AsBytes(&skinningConstants, 1), skinningConstantsOffset);

		Dispatch({
			.pipeline = renderer.skinningPipeline,
			.bindingGroups = {
				{
					.bindingGroup = renderer.skinningBindingGroup,
					.dynamicOffsets = {
						skinningConstantsOffset,
					},
				},
			},
			.workgroupCountX = (skinningConstants.vertexCount + 63) / 64,
			.workgroupCountY = 1,
			.workgroupCountZ = 1,
		});
	}

	void DrawMesh(uint32 handle, Mat4f mvp)
	{
		const Mesh* mesh = GetSlot(renderer.meshes, handle);
		if (!mesh)
		{
			return;
		}

		// #TODO: Only needed once per pass
		WriteBuffer(renderer.meshConstantsBuffer, AsBytes(&mvp, 1));

		for (const MeshSection& section : mesh->sections)
		{
			Draw({
				.pipeline = renderer.meshPipeline,
				.bindingGroups = {
					{ .bindingGroup = renderer.meshBindingGroup },
				},
				.vertexBuffers = {
					renderer.meshPositionsBuffer,
				},
				.indexBuffer = renderer.meshIndicesBuffer,
				.vertexOffset = static_cast<uint32>(mesh->positionsOffset + section.vertexOffset),
				.indexOffset = static_cast<uint32>(mesh->indicesOffset + section.indexOffset),
				.triangleCount = static_cast<uint32>(section.triangleCount),
				.instanceCount = 1,
			});
		}
	}
}
