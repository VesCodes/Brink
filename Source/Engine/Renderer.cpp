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

	// #TODO: uint32 -> uint64 offset/size

	struct SkinningConstants
	{
		uint32 positionsOffset;
		uint32 boneIndicesOffset;
		uint32 boneWeightsOffset;
		uint32 boneTransformsOffset;
		uint32 skinnedPositionsOffset;
		uint32 vertexCount;
	};

	struct Mesh
	{
		TSpan<MeshSection> sections;
		uint32 vertexCount;

		uint32 positionsOffset;
		uint32 positionsSize;

		uint32 indicesOffset;
		uint32 indicesSize;

		uint32 skinningPositionsOffset;
		uint32 skinningPositionsSize;

		uint32 skinningBoneIndicesOffset;
		uint32 skinningBoneIndicesSize;

		uint32 skinningBoneWeightsOffset;
		uint32 skinningBoneWeightsSize;

		uint32 skinningBoneTransformsOffset;
		uint32 skinningBoneTransformsSize;
	};

	struct
	{
		Arena arena;

		TPool<Mesh> meshes;

		uint32 meshBufferOffset;
		uint32 meshBufferSize;
		uint32 meshBuffer;

		uint32 meshConstantsBuffer;
		uint32 meshBindingGroup;
		uint32 meshPipeline;

		uint32 skinningConstantsBuffer;
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

		renderer.meshBufferOffset = 0;
		renderer.meshBufferSize = BK_MEGABYTES(16);
		renderer.meshBuffer = CreateBuffer({
			.name = "Mesh Buffer",
			.type = GfxBufferType::Storage | GfxBufferType::Vertex | GfxBufferType::Index,
			.access = GfxBufferAccess::GpuOnly,
			.size = renderer.meshBufferSize,
		});

		renderer.meshConstantsBuffer = CreateBuffer({
			.name = "Mesh Constants",
			.type = GfxBufferType::Uniform,
			.access = GfxBufferAccess::GpuOnly,
			.size = sizeof(MeshConstants),
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

		renderer.skinningConstantsBuffer = CreateBuffer({
			.name = "Skinning Constants",
			.type = GfxBufferType::Storage,
			.access = GfxBufferAccess::GpuOnly,
			.size = AlignUp(sizeof(SkinningConstants), 256) * renderer.meshes.capacity,
		});

		uint32 skinningBindingLayout = CreateBindingLayout({
			.name = "Skinning Layout",
			.bindings = {
				{ .type = GfxBindingType::DynamicReadOnlyStorageBuffer, .stage = GfxBindingStage::Compute },
				{ .type = GfxBindingType::StorageBuffer, .stage = GfxBindingStage::Compute },
			},
		});

		renderer.skinningBindingGroup = CreateBindingGroup({
			.name = "Skinning Group",
			.bindingLayout = skinningBindingLayout,
			.bindings = {
				{ .buffer = renderer.skinningConstantsBuffer, .size = AlignUp(sizeof(SkinningConstants), 256) },
				{ .buffer = renderer.meshBuffer },
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
			mesh->vertexCount = desc.positions.length;

			TSpan<uint8> meshPositions = AsBytes(desc.positions);
			TSpan<uint8> meshIndices = AsBytes(desc.indices);

			BK_ASSERT(renderer.meshBufferOffset + meshPositions.length <= renderer.meshBufferSize);
			mesh->positionsOffset = renderer.meshBufferOffset;
			mesh->positionsSize = WriteBuffer(renderer.meshBuffer, meshPositions, renderer.meshBufferOffset);
			renderer.meshBufferOffset += mesh->positionsSize;

			BK_ASSERT(renderer.meshBufferOffset + meshIndices.length <= renderer.meshBufferSize);
			mesh->indicesOffset = renderer.meshBufferOffset;
			mesh->indicesSize = WriteBuffer(renderer.meshBuffer, meshIndices, renderer.meshBufferOffset);
			renderer.meshBufferOffset += mesh->indicesSize;

			mesh->skinningPositionsOffset = UINT32_MAX;
			mesh->skinningBoneIndicesOffset = UINT32_MAX;
			mesh->skinningBoneWeightsOffset = UINT32_MAX;
			mesh->skinningBoneTransformsOffset = UINT32_MAX;

			if (desc.boneIndices.length != 0)
			{
				BK_ASSERT(desc.boneIndices.length == desc.boneWeights.length);

				BK_ASSERT(renderer.meshBufferOffset + meshPositions.length <= renderer.meshBufferSize);
				mesh->skinningPositionsOffset = renderer.meshBufferOffset;
				mesh->skinningPositionsSize = WriteBuffer(renderer.meshBuffer, meshPositions, renderer.meshBufferOffset);
				renderer.meshBufferOffset += mesh->skinningPositionsSize;

				// #TODO: Change bone indices to uint16 in skinning shader + when loading MeshDesc
				TSpan<uint32> boneIndicesU32 = Push<uint32>(scratch.arena, desc.boneIndices.length);
				for (size_t i = 0; i < boneIndicesU32.length; ++i)
				{
					boneIndicesU32[i] = desc.boneIndices[i];
				}

				TSpan<uint8> boneIndices = AsBytes(boneIndicesU32);
				TSpan<uint8> boneWeights = AsBytes(desc.boneWeights);

				BK_ASSERT(renderer.meshBufferOffset + boneIndices.length <= renderer.meshBufferSize);
				mesh->skinningBoneIndicesOffset = renderer.meshBufferOffset;
				mesh->skinningBoneIndicesSize = WriteBuffer(renderer.meshBuffer, boneIndices, renderer.meshBufferOffset);
				renderer.meshBufferOffset += mesh->skinningBoneIndicesSize;

				BK_ASSERT(renderer.meshBufferOffset + boneWeights.length <= renderer.meshBufferSize);
				mesh->skinningBoneWeightsOffset = renderer.meshBufferOffset;
				mesh->skinningBoneWeightsSize = WriteBuffer(renderer.meshBuffer, boneWeights, renderer.meshBufferOffset);
				renderer.meshBufferOffset += mesh->skinningBoneWeightsSize;
			}
		}

		return handle;
	}

	void DestroyMesh(uint32 handle)
	{
		// #TODO: Mesh buffer fragmentation; mesh sections allocation
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
			BK_ASSERT(renderer.meshBufferOffset + skinningBoneTransforms.length <= renderer.meshBufferSize);
			mesh->skinningBoneTransformsOffset = renderer.meshBufferOffset;
			mesh->skinningBoneTransformsSize = WriteBuffer(renderer.meshBuffer, skinningBoneTransforms, mesh->skinningBoneTransformsOffset);
			renderer.meshBufferOffset += mesh->skinningBoneTransformsSize;
		}
		else
		{
			BK_ASSERT(skinningBoneTransforms.length <= mesh->skinningBoneTransformsSize);
			WriteBuffer(renderer.meshBuffer, skinningBoneTransforms, mesh->skinningBoneTransformsOffset);
		}

		SkinningConstants skinningConstants;
		skinningConstants.positionsOffset = mesh->skinningPositionsOffset;
		skinningConstants.boneIndicesOffset = mesh->skinningBoneIndicesOffset;
		skinningConstants.boneWeightsOffset = mesh->skinningBoneWeightsOffset;
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
					{ .buffer = renderer.meshBuffer, .offset = mesh->positionsOffset, .size = mesh->positionsSize },
				},
				.indexBuffer = { .buffer = renderer.meshBuffer, .offset = mesh->indicesOffset, .size = mesh->indicesSize },
				.vertexOffset = static_cast<uint32>(section.vertexOffset),
				.indexOffset = static_cast<uint32>(section.indexOffset),
				.triangleCount = static_cast<uint32>(section.triangleCount),
				.instanceCount = 1,
			});
		}
	}
}
