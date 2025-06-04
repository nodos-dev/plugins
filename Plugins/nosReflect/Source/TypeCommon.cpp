// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include "TypeCommon.h"
namespace flatbuffers
{

EnumVal* EnumDef::ReverseLookup(int64_t enum_idx,
	bool skip_union_default) const {
	auto skip_first = static_cast<int>(is_union && skip_union_default);
	for (auto it = Vals().begin() + skip_first; it != Vals().end(); ++it) {
		if ((*it)->GetAsInt64() == enum_idx) { return *it; }
	}
	return nullptr;
}
}
namespace nos::reflect
{

bool AreFlatBuffersEqual(const nosTypeInfo* type,
		                 void* first,
		                 void* second)
{
	switch (type->BaseType)
	{
	case NOS_BASE_TYPE_STRUCT:
		{
			if (type->ByteSize)
			{
				return memcmp(first, second, type->ByteSize) == 0;
			}
			// This is a table.
			auto firstTable = static_cast<flatbuffers::Table*>(first);
			auto secondTable = static_cast<flatbuffers::Table*>(second);
			for (int i = 0; i < type->FieldCount; ++i)
			{
				auto field = &type->Fields[i];
				auto field1Exists = firstTable->CheckField(field->Offset);
				auto field2Exists = secondTable->CheckField(field->Offset);
				if ((field1Exists ^ field2Exists) && (field->Type->BaseType == NOS_BASE_TYPE_STRING || field->Type->BaseType == NOS_BASE_TYPE_ARRAY))
				{
					// If one is present but the other is not, check the size is 0 for the present one.
					// TODO: This might not be desired behaviour in some cases.
					auto presentVec = field1Exists ? firstTable->GetPointer<flatbuffers::Vector<uint8_t>*>(field->Offset) 
						: (field2Exists ? secondTable->GetPointer<flatbuffers::Vector<uint8_t>*>(field->Offset) : nullptr);
					if (!presentVec)
						continue;
					if (presentVec->size() != 0)
						return false;
					continue;
				}
				if (field1Exists != field2Exists)
					return false;
				// Skip if fields are not present in the source.
				if (!field1Exists)
					continue;
				void* field1, *field2;
				if (field->Type->ByteSize)
				{
					field1 = firstTable->GetStruct<void*>(field->Offset);
					field2 = secondTable->GetStruct<void*>(field->Offset);
				}
				else
				{
					if (field->Type->BaseType == NOS_BASE_TYPE_STRING)
					{
						auto str1 = firstTable->GetPointer<const flatbuffers::String*>(field->Offset);
						auto str2 = secondTable->GetPointer<const flatbuffers::String*>(field->Offset);
						field1 = (void*)str1->c_str();
						field2 = (void*)str2->c_str();
					}
					else
					{
						field1 = firstTable->GetPointer<flatbuffers::Table*>(field->Offset);
						field2 = secondTable->GetPointer<flatbuffers::Table*>(field->Offset);	
					}
					
				}
				if (!AreFlatBuffersEqual(field->Type, field1, field2))
					return false;
			}
			return true;
		}
	case NOS_BASE_TYPE_STRING:
		{
			auto firstString = static_cast<const char*>(first);
			auto secondString = static_cast<const char*>(second);
			return strcmp(firstString, secondString) == 0;
		}
	case NOS_BASE_TYPE_ARRAY:
		{
			auto vec1 = static_cast<const flatbuffers::Vector<uint8_t>*>(first);
			auto vec2 = static_cast<const flatbuffers::Vector<uint8_t>*>(second);
			if (vec1->size() != vec2->size())
				return false;
			auto elementType = type->ElementType;
			if (elementType->ByteSize)
			{
				for (size_t i = 0; i < vec1->size(); ++i)
				{
					if (memcmp(vec1->data() + i * type->ByteSize, vec2->data() + i * type->ByteSize, type->ByteSize) != 0)
						return false;
				}
			}
			else
			{
				if (elementType->BaseType == NOS_BASE_TYPE_STRING)
				{
					auto vec1Vectors = reinterpret_cast<const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>*>(vec1);
					auto vec2Vectors = reinterpret_cast<const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>*>(vec2);
					for (size_t i = 0; i < vec1Vectors->size(); ++i)
					{
						auto elem1 = vec1Vectors->Get(i);
						auto elem2 = vec2Vectors->Get(i);
						if (!AreFlatBuffersEqual(elementType, (void*)elem1->c_str(), (void*)elem2->c_str()))
							return false;
					}
				}
				else
				{
					auto vec1Tables = reinterpret_cast<const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::Table>>*>(vec1);
					auto vec2Tables = reinterpret_cast<const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::Table>>*>(vec2);
					for (size_t i = 0; i < vec1Tables->size(); ++i)
					{
						auto elem1 = vec1Tables->Get(i);
						auto elem2 = vec2Tables->Get(i);
						if (!AreFlatBuffersEqual(elementType, (void*)elem1, (void*)elem2))
							return false;
					}
				}
			}
			return true;
		}
	case NOS_BASE_TYPE_UNION:
		{
			nosEngine.LogE("Union comparison not implemented yet.");
			break;
		}
	default: // Scalars
		{
			if (type->ByteSize)
			{
				return memcmp(first, second, type->ByteSize) == 0;
			}
			break;
		}
	}
	return true;
}

CompareResult CompareStrings(const char* first, const char* second)
{
	int res = strcmp(first, second);
	if (res == 0)
		return CompareResult::Equal;
	if (res < 0)
		return CompareResult::Less;
	return CompareResult::Greater;
}

} // namespace nos::engine