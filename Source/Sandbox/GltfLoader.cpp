#include "GltfLoader.h"

#include "Core/Json.h"
#include "Core/Math.h"
#include "Core/Memory.h"

#include "Engine/AnimSequence.h"
#include "Engine/Mesh.h"
#include "Engine/Skeleton.h"

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
		uint8* data;
	};

	struct GltfNode
	{
		String name;
		TSpan<int32> children;
		int32 mesh;
		int32 skin;
		Vec3f translation;
		Quat4f rotation;
		Vec3f scale;
	};

	struct GltfMeshPrimitive
	{
		struct
		{
			int32 position;
			int32 joints_0;
			int32 weights_0;
		} attributes;

		int32 indices;
		int32 material;
	};

	struct GltfMesh
	{
		String name;
		TSpan<GltfMeshPrimitive> primitives;
	};

	struct GltfSkin
	{
		String name;
		int32 inverseBindMatrices;
		int32 skeleton;
		TSpan<int32> joints;
	};

	struct GltfMaterial
	{
		String name;

		Vec4f baseColorFactor;
		int32 baseColorTexture;
		int32 baseColorTexCoord;

		float metallicFactor;
		float roughnessFactor;
		int32 metallicRoughnessTexture;
		int32 metallicRoughnessTexCoord;
	};

	struct GltfTexture
	{
		String name;
		int32 source;
		int32 sampler;
	};

	struct GltfImage
	{
		String name;
		String uri;
		String mimeType;
		int32 bufferView;
	};

	enum class GltfFilterType : int32
	{
		Nearest = 9728,
		Linear = 9729,
		NearestMipMapNearest = 9984,
		LinearMipMapNearest = 9985,
		NearestMipMapLinear = 9986,
		LinearMipMapLinear = 9987,
	};

	enum class GltfWrapType : int32
	{
		Repeat = 10497,
		ClampToEdge = 33071,
		MirroredRepeat = 33648,
	};

	struct GltfSampler
	{
		String name;
		GltfFilterType minFilter;
		GltfFilterType magFilter;
		GltfWrapType wrapS;
		GltfWrapType wrapT;
	};

	enum class GltfAnimationTargetPath : uint8
	{
		Translation,
		Rotation,
		Scale,
		Weights,
	};

	enum class GltfAnimationInterpolation : uint8
	{
		Linear,
		Step,
		CubicSpline,
	};

	struct GltfAnimationChannel
	{
		int32 sampler;
		int32 targetNode;
		GltfAnimationTargetPath targetPath;
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

		TSpan<GltfAccessor> accessors;
		TSpan<GltfBufferView> bufferViews;
		TSpan<GltfBuffer> buffers;
		TSpan<GltfNode> nodes;
		TSpan<GltfMesh> meshes;
		TSpan<GltfSkin> skins;
		TSpan<GltfMaterial> materials;
		TSpan<GltfTexture> textures;
		TSpan<GltfImage> images;
		TSpan<GltfSampler> samplers;
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
			result.children = Push<int32>(arena, children->children);

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
				result.translation.elements[elementIdx] = static_cast<float>(element->asNumber);
			}
		}
		else
		{
			result.translation = Vec3f::Zero;
		}

		if (JsonValue* rotation = FindJsonValueInObject(node, "rotation"))
		{
			size_t elementIdx = 0;
			for (JsonValue* element = FindJsonValueInArray(rotation, 0); element; element = element->sibling, ++elementIdx)
			{
				result.rotation.elements[elementIdx] = static_cast<float>(element->asNumber);
			}
		}
		else
		{
			result.rotation = Quat4f::Identity;
		}

		if (JsonValue* scale = FindJsonValueInObject(node, "scale"))
		{
			size_t elementIdx = 0;
			for (JsonValue* element = FindJsonValueInArray(scale, 0); element; element = element->sibling, ++elementIdx)
			{
				result.scale.elements[elementIdx] = static_cast<float>(element->asNumber);
			}
		}
		else
		{
			result.scale = Vec3f::One;
		}

		return result;
	}

	GltfMesh ParseMesh(Arena& arena, JsonValue* mesh)
	{
		GltfMesh result = {};

		if (JsonValue* name = FindJsonValueInObject(mesh, "name"))
		{
			result.name = name->value;
		}

		if (JsonValue* primitives = FindJsonValueInObject(mesh, "primitives"))
		{
			result.primitives = Push<GltfMeshPrimitive>(arena, primitives->children);

			size_t primitiveIdx = 0;
			for (JsonValue* primitive = FindJsonValueInArray(primitives, 0); primitive; primitive = primitive->sibling, ++primitiveIdx)
			{
				JsonValue* attribs = FindJsonValueInObject(primitive, "attributes");

				if (JsonValue* positionAttrib = FindJsonValueInObject(attribs, "POSITION"))
				{
					result.primitives[primitiveIdx].attributes.position = static_cast<int32>(positionAttrib->asNumber);
				}
				else
				{
					result.primitives[primitiveIdx].attributes.position = -1;
				}

				if (JsonValue* joints0Attrib = FindJsonValueInObject(attribs, "JOINTS_0"))
				{
					result.primitives[primitiveIdx].attributes.joints_0 = static_cast<int32>(joints0Attrib->asNumber);
				}
				else
				{
					result.primitives[primitiveIdx].attributes.joints_0 = -1;
				}

				if (JsonValue* weights0Attrib = FindJsonValueInObject(attribs, "WEIGHTS_0"))
				{
					result.primitives[primitiveIdx].attributes.weights_0 = static_cast<int32>(weights0Attrib->asNumber);
				}
				else
				{
					result.primitives[primitiveIdx].attributes.weights_0 = -1;
				}

				if (JsonValue* indices = FindJsonValueInObject(primitive, "indices"))
				{
					result.primitives[primitiveIdx].indices = static_cast<int32>(indices->asNumber);
				}
				else
				{
					result.primitives[primitiveIdx].indices = -1;
				}

				if (JsonValue* material = FindJsonValueInObject(primitive, "material"))
				{
					result.primitives[primitiveIdx].material = static_cast<int32>(material->asNumber);
				}
				else
				{
					result.primitives[primitiveIdx].material = -1;
				}
			}
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
			result.joints = Push<int32>(arena, joints->children);

			size_t jointIdx = 0;
			for (JsonValue* joint = FindJsonValueInArray(joints, 0); joint; joint = joint->sibling, ++jointIdx)
			{
				result.joints[jointIdx] = static_cast<int32>(joint->asNumber);
			}
		}

		return result;
	}

	GltfMaterial ParseMaterial(JsonValue* material)
	{
		GltfMaterial result = {};

		if (JsonValue* name = FindJsonValueInObject(material, "name"))
		{
			result.name = name->value;
		}

		if (JsonValue* pbrMetallicRoughness = FindJsonValueInObject(material, "pbrMetallicRoughness"))
		{
			if (JsonValue* baseColorFactor = FindJsonValueInObject(pbrMetallicRoughness, "baseColorFactor"))
			{
				size_t elementIdx = 0;
				for (JsonValue* element = FindJsonValueInArray(baseColorFactor, 0); element; element = element->sibling, ++elementIdx)
				{
					result.baseColorFactor.elements[elementIdx] = static_cast<float>(element->asNumber);
				}
			}
			else
			{
				result.baseColorFactor = Vec4f::One;
			}

			if (JsonValue* baseColorTexture = FindJsonValue(pbrMetallicRoughness, "baseColorTexture.index"))
			{
				result.baseColorTexture = static_cast<int32>(baseColorTexture->asNumber);
			}
			else
			{
				result.baseColorTexture = -1;
			}

			if (JsonValue* baseColorTexCoord = FindJsonValue(pbrMetallicRoughness, "baseColorTexture.texCoord"))
			{
				result.baseColorTexCoord = static_cast<int32>(baseColorTexCoord->asNumber);
			}

			if (JsonValue* metallicFactor = FindJsonValueInObject(pbrMetallicRoughness, "metallicFactor"))
			{
				result.metallicFactor = static_cast<float>(metallicFactor->asNumber);
			}
			else
			{
				result.metallicFactor = 1.0f;
			}

			if (JsonValue* roughnessFactor = FindJsonValueInObject(pbrMetallicRoughness, "roughnessFactor"))
			{
				result.roughnessFactor = static_cast<float>(roughnessFactor->asNumber);
			}
			else
			{
				result.roughnessFactor = 1.0f;
			}

			if (JsonValue* metallicRoughnessTexture = FindJsonValue(pbrMetallicRoughness, "metallicRoughnessTexture.index"))
			{
				result.metallicRoughnessTexture = static_cast<int32>(metallicRoughnessTexture->asNumber);
			}
			else
			{
				result.metallicRoughnessTexture = -1;
			}

			if (JsonValue* metallicRoughnessTexCoord = FindJsonValue(pbrMetallicRoughness, "metallicRoughnessTexture.texCoord"))
			{
				result.metallicRoughnessTexCoord = static_cast<int32>(metallicRoughnessTexCoord->asNumber);
			}
		}
		else
		{
			result.baseColorFactor = Vec4f::One;
			result.baseColorTexture = -1;
			result.metallicFactor = 1.0f;
			result.roughnessFactor = 1.0f;
			result.metallicRoughnessTexture = -1;
		}

		// #TODO: Normal, emissive, occlusion, alpha

		return result;
	}

	GltfTexture ParseTexture(JsonValue* texture)
	{
		GltfTexture result = {};

		if (JsonValue* name = FindJsonValueInObject(texture, "name"))
		{
			result.name = name->value;
		}

		if (JsonValue* sampler = FindJsonValueInObject(texture, "sampler"))
		{
			result.sampler = static_cast<int32>(sampler->asNumber);
		}
		else
		{
			result.sampler = -1;
		}

		if (JsonValue* source = FindJsonValueInObject(texture, "source"))
		{
			result.source = static_cast<int32>(source->asNumber);
		}
		else
		{
			result.source = -1;
		}

		return result;
	}

	GltfImage ParseImage(JsonValue* image)
	{
		GltfImage result = {};

		if (JsonValue* name = FindJsonValueInObject(image, "name"))
		{
			result.name = name->value;
		}

		if (JsonValue* uri = FindJsonValueInObject(image, "uri"))
		{
			result.uri = uri->value;
		}

		if (JsonValue* mimeType = FindJsonValueInObject(image, "mimeType"))
		{
			result.mimeType = mimeType->value;
		}

		if (JsonValue* bufferView = FindJsonValueInObject(image, "bufferView"))
		{
			result.bufferView = static_cast<int32>(bufferView->asNumber);
		}
		else
		{
			result.bufferView = -1;
		}

		return result;
	}

	GltfSampler ParseSampler(JsonValue* sampler)
	{
		GltfSampler result = {};

		if (JsonValue* name = FindJsonValueInObject(sampler, "name"))
		{
			result.name = name->value;
		}

		if (JsonValue* minFilter = FindJsonValueInObject(sampler, "minFilter"))
		{
			result.minFilter = static_cast<GltfFilterType>(minFilter->asNumber);
		}
		else
		{
			result.minFilter = GltfFilterType::NearestMipMapLinear;
		}

		if (JsonValue* magFilter = FindJsonValueInObject(sampler, "magFilter"))
		{
			result.magFilter = static_cast<GltfFilterType>(magFilter->asNumber);
		}
		else
		{
			result.magFilter = GltfFilterType::Linear;
		}

		if (JsonValue* wrapS = FindJsonValueInObject(sampler, "wrapS"))
		{
			result.wrapS = static_cast<GltfWrapType>(wrapS->asNumber);
		}
		else
		{
			result.wrapS = GltfWrapType::Repeat;
		}

		if (JsonValue* wrapT = FindJsonValueInObject(sampler, "wrapT"))
		{
			result.wrapT = static_cast<GltfWrapType>(wrapT->asNumber);
		}
		else
		{
			result.wrapT = GltfWrapType::Repeat;
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
			result.channels = Push<GltfAnimationChannel>(arena, channels->children);

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
						result.channels[channelIdx].targetNode = static_cast<int32>(node->asNumber);
					}
					else
					{
						result.channels[channelIdx].targetNode = -1;
					}

					if (JsonValue* path = FindJsonValueInObject(target, "path"))
					{
						if (path->value == "translation")
						{
							result.channels[channelIdx].targetPath = GltfAnimationTargetPath::Translation;
						}
						else if (path->value == "rotation")
						{
							result.channels[channelIdx].targetPath = GltfAnimationTargetPath::Rotation;
						}
						else if (path->value == "scale")
						{
							result.channels[channelIdx].targetPath = GltfAnimationTargetPath::Scale;
						}
						else if (path->value == "weights")
						{
							result.channels[channelIdx].targetPath = GltfAnimationTargetPath::Weights;
						}
					}
				}
			}
		}

		if (JsonValue* samplers = FindJsonValueInObject(animation, "samplers"))
		{
			result.samplers = Push<GltfAnimationSampler>(arena, samplers->children);

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

	bool LoadGltfAsset(Arena& arena, TSpan<uint8> buffer, GltfAsset& asset)
	{
		JsonValue* gltf = ParseJson(arena, String(reinterpret_cast<const char*>(buffer.data), buffer.length));
		if (!gltf)
		{
			return false;
		}

		// #TODO: Check asset.version and asset.minVersion

		JsonValue* gltfAccessors = FindJsonValueInObject(gltf, "accessors");
		if (gltfAccessors && gltfAccessors->children > 0)
		{
			asset.accessors = Push<GltfAccessor>(arena, gltfAccessors->children);

			size_t accessorIdx = 0;
			for (JsonValue* gltfAccessor = FindJsonValueInArray(gltfAccessors, 0); gltfAccessor; gltfAccessor = gltfAccessor->sibling, ++accessorIdx)
			{
				asset.accessors[accessorIdx] = ParseAccessor(gltfAccessor);
			}
		}

		JsonValue* gltfBufferViews = FindJsonValueInObject(gltf, "bufferViews");
		if (gltfBufferViews && gltfBufferViews->children > 0)
		{
			asset.bufferViews = Push<GltfBufferView>(arena, gltfBufferViews->children);

			size_t bufferViewIdx = 0;
			for (JsonValue* gltfBufferView = FindJsonValueInArray(gltfBufferViews, 0); gltfBufferView; gltfBufferView = gltfBufferView->sibling, ++bufferViewIdx)
			{
				asset.bufferViews[bufferViewIdx] = ParseBufferView(gltfBufferView);
			}
		}

		JsonValue* gltfBuffers = FindJsonValueInObject(gltf, "buffers");
		if (gltfBuffers && gltfBuffers->children > 0)
		{
			asset.buffers = Push<GltfBuffer>(arena, gltfBuffers->children);

			size_t bufferIdx = 0;
			for (JsonValue* gltfBuffer = FindJsonValueInArray(gltfBuffers, 0); gltfBuffer; gltfBuffer = gltfBuffer->sibling, ++bufferIdx)
			{
				asset.buffers[bufferIdx] = ParseBuffer(gltfBuffer);
			}
		}

		JsonValue* gltfNodes = FindJsonValueInObject(gltf, "nodes");
		if (gltfNodes && gltfNodes->children > 0)
		{
			asset.nodes = Push<GltfNode>(arena, gltfNodes->children);

			size_t nodeIdx = 0;
			for (JsonValue* gltfNode = FindJsonValueInArray(gltfNodes, 0); gltfNode; gltfNode = gltfNode->sibling, ++nodeIdx)
			{
				asset.nodes[nodeIdx] = ParseNode(arena, gltfNode);
			}
		}

		JsonValue* gltfMeshes = FindJsonValueInObject(gltf, "meshes");
		if (gltfMeshes && gltfMeshes->children > 0)
		{
			asset.meshes = Push<GltfMesh>(arena, gltfMeshes->children);

			size_t meshIdx = 0;
			for (JsonValue* gltfMesh = FindJsonValueInArray(gltfMeshes, 0); gltfMesh; gltfMesh = gltfMesh->sibling, ++meshIdx)
			{
				asset.meshes[meshIdx] = ParseMesh(arena, gltfMesh);
			}
		}

		JsonValue* gltfSkins = FindJsonValueInObject(gltf, "skins");
		if (gltfSkins && gltfSkins->children > 0)
		{
			asset.skins = Push<GltfSkin>(arena, gltfSkins->children);

			size_t skinIdx = 0;
			for (JsonValue* gltfSkin = FindJsonValueInArray(gltfSkins, 0); gltfSkin; gltfSkin = gltfSkin->sibling, ++skinIdx)
			{
				asset.skins[skinIdx] = ParseSkin(arena, gltfSkin);
			}
		}

		JsonValue* gltfMaterials = FindJsonValueInObject(gltf, "materials");
		if (gltfMaterials && gltfMaterials->children > 0)
		{
			asset.materials = Push<GltfMaterial>(arena, gltfMaterials->children);

			size_t materialIdx = 0;
			for (JsonValue* gltfMaterial = FindJsonValueInArray(gltfMaterials, 0); gltfMaterial; gltfMaterial = gltfMaterial->sibling, ++materialIdx)
			{
				asset.materials[materialIdx] = ParseMaterial(gltfMaterial);
			}
		}

		JsonValue* gltfTextures = FindJsonValueInObject(gltf, "textures");
		if (gltfTextures && gltfTextures->children > 0)
		{
			asset.textures = Push<GltfTexture>(arena, gltfTextures->children);

			size_t textureIdx = 0;
			for (JsonValue* gltfTexture = FindJsonValueInArray(gltfTextures, 0); gltfTexture; gltfTexture = gltfTexture->sibling, ++textureIdx)
			{
				asset.textures[textureIdx] = ParseTexture(gltfTexture);
			}
		}

		JsonValue* gltfImages = FindJsonValueInObject(gltf, "images");
		if (gltfImages && gltfImages->children > 0)
		{
			asset.images = Push<GltfImage>(arena, gltfImages->children);

			size_t imageIdx = 0;
			for (JsonValue* gltfImage = FindJsonValueInArray(gltfImages, 0); gltfImage; gltfImage = gltfImage->sibling, ++imageIdx)
			{
				asset.images[imageIdx] = ParseImage(gltfImage);
			}
		}

		JsonValue* gltfSamplers = FindJsonValueInObject(gltf, "samplers");
		if (gltfSamplers && gltfSamplers->children > 0)
		{
			asset.samplers = Push<GltfSampler>(arena, gltfSamplers->children);

			size_t samplerIdx = 0;
			for (JsonValue* gltfSampler = FindJsonValueInArray(gltfSamplers, 0); gltfSampler; gltfSampler = gltfSampler->sibling, ++samplerIdx)
			{
				asset.samplers[samplerIdx] = ParseSampler(gltfSampler);
			}
		}

		JsonValue* gltfAnimations = FindJsonValueInObject(gltf, "animations");
		if (gltfAnimations && gltfAnimations->children > 0)
		{
			asset.animations = Push<GltfAnimation>(arena, gltfAnimations->children);

			size_t animationIdx = 0;
			for (JsonValue* gltfAnimation = FindJsonValueInArray(gltfAnimations, 0); gltfAnimation; gltfAnimation = gltfAnimation->sibling, ++animationIdx)
			{
				asset.animations[animationIdx] = ParseAnimation(arena, gltfAnimation);
			}
		}

		return true;
	}

	bool LoadGlbAsset(Arena& arena, TSpan<uint8> buffer, GltfAsset& asset)
	{
		GlbHeader* header = reinterpret_cast<GlbHeader*>(buffer.data);
		if (buffer.length < sizeof(GlbHeader) || header->magic != 0x46546C67)
		{
			return false;
		}

		if (header->version != 2)
		{
			return false;
		}

		BK_ASSERT(header->length <= buffer.length);

		GlbChunk* chunkHeader;
		size_t chunkOffset = sizeof(GlbHeader);

		chunkHeader = reinterpret_cast<GlbChunk*>(buffer.data + chunkOffset);
		chunkOffset += sizeof(GlbChunk) + chunkHeader->length;
		BK_ASSERT(chunkHeader->type == 0x4E4F534A);

		TSpan jsonChunk(reinterpret_cast<uint8*>(chunkHeader + 1), chunkHeader->length);
		if (!LoadGltfAsset(arena, jsonChunk, asset))
		{
			return false;
		}

		if (chunkOffset < header->length)
		{
			chunkHeader = reinterpret_cast<GlbChunk*>(buffer.data + chunkOffset);
			chunkOffset += sizeof(GlbChunk) + chunkHeader->length;
			BK_ASSERT(chunkHeader->type == 0x004E4942);

			BK_ASSERT(asset.buffers.length != 0);
			BK_ASSERT(asset.buffers[0].uri.length == 0);
			BK_ASSERT(asset.buffers[0].byteLength <= chunkHeader->length);

			asset.buffers[0].data = reinterpret_cast<uint8*>(chunkHeader + 1);
		}

		return true;
	}

	TSpan<uint8> GetBuffer(const GltfAsset& asset, const GltfBufferView& bufferView)
	{
		GltfBuffer& buffer = asset.buffers[bufferView.buffer];

		if (!buffer.data)
		{
			BK_ASSERT(buffer.uri.length != 0);
			// #TODO: Load from base64 / file
		}

		BK_ASSERT(buffer.byteLength >= bufferView.byteOffset + bufferView.byteLength);
		return TSpan(buffer.data + bufferView.byteOffset, bufferView.byteLength);
	}

	size_t GetAccessorBufferSize(const GltfAccessor& accessor)
	{
		return GetComponentTypeSize(accessor.componentType) * accessor.componentElements * accessor.count;
	}

	void CopyAccessorBuffer(const GltfAsset& asset, const GltfAccessor& accessor, TSpan<uint8> result)
	{
		size_t componentSize = GetComponentTypeSize(accessor.componentType) * accessor.componentElements;
		BK_ASSERT(result.length >= componentSize * accessor.count);

		if (accessor.bufferView == -1)
		{
			ZeroMemory(result.data, componentSize * accessor.count);
		}
		else
		{
			const GltfBufferView& bufferView = asset.bufferViews[accessor.bufferView];
			TSpan<uint8> buffer = GetBuffer(asset, bufferView);

			uint8* srcBufferPtr = buffer.data + accessor.byteOffset;
			uint8* dstBufferPtr = result.data;

			if (bufferView.byteStride == 0)
			{
				CopyMemory(dstBufferPtr, srcBufferPtr, componentSize * accessor.count);
			}
			else
			{
				for (size_t componentIdx = 0; componentIdx < accessor.count; ++componentIdx)
				{
					CopyMemory(dstBufferPtr, srcBufferPtr, componentSize);

					srcBufferPtr += bufferView.byteStride;
					dstBufferPtr += componentSize;
				}
			}
		}

		// #TODO: Sparse accessors
	}

	bool LoadGlbMeshes(Arena& arena, TSpan<uint8> fileData, TSpan<MeshDesc>& meshes)
	{
		ArenaScope scratch = GetScratchArena(&arena);

		GltfAsset gltfAsset;
		if (!LoadGlbAsset(scratch.arena, fileData, gltfAsset))
		{
			return false;
		}

		meshes = PushZeroed<MeshDesc>(arena, gltfAsset.meshes.length);

		for (size_t meshIdx = 0; meshIdx < meshes.length; ++meshIdx)
		{
			const GltfMesh& gltfMesh = gltfAsset.meshes[meshIdx];

			MeshDesc& mesh = meshes[meshIdx];

			mesh.name = Copy(arena, gltfMesh.name);
			mesh.sections = PushZeroed<MeshSection>(arena, gltfMesh.primitives.length);

			TSpan<TSpan<uint8>> positionBuffers = PushZeroed<TSpan<uint8>>(scratch.arena, mesh.sections.length);
			size_t positionBufferSize = 0;
			size_t vertexCount = 0;

			TSpan<TSpan<uint8>> jointIndexBuffers = PushZeroed<TSpan<uint8>>(scratch.arena, mesh.sections.length);
			size_t jointIndexBufferSize = 0;

			TSpan<TSpan<uint8>> jointWeightBuffers = PushZeroed<TSpan<uint8>>(scratch.arena, mesh.sections.length);
			size_t jointWeightBufferSize = 0;

			TSpan<TSpan<uint8>> indexBuffers = PushZeroed<TSpan<uint8>>(scratch.arena, mesh.sections.length);
			size_t indexBufferSize = 0;
			size_t indexCount = 0;

			for (size_t sectionIdx = 0; sectionIdx < mesh.sections.length; ++sectionIdx)
			{
				const GltfMeshPrimitive& gltfPrimitive = gltfMesh.primitives[sectionIdx];

				MeshSection& section = mesh.sections[sectionIdx];
				section.vertexOffset = vertexCount;
				section.indexOffset = indexCount;

				// #TODO: Need to either conform these buffers to the expected layout or bubble up the details

				if (gltfPrimitive.attributes.position != -1)
				{
					const GltfAccessor& accessor = gltfAsset.accessors[gltfPrimitive.attributes.position];
					BK_ASSERTF(accessor.componentType == GltfComponentType::Float32, "Unexpected buffer layout");
					BK_ASSERTF(accessor.componentElements == 3, "Unexpected buffer layout");
					BK_ASSERTF(accessor.normalized == false, "Unexpected buffer layout");

					positionBuffers[sectionIdx] = Push(scratch.arena, GetAccessorBufferSize(accessor));
					CopyAccessorBuffer(gltfAsset, accessor, positionBuffers[sectionIdx]);

					positionBufferSize += positionBuffers[sectionIdx].length;
					vertexCount += accessor.count;

					section.triangleCount = accessor.count / 3;
				}

				if (gltfPrimitive.attributes.joints_0 != -1)
				{
					const GltfAccessor& accessor = gltfAsset.accessors[gltfPrimitive.attributes.joints_0];
					BK_ASSERTF(accessor.componentType == GltfComponentType::Uint8, "Unexpected buffer layout");
					BK_ASSERTF(accessor.componentElements == 4, "Unexpected buffer layout");
					BK_ASSERTF(accessor.normalized == false, "Unexpected buffer layout");

					jointIndexBuffers[sectionIdx] = Push(scratch.arena, GetAccessorBufferSize(accessor));
					CopyAccessorBuffer(gltfAsset, accessor, jointIndexBuffers[sectionIdx]);

					jointIndexBufferSize += jointIndexBuffers[sectionIdx].length;
				}

				if (gltfPrimitive.attributes.weights_0 != -1)
				{
					const GltfAccessor& accessor = gltfAsset.accessors[gltfPrimitive.attributes.weights_0];
					BK_ASSERTF(accessor.componentType == GltfComponentType::Float32, "Unexpected buffer layout");
					BK_ASSERTF(accessor.componentElements == 4, "Unexpected buffer layout");
					BK_ASSERTF(accessor.normalized == false, "Unexpected buffer layout");

					jointWeightBuffers[sectionIdx] = Push(scratch.arena, GetAccessorBufferSize(accessor));
					CopyAccessorBuffer(gltfAsset, accessor, jointWeightBuffers[sectionIdx]);

					jointWeightBufferSize += jointWeightBuffers[sectionIdx].length;
				}

				if (gltfPrimitive.indices != -1)
				{
					const GltfAccessor& accessor = gltfAsset.accessors[gltfPrimitive.indices];
					BK_ASSERTF(accessor.componentType == GltfComponentType::Uint16, "Unexpected buffer layout");
					BK_ASSERTF(accessor.componentElements == 1, "Unexpected buffer layout");
					BK_ASSERTF(accessor.normalized == false, "Unexpected buffer layout");

					indexBuffers[sectionIdx] = Push(scratch.arena, GetAccessorBufferSize(accessor));
					CopyAccessorBuffer(gltfAsset, accessor, indexBuffers[sectionIdx]);

					indexBufferSize += indexBuffers[sectionIdx].length;
					indexCount += accessor.count;

					section.triangleCount = accessor.count / 3;
				}
			}

			mesh.positions = Push<Vec3f>(arena, positionBufferSize / sizeof(Vec3f));

			uint8* positionBuffer = reinterpret_cast<uint8*>(mesh.positions.data);
			for (TSpan buffer : positionBuffers)
			{
				CopyMemory(positionBuffer, buffer.data, buffer.length);
				positionBuffer += buffer.length;
			}

			mesh.boneIndices = Push(arena, jointIndexBufferSize);

			uint8* jointIndexBuffer = mesh.boneIndices.data;
			for (TSpan buffer : jointIndexBuffers)
			{
				CopyMemory(jointIndexBuffer, buffer.data, buffer.length);
				jointIndexBuffer += buffer.length;
			}

			mesh.boneWeights = Push<float>(arena, jointWeightBufferSize / sizeof(float));

			uint8* jointWeightBuffer = reinterpret_cast<uint8*>(mesh.boneWeights.data);
			for (TSpan buffer : jointWeightBuffers)
			{
				CopyMemory(jointWeightBuffer, buffer.data, buffer.length);
				jointWeightBuffer += buffer.length;
			}

			mesh.indices = Push<uint16>(arena, indexBufferSize / sizeof(uint16));

			uint8* indexBuffer = reinterpret_cast<uint8*>(mesh.indices.data);
			for (TSpan buffer : indexBuffers)
			{
				CopyMemory(indexBuffer, buffer.data, buffer.length);
				indexBuffer += buffer.length;
			}
		}

		return true;
	}

	bool LoadGlbSkeletons(Arena& arena, TSpan<uint8> fileData, TSpan<Skeleton>& skeletons)
	{
		ArenaScope scratch = GetScratchArena(&arena);

		GltfAsset gltfAsset;
		if (!LoadGlbAsset(scratch.arena, fileData, gltfAsset))
		{
			return false;
		}

		TSpan<int32> nodeToParentIdx = Push<int32>(scratch.arena, gltfAsset.nodes.length);
		for (int32 nodeIdx = 0; nodeIdx < nodeToParentIdx.length; ++nodeIdx)
		{
			const GltfNode& gltfNode = gltfAsset.nodes[nodeIdx];
			for (int32 childNodeIdx : gltfNode.children)
			{
				nodeToParentIdx[childNodeIdx] = nodeIdx;
			}
		}

		skeletons = PushZeroed<Skeleton>(arena, gltfAsset.skins.length);

		for (size_t skeletonIdx = 0; skeletonIdx < skeletons.length; ++skeletonIdx)
		{
			const GltfSkin& gltfSkin = gltfAsset.skins[skeletonIdx];

			Skeleton& skeleton = skeletons[skeletonIdx];
			skeleton.bones = Push<Bone>(arena, gltfSkin.joints.length);

			for (int32 boneIdx = 0; boneIdx < skeleton.bones.length; ++boneIdx)
			{
				const int32 gltfJointNodeIdx = gltfSkin.joints[boneIdx];
				const int32 gltfJointParentNodeIdx = nodeToParentIdx[gltfJointNodeIdx];
				const GltfNode& gltfJointNode = gltfAsset.nodes[gltfJointNodeIdx];

				Bone& bone = skeleton.bones[boneIdx];

				// #TODO: Fallback joint name
				bone.name = Copy(arena, gltfJointNode.name);

				bone.parentIdx = -1;
				for (int32 parentBoneIdx = 0; parentBoneIdx < boneIdx; ++parentBoneIdx)
				{
					if (gltfSkin.joints[parentBoneIdx] == gltfJointParentNodeIdx)
					{
						bone.parentIdx = parentBoneIdx;
						break;
					}
				}
			}

			if (gltfSkin.inverseBindMatrices != -1)
			{
				const GltfAccessor& accessor = gltfAsset.accessors[gltfSkin.inverseBindMatrices];
				BK_ASSERTF(accessor.componentType == GltfComponentType::Float32, "Unexpected buffer layout");
				BK_ASSERTF(accessor.componentElements == 16, "Unexpected buffer layout");
				BK_ASSERTF(accessor.normalized == false, "Unexpected buffer layout");

				skeleton.invBindPose = Push<Mat4f>(arena, accessor.count);
				CopyAccessorBuffer(gltfAsset, accessor, AsBytes(skeleton.invBindPose));
			}
		}

		return true;
	}

	bool LoadGlbAnimations(Arena& arena, TSpan<uint8> fileData, const Skeleton& skeleton, TSpan<AnimSequence>& animations)
	{
		ArenaScope scratch = GetScratchArena(&arena);

		GltfAsset gltfAsset;
		if (!LoadGlbAsset(scratch.arena, fileData, gltfAsset))
		{
			return false;
		}

		TSpan<int32> nodeToBoneIdx = Push<int32>(scratch.arena, gltfAsset.nodes.length);
		for (int32 nodeIdx = 0; nodeIdx < nodeToBoneIdx.length; ++nodeIdx)
		{
			const GltfNode& gltfNode = gltfAsset.nodes[nodeIdx];

			nodeToBoneIdx[nodeIdx] = -1;
			for (int32 boneIdx = 0; boneIdx < skeleton.bones.length; ++boneIdx)
			{
				const Bone& bone = skeleton.bones[boneIdx];
				if (bone.name == gltfNode.name)
				{
					nodeToBoneIdx[nodeIdx] = boneIdx;
					break;
				}
			}
		}

		animations = Push<AnimSequence>(arena, gltfAsset.animations.length);

		for (size_t animationIdx = 0; animationIdx < animations.length; ++animationIdx)
		{
			const GltfAnimation& gltfAnimation = gltfAsset.animations[animationIdx];

			AnimSequence& animation = animations[animationIdx];

			animation.name = Copy(arena, gltfAnimation.name);
			animation.duration = 0.0f;
			animation.tracks = PushZeroed<AnimTrack>(arena, skeleton.bones.length);

			for (const GltfAnimationChannel& channel : gltfAnimation.channels)
			{
				int32 trackIdx = channel.targetNode >= 0 ? nodeToBoneIdx[channel.targetNode] : -1;
				if (trackIdx == -1)
				{
					continue;
				}

				const GltfAnimationSampler& sampler = gltfAnimation.samplers[channel.sampler];
				AnimTrack& track = animation.tracks[trackIdx];

				const GltfAccessor& inputAccessor = gltfAsset.accessors[sampler.input];
				BK_ASSERTF(inputAccessor.componentType == GltfComponentType::Float32, "Unexpected buffer layout");
				BK_ASSERTF(inputAccessor.normalized == false, "Unexpected buffer layout");

				TSpan<float> keyframeTimes = Push<float>(arena, inputAccessor.count * inputAccessor.componentElements);
				CopyAccessorBuffer(gltfAsset, inputAccessor, AsBytes(keyframeTimes));

				if (keyframeTimes.length == 0)
				{
					continue;
				}

				animation.duration = Max(animation.duration, keyframeTimes[keyframeTimes.length - 1]);

				switch (channel.targetPath)
				{
					case GltfAnimationTargetPath::Translation:
					{
						const GltfAccessor& outputAccessor = gltfAsset.accessors[sampler.output];
						BK_ASSERTF(outputAccessor.componentType == GltfComponentType::Float32, "Unexpected buffer layout");
						BK_ASSERTF(outputAccessor.componentElements == 3, "Unexpected buffer layout");
						BK_ASSERTF(outputAccessor.normalized == false, "Unexpected buffer layout");

						track.translationTimes = keyframeTimes;
						track.translations = Push<Vec3f>(arena, outputAccessor.count);
						CopyAccessorBuffer(gltfAsset, outputAccessor, AsBytes(track.translations));

						break;
					}

					case GltfAnimationTargetPath::Rotation:
					{
						const GltfAccessor& outputAccessor = gltfAsset.accessors[sampler.output];
						BK_ASSERTF(outputAccessor.componentType == GltfComponentType::Float32, "Unexpected buffer layout");
						BK_ASSERTF(outputAccessor.componentElements == 4, "Unexpected buffer layout");
						BK_ASSERTF(outputAccessor.normalized == false, "Unexpected buffer layout");

						track.rotationTimes = keyframeTimes;
						track.rotations = Push<Quat4f>(arena, outputAccessor.count);
						CopyAccessorBuffer(gltfAsset, outputAccessor, AsBytes(track.rotations));

						break;
					}

					case GltfAnimationTargetPath::Scale:
					{
						const GltfAccessor& outputAccessor = gltfAsset.accessors[sampler.output];
						BK_ASSERTF(outputAccessor.componentType == GltfComponentType::Float32, "Unexpected buffer layout");
						BK_ASSERTF(outputAccessor.componentElements == 3, "Unexpected buffer layout");
						BK_ASSERTF(outputAccessor.normalized == false, "Unexpected buffer layout");

						track.scaleTimes = keyframeTimes;
						track.scales = Push<Vec3f>(arena, outputAccessor.count);
						CopyAccessorBuffer(gltfAsset, outputAccessor, AsBytes(track.scales));

						break;
					}

					default: break;
				}
			}
		}

		return true;
	}
}
