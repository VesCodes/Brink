#include "GltfLoader.h"

#include "Core/Json.h"
#include "Core/Memory.h"
#include "Core/Platform.h"

namespace Bk
{
	struct GlbHeader
	{
		uint32 magic;
		uint32 version;
		uint32 length;
	};

	struct GlbChunk
	{
		uint32 length;
		uint32 type;
	};

	struct GltfAccessor
	{
		int32 bufferView;
		int32 byteOffset;
		int32 componentType;
		int32 componentCount;
		bool normalized;
		int32 count;
	};

	constexpr int32 GltfComponentType_Int8 = 5120;
	constexpr int32 GltfComponentType_Uint8 = 5121;
	constexpr int32 GltfComponentType_Int16 = 5122;
	constexpr int32 GltfComponentType_Uint16 = 5123;
	constexpr int32 GltfComponentType_Uint32 = 5125;
	constexpr int32 GltfComponentType_Float32 = 5126;

	int32 GetGltfComponentTypeSize(int32 componentType)
	{
		switch (componentType)
		{
			case GltfComponentType_Int8:
			case GltfComponentType_Uint8:
				return 1;

			case GltfComponentType_Int16:
			case GltfComponentType_Uint16:
				return 2;

			case GltfComponentType_Uint32:
			case GltfComponentType_Float32:
				return 4;

			default: return 0;
		}
	}

	struct GltfBufferView
	{
		int32 buffer;
		int32 byteOffset;
		int32 byteLength;
		int32 byteStride;
	};

	struct GltfLoader
	{
		Arena& arena;
		String filePath;
		JsonValue* gltf;
		TSpan<uint8> buffer;
	};

	GltfAccessor ParseAccessor(JsonValue* value)
	{
		GltfAccessor result = {};

		if (JsonValue* bufferView = FindJsonValue(value, "bufferView"))
		{
			result.bufferView = static_cast<int32>(bufferView->asNumber);
		}
		else
		{
			result.bufferView = -1;
		}

		if (JsonValue* byteOffset = FindJsonValue(value, "byteOffset"))
		{
			result.byteOffset = static_cast<int32>(byteOffset->asNumber);
		}

		if (JsonValue* componentType = FindJsonValue(value, "componentType"))
		{
			result.componentType = static_cast<int32>(componentType->asNumber);
		}

		if (JsonValue* type = FindJsonValue(value, "type"))
		{
			if (type->value == "SCALAR")
			{
				result.componentCount = 1;
			}
			else if (type->value == "VEC2")
			{
				result.componentCount = 2;
			}
			else if (type->value == "VEC3")
			{
				result.componentCount = 3;
			}
			else if (type->value == "VEC4" || type->value == "MAT2")
			{
				result.componentCount = 4;
			}
			else if (type->value == "MAT3")
			{
				result.componentCount = 9;
			}
			else if (type->value == "MAT4")
			{
				result.componentCount = 16;
			}
		}

		if (JsonValue* normalized = FindJsonValue(value, "normalized"))
		{
			result.normalized = normalized->asBool;
		}

		if (JsonValue* count = FindJsonValue(value, "count"))
		{
			result.count = static_cast<int32>(count->asNumber);
		}

		return result;
	}

	GltfBufferView ParseBufferView(JsonValue* value)
	{
		GltfBufferView result = {};

		if (JsonValue* buffer = FindJsonValue(value, "buffer"))
		{
			result.buffer = static_cast<int32>(buffer->asNumber);
		}

		if (JsonValue* byteOffset = FindJsonValue(value, "byteOffset"))
		{
			result.byteOffset = static_cast<int32>(byteOffset->asNumber);
		}

		if (JsonValue* byteLength = FindJsonValue(value, "byteLength"))
		{
			result.byteLength = static_cast<int32>(byteLength->asNumber);
		}

		if (JsonValue* byteStride = FindJsonValue(value, "byteStride"))
		{
			result.byteStride = static_cast<int32>(byteStride->asNumber);
		}

		return result;
	}

	bool GetBufferFromAccessor(GltfLoader& loader, size_t accessorIdx, TSpan<uint8>& result)
	{
		JsonValue* gltfAccessor = FindJsonValueInArray(FindJsonValue(loader.gltf, "accessors"), accessorIdx);
		if (!gltfAccessor)
		{
			return false;
		}

		GltfAccessor accessor = ParseAccessor(gltfAccessor);
		int32 accessorByteLength = GetGltfComponentTypeSize(accessor.componentType) * accessor.componentCount * accessor.count;

		JsonValue* gltfBufferView = FindJsonValueInArray(FindJsonValue(loader.gltf, "bufferViews"), accessor.bufferView);
		if (!gltfBufferView)
		{
			return false;
		}

		GltfBufferView bufferView = ParseBufferView(gltfBufferView);
		BK_ASSERT(accessorByteLength <= bufferView.byteLength);
		BK_ASSERTF(bufferView.byteStride == 0, "Not implemented"); // #TODO: Strided copy

		JsonValue* gltfBuffer = FindJsonValueInArray(FindJsonValue(loader.gltf, "buffers"), bufferView.buffer);
		if (!gltfBuffer)
		{
			return false;
		}

		if (JsonValue* bufferUri = FindJsonValue(gltfBuffer, "uri"))
		{
			BK_ASSERTF(!bufferUri->value.StartsWith("data:"), "Not implemented"); // #TODO: Base64 decode

			ArenaScope scratch = GetScratchArena(&loader.arena);

			StringBuilder builder(scratch.arena);
			builder.AppendPath(GetDirectoryName(loader.filePath));
			builder.AppendPath(bufferUri->value);

			String filePath = builder.ToString(scratch.arena);
			FileHandle fileHandle = OpenFile(filePath, FileAccess::Read);

			if (!fileHandle)
			{
				return false;
			}

			TSpan<uint8> fileBuffer = scratch.arena.Push<uint8>(GetFileSize(fileHandle));
			if (ReadFile(fileHandle, fileBuffer) != fileBuffer.length)
			{
				CloseFile(fileHandle);
				return false;
			}

			CloseFile(fileHandle);

			result = loader.arena.Push<uint8>(accessorByteLength);
			MemoryCopy(result.data, fileBuffer.data + bufferView.byteOffset + accessor.byteOffset, result.length);
		}
		else
		{
			result = loader.arena.Push<uint8>(accessorByteLength);
			MemoryCopy(result.data, loader.buffer.data + bufferView.byteOffset + accessor.byteOffset, result.length);
		}

		return true;
	}

	bool LoadGltfMeshes(Arena& arena, String filePath, TSpan<Mesh>& meshes)
	{
		ArenaScope scratch = GetScratchArena(&arena);

		FileHandle fileHandle = OpenFile(filePath, FileAccess::Read);
		if (!fileHandle)
		{
			return false;
		}

		TSpan<uint8> fileBuffer = scratch.arena.Push<uint8>(GetFileSize(fileHandle));
		if (ReadFile(fileHandle, fileBuffer) != fileBuffer.length)
		{
			CloseFile(fileHandle);
			return false;
		}

		CloseFile(fileHandle);

		GlbHeader* header = (GlbHeader*)fileBuffer.data;
		BK_ASSERT(header->magic == 0x46546C67);
		BK_ASSERT(header->version == 2);
		BK_ASSERT(header->length == fileBuffer.length);

		GlbChunk* jsonChunk = (GlbChunk*)(header + 1);
		BK_ASSERT(jsonChunk->type == 0x4E4F534A);
		uint8* jsonChunkData = (uint8*)(jsonChunk + 1);

		// #TODO: Binary chunk could be omitted
		GlbChunk* binaryChunk = (GlbChunk*)(jsonChunkData + jsonChunk->length);
		BK_ASSERT(binaryChunk->type == 0x004E4942);
		uint8* binaryChunkData = (uint8*)(binaryChunk + 1);

		GltfLoader loader = { .arena = arena, .filePath = filePath };
		loader.gltf = ParseJson(scratch.arena, String((char*)jsonChunkData, jsonChunk->length));
		loader.buffer = TSpan(binaryChunkData, binaryChunk->length);

		JsonValue* gltfMeshes = FindJsonValue(loader.gltf, "meshes");
		if (!gltfMeshes || gltfMeshes->children == 0)
		{
			return false;
		}

		meshes = loader.arena.PushZeroed<Mesh>(gltfMeshes->children);

		size_t meshIdx = 0;
		for (JsonValue* gltfMesh = FindJsonValueInArray(gltfMeshes, 0); gltfMesh; gltfMesh = gltfMesh->sibling, ++meshIdx)
		{
			Mesh& mesh = meshes[meshIdx];

			JsonValue* primitives = FindJsonValue(gltfMesh, "primitives");
			if (!primitives || primitives->children == 0)
			{
				continue;
			}

			mesh.sections = loader.arena.PushZeroed<MeshSection>(primitives->children);

			size_t sectionIdx = 0;
			for (JsonValue* primitive = FindJsonValueInArray(primitives, 0); primitive; primitive = primitive->sibling, ++sectionIdx)
			{
				MeshSection& section = mesh.sections[sectionIdx];

				JsonValue* positionAttrib = FindJsonValue(primitive, "attributes.POSITION");
				if (positionAttrib)
				{
					GetBufferFromAccessor(loader, size_t(positionAttrib->asNumber), section.positionBuffer);
				}

				JsonValue* indicesAttrib = FindJsonValue(primitive, "indices");
				if (indicesAttrib)
				{
					GetBufferFromAccessor(loader, size_t(indicesAttrib->asNumber), section.indexBuffer);
				}
			}
		}

		return true;
	}
}
