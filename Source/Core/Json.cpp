#include "Json.h"

#include "Memory.h"

namespace Bk
{
	JsonValue* ParseJson(Arena& arena, String json)
	{
		ArenaMarker marker = PushMarker(arena);

		JsonValue* root = nullptr;
		JsonValue* scope = nullptr;

		for (size_t position = 0; position < json.length; position += 1)
		{
			JsonValue* current = nullptr;
			char c = json.data[position];

			if (c == '{' || c == '[')
			{
				JsonValue* newScope = PushZeroed<JsonValue>(arena);
				newScope->type = c == '{' ? JsonType::Object : JsonType::Array;
				newScope->value.data = json.data + position;
				newScope->parent = scope;

				scope = newScope;
			}
			else if (c == '}' || c == ']')
			{
				if (!scope)
				{
					// Invalid object/array
					break;
				}

				current = scope;
				current->value.length = static_cast<size_t>(json.data + position + 1 - current->value.data);
				current->sibling = nullptr;
			}
			else if (c == '"')
			{
				position += 1;
				size_t start = position;

				for (; position < json.length; position += 1)
				{
					c = json.data[position];

					if (c == '"')
					{
						current = PushZeroed<JsonValue>(arena);
						current->type = JsonType::String;
						current->value = Slice(json, start, position - start);
						current->parent = scope;

						break;
					}

					if (c == '\\' && position + 1 < json.length)
					{
						static constexpr AsciiSet escapeChars("\"\\/bfnrt");

						position += 1;
						c = json.data[position];

						if (c == 'u' && position + 4 < json.length)
						{
							char c1 = json.data[position + 1];
							char c2 = json.data[position + 2];
							char c3 = json.data[position + 3];
							char c4 = json.data[position + 4];

							if (!IsHexDigit(c1) || !IsHexDigit(c2) || !IsHexDigit(c3) || !IsHexDigit(c4))
							{
								// Invalid codepoint
								break;
							}

							position += 4;
						}
						else if (!Contains(escapeChars, c))
						{
							// Invalid escape sequence
							break;
						}
					}
				}

				if (!current)
				{
					// Invalid string
					break;
				}
			}
			else if (c == '-' || IsDigit(c))
			{
				size_t start = position;

				static constexpr AsciiSet numberChars("0123456789.+-eE");
				for (; position < json.length; position += 1)
				{
					c = json.data[position];

					if (!Contains(numberChars, c))
					{
						current = PushZeroed<JsonValue>(arena);
						current->type = JsonType::Number;
						current->value = Slice(json, start, position - start);
						current->parent = scope;
						ParseValue(current->value, current->asNumber);

						position -= 1;
						break;
					}
				}

				if (position == json.length)
				{
					current = PushZeroed<JsonValue>(arena);
					current->type = JsonType::Number;
					current->value = Slice(json, start, position - start);
					current->parent = scope;
					ParseValue(current->value, current->asNumber);
				}
			}
			else if (c == 't')
			{
				String value = Slice(json, position, 4);
				if (value == "true")
				{
					current = PushZeroed<JsonValue>(arena);
					current->type = JsonType::Bool;
					current->value = value;
					current->parent = scope;
					current->asBool = true;

					position += 3;
				}
			}
			else if (c == 'f')
			{
				String value = Slice(json, position, 5);
				if (value == "false")
				{
					current = PushZeroed<JsonValue>(arena);
					current->type = JsonType::Bool;
					current->value = value;
					current->parent = scope;
					current->asBool = false;

					position += 4;
				}
			}
			else if (c == 'n')
			{
				String value = Slice(json, position, 4);
				if (value == "null")
				{
					current = PushZeroed<JsonValue>(arena);
					current->type = JsonType::Null;
					current->value = value;
					current->parent = scope;

					position += 3;
				}
			}

			if (current)
			{
				scope = current->parent;
				if (scope)
				{
					// Hijack the scope's sibling pointer (since it won't be resolved while traversing its children) to
					// keep track of its last child element, so children can be easily linked together as siblings

					if (scope->sibling)
					{
						scope->sibling->sibling = current;
					}

					scope->sibling = current;
					scope->children += 1;
				}
				else if (!root)
				{
					root = current;
				}
			}
		}

		if (!root)
		{
			PopMarker(arena, marker);
		}

		return root;
	}

	JsonValue* FindJsonValue(JsonValue* value, String path)
	{
		while (value && path.length != 0)
		{
			size_t pathDelimIdx = Find(path, '.');
			String pathSlice = Slice(path, 0, pathDelimIdx);

			uint64 idx;
			if (value->type == JsonType::Array && ParseValue(pathSlice, idx))
			{
				value = FindJsonValueInArray(value, idx);
			}
			else if (value->type == JsonType::Object)
			{
				value = FindJsonValueInObject(value, pathSlice);
			}
			else
			{
				value = nullptr;
				break;
			}

			if (pathDelimIdx == SIZE_MAX)
			{
				break;
			}

			path = Slice(path, pathDelimIdx + 1);
		}

		return value;
	}

	JsonValue* FindJsonValueInObject(JsonValue* object, String key)
	{
		if (!object || object->type != JsonType::Object || object->children < 2)
		{
			return nullptr;
		}

		JsonValue* child = object + 1;
		while (child && child->sibling)
		{
			if (child->value == key)
			{
				return child->sibling;
			}

			child = child->sibling->sibling;
		}

		return nullptr;
	}

	JsonValue* FindJsonValueInArray(JsonValue* array, size_t idx)
	{
		if (!array || array->type != JsonType::Array || array->children <= idx)
		{
			return nullptr;
		}

		JsonValue* child = array + 1;
		for (size_t childIdx = 0; child && childIdx < idx; ++childIdx)
		{
			child = child->sibling;
		}

		return child;
	}
}
