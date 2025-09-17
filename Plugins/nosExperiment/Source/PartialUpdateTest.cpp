// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginAPI.h>

#include <Builtins_generated.h>

#include <Nodos/Plugin.hpp>
#include "Test_generated.h"


// Nodos SDK
#include <PluginManifest_generated.h>

namespace nos::experiment
{
NOS_REGISTER_NAME(Type)
NOS_REGISTER_NAME(Table)
NOS_REGISTER_NAME(PartialUpdateTest)
struct PartialUpdateTestNode : NodeContext
{
    // TODO: Transfer
    //std::optional<nos::TypeInfo> Type = {};
    //nos::Name VisualizerName = {};
    //
    //nosResult OnCreate(const fb::Node* node) override
    //{
	//	return NOS_RESULT_SUCCESS;
    //}
    //
	//std::vector<nosName> AllTypeNames;
    //
    //
    //void SetTableOfExistingTable() {
    //    auto pinId = GetPinId(NSN_Table);
    //    if (!pinId) {
    //        nosEngine.LogE("Pin not found!");
    //        return;
    //    }
    //
    //    nosBuffer buf;
    //    if (nosEngine.GetDefaultValueOfType(nos::Name("nos.experiment.TestTable"), &buf) != NOS_RESULT_SUCCESS) {
    //        nosEngine.LogE("Failed to get default value of type");
    //        return;
    //    }
    //    nos::experiment::TTestTable testTable = {};
    //    InterpretPinValue<nos::experiment::TestTable>(buf)->UnPackTo(&testTable);
    //    testTable.test1 = "Set by node function: SetTableOfExistingTable()";
    //    testTable.test2 = std::make_unique<nos::experiment::TestStruct2>();
    //    testTable.test2->mutable_test1().mutate_test1(15.0f);
    //    testTable.test2->mutate_test2(30.0f);
    //    nos::Buffer finalBuf = nos::Buffer::From(testTable);
    //
    //    SetField(*pinId, { { NOS_DATA_PATH_FIELD_COMPONENT, NOS_NAME_STATIC("table") } , { NOS_DATA_PATH_FIELD_COMPONENT, NOS_NAME_STATIC("table1") } }, finalBuf);
    //}
    //
    //void SetTablesWholeArray(NodeExecuteParams& execParams) {
    //    auto pinId = GetPinId(NSN_Table);
    //    if (!pinId) {
    //        nosEngine.LogE("Pin not found!");
    //        return;
    //    }
    //
    //    nosBuffer tableDef;
    //    if (nosEngine.GetDefaultValueOfType(nos::Name("nos.experiment.TestTable2"), &tableDef) != NOS_RESULT_SUCCESS) {
    //        nosEngine.LogE("Failed to get default value of type");
    //        return;
    //    }
    //    nos::experiment::TTestTable2 elements[2]{};
    //    // Set first element
    //    {
    //        auto& testTable = elements[0];
    //        InterpretPinValue<nos::experiment::TestTable2>(tableDef)->UnPackTo(&testTable);
    //        testTable.table1 = std::make_unique<nos::experiment::TTestTable>();
    //        testTable.table1->test1 = "First Element";
    //        testTable.table1->test2 = std::make_unique<nos::experiment::TestStruct2>();
    //        testTable.table1->test2->mutable_test1().mutate_test1(10.0f);
    //        testTable.table1->test2->mutate_test2(20.0f);
    //        testTable.test_enum = nos::experiment::TestEnumUint::TEST1;
    //    }
    //    // Set second element
    //    {
    //        auto& testTable = elements[1];
    //        InterpretPinValue<nos::experiment::TestTable2>(tableDef)->UnPackTo(&testTable);
    //        testTable.table1 = std::make_unique<nos::experiment::TTestTable>();
    //        testTable.table1->test1 = "Second Element";
    //        testTable.table1->test2 = std::make_unique<nos::experiment::TestStruct2>();
    //        testTable.table1->test2->mutable_test1().mutate_test1(30.0f);
    //        testTable.table1->test2->mutate_test2(60.0f);
    //        testTable.test_enum = nos::experiment::TestEnumUint::TEST2;
    //    }
    //
	//	SetField(*pinId, { nosDataPathComponent{ NOS_DATA_PATH_FIELD_COMPONENT, NOS_NAME_STATIC("table_array")} }, {});
    //    for (size_t i = 0; i < sizeof(elements) / sizeof(elements[0]); i++) {
	//		AddElementToArray(*pinId,
	//			{ { NOS_DATA_PATH_FIELD_COMPONENT, NOS_NAME_STATIC("table_array") }, { NOS_DATA_PATH_ARRAY_ELEMENT, i } },
	//			nos::Buffer::From(elements[i]));
    //    }
    //}
    //
    //void RemoveElement1() {
    //    auto pinId = GetPinId(NSN_Table);
    //    if (!pinId)
    //        nosEngine.LogE("Pin not found!");
    //
	//	RemoveElementFromArray(*pinId, { { NOS_DATA_PATH_FIELD_COMPONENT, NOS_NAME_STATIC("table_array") }, { NOS_DATA_PATH_ARRAY_ELEMENT, 1 } });
    //}
    //
    //static nosResult GetFunctions(size_t* count, nosName* names, nosPfnNodeFunctionExecute* fns)
    //{
    //    *count = 3;
    //    if (!names || !fns)
    //        return NOS_RESULT_SUCCESS;
    //
    //    names[0] = NOS_NAME_STATIC("SetTableOfExistingTable");
    //    fns[0] = [](void* ctx, nosFunctionExecuteParams* params)
    //        {
    //            auto writeImage = (PartialUpdateTestNode*)ctx;
    //            writeImage->SetTableOfExistingTable();
    //            return NOS_RESULT_SUCCESS;
    //        };
    //
    //    names[1] = NOS_NAME_STATIC("SetTablesWholeArray");
    //    fns[1] = [](void* ctx, nosFunctionExecuteParams* params)
    //        {
	//			NodeExecuteParams execParams = params->ParentNodeExecuteParams;
    //            auto writeImage = (PartialUpdateTestNode*)ctx;
    //            writeImage->SetTablesWholeArray(execParams);
    //            return NOS_RESULT_SUCCESS;
    //        };
    //
    //    names[2] = NOS_NAME_STATIC("RemoveElement1");
    //    fns[2] = [](void* ctx, nosFunctionExecuteParams* params)
    //        {
    //            auto writeImage = (PartialUpdateTestNode*)ctx;
    //            writeImage->RemoveElement1();
    //            return NOS_RESULT_SUCCESS;
    //        };
    //
    //    return NOS_RESULT_SUCCESS;
    //}
};

nosResult RegisterPartialUpdateTest(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NSN_PartialUpdateTest, PartialUpdateTestNode, fn);
	return NOS_RESULT_SUCCESS;
}

} // namespace nos