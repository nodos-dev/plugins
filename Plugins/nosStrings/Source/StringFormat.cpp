// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>

// stl
#include <format>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace nos::strings
{
// Placeholder syntax: "Hello {user}, you have {count:d} messages, ratio {ratio:.2f}".
// Each unique {name} opens an input pin named after it. The optional ":spec" is a
// std::format specification; the spec's presentation type determines the pin type:
//   d b B o x X c          -> integer pin ("long")
//   a A e E f F g G %       -> floating pin ("double")
//   (none) / s / alignment  -> string pin ("string")
// Literal braces are written as "{{" and "}}".

static constexpr std::string_view RESERVED_PINS[] = { "Format", "Output" };

enum class ArgType : uint8_t
{
	STRING,
	INT,
	DOUBLE,
};

static const char* TypeNameOf(ArgType type)
{
	switch (type)
	{
	case ArgType::INT:    return "long";
	case ArgType::DOUBLE: return "double";
	default:              return "string";
	}
}

static ArgType InferType(std::string_view spec)
{
	if (spec.empty())
		return ArgType::STRING;
	char last = spec.back();
	if (std::string_view("dbBoxXc").find(last) != std::string_view::npos)
		return ArgType::INT;
	if (std::string_view("aAeEfFgG%").find(last) != std::string_view::npos)
		return ArgType::DOUBLE;
	return ArgType::STRING;
}

static bool IsReserved(std::string_view name)
{
	for (auto reserved : RESERVED_PINS)
		if (name == reserved)
			return true;
	return false;
}

static std::string Trim(std::string_view s)
{
	size_t b = s.find_first_not_of(" \t");
	if (b == std::string_view::npos)
		return {};
	size_t e = s.find_last_not_of(" \t");
	return std::string(s.substr(b, e - b + 1));
}

struct StringFormatNode : NodeContext
{
	struct Segment
	{
		bool IsLiteral = true;
		std::string Literal;  // valid when IsLiteral
		std::string Name;     // placeholder name (when !IsLiteral)
		std::string Spec;     // std::format spec without the leading ':'
		ArgType Type = ArgType::STRING;
	};

	std::string Format;
	std::vector<Segment> Segments;
	// Unique placeholder name -> resolved type (type is taken from its first occurrence).
	std::unordered_map<std::string, ArgType> Placeholders;
	std::string ParseError;

	nosResult OnCreate(nosFbNodePtr node) override
	{
		if (auto* pins = node->pins())
		{
			for (auto const* pin : *pins)
			{
				if (pin->name()->string_view() == "Format" && pin->data() && pin->data()->size() > 0)
				{
					Format = std::string(reinterpret_cast<const char*>(pin->data()->Data()));
					break;
				}
			}
		}
		Parse();
		UpdatePins();
		return NOS_RESULT_SUCCESS;
	}

	void OnPinValueChanged(nos::Name pinName, uuid const& pinId, nosBuffer value) override
	{
		if (pinName != NOS_NAME("Format"))
			return;
		std::string newFormat = value.Data ? static_cast<const char*>(value.Data) : "";
		if (Format == newFormat)
			return;
		Format = std::move(newFormat);
		Parse();
		UpdatePins();
	}

	// Parse Format into Segments + the unique Placeholders map.
	void Parse()
	{
		Segments.clear();
		Placeholders.clear();
		ParseError.clear();

		std::string literal;
		auto flushLiteral = [&]()
		{
			if (!literal.empty())
			{
				Segments.push_back({ true, literal, {}, {}, ArgType::STRING });
				literal.clear();
			}
		};

		for (size_t i = 0; i < Format.size();)
		{
			char c = Format[i];
			if (c == '{')
			{
				if (i + 1 < Format.size() && Format[i + 1] == '{')
				{
					literal += '{';
					i += 2;
					continue;
				}
				size_t close = Format.find('}', i + 1);
				if (close == std::string::npos)
				{
					ParseError = "Unterminated '{' in format string";
					literal += Format.substr(i);
					break;
				}
				std::string field = Format.substr(i + 1, close - (i + 1));
				i = close + 1;

				auto colon = field.find(':');
				std::string name = Trim(colon == std::string::npos ? field : field.substr(0, colon));
				std::string spec = colon == std::string::npos ? std::string() : field.substr(colon + 1);

				if (name.empty())
				{
					ParseError = "Empty placeholder name in format string";
					continue;
				}
				if (IsReserved(name))
				{
					ParseError = "Placeholder name '" + name + "' is reserved";
					continue;
				}

				ArgType type = InferType(spec);
				auto [it, inserted] = Placeholders.try_emplace(name, type);
				if (!inserted)
					type = it->second;  // first occurrence wins for typing

				flushLiteral();
				Segments.push_back({ false, {}, name, spec, type });
			}
			else if (c == '}')
			{
				if (i + 1 < Format.size() && Format[i + 1] == '}')
				{
					literal += '}';
					i += 2;
					continue;
				}
				literal += '}';
				i += 1;
			}
			else
			{
				literal += c;
				i += 1;
			}
		}
		flushLiteral();
	}

	// Reconcile the dynamic input pins with the current set of placeholders.
	void UpdatePins()
	{
		flatbuffers::FlatBufferBuilder fbb;
		std::vector<flatbuffers::Offset<nos::fb::Pin>> pinsToAdd;
		std::vector<flatbuffers::Offset<nos::PartialPinUpdate>> pinsToUpdate;
		std::vector<nos::fb::UUID> pinsToDelete;

		for (auto const& [name, type] : Placeholders)
		{
			nos::Name pinName(name.c_str());
			if (auto* pin = GetPin(pinName))
			{
				if (pin->TypeName != nos::Name(TypeNameOf(type)) || pin->IsOrphan)
				{
					pinsToUpdate.push_back(CreatePartialPinUpdateDirect(
						fbb,
						&pin->Id,
						0,
						nos::fb::CreatePinOrphanStateDirect(fbb, fb::PinOrphanStateType::ACTIVE),
						TypeNameOf(type),
						name.c_str()));
				}
			}
			else
			{
				uuid id = nosEngine.GenerateID();
				std::vector<uint8_t> data = DefaultData(type);
				pinsToAdd.push_back(fb::CreatePinDirect(
					fbb,
					&id,
					name.c_str(),
					TypeNameOf(type),
					nos::fb::ShowAs::INPUT_PIN,
					nos::fb::CanShowAs::INPUT_PIN_OR_PROPERTY,
					0, &data));
			}
		}

		// Delete dynamic pins that are no longer referenced by the format string.
		for (auto const& [name, id] : PinName2Id)
		{
			if (IsReserved(name.AsString()))
				continue;
			if (!Placeholders.contains(name.AsString()))
				pinsToDelete.push_back(id);
		}

		if (!pinsToAdd.empty() || !pinsToDelete.empty() || !pinsToUpdate.empty())
		{
			HandleEvent(CreateAppEvent(fbb, CreatePartialNodeUpdateDirect(
				fbb, &NodeId, ClearFlags::NONE,
				&pinsToDelete, &pinsToAdd, 0, 0, 0, 0, 0, &pinsToUpdate)));
		}

		UpdateStatus();
	}

	static std::vector<uint8_t> DefaultData(ArgType type)
	{
		switch (type)
		{
		case ArgType::INT:    return std::vector<uint8_t>(sizeof(int64_t), 0);
		case ArgType::DOUBLE: return std::vector<uint8_t>(sizeof(double), 0);
		default:              return std::vector<uint8_t>(1, 0);  // empty, null-terminated string
		}
	}

	void UpdateStatus()
	{
		if (!ParseError.empty())
		{
			SetPinOrphanState(NOS_NAME("Output"), fb::PinOrphanStateType::ORPHAN, ParseError.c_str());
			SetNodeStatusMessage(ParseError, fb::NodeStatusMessageType::FAILURE);
		}
		else
		{
			SetPinOrphanState(NOS_NAME("Output"), fb::PinOrphanStateType::ACTIVE);
			ClearNodeStatusMessages();
		}
	}

	std::string FormatValue(Segment const& seg, NodeExecuteParams const& pins)
	{
		nos::Name pinName(seg.Name.c_str());
		auto it = pins.find(pinName);
		std::string fmt = seg.Spec.empty() ? "{}" : "{:" + seg.Spec + "}";
		const void* data = (it != pins.end() && it->second.Object) ? pins.GetPinBuffer(pinName).Data : nullptr;

		switch (seg.Type)
		{
		case ArgType::INT:
		{
			int64_t v = data ? *static_cast<const int64_t*>(data) : 0;
			return std::vformat(fmt, std::make_format_args(v));
		}
		case ArgType::DOUBLE:
		{
			double v = data ? *static_cast<const double*>(data) : 0.0;
			return std::vformat(fmt, std::make_format_args(v));
		}
		default:
		{
			std::string v = data ? static_cast<const char*>(data) : "";
			return std::vformat(fmt, std::make_format_args(v));
		}
		}
	}

	nosResult ExecuteNode(nos::NodeExecuteParams const& pins) override
	{
		if (!ParseError.empty())
		{
			SetNodeStatusMessage(ParseError, fb::NodeStatusMessageType::FAILURE);
			return NOS_RESULT_FAILED;
		}

		std::string result;
		try
		{
			for (auto const& seg : Segments)
				result += seg.IsLiteral ? seg.Literal : FormatValue(seg, pins);
		}
		catch (std::format_error const& e)
		{
			std::string msg = std::string("Invalid format spec: ") + e.what();
			SetNodeStatusMessage(msg, fb::NodeStatusMessageType::FAILURE);
			return NOS_RESULT_FAILED;
		}

		SetPinValue(NOS_NAME("Output"), result.c_str());
		return NOS_RESULT_SUCCESS;
	}
};

nosResult RegisterStringFormat(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("StringFormat"), StringFormatNode, fn);
	return NOS_RESULT_SUCCESS;
}

} // namespace nos::strings
