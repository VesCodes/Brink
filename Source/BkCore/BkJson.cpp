#include "BkJson.h"

#include "BkArena.h"

namespace Bk
{
	JsonValue* ParseJson(Arena& arena, String json)
	{
		ArenaMarker marker = arena.GetMarker();

		JsonValue* root = nullptr;
		JsonValue* scope = nullptr;

		for (size_t position = 0; position < json.length; position += 1)
		{
			JsonValue* current = nullptr;
			char c = json.data[position];

			if (c == '{' || c == '[')
			{
				JsonValue* newScope = arena.PushZeroed<JsonValue>();
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
				current->value.length = size_t(json.data + position + 1 - current->value.data);
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
						current = arena.PushZeroed<JsonValue>();
						current->type = JsonType::String;
						current->value = json.Range(start, position);
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
						else if (!escapeChars.Contains(c))
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

					if (!numberChars.Contains(c))
					{
						current = arena.PushZeroed<JsonValue>();
						current->type = JsonType::Number;
						current->value = json.Range(start, position);
						current->parent = scope;
						current->value.Parse(current->asNumber);

						position -= 1;
						break;
					}
				}

				if (position == json.length)
				{
					current = arena.PushZeroed<JsonValue>();
					current->type = JsonType::Number;
					current->value = json.Range(start, position);
					current->parent = scope;
					current->value.Parse(current->asNumber);
				}
			}
			else if (c == 't')
			{
				String value = json.Slice(position, 4);
				if (value == "true")
				{
					current = arena.PushZeroed<JsonValue>();
					current->type = JsonType::Bool;
					current->value = value;
					current->parent = scope;
					current->asBool = true;

					position += 3;
				}
			}
			else if (c == 'f')
			{
				String value = json.Slice(position, 5);
				if (value == "false")
				{
					current = arena.PushZeroed<JsonValue>();
					current->type = JsonType::Bool;
					current->value = value;
					current->parent = scope;
					current->asBool = false;

					position += 4;
				}
			}
			else if (c == 'n')
			{
				String value = json.Slice(position, 4);
				if (value == "null")
				{
					current = arena.PushZeroed<JsonValue>();
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
			arena.SetMarker(marker);
		}

		return root;
	}

	JsonValue* FindJsonValue(JsonValue* value, String path)
	{
		while (value && path.length > 0)
		{
			size_t pathDelimIdx = path.Find('.');
			String pathSlice = path.Range(0, pathDelimIdx);

			uint64 index;
			if (value->type == JsonType::Array && pathSlice.Parse(index))
			{
				value = FindJsonValueInArray(value, index);
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

			path = path.Slice(pathDelimIdx + 1);
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

	JsonValue* FindJsonValueInArray(JsonValue* array, size_t index)
	{
		if (!array || array->type != JsonType::Array || array->children <= index)
		{
			return nullptr;
		}

		JsonValue* child = array + 1;
		for (size_t childIdx = 0; child && childIdx < index; ++childIdx)
		{
			child = child->sibling;
		}

		return child;
	}
}
