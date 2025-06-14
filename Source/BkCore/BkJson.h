#pragma once

#include "BkCore.h"
#include "BkString.h"

namespace Bk
{
	struct Arena;

	enum class JsonType : uint8
	{
		Null,
		Bool,
		Number,
		String,
		Array,
		Object,
	};

	struct JsonValue
	{
		JsonType type;
		String value;
		JsonValue* parent;
		JsonValue* sibling;

		union
		{
			bool asBool;
			double asNumber;
			size_t children;
		};
	};

	JsonValue* ParseJson(Arena& arena, String json);
	JsonValue* FindJsonValue(JsonValue* value, String path);
	JsonValue* FindJsonValueInObject(JsonValue* object, String key);
	JsonValue* FindJsonValueInArray(JsonValue* array, size_t index);
}
