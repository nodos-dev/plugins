// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginAPI.h>

#include <Builtins_generated.h>

#include <Nodos/Plugin.hpp>
#include "nosExperiment/Test_generated.h"


// Nodos SDK
#include <PluginManifest_generated.h>

namespace nos::experiment
{
NOS_REGISTER_NAME(Type)
NOS_REGISTER_NAME(Table)
NOS_REGISTER_NAME(PartialUpdateTest)
struct PartialUpdateTestNode : NodeContext
{
    std::optional<nos::TypeInfo> Type = {};
    nos::Name VisualizerName = {};

    nosResult OnCreate(const fb::Node* node) override
    {
		return NOS_RESULT_SUCCESS;
    }

	std::vector<nosName> AllTypeNames;


    void SetTableOfExistingTable() {
        auto pinId = GetPinId(NSN_Table);
        if (!pinId) {
            nosEngine.LogE("Pin not found!");
            return;
        }

        nosBuffer buf;
        if (nosEngine.GetDefaultValueOfType(nos::Name("nos.experiment.TestTable"), &buf) != NOS_RESULT_SUCCESS) {
            nosEngine.LogE("Failed to get default value of type");
            return;
        }
        nos::experiment::TTestTable testTable = {};
        InterpretPinValue<nos::experiment::TestTable>(buf)->UnPackTo(&testTable);
        testTable.test1 = "Set by node function: SetTableOfExistingTable()";
        testTable.test2 = std::make_unique<nos::experiment::TestStruct2>();
        testTable.test2->mutable_test1().mutate_test1(15.0f);
        testTable.test2->mutate_test2(30.0f);
        nos::Buffer finalBuf = nos::Buffer::From(testTable);

        SetField(*pinId, { { NOS_DATA_PATH_FIELD_COMPONENT, NOS_NAME_STATIC("table") } , { NOS_DATA_PATH_FIELD_COMPONENT, NOS_NAME_STATIC("table1") } }, finalBuf);
    }

    void SetTablesWholeArray(NodeExecuteParams& execParams) {
        auto pinId = GetPinId(NSN_Table);
        if (!pinId) {
            nosEngine.LogE("Pin not found!");
            return;
        }

        nosBuffer tableDef;
        if (nosEngine.GetDefaultValueOfType(nos::Name("nos.experiment.TestTable2"), &tableDef) != NOS_RESULT_SUCCESS) {
            nosEngine.LogE("Failed to get default value of type");
            return;
        }
        nos::experiment::TTestTable2 elements[2]{};
        // Set first element
        {
            auto& testTable = elements[0];
            InterpretPinValue<nos::experiment::TestTable2>(tableDef)->UnPackTo(&testTable);
            testTable.table1 = std::make_unique<nos::experiment::TTestTable>();
            testTable.table1->test1 = "First Element";
            testTable.table1->test2 = std::make_unique<nos::experiment::TestStruct2>();
            testTable.table1->test2->mutable_test1().mutate_test1(10.0f);
            testTable.table1->test2->mutate_test2(20.0f);
            testTable.test_enum = nos::experiment::TestEnumUint::TEST1;
        }
        // Set second element
        {
            auto& testTable = elements[1];
            InterpretPinValue<nos::experiment::TestTable2>(tableDef)->UnPackTo(&testTable);
            testTable.table1 = std::make_unique<nos::experiment::TTestTable>();
            testTable.table1->test1 = "Second Element";
            testTable.table1->test2 = std::make_unique<nos::experiment::TestStruct2>();
            testTable.table1->test2->mutable_test1().mutate_test1(30.0f);
            testTable.table1->test2->mutate_test2(60.0f);
            testTable.test_enum = nos::experiment::TestEnumUint::TEST2;
        }

		SetField(*pinId, { nosDataPathComponent{ NOS_DATA_PATH_FIELD_COMPONENT, NOS_NAME_STATIC("table_array")} }, {});
        for (size_t i = 0; i < sizeof(elements) / sizeof(elements[0]); i++) {
			AddElementToArray(*pinId,
				{ { NOS_DATA_PATH_FIELD_COMPONENT, NOS_NAME_STATIC("table_array") }, { NOS_DATA_PATH_ARRAY_ELEMENT, i } },
				nos::Buffer::From(elements[i]));
        }
    }

    void RemoveElement1() {
        auto pinId = GetPinId(NSN_Table);
        if (!pinId)
            nosEngine.LogE("Pin not found!");

		RemoveElementFromArray(*pinId, { { NOS_DATA_PATH_FIELD_COMPONENT, NOS_NAME_STATIC("table_array") }, { NOS_DATA_PATH_ARRAY_ELEMENT, 1 } });
    }

    nosResult SetPinSubfieldVisualizer(nosFunctionExecuteParams* params) {
        auto pinId = GetPinId(NSN_Table);
        if (!pinId)
            nosEngine.LogE("Pin not found!");

        fb::TVisualizer viz{};

        auto pinValues = GetPinValues(params->FunctionNodeExecuteParams);
        auto vizType = GetPinValue<fb::VisualizerType>(pinValues, NOS_NAME_STATIC("Type"));
        if (!vizType)
            nosEngine.LogE("Visualizer Type pin is empty!");
        auto subfieldPath = GetPinValue<const char>(pinValues, NOS_NAME_STATIC("SubfieldPath"));
        if (!subfieldPath)
            nosEngine.LogE("SubfieldPath pin is empty!");
        if (!vizType || !subfieldPath)
            return NOS_RESULT_INVALID_ARGUMENT;
        viz.visualizer_subfield_path = subfieldPath;
        viz.type = *vizType;

        if (auto vizName = GetPinValue<const char>(pinValues, NOS_NAME_STATIC("Name")))
            viz.name = vizName;

        if (auto vizFileExts = GetPinValue<const char>(pinValues, NOS_NAME_STATIC("FileExtensions")))
            viz.file_extensions = { std::string(vizFileExts) };

        if (auto vizDiscardedNames = GetPinValue<const char>(pinValues, NOS_NAME_STATIC("DiscardedNames")))
            viz.discarded_names = { std::string(vizDiscardedNames) };

        if (auto vizHideVal = GetPinValue<const char>(pinValues, NOS_NAME_STATIC("HideValue")))
            viz.hide_value = *vizHideVal;

        if (auto filePickerType = GetPinValue<fb::FilePickerType>(pinValues, NOS_NAME_STATIC("FilePickerType")))
            viz.file_picker_type = *filePickerType;

        SetPinVisualizer(NSN_Table, viz);
        return NOS_RESULT_SUCCESS;
    }

    static nosResult GetFunctions(size_t* count, nosName* names, nosPfnNodeFunctionExecute* fns)
    {
        *count = 4;
        if (!names || !fns)
            return NOS_RESULT_SUCCESS;

        names[0] = NOS_NAME_STATIC("SetTableOfExistingTable");
        fns[0] = [](void* ctx, nosFunctionExecuteParams* params)
            {
                auto node = (PartialUpdateTestNode*)ctx;
                node->SetTableOfExistingTable();
                return NOS_RESULT_SUCCESS;
            };

        names[1] = NOS_NAME_STATIC("SetTablesWholeArray");
        fns[1] = [](void* ctx, nosFunctionExecuteParams* params)
            {
				NodeExecuteParams execParams = params->ParentNodeExecuteParams;
                auto node = (PartialUpdateTestNode*)ctx;
                node->SetTablesWholeArray(execParams);
                return NOS_RESULT_SUCCESS;
            };

        names[2] = NOS_NAME_STATIC("RemoveElement1");
        fns[2] = [](void* ctx, nosFunctionExecuteParams* params)
            {
                auto node = (PartialUpdateTestNode*)ctx;
                node->RemoveElement1();
                return NOS_RESULT_SUCCESS;
            };

        names[3] = NOS_NAME_STATIC("SetPinSubfieldVisualizer");
        fns[3] = [](void* ctx, nosFunctionExecuteParams* params)
            {
                auto node = (PartialUpdateTestNode*)ctx;
                return node->SetPinSubfieldVisualizer(params);
            };

        return NOS_RESULT_SUCCESS;
    }
};

nosResult RegisterPartialUpdateTest(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NSN_PartialUpdateTest, PartialUpdateTestNode, fn);
	return NOS_RESULT_SUCCESS;
}

} // namespace nos