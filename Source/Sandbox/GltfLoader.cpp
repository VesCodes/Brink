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
		GltfComponentType componentType;
		int32 componentElements;
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

	struct GltfSkin
	{
		int32 inverseBindMatrices;
		int32 skeleton;
		TSpan<int32> joints;
	};

	struct GltfAsset
	{
		String filePath;
		TSpan<uint8> buffer;

		TSpan<GltfAccessor> accessors;
		TSpan<GltfBufferView> bufferViews;
		TSpan<GltfBuffer> buffers;
		TSpan<GltfSkin> skins;
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

	GltfAccessor ParseAccessor(JsonValue* accessor)
	{
		GltfAccessor result = {};

		if (JsonValue* bufferView = FindJsonValueInObject(accessor, "bufferView"))
		{
			result.bufferView = static_cast<int32>(bufferView->asNumber);
		}
		else
		{
			result.bufferView = -1;
		}

		if (JsonValue* byteOffset = FindJsonValueInObject(accessor, "byteOffset"))
		{
			result.byteOffset = static_cast<int32>(byteOffset->asNumber);
		}

		if (JsonValue* componentType = FindJsonValueInObject(accessor, "componentType"))
		{
			result.componentType = static_cast<GltfComponentType>(componentType->asNumber);
		}

		if (JsonValue* type = FindJsonValueInObject(accessor, "type"))
		{
			if (type->value == "SCALAR")
			{
				result.componentElements = 1;
			}
			else if (type->value == "VEC2")
			{
				result.componentElements = 2;
			}
			else if (type->value == "VEC3")
			{
				result.componentElements = 3;
			}
			else if (type->value == "VEC4" || type->value == "MAT2")
			{
				result.componentElements = 4;
			}
			else if (type->value == "MAT3")
			{
				result.componentElements = 9;
			}
			else if (type->value == "MAT4")
			{
				result.componentElements = 16;
			}
		}

		if (JsonValue* normalized = FindJsonValueInObject(accessor, "normalized"))
		{
			result.normalized = normalized->asBool;
		}

		if (JsonValue* count = FindJsonValueInObject(accessor, "count"))
		{
			result.count = static_cast<int32>(count->asNumber);
		}

		return result;
	}

	GltfBufferView ParseBufferView(JsonValue* bufferView)
	{
		GltfBufferView result = {};

		if (JsonValue* buffer = FindJsonValueInObject(bufferView, "buffer"))
		{
			result.buffer = static_cast<int32>(buffer->asNumber);
		}

		if (JsonValue* byteOffset = FindJsonValueInObject(bufferView, "byteOffset"))
		{
			result.byteOffset = static_cast<int32>(byteOffset->asNumber);
		}

		if (JsonValue* byteLength = FindJsonValueInObject(bufferView, "byteLength"))
		{
			result.byteLength = static_cast<int32>(byteLength->asNumber);
		}

		if (JsonValue* byteStride = FindJsonValueInObject(bufferView, "byteStride"))
		{
			result.byteStride = static_cast<int32>(byteStride->asNumber);
		}

		return result;
	}

	GltfBuffer ParseBuffer(JsonValue* buffer)
	{
		GltfBuffer result = {};

		if (JsonValue* bufferUri = FindJsonValueInObject(buffer, "uri"))
		{
			result.uri = bufferUri->value;
		}

		if (JsonValue* byteLength = FindJsonValueInObject(buffer, "byteLength"))
		{
			result.byteLength = static_cast<int32>(byteLength->asNumber);
		}

		return result;
	}

	GltfSkin ParseSkin(Arena& arena, JsonValue* skin)
	{
		GltfSkin result = {};

		if (JsonValue* inverseBindMatrices = FindJsonValueInObject(skin, "inverseBindMatrices"))
		{
			result.inverseBindMatrices = static_cast<int32>(inverseBindMatrices->asNumber);
		}
		else
		{
			result.inverseBindMatrices = -1;
		}

		if (JsonValue* skeleton = FindJsonValueInObject(skin, "skeleton"))
		{
			result.skeleton = static_cast<int32>(skeleton->asNumber);
		}
		else
		{
			result.skeleton = -1;
		}

		if (JsonValue* joints = FindJsonValueInObject(skin, "joints"))
		{
			result.joints = arena.Push<int32>(joints->children);

			size_t jointIdx = 0;
			for (JsonValue* joint = FindJsonValueInArray(joints, 0); joint; joint = joint->sibling, ++jointIdx)
			{
				result.joints[jointIdx] = static_cast<int32>(joint->asNumber);
			}
		}

		return result;
	}

	GltfAsset ParseGltf(Arena& arena, JsonValue* gltf)
	{
		GltfAsset result = {};

		JsonValue* gltfAccessors = FindJsonValueInObject(gltf, "accessors");
		if (gltfAccessors && gltfAccessors->children > 0)
		{
			result.accessors = arena.Push<GltfAccessor>(gltfAccessors->children);

			size_t accessorIdx = 0;
			for (JsonValue* gltfAccessor = FindJsonValueInArray(gltfAccessors, 0); gltfAccessor; gltfAccessor = gltfAccessor->sibling, ++accessorIdx)
			{
				result.accessors[accessorIdx] = ParseAccessor(gltfAccessor);
			}
		}

		JsonValue* gltfBufferViews = FindJsonValueInObject(gltf, "bufferViews");
		if (gltfBufferViews && gltfBufferViews->children > 0)
		{
			result.bufferViews = arena.Push<GltfBufferView>(gltfBufferViews->children);

			size_t bufferViewIdx = 0;
			for (JsonValue* gltfBufferView = FindJsonValueInArray(gltfBufferViews, 0); gltfBufferView; gltfBufferView = gltfBufferView->sibling, ++bufferViewIdx)
			{
				result.bufferViews[bufferViewIdx] = ParseBufferView(gltfBufferView);
			}
		}

		JsonValue* gltfBuffers = FindJsonValueInObject(gltf, "buffers");
		if (gltfBuffers && gltfBuffers->children > 0)
		{
			result.buffers = arena.Push<GltfBuffer>(gltfBuffers->children);

			size_t bufferIdx = 0;
			for (JsonValue* gltfBuffer = FindJsonValueInArray(gltfBuffers, 0); gltfBuffer; gltfBuffer = gltfBuffer->sibling, ++bufferIdx)
			{
				result.buffers[bufferIdx] = ParseBuffer(gltfBuffer);
			}
		}

		JsonValue* gltfSkins = FindJsonValueInObject(gltf, "skins");
		if (gltfSkins && gltfSkins->children > 0)
		{
			result.skins = arena.Push<GltfSkin>(gltfSkins->children);

			size_t skinIdx = 0;
			for (JsonValue* gltfSkin = FindJsonValueInArray(gltfSkins, 0); gltfSkin; gltfSkin = gltfSkin->sibling, ++skinIdx)
			{
				result.skins[skinIdx] = ParseSkin(arena, gltfSkin);
			}
		}

		return result;
	}

	TSpan<uint8> MaterializeBuffer(Arena& arena, const GltfAsset& asset, const GltfAccessor& accessor)
	{
		TSpan<uint8> result = {};

		if (accessor.bufferView == -1)
		{
			return result;
		}

		const GltfBufferView& bufferView = asset.bufferViews[accessor.bufferView];
		const GltfBuffer& buffer = asset.buffers[bufferView.buffer];

		// #TODO: load from base64/file uri
		BK_ASSERT(buffer.uri.length == 0);
		BK_ASSERT(asset.buffer.length != 0);

		size_t componentSize = GetComponentTypeSize(accessor.componentType) * accessor.componentElements;

		result = arena.Push<uint8>(componentSize * accessor.count);

		uint8* srcBufferPtr = asset.buffer.data + bufferView.byteOffset + accessor.byteOffset;
		uint8* dstBufferPtr = result.data;

		if (bufferView.byteStride == 0)
		{
			MemoryCopy(dstBufferPtr, srcBufferPtr, result.length);
		}
		else
		{
			for (size_t componentIdx = 0; componentIdx < accessor.count; ++componentIdx)
			{
				MemoryCopy(dstBufferPtr, srcBufferPtr, componentSize);

				srcBufferPtr += bufferView.byteStride;
				dstBufferPtr += componentSize;
			}
		}

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

		JsonValue* gltfMeshes = FindJsonValueInObject(gltf, "meshes");
		if (!gltfMeshes || gltfMeshes->children == 0)
		{
			return false;
		}

		GltfAsset asset = ParseGltf(scratch.arena, gltf);
		asset.filePath = String::Empty;
		asset.buffer = TSpan(binaryChunkData, binaryChunk->length);

		meshes = arena.PushZeroed<Mesh>(gltfMeshes->children);

		size_t meshIdx = 0;
		for (JsonValue* gltfMesh = FindJsonValueInArray(gltfMeshes, 0); gltfMesh; gltfMesh = gltfMesh->sibling, ++meshIdx)
		{
			Mesh& mesh = meshes[meshIdx];

			JsonValue* primitives = FindJsonValueInObject(gltfMesh, "primitives");
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

				if (JsonValue* positionAttrib = FindJsonValue(primitive, "attributes.POSITION"))
				{
					const GltfAccessor& accessor = asset.accessors[size_t(positionAttrib->asNumber)];

					positionBuffers[sectionIdx] = MaterializeBuffer(scratch.arena, asset, accessor);
					positionBufferSize += positionBuffers[sectionIdx].length;
					vertexCount += accessor.count;

					section.triangleCount = accessor.count / 3;
				}

				if (JsonValue* indicesAttrib = FindJsonValueInObject(primitive, "indices"))
				{
					const GltfAccessor& accessor = asset.accessors[size_t(indicesAttrib->asNumber)];

					indexBuffers[sectionIdx] = MaterializeBuffer(scratch.arena, asset, accessor);
					indexBufferSize += indexBuffers[sectionIdx].length;
					indexCount += accessor.count;

					section.triangleCount = accessor.count / 3;
				}
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
