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

	enum class GltfComponentType : int32
	{
		Int8 = 5120,
		Uint8 = 5121,
		Int16 = 5122,
		Uint16 = 5123,
		Uint32 = 5125,
		Float32 = 5126,
	};

	struct GltfAccessor
	{
		int32 bufferView;
		int32 byteOffset;
		int32 componentCount;
		GltfComponentType componentType;
		bool normalized;
		int32 count;
	};

	struct GltfBufferView
	{
		int32 buffer;
		int32 byteOffset;
		int32 byteLength;
		int32 byteStride;
	};

	struct GltfBuffer
	{
		String uri;
		int32 byteLength;
	};

	int32 GetComponentTypeSize(GltfComponentType componentType)
	{
		switch (componentType)
		{
			case GltfComponentType::Int8:
			case GltfComponentType::Uint8:
				return 1;

			case GltfComponentType::Int16:
			case GltfComponentType::Uint16:
				return 2;

			case GltfComponentType::Uint32:
			case GltfComponentType::Float32:
				return 4;

			default: return 0;
		}
	}

	bool GetAccessor(JsonValue* gltfAccessors, size_t idx, GltfAccessor& result)
	{
		JsonValue* gltfAccessor = FindJsonValueInArray(gltfAccessors, idx);
		if (!gltfAccessor)
		{
			return false;
		}

		result = {};

		if (JsonValue* bufferView = FindJsonValueInObject(gltfAccessor, "bufferView"))
		{
			result.bufferView = static_cast<int32>(bufferView->asNumber);
		}
		else
		{
			result.bufferView = -1;
		}

		if (JsonValue* byteOffset = FindJsonValueInObject(gltfAccessor, "byteOffset"))
		{
			result.byteOffset = static_cast<int32>(byteOffset->asNumber);
		}

		if (JsonValue* componentType = FindJsonValueInObject(gltfAccessor, "componentType"))
		{
			result.componentType = static_cast<GltfComponentType>(componentType->asNumber);
		}

		if (JsonValue* type = FindJsonValueInObject(gltfAccessor, "type"))
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

		if (JsonValue* normalized = FindJsonValueInObject(gltfAccessor, "normalized"))
		{
			result.normalized = normalized->asBool;
		}

		if (JsonValue* count = FindJsonValueInObject(gltfAccessor, "count"))
		{
			result.count = static_cast<int32>(count->asNumber);
		}

		return true;
	}

	bool GetBufferView(JsonValue* gltfBufferViews, size_t idx, GltfBufferView& result)
	{
		JsonValue* gltfBufferView = FindJsonValueInArray(gltfBufferViews, idx);
		if (!gltfBufferView)
		{
			return false;
		}

		result = {};

		if (JsonValue* buffer = FindJsonValueInObject(gltfBufferView, "buffer"))
		{
			result.buffer = static_cast<int32>(buffer->asNumber);
		}

		if (JsonValue* byteOffset = FindJsonValueInObject(gltfBufferView, "byteOffset"))
		{
			result.byteOffset = static_cast<int32>(byteOffset->asNumber);
		}

		if (JsonValue* byteLength = FindJsonValueInObject(gltfBufferView, "byteLength"))
		{
			result.byteLength = static_cast<int32>(byteLength->asNumber);
		}

		if (JsonValue* byteStride = FindJsonValueInObject(gltfBufferView, "byteStride"))
		{
			result.byteStride = static_cast<int32>(byteStride->asNumber);
		}

		return true;
	}

	bool GetBuffer(JsonValue* gltfBuffers, size_t idx, GltfBuffer& result)
	{
		JsonValue* gltfBuffer = FindJsonValueInArray(gltfBuffers, idx);
		if (!gltfBuffer)
		{
			return false;
		}

		result = {};

		if (JsonValue* bufferUri = FindJsonValueInObject(gltfBuffer, "uri"))
		{
			result.uri = bufferUri->value;
		}

		if (JsonValue* byteLength = FindJsonValueInObject(gltfBuffer, "byteLength"))
		{
			result.byteLength = static_cast<int32>(byteLength->asNumber);
		}

		return true;
	}

	TSpan<uint8> MaterializeBuffer(Arena& arena, JsonValue* gltf, TSpan<uint8> gltfBuffer, const GltfAccessor& accessor)
	{
		TSpan<uint8> result = {};

		JsonValue* gltfBufferViews = FindJsonValue(gltf, "bufferViews");
		JsonValue* gltfBuffers = FindJsonValue(gltf, "buffers");

		GltfBufferView bufferView;
		if (!GetBufferView(gltfBufferViews, accessor.bufferView, bufferView))
		{
			return result;
		}

		// #TODO: strided copy
		BK_ASSERT(bufferView.byteStride == 0);

		GltfBuffer buffer;
		if (!GetBuffer(gltfBuffers, bufferView.buffer, buffer))
		{
			return result;
		}

		// #TODO: load from base64/file uri
		BK_ASSERT(buffer.uri.length == 0);

		result = arena.Push<uint8>(GetComponentTypeSize(accessor.componentType) * accessor.count * accessor.componentCount);
		MemoryCopy(result.data, gltfBuffer.data + bufferView.byteOffset + accessor.byteOffset, result.length);

		return result;
	}

	bool LoadGlbMeshes(Arena& arena, String filePath, TSpan<Mesh>& meshes)
	{
		ArenaScope scratch = GetScratchArena(&arena);

		FileHandle fileHandle = OpenFile(filePath, FileAccess::Read);
		if (!fileHandle)
		{
			return false;
		}

		bool result = false;

		TSpan<uint8> fileData = scratch.arena.Push<uint8>(GetFileSize(fileHandle));
		if (ReadFile(fileHandle, fileData) == fileData.length)
		{
			result = LoadGlbMeshes(arena, fileData, meshes);
		}

		CloseFile(fileHandle);

		return result;
	}

	bool LoadGlbMeshes(Arena& arena, TSpan<uint8> fileData, TSpan<Mesh>& meshes)
	{
		ArenaScope scratch = GetScratchArena(&arena);

		GlbHeader* header = (GlbHeader*)fileData.data;
		BK_ASSERT(header->magic == 0x46546C67);
		BK_ASSERT(header->version == 2);
		BK_ASSERT(header->length == fileData.length);

		GlbChunk* jsonChunk = (GlbChunk*)(header + 1);
		BK_ASSERT(jsonChunk->type == 0x4E4F534A);
		uint8* jsonChunkData = (uint8*)(jsonChunk + 1);

		// #TODO: Binary chunk could be omitted
		GlbChunk* binaryChunk = (GlbChunk*)(jsonChunkData + jsonChunk->length);
		BK_ASSERT(binaryChunk->type == 0x004E4942);
		uint8* binaryChunkData = (uint8*)(binaryChunk + 1);

		JsonValue* gltf = ParseJson(scratch.arena, String((char*)jsonChunkData, jsonChunk->length));
		TSpan<uint8> gltfBuffer = TSpan(binaryChunkData, binaryChunk->length);

		JsonValue* gltfAccessors = FindJsonValue(gltf, "accessors");
		JsonValue* gltfMeshes = FindJsonValue(gltf, "meshes");

		if (!gltfMeshes || gltfMeshes->children == 0)
		{
			return false;
		}

		meshes = arena.PushZeroed<Mesh>(gltfMeshes->children);

		size_t meshIdx = 0;
		for (JsonValue* gltfMesh = FindJsonValueInArray(gltfMeshes, 0); gltfMesh; gltfMesh = gltfMesh->sibling, ++meshIdx)
		{
			Mesh& mesh = meshes[meshIdx];

			JsonValue* primitives = FindJsonValue(gltfMesh, "primitives");
			if (!primitives || primitives->children == 0)
			{
				continue;
			}

			mesh.sections = arena.PushZeroed<MeshSection>(primitives->children);

			// #TODO: Could have different typed buffers across sections; MaterializeBuffer should probably do some conforming

			TSpan<TSpan<uint8>> positionBuffers = scratch.arena.PushZeroed<TSpan<uint8>>(primitives->children);
			size_t positionBufferSize = 0;
			size_t vertexCount = 0;

			TSpan<TSpan<uint8>> indexBuffers = scratch.arena.PushZeroed<TSpan<uint8>>(primitives->children);
			size_t indexBufferSize = 0;
			size_t indexCount = 0;

			size_t sectionIdx = 0;
			for (JsonValue* primitive = FindJsonValueInArray(primitives, 0); primitive; primitive = primitive->sibling, ++sectionIdx)
			{
				MeshSection& section = mesh.sections[sectionIdx];
				section.vertexOffset = vertexCount;
				section.indexOffset = indexCount;

				JsonValue* positionAttrib = FindJsonValue(primitive, "attributes.POSITION");

				GltfAccessor positionAccessor;
				if (positionAttrib && GetAccessor(gltfAccessors, size_t(positionAttrib->asNumber), positionAccessor))
				{
					positionBuffers[sectionIdx] = MaterializeBuffer(scratch.arena, gltf, gltfBuffer, positionAccessor);
					positionBufferSize += positionBuffers[sectionIdx].length;
					vertexCount += positionAccessor.componentCount * positionAccessor.count;
				}

				JsonValue* indicesAttrib = FindJsonValue(primitive, "indices");

				GltfAccessor indicesAccessor;
				if (indicesAttrib && GetAccessor(gltfAccessors, size_t(indicesAttrib->asNumber), indicesAccessor))
				{
					indexBuffers[sectionIdx] = MaterializeBuffer(scratch.arena, gltf, gltfBuffer, indicesAccessor);
					indexBufferSize += indexBuffers[sectionIdx].length;
					indexCount += indicesAccessor.componentCount * indicesAccessor.count;
				}

				section.triangleCount = (indexCount - section.indexOffset) / 3;
			}

			mesh.positionBuffer = arena.Push<uint8>(positionBufferSize);

			uint8* positionBuffer = mesh.positionBuffer.data;
			for (TSpan buffer : positionBuffers)
			{
				MemoryCopy(positionBuffer, buffer.data, buffer.length);
				positionBuffer += buffer.length;
			}

			mesh.indexBuffer = arena.Push<uint8>(indexBufferSize);

			uint8* indexBuffer = mesh.indexBuffer.data;
			for (TSpan buffer : indexBuffers)
			{
				MemoryCopy(indexBuffer, buffer.data, buffer.length);
				indexBuffer += buffer.length;
			}
		}

		return true;
	}
}
