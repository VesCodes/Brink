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

	struct GltfNode
	{
		String name;
		TSpan<int32> children;
		int32 mesh;
		int32 skin;
		HMM_Vec3 translation;
		HMM_Quat rotation;
		HMM_Vec3 scale;
	};

	struct GltfSkin
	{
		String name;
		int32 inverseBindMatrices;
		int32 skeleton;
		TSpan<int32> joints;
	};

	enum class GltfAnimationTarget : uint8
	{
		Translation,
		Rotation,
		Scale,
		Weights,
	};

	struct GltfAnimationChannel
	{
		int32 sampler;
		int32 node;
		GltfAnimationTarget target;
	};

	enum class GltfAnimationInterpolation : uint8
	{
		Linear,
		Step,
		CubicSpline,
	};

	struct GltfAnimationSampler
	{
		int32 input;
		int32 output;
		GltfAnimationInterpolation interpolation;
	};

	struct GltfAnimation
	{
		String name;
		TSpan<GltfAnimationChannel> channels;
		TSpan<GltfAnimationSampler> samplers;
	};

	struct GltfAsset
	{
		String filePath;
		TSpan<uint8> buffer;

		TSpan<GltfAccessor> accessors;
		TSpan<GltfBufferView> bufferViews;
		TSpan<GltfBuffer> buffers;
		TSpan<GltfNode> nodes;
		TSpan<GltfSkin> skins;
		TSpan<GltfAnimation> animations;
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

	GltfNode ParseNode(Arena& arena, JsonValue* node)
	{
		GltfNode result = {};

		if (JsonValue* name = FindJsonValueInObject(node, "name"))
		{
			result.name = name->value;
		}

		if (JsonValue* children = FindJsonValueInObject(node, "children"))
		{
			result.children = arena.Push<int32>(children->children);

			size_t childIdx = 0;
			for (JsonValue* child = FindJsonValueInArray(children, 0); child; child = child->sibling, ++childIdx)
			{
				result.children[childIdx] = static_cast<int32>(child->asNumber);
			}
		}

		if (JsonValue* mesh = FindJsonValueInObject(node, "mesh"))
		{
			result.mesh = static_cast<int32>(mesh->asNumber);
		}
		else
		{
			result.mesh = -1;
		}

		if (JsonValue* skin = FindJsonValueInObject(node, "skin"))
		{
			result.skin = static_cast<int32>(skin->asNumber);
		}
		else
		{
			result.skin = -1;
		}

		if (JsonValue* translation = FindJsonValueInObject(node, "translation"))
		{
			size_t elementIdx = 0;
			for (JsonValue* element = FindJsonValueInArray(translation, 0); element; element = element->sibling, ++elementIdx)
			{
				result.translation.Elements[elementIdx] = static_cast<float>(element->asNumber);
			}
		}
		else
		{
			result.translation = HMM_V3(0, 0, 0);
		}

		if (JsonValue* rotation = FindJsonValueInObject(node, "rotation"))
		{
			size_t elementIdx = 0;
			for (JsonValue* element = FindJsonValueInArray(rotation, 0); element; element = element->sibling, ++elementIdx)
			{
				result.rotation.Elements[elementIdx] = static_cast<float>(element->asNumber);
			}
		}
		else
		{
			result.rotation = HMM_Q(0, 0, 0, 1);
		}

		if (JsonValue* scale = FindJsonValueInObject(node, "scale"))
		{
			size_t elementIdx = 0;
			for (JsonValue* element = FindJsonValueInArray(scale, 0); element; element = element->sibling, ++elementIdx)
			{
				result.scale.Elements[elementIdx] = static_cast<float>(element->asNumber);
			}
		}
		else
		{
			result.scale = HMM_V3(1, 1, 1);
		}

		return result;
	}

	GltfSkin ParseSkin(Arena& arena, JsonValue* skin)
	{
		GltfSkin result = {};

		if (JsonValue* name = FindJsonValueInObject(skin, "name"))
		{
			result.name = name->value;
		}

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

	GltfAnimation ParseAnimation(Arena& arena, JsonValue* animation)
	{
		GltfAnimation result = {};

		if (JsonValue* name = FindJsonValueInObject(animation, "name"))
		{
			result.name = name->value;
		}

		if (JsonValue* channels = FindJsonValueInObject(animation, "channels"))
		{
			result.channels = arena.Push<GltfAnimationChannel>(channels->children);

			size_t channelIdx = 0;
			for (JsonValue* channel = FindJsonValueInArray(channels, 0); channel; channel = channel->sibling, ++channelIdx)
			{
				if (JsonValue* sampler = FindJsonValueInObject(channel, "sampler"))
				{
					result.channels[channelIdx].sampler = static_cast<int32>(sampler->asNumber);
				}

				if (JsonValue* target = FindJsonValueInObject(channel, "target"))
				{
					if (JsonValue* node = FindJsonValueInObject(target, "node"))
					{
						result.channels[channelIdx].node = static_cast<int32>(node->asNumber);
					}
					else
					{
						result.channels[channelIdx].node = -1;
					}

					if (JsonValue* path = FindJsonValueInObject(target, "path"))
					{
						if (path->value == "translation")
						{
							result.channels[channelIdx].target = GltfAnimationTarget::Translation;
						}
						else if (path->value == "rotation")
						{
							result.channels[channelIdx].target = GltfAnimationTarget::Rotation;
						}
						else if (path->value == "scale")
						{
							result.channels[channelIdx].target = GltfAnimationTarget::Scale;
						}
						else if (path->value == "weights")
						{
							result.channels[channelIdx].target = GltfAnimationTarget::Weights;
						}
					}
				}
			}
		}

		if (JsonValue* samplers = FindJsonValueInObject(animation, "samplers"))
		{
			result.samplers = arena.Push<GltfAnimationSampler>(samplers->children);

			size_t samplerIdx = 0;
			for (JsonValue* sampler = FindJsonValueInArray(samplers, 0); sampler; sampler = sampler->sibling, ++samplerIdx)
			{
				if (JsonValue* input = FindJsonValueInObject(sampler, "input"))
				{
					result.samplers[samplerIdx].input = static_cast<int32>(input->asNumber);
				}

				if (JsonValue* output = FindJsonValueInObject(sampler, "output"))
				{
					result.samplers[samplerIdx].output = static_cast<int32>(output->asNumber);
				}

				if (JsonValue* interpolation = FindJsonValueInObject(sampler, "interpolation"))
				{
					if (interpolation->value == "LINEAR")
					{
						result.samplers[samplerIdx].interpolation = GltfAnimationInterpolation::Linear;
					}
					else if (interpolation->value == "STEP")
					{
						result.samplers[samplerIdx].interpolation = GltfAnimationInterpolation::Step;
					}
					else if (interpolation->value == "CUBICSPLINE")
					{
						result.samplers[samplerIdx].interpolation = GltfAnimationInterpolation::CubicSpline;
					}
				}
				else
				{
					result.samplers[samplerIdx].interpolation = GltfAnimationInterpolation::Linear;
				}
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

		JsonValue* gltfNodes = FindJsonValueInObject(gltf, "nodes");
		if (gltfNodes && gltfNodes->children > 0)
		{
			result.nodes = arena.Push<GltfNode>(gltfNodes->children);

			size_t nodeIdx = 0;
			for (JsonValue* gltfNode = FindJsonValueInArray(gltfNodes, 0); gltfNode; gltfNode = gltfNode->sibling, ++nodeIdx)
			{
				result.nodes[nodeIdx] = ParseNode(arena, gltfNode);
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

		JsonValue* gltfAnimations = FindJsonValueInObject(gltf, "animations");
		if (gltfAnimations && gltfAnimations->children > 0)
		{
			result.animations = arena.Push<GltfAnimation>(gltfAnimations->children);

			size_t animationIdx = 0;
			for (JsonValue* gltfAnimation = FindJsonValueInArray(gltfAnimations, 0); gltfAnimation; gltfAnimation = gltfAnimation->sibling, ++animationIdx)
			{
				result.animations[animationIdx] = ParseAnimation(arena, gltfAnimation);
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

		// #TODO: Apply mesh <-> skin mapping via nodes
		TSpan<HMM_Mat4> invBindPose = {};
		if (asset.skins.length > 0)
		{
			const GltfSkin& skin = asset.skins[0];
			if (skin.inverseBindMatrices != -1)
			{
				const GltfAccessor& accessor = asset.accessors[skin.inverseBindMatrices];
				BK_ASSERTF(accessor.componentType == GltfComponentType::Float32, "Unexpected buffer layout");
				BK_ASSERTF(accessor.componentElements == 16, "Unexpected buffer layout");
				BK_ASSERTF(accessor.normalized == false, "Unexpected buffer layout");

				TSpan<uint8> invBindBuffer = MaterializeBuffer(arena, asset, accessor);
				invBindPose.data = (HMM_Mat4*)invBindBuffer.data;
				invBindPose.length = invBindBuffer.length / sizeof(HMM_Mat4);
			}
		}

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

			TSpan<TSpan<uint8>> positionBuffers = scratch.arena.PushZeroed<TSpan<uint8>>(primitives->children);
			size_t positionBufferSize = 0;
			size_t vertexCount = 0;

			TSpan<TSpan<uint8>> boneIndexBuffers = scratch.arena.PushZeroed<TSpan<uint8>>(primitives->children);
			size_t boneIndexBufferSize = 0;

			TSpan<TSpan<uint8>> boneWeightBuffers = scratch.arena.PushZeroed<TSpan<uint8>>(primitives->children);
			size_t boneWeightBufferSize = 0;

			TSpan<TSpan<uint8>> indexBuffers = scratch.arena.PushZeroed<TSpan<uint8>>(primitives->children);
			size_t indexBufferSize = 0;
			size_t indexCount = 0;

			size_t sectionIdx = 0;
			for (JsonValue* primitive = FindJsonValueInArray(primitives, 0); primitive; primitive = primitive->sibling, ++sectionIdx)
			{
				MeshSection& section = mesh.sections[sectionIdx];
				section.vertexOffset = vertexCount;
				section.indexOffset = indexCount;

				// #TODO: Need to either conform these buffers to the expected layout or bubble up the details

				if (JsonValue* positionAttrib = FindJsonValue(primitive, "attributes.POSITION"))
				{
					const GltfAccessor& accessor = asset.accessors[size_t(positionAttrib->asNumber)];
					BK_ASSERTF(accessor.componentType == GltfComponentType::Float32, "Unexpected buffer layout");
					BK_ASSERTF(accessor.componentElements == 3, "Unexpected buffer layout");
					BK_ASSERTF(accessor.normalized == false, "Unexpected buffer layout");

					positionBuffers[sectionIdx] = MaterializeBuffer(scratch.arena, asset, accessor);
					positionBufferSize += positionBuffers[sectionIdx].length;
					vertexCount += accessor.count;

					section.triangleCount = accessor.count / 3;
				}

				if (JsonValue* jointsAttrib = FindJsonValue(primitive, "attributes.JOINTS_0"))
				{
					const GltfAccessor& accessor = asset.accessors[size_t(jointsAttrib->asNumber)];
					BK_ASSERTF(accessor.componentType == GltfComponentType::Uint8, "Unexpected buffer layout");
					BK_ASSERTF(accessor.componentElements == 4, "Unexpected buffer layout");
					BK_ASSERTF(accessor.normalized == false, "Unexpected buffer layout");

					boneIndexBuffers[sectionIdx] = MaterializeBuffer(scratch.arena, asset, accessor);
					boneIndexBufferSize += boneIndexBuffers[sectionIdx].length;
				}

				if (JsonValue* weightsAttrib = FindJsonValue(primitive, "attributes.WEIGHTS_0"))
				{
					const GltfAccessor& accessor = asset.accessors[size_t(weightsAttrib->asNumber)];
					BK_ASSERTF(accessor.componentType == GltfComponentType::Float32, "Unexpected buffer layout");
					BK_ASSERTF(accessor.componentElements == 4, "Unexpected buffer layout");
					BK_ASSERTF(accessor.normalized == false, "Unexpected buffer layout");

					boneWeightBuffers[sectionIdx] = MaterializeBuffer(scratch.arena, asset, accessor);
					boneWeightBufferSize += boneWeightBuffers[sectionIdx].length;
				}

				if (JsonValue* indicesAttrib = FindJsonValueInObject(primitive, "indices"))
				{
					const GltfAccessor& accessor = asset.accessors[size_t(indicesAttrib->asNumber)];
					BK_ASSERTF(accessor.componentType == GltfComponentType::Uint16, "Unexpected buffer layout");
					BK_ASSERTF(accessor.componentElements == 1, "Unexpected buffer layout");
					BK_ASSERTF(accessor.normalized == false, "Unexpected buffer layout");

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

			mesh.boneIndexBuffer = arena.Push<uint8>(boneIndexBufferSize);

			uint8* boneIndexBuffer = mesh.boneIndexBuffer.data;
			for (TSpan buffer : boneIndexBuffers)
			{
				MemoryCopy(boneIndexBuffer, buffer.data, buffer.length);
				boneIndexBuffer += buffer.length;
			}

			mesh.boneWeightBuffer = arena.Push<uint8>(boneWeightBufferSize);

			uint8* boneWeightBuffer = mesh.boneWeightBuffer.data;
			for (TSpan buffer : boneWeightBuffers)
			{
				MemoryCopy(boneWeightBuffer, buffer.data, buffer.length);
				boneWeightBuffer += buffer.length;
			}

			mesh.indexBuffer = arena.Push<uint8>(indexBufferSize);

			uint8* indexBuffer = mesh.indexBuffer.data;
			for (TSpan buffer : indexBuffers)
			{
				MemoryCopy(indexBuffer, buffer.data, buffer.length);
				indexBuffer += buffer.length;
			}

			mesh.invBindPose = invBindPose;
		}

		return true;
	}
}
