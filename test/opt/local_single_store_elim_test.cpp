// Copyright (c) 2017 Valve Corporation
// Copyright (c) 2017 LunarG Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "pass_fixture.h"
#include "pass_utils.h"

namespace {

using namespace spvtools;

using LocalSingleStoreElimTest = PassTest<::testing::Test>;

TEST_F(LocalSingleStoreElimTest, PositiveAndNegative) {
  // Single store to v is optimized. Multiple store to
  // f is not optimized.
  //
  // #version 140
  //
  // in vec4 BaseColor;
  // in float fi;
  //
  // void main()
  // {
  //     vec4 v = BaseColor;
  //     float f = fi;
  //     if (f < 0)
  //         f = 0.0;
  //     gl_FragColor = v + f;
  // }

  const std::string predefs_before =
      R"(OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %main "main" %BaseColor %fi %gl_FragColor
OpExecutionMode %main OriginUpperLeft
OpSource GLSL 140
OpName %main "main"
OpName %v "v"
OpName %BaseColor "BaseColor"
OpName %f "f"
OpName %fi "fi"
OpName %gl_FragColor "gl_FragColor"
%void = OpTypeVoid
%9 = OpTypeFunction %void
%float = OpTypeFloat 32
%v4float = OpTypeVector %float 4
%_ptr_Function_v4float = OpTypePointer Function %v4float
%_ptr_Input_v4float = OpTypePointer Input %v4float
%BaseColor = OpVariable %_ptr_Input_v4float Input
%_ptr_Function_float = OpTypePointer Function %float
%_ptr_Input_float = OpTypePointer Input %float
%fi = OpVariable %_ptr_Input_float Input
%float_0 = OpConstant %float 0
%bool = OpTypeBool
%_ptr_Output_v4float = OpTypePointer Output %v4float
%gl_FragColor = OpVariable %_ptr_Output_v4float Output
)";

  const std::string predefs_after =
      R"(OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %main "main" %BaseColor %fi %gl_FragColor
OpExecutionMode %main OriginUpperLeft
OpSource GLSL 140
OpName %main "main"
OpName %BaseColor "BaseColor"
OpName %f "f"
OpName %fi "fi"
OpName %gl_FragColor "gl_FragColor"
%void = OpTypeVoid
%9 = OpTypeFunction %void
%float = OpTypeFloat 32
%v4float = OpTypeVector %float 4
%_ptr_Function_v4float = OpTypePointer Function %v4float
%_ptr_Input_v4float = OpTypePointer Input %v4float
%BaseColor = OpVariable %_ptr_Input_v4float Input
%_ptr_Function_float = OpTypePointer Function %float
%_ptr_Input_float = OpTypePointer Input %float
%fi = OpVariable %_ptr_Input_float Input
%float_0 = OpConstant %float 0
%bool = OpTypeBool
%_ptr_Output_v4float = OpTypePointer Output %v4float
%gl_FragColor = OpVariable %_ptr_Output_v4float Output
)";

  const std::string before =
      R"(%main = OpFunction %void None %9
%19 = OpLabel
%v = OpVariable %_ptr_Function_v4float Function
%f = OpVariable %_ptr_Function_float Function
%20 = OpLoad %v4float %BaseColor
OpStore %v %20
%21 = OpLoad %float %fi
OpStore %f %21
%22 = OpLoad %float %f
%23 = OpFOrdLessThan %bool %22 %float_0
OpSelectionMerge %24 None
OpBranchConditional %23 %25 %24
%25 = OpLabel
OpStore %f %float_0
OpBranch %24
%24 = OpLabel
%26 = OpLoad %v4float %v
%27 = OpLoad %float %f
%28 = OpCompositeConstruct %v4float %27 %27 %27 %27
%29 = OpFAdd %v4float %26 %28
OpStore %gl_FragColor %29
OpReturn
OpFunctionEnd
)";

  const std::string after =
      R"(%main = OpFunction %void None %9
%19 = OpLabel
%f = OpVariable %_ptr_Function_float Function
%20 = OpLoad %v4float %BaseColor
%21 = OpLoad %float %fi
OpStore %f %21
%22 = OpLoad %float %f
%23 = OpFOrdLessThan %bool %22 %float_0
OpSelectionMerge %24 None
OpBranchConditional %23 %25 %24
%25 = OpLabel
OpStore %f %float_0
OpBranch %24
%24 = OpLabel
%27 = OpLoad %float %f
%28 = OpCompositeConstruct %v4float %27 %27 %27 %27
%29 = OpFAdd %v4float %20 %28
OpStore %gl_FragColor %29
OpReturn
OpFunctionEnd
)";

  SinglePassRunAndCheck<opt::LocalSingleStoreElimPass>(
      predefs_before + before, predefs_after + after, true, true);
}

TEST_F(LocalSingleStoreElimTest, MultipleLoads) {
  // Single store to multiple loads of v is optimized.
  //
  // #version 140
  //
  // in vec4 BaseColor;
  // in float fi;
  //
  // void main()
  // {
  //     vec4 v = BaseColor;
  //     float f = fi;
  //     if (f < 0)
  //         f = 0.0;
  //     gl_FragColor = v + f;
  // }

  const std::string predefs_before =
      R"(OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %main "main" %BaseColor %fi %gl_FragColor
OpExecutionMode %main OriginUpperLeft
OpSource GLSL 140
OpName %main "main"
OpName %v "v"
OpName %BaseColor "BaseColor"
OpName %fi "fi"
OpName %r "r"
OpName %gl_FragColor "gl_FragColor"
%void = OpTypeVoid
%9 = OpTypeFunction %void
%float = OpTypeFloat 32
%v4float = OpTypeVector %float 4
%_ptr_Function_v4float = OpTypePointer Function %v4float
%_ptr_Input_v4float = OpTypePointer Input %v4float
%BaseColor = OpVariable %_ptr_Input_v4float Input
%_ptr_Input_float = OpTypePointer Input %float
%fi = OpVariable %_ptr_Input_float Input
%float_0 = OpConstant %float 0
%bool = OpTypeBool
%float_1 = OpConstant %float 1
%_ptr_Output_v4float = OpTypePointer Output %v4float
%gl_FragColor = OpVariable %_ptr_Output_v4float Output
)";

  const std::string predefs_after =
      R"(OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %main "main" %BaseColor %fi %gl_FragColor
OpExecutionMode %main OriginUpperLeft
OpSource GLSL 140
OpName %main "main"
OpName %BaseColor "BaseColor"
OpName %fi "fi"
OpName %r "r"
OpName %gl_FragColor "gl_FragColor"
%void = OpTypeVoid
%9 = OpTypeFunction %void
%float = OpTypeFloat 32
%v4float = OpTypeVector %float 4
%_ptr_Function_v4float = OpTypePointer Function %v4float
%_ptr_Input_v4float = OpTypePointer Input %v4float
%BaseColor = OpVariable %_ptr_Input_v4float Input
%_ptr_Input_float = OpTypePointer Input %float
%fi = OpVariable %_ptr_Input_float Input
%float_0 = OpConstant %float 0
%bool = OpTypeBool
%float_1 = OpConstant %float 1
%_ptr_Output_v4float = OpTypePointer Output %v4float
%gl_FragColor = OpVariable %_ptr_Output_v4float Output
)";

  const std::string before =
      R"(%main = OpFunction %void None %9
%19 = OpLabel
%v = OpVariable %_ptr_Function_v4float Function
%r = OpVariable %_ptr_Function_v4float Function
%20 = OpLoad %v4float %BaseColor
OpStore %v %20
%21 = OpLoad %float %fi
%22 = OpFOrdLessThan %bool %21 %float_0
OpSelectionMerge %23 None
OpBranchConditional %22 %24 %25
%24 = OpLabel
%26 = OpLoad %v4float %v
OpStore %r %26
OpBranch %23
%25 = OpLabel
%27 = OpLoad %v4float %v
%28 = OpCompositeConstruct %v4float %float_1 %float_1 %float_1 %float_1
%29 = OpFSub %v4float %28 %27
OpStore %r %29
OpBranch %23
%23 = OpLabel
%30 = OpLoad %v4float %r
OpStore %gl_FragColor %30
OpReturn
OpFunctionEnd
)";

  const std::string after =
      R"(%main = OpFunction %void None %9
%19 = OpLabel
%r = OpVariable %_ptr_Function_v4float Function
%20 = OpLoad %v4float %BaseColor
%21 = OpLoad %float %fi
%22 = OpFOrdLessThan %bool %21 %float_0
OpSelectionMerge %23 None
OpBranchConditional %22 %24 %25
%24 = OpLabel
OpStore %r %20
OpBranch %23
%25 = OpLabel
%28 = OpCompositeConstruct %v4float %float_1 %float_1 %float_1 %float_1
%29 = OpFSub %v4float %28 %20
OpStore %r %29
OpBranch %23
%23 = OpLabel
%30 = OpLoad %v4float %r
OpStore %gl_FragColor %30
OpReturn
OpFunctionEnd
)";

  SinglePassRunAndCheck<opt::LocalSingleStoreElimPass>(
      predefs_before + before, predefs_after + after, true, true);
}

TEST_F(LocalSingleStoreElimTest, NoStoreElimWithInterveningAccessChainLoad) {
  // Last load of v is eliminated, but access chain load and store of v isn't
  //
  // #version 140
  //
  // in vec4 BaseColor;
  //
  // void main()
  // {
  //     vec4 v = BaseColor;
  //     float f = v[3];
  //     gl_FragColor = v * f;
  // }

  const std::string predefs_before =
      R"(OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %main "main" %BaseColor %gl_FragColor
OpExecutionMode %main OriginUpperLeft
OpSource GLSL 140
OpName %main "main"
OpName %v "v"
OpName %BaseColor "BaseColor"
OpName %f "f"
OpName %gl_FragColor "gl_FragColor"
%void = OpTypeVoid
%8 = OpTypeFunction %void
%float = OpTypeFloat 32
%v4float = OpTypeVector %float 4
%_ptr_Function_v4float = OpTypePointer Function %v4float
%_ptr_Input_v4float = OpTypePointer Input %v4float
%BaseColor = OpVariable %_ptr_Input_v4float Input
%_ptr_Function_float = OpTypePointer Function %float
%uint = OpTypeInt 32 0
%uint_3 = OpConstant %uint 3
%_ptr_Output_v4float = OpTypePointer Output %v4float
%gl_FragColor = OpVariable %_ptr_Output_v4float Output
)";

  const std::string predefs_after =
      R"(OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %main "main" %BaseColor %gl_FragColor
OpExecutionMode %main OriginUpperLeft
OpSource GLSL 140
OpName %main "main"
OpName %v "v"
OpName %BaseColor "BaseColor"
OpName %gl_FragColor "gl_FragColor"
%void = OpTypeVoid
%8 = OpTypeFunction %void
%float = OpTypeFloat 32
%v4float = OpTypeVector %float 4
%_ptr_Function_v4float = OpTypePointer Function %v4float
%_ptr_Input_v4float = OpTypePointer Input %v4float
%BaseColor = OpVariable %_ptr_Input_v4float Input
%_ptr_Function_float = OpTypePointer Function %float
%uint = OpTypeInt 32 0
%uint_3 = OpConstant %uint 3
%_ptr_Output_v4float = OpTypePointer Output %v4float
%gl_FragColor = OpVariable %_ptr_Output_v4float Output
)";

  const std::string before =
      R"(%main = OpFunction %void None %8
%17 = OpLabel
%v = OpVariable %_ptr_Function_v4float Function
%f = OpVariable %_ptr_Function_float Function
%18 = OpLoad %v4float %BaseColor
OpStore %v %18
%19 = OpAccessChain %_ptr_Function_float %v %uint_3
%20 = OpLoad %float %19
OpStore %f %20
%21 = OpLoad %v4float %v
%22 = OpLoad %float %f
%23 = OpVectorTimesScalar %v4float %21 %22
OpStore %gl_FragColor %23
OpReturn
OpFunctionEnd
)";

  const std::string after =
      R"(%main = OpFunction %void None %8
%17 = OpLabel
%v = OpVariable %_ptr_Function_v4float Function
%18 = OpLoad %v4float %BaseColor
OpStore %v %18
%19 = OpAccessChain %_ptr_Function_float %v %uint_3
%20 = OpLoad %float %19
%23 = OpVectorTimesScalar %v4float %18 %20
OpStore %gl_FragColor %23
OpReturn
OpFunctionEnd
)";

  SinglePassRunAndCheck<opt::LocalSingleStoreElimPass>(
      predefs_before + before, predefs_after + after, true, true);
}

TEST_F(LocalSingleStoreElimTest, NoReplaceOfDominatingPartialStore) {
  // Note: SPIR-V hand edited to initialize v to vec4(0.0)
  //
  // #version 140
  //
  // in vec4 BaseColor;
  //
  // void main()
  // {
  //     vec4 v;
  //     float v[1] = 1.0;
  //     gl_FragColor = v;
  // }

  const std::string assembly =
      R"(OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %main "main" %gl_FragColor %BaseColor
OpExecutionMode %main OriginUpperLeft
OpSource GLSL 140
OpName %main "main"
OpName %v "v"
OpName %gl_FragColor "gl_FragColor"
OpName %BaseColor "BaseColor"
%void = OpTypeVoid
%7 = OpTypeFunction %void
%float = OpTypeFloat 32
%v4float = OpTypeVector %float 4
%_ptr_Function_v4float = OpTypePointer Function %v4float
%float_0 = OpConstant %float 0
%12 = OpConstantComposite %v4float %float_0 %float_0 %float_0 %float_0
%float_1 = OpConstant %float 1
%uint = OpTypeInt 32 0
%uint_1 = OpConstant %uint 1
%_ptr_Function_float = OpTypePointer Function %float
%_ptr_Output_v4float = OpTypePointer Output %v4float
%gl_FragColor = OpVariable %_ptr_Output_v4float Output
%_ptr_Input_v4float = OpTypePointer Input %v4float
%BaseColor = OpVariable %_ptr_Input_v4float Input
%main = OpFunction %void None %7
%19 = OpLabel
%v = OpVariable %_ptr_Function_v4float Function %12
%20 = OpAccessChain %_ptr_Function_float %v %uint_1
OpStore %20 %float_1
%21 = OpLoad %v4float %v
OpStore %gl_FragColor %21
OpReturn
OpFunctionEnd
)";

  SinglePassRunAndCheck<opt::LocalSingleStoreElimPass>(assembly, assembly, true,
                                                       true);
}

TEST_F(LocalSingleStoreElimTest, ElimIfCopyObjectInFunction) {
  // Note: hand edited to insert OpCopyObject
  //
  // #version 140
  //
  // in vec4 BaseColor;
  // in float fi;
  //
  // void main()
  // {
  //     vec4 v = BaseColor;
  //     float f = fi;
  //     if (f < 0)
  //         f = 0.0;
  //     gl_FragColor = v + f;
  // }

  const std::string predefs_before =
      R"(OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %main "main" %BaseColor %fi %gl_FragColor
OpExecutionMode %main OriginUpperLeft
OpSource GLSL 140
OpName %main "main"
OpName %v "v"
OpName %BaseColor "BaseColor"
OpName %f "f"
OpName %fi "fi"
OpName %gl_FragColor "gl_FragColor"
%void = OpTypeVoid
%9 = OpTypeFunction %void
%float = OpTypeFloat 32
%v4float = OpTypeVector %float 4
%_ptr_Function_v4float = OpTypePointer Function %v4float
%_ptr_Input_v4float = OpTypePointer Input %v4float
%BaseColor = OpVariable %_ptr_Input_v4float Input
%_ptr_Function_float = OpTypePointer Function %float
%_ptr_Input_float = OpTypePointer Input %float
%fi = OpVariable %_ptr_Input_float Input
%float_0 = OpConstant %float 0
%bool = OpTypeBool
%_ptr_Output_v4float = OpTypePointer Output %v4float
%gl_FragColor = OpVariable %_ptr_Output_v4float Output
)";

  const std::string predefs_after =
      R"(OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %main "main" %BaseColor %fi %gl_FragColor
OpExecutionMode %main OriginUpperLeft
OpSource GLSL 140
OpName %main "main"
OpName %BaseColor "BaseColor"
OpName %f "f"
OpName %fi "fi"
OpName %gl_FragColor "gl_FragColor"
%void = OpTypeVoid
%9 = OpTypeFunction %void
%float = OpTypeFloat 32
%v4float = OpTypeVector %float 4
%_ptr_Function_v4float = OpTypePointer Function %v4float
%_ptr_Input_v4float = OpTypePointer Input %v4float
%BaseColor = OpVariable %_ptr_Input_v4float Input
%_ptr_Function_float = OpTypePointer Function %float
%_ptr_Input_float = OpTypePointer Input %float
%fi = OpVariable %_ptr_Input_float Input
%float_0 = OpConstant %float 0
%bool = OpTypeBool
%_ptr_Output_v4float = OpTypePointer Output %v4float
%gl_FragColor = OpVariable %_ptr_Output_v4float Output
)";

  const std::string before =
      R"(%main = OpFunction %void None %9
%19 = OpLabel
%v = OpVariable %_ptr_Function_v4float Function
%f = OpVariable %_ptr_Function_float Function
%20 = OpLoad %v4float %BaseColor
OpStore %v %20
%21 = OpLoad %float %fi
OpStore %f %21
%22 = OpLoad %float %f
%23 = OpFOrdLessThan %bool %22 %float_0
OpSelectionMerge %24 None
OpBranchConditional %23 %25 %24
%25 = OpLabel
OpStore %f %float_0
OpBranch %24
%24 = OpLabel
%26 = OpCopyObject %_ptr_Function_v4float %v
%27 = OpLoad %v4float %26
%28 = OpLoad %float %f
%29 = OpCompositeConstruct %v4float %28 %28 %28 %28
%30 = OpFAdd %v4float %27 %29
OpStore %gl_FragColor %30
OpReturn
OpFunctionEnd
)";

  const std::string after =
      R"(%main = OpFunction %void None %9
%19 = OpLabel
%f = OpVariable %_ptr_Function_float Function
%20 = OpLoad %v4float %BaseColor
%21 = OpLoad %float %fi
OpStore %f %21
%22 = OpLoad %float %f
%23 = OpFOrdLessThan %bool %22 %float_0
OpSelectionMerge %24 None
OpBranchConditional %23 %25 %24
%25 = OpLabel
OpStore %f %float_0
OpBranch %24
%24 = OpLabel
%28 = OpLoad %float %f
%29 = OpCompositeConstruct %v4float %28 %28 %28 %28
%30 = OpFAdd %v4float %20 %29
OpStore %gl_FragColor %30
OpReturn
OpFunctionEnd
)";

  SinglePassRunAndCheck<opt::LocalSingleStoreElimPass>(
      predefs_before + before, predefs_after + after, true, true);
}

TEST_F(LocalSingleStoreElimTest, NoOptIfStoreNotDominating) {
  // Single store to f not optimized because it does not dominate
  // the load.
  //
  // #version 140
  //
  // in vec4 BaseColor;
  // in float fi;
  //
  // void main()
  // {
  //     float f;
  //     if (fi < 0)
  //         f = 0.5;
  //     if (fi < 0)
  //         gl_FragColor = BaseColor * f;
  //     else
  //         gl_FragColor = BaseColor;
  // }

  const std::string assembly =
      R"(OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %main "main" %fi %gl_FragColor %BaseColor
OpExecutionMode %main OriginUpperLeft
OpSource GLSL 140
OpName %main "main"
OpName %fi "fi"
OpName %f "f"
OpName %gl_FragColor "gl_FragColor"
OpName %BaseColor "BaseColor"
%void = OpTypeVoid
%8 = OpTypeFunction %void
%float = OpTypeFloat 32
%_ptr_Input_float = OpTypePointer Input %float
%fi = OpVariable %_ptr_Input_float Input
%float_0 = OpConstant %float 0
%bool = OpTypeBool
%_ptr_Function_float = OpTypePointer Function %float
%float_0_5 = OpConstant %float 0.5
%v4float = OpTypeVector %float 4
%_ptr_Output_v4float = OpTypePointer Output %v4float
%gl_FragColor = OpVariable %_ptr_Output_v4float Output
%_ptr_Input_v4float = OpTypePointer Input %v4float
%BaseColor = OpVariable %_ptr_Input_v4float Input
%main = OpFunction %void None %8
%18 = OpLabel
%f = OpVariable %_ptr_Function_float Function
%19 = OpLoad %float %fi
%20 = OpFOrdLessThan %bool %19 %float_0
OpSelectionMerge %21 None
OpBranchConditional %20 %22 %21
%22 = OpLabel
OpStore %f %float_0_5
OpBranch %21
%21 = OpLabel
%23 = OpLoad %float %fi
%24 = OpFOrdLessThan %bool %23 %float_0
OpSelectionMerge %25 None
OpBranchConditional %24 %26 %27
%26 = OpLabel
%28 = OpLoad %v4float %BaseColor
%29 = OpLoad %float %f
%30 = OpVectorTimesScalar %v4float %28 %29
OpStore %gl_FragColor %30
OpBranch %25
%27 = OpLabel
%31 = OpLoad %v4float %BaseColor
OpStore %gl_FragColor %31
OpBranch %25
%25 = OpLabel
OpReturn
OpFunctionEnd
)";

  SinglePassRunAndCheck<opt::LocalSingleStoreElimPass>(assembly, assembly, true,
                                                       true);
}

TEST_F(LocalSingleStoreElimTest, OptInitializedVariableLikeStore) {
  // Initialized variable f is optimized like it was a store.
  // Note: The SPIR-V was edited to turn the store to f to an
  // an initialization.
  //
  // #version 140
  //
  // void main()
  // {
  //     float f = 0.0;
  //     gl_FragColor = vec4(f);
  // }

  const std::string predefs_before =
      R"(OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %main "main" %gl_FragColor
OpExecutionMode %main OriginUpperLeft
OpSource GLSL 140
OpName %main "main"
OpName %f "f"
OpName %gl_FragColor "gl_FragColor"
OpDecorate %gl_FragColor Location 0
%void = OpTypeVoid
%6 = OpTypeFunction %void
%float = OpTypeFloat 32
%_ptr_Function_float = OpTypePointer Function %float
%float_0 = OpConstant %float 0
%v4float = OpTypeVector %float 4
%_ptr_Output_v4float = OpTypePointer Output %v4float
%gl_FragColor = OpVariable %_ptr_Output_v4float Output
)";

  const std::string predefs_after =
      R"(OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %main "main" %gl_FragColor
OpExecutionMode %main OriginUpperLeft
OpSource GLSL 140
OpName %main "main"
OpName %gl_FragColor "gl_FragColor"
OpDecorate %gl_FragColor Location 0
%void = OpTypeVoid
%6 = OpTypeFunction %void
%float = OpTypeFloat 32
%_ptr_Function_float = OpTypePointer Function %float
%float_0 = OpConstant %float 0
%v4float = OpTypeVector %float 4
%_ptr_Output_v4float = OpTypePointer Output %v4float
%gl_FragColor = OpVariable %_ptr_Output_v4float Output
)";

  const std::string before =
      R"(%main = OpFunction %void None %6
%12 = OpLabel
%f = OpVariable %_ptr_Function_float Function %float_0
%13 = OpLoad %float %f
%14 = OpCompositeConstruct %v4float %13 %13 %13 %13
OpStore %gl_FragColor %14
OpReturn
OpFunctionEnd
)";

  const std::string after =
      R"(%main = OpFunction %void None %6
%12 = OpLabel
%14 = OpCompositeConstruct %v4float %float_0 %float_0 %float_0 %float_0
OpStore %gl_FragColor %14
OpReturn
OpFunctionEnd
)";

  SinglePassRunAndCheck<opt::LocalSingleStoreElimPass>(
      predefs_before + before, predefs_after + after, true, true);
}

TEST_F(LocalSingleStoreElimTest, OptStoreOfNonCombinator) {
  // Unused local store is removed but non-combinator operand remains

  const std::string predefs_before =
      R"(OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %main "main" %gl_GlobalInvocationID
OpExecutionMode %main OriginUpperLeft
OpSource GLSL 140
OpDecorate %gl_GlobalInvocationID BuiltIn GlobalInvocationId
%void = OpTypeVoid
%6 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%v2uint = OpTypeVector %uint 2
%_ptr_Function_uint = OpTypePointer Function %uint
%_ptr_Uniform_uint = OpTypePointer Uniform %uint
%_ptr_Input_v2uint = OpTypePointer Input %v2uint
%164 = OpTypeImage %uint 2D 0 0 0 2 R32ui
%_ptr_UniformConstant_164 = OpTypePointer UniformConstant %164
%5661 = OpVariable %_ptr_UniformConstant_164 UniformConstant
%gl_GlobalInvocationID = OpVariable %_ptr_Input_v2uint Input
%4930 = OpVariable %_ptr_Uniform_uint Uniform
%_ptr_Image_uint = OpTypePointer Image %uint
%uint_0 = OpConstant %uint 0
%uint_1 = OpConstant %uint 1
)";

  const std::string predefs_after =
      R"(OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %2 "main" %gl_GlobalInvocationID
OpExecutionMode %2 OriginUpperLeft
OpSource GLSL 140
OpDecorate %gl_GlobalInvocationID BuiltIn GlobalInvocationId
%void = OpTypeVoid
%5 = OpTypeFunction %void
%uint = OpTypeInt 32 0
%v2uint = OpTypeVector %uint 2
%_ptr_Function_uint = OpTypePointer Function %uint
%_ptr_Uniform_uint = OpTypePointer Uniform %uint
%_ptr_Input_v2uint = OpTypePointer Input %v2uint
%11 = OpTypeImage %uint 2D 0 0 0 2 R32ui
%_ptr_UniformConstant_11 = OpTypePointer UniformConstant %11
%13 = OpVariable %_ptr_UniformConstant_11 UniformConstant
%gl_GlobalInvocationID = OpVariable %_ptr_Input_v2uint Input
%14 = OpVariable %_ptr_Uniform_uint Uniform
%_ptr_Image_uint = OpTypePointer Image %uint
%uint_0 = OpConstant %uint 0
%uint_1 = OpConstant %uint 1
)";

  const std::string before =
      R"(%main = OpFunction %void None %6
%12 = OpLabel
%5297 = OpVariable %_ptr_Function_uint Function
%4061 = OpLoad %164 %5661
%16843 = OpLoad %v2uint %gl_GlobalInvocationID
%24748 = OpLoad %uint %4930
%15928 = OpImageTexelPointer %_ptr_Image_uint %4061 %16843 %uint_0
%21946 = OpAtomicExchange %uint %15928 %uint_1 %uint_0 %24748
OpStore %5297 %21946
OpReturn
OpFunctionEnd
)";

  const std::string after =
      R"(%2 = OpFunction %void None %5
%18 = OpLabel
%20 = OpLoad %11 %13
%21 = OpLoad %v2uint %gl_GlobalInvocationID
%22 = OpLoad %uint %14
%23 = OpImageTexelPointer %_ptr_Image_uint %20 %21 %uint_0
%24 = OpAtomicExchange %uint %23 %uint_1 %uint_0 %22
OpReturn
OpFunctionEnd
)";

  SinglePassRunAndCheck<opt::LocalSingleStoreElimPass>(
      predefs_before + before, predefs_after + after, true, true);
}

TEST_F(LocalSingleStoreElimTest, PointerVariable) {
  // Test that checks if a pointer variable is removed.

  const std::string before =
      R"(OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %1 "main" %2
OpExecutionMode %1 OriginUpperLeft
OpMemberDecorate %_struct_3 0 Offset 0
OpDecorate %_runtimearr__struct_3 ArrayStride 16
OpMemberDecorate %_struct_5 0 Offset 0
OpDecorate %_struct_5 BufferBlock
OpMemberDecorate %_struct_6 0 Offset 0
OpDecorate %_struct_6 BufferBlock
OpDecorate %2 Location 0
OpDecorate %7 DescriptorSet 0
OpDecorate %7 Binding 0
%void = OpTypeVoid
%10 = OpTypeFunction %void
%int = OpTypeInt 32 1
%uint = OpTypeInt 32 0
%float = OpTypeFloat 32
%v4float = OpTypeVector %float 4
%_ptr_Output_v4float = OpTypePointer Output %v4float
%_ptr_Uniform_v4float = OpTypePointer Uniform %v4float
%_struct_3 = OpTypeStruct %v4float
%_runtimearr__struct_3 = OpTypeRuntimeArray %_struct_3
%_struct_5 = OpTypeStruct %_runtimearr__struct_3
%_ptr_Uniform__struct_5 = OpTypePointer Uniform %_struct_5
%_struct_6 = OpTypeStruct %int
%_ptr_Uniform__struct_6 = OpTypePointer Uniform %_struct_6
%_ptr_Function__ptr_Uniform__struct_5 = OpTypePointer Function %_ptr_Uniform__struct_5
%_ptr_Function__ptr_Uniform__struct_6 = OpTypePointer Function %_ptr_Uniform__struct_6
%int_0 = OpConstant %int 0
%uint_0 = OpConstant %uint 0
%2 = OpVariable %_ptr_Output_v4float Output
%7 = OpVariable %_ptr_Uniform__struct_5 Uniform
%1 = OpFunction %void None %10
%23 = OpLabel
%24 = OpVariable %_ptr_Function__ptr_Uniform__struct_5 Function
OpStore %24 %7
%26 = OpLoad %_ptr_Uniform__struct_5 %24
%27 = OpAccessChain %_ptr_Uniform_v4float %26 %int_0 %uint_0 %int_0
%28 = OpLoad %v4float %27
%29 = OpCopyObject %v4float %28
OpStore %2 %28
OpReturn
OpFunctionEnd
)";

  const std::string after =
      R"(OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %1 "main" %2
OpExecutionMode %1 OriginUpperLeft
OpMemberDecorate %_struct_3 0 Offset 0
OpDecorate %_runtimearr__struct_3 ArrayStride 16
OpMemberDecorate %_struct_5 0 Offset 0
OpDecorate %_struct_5 BufferBlock
OpMemberDecorate %_struct_6 0 Offset 0
OpDecorate %_struct_6 BufferBlock
OpDecorate %2 Location 0
OpDecorate %7 DescriptorSet 0
OpDecorate %7 Binding 0
%void = OpTypeVoid
%10 = OpTypeFunction %void
%int = OpTypeInt 32 1
%uint = OpTypeInt 32 0
%float = OpTypeFloat 32
%v4float = OpTypeVector %float 4
%_ptr_Output_v4float = OpTypePointer Output %v4float
%_ptr_Uniform_v4float = OpTypePointer Uniform %v4float
%_struct_3 = OpTypeStruct %v4float
%_runtimearr__struct_3 = OpTypeRuntimeArray %_struct_3
%_struct_5 = OpTypeStruct %_runtimearr__struct_3
%_ptr_Uniform__struct_5 = OpTypePointer Uniform %_struct_5
%_struct_6 = OpTypeStruct %int
%_ptr_Uniform__struct_6 = OpTypePointer Uniform %_struct_6
%_ptr_Function__ptr_Uniform__struct_5 = OpTypePointer Function %_ptr_Uniform__struct_5
%_ptr_Function__ptr_Uniform__struct_6 = OpTypePointer Function %_ptr_Uniform__struct_6
%int_0 = OpConstant %int 0
%uint_0 = OpConstant %uint 0
%2 = OpVariable %_ptr_Output_v4float Output
%7 = OpVariable %_ptr_Uniform__struct_5 Uniform
%1 = OpFunction %void None %10
%23 = OpLabel
%27 = OpAccessChain %_ptr_Uniform_v4float %7 %int_0 %uint_0 %int_0
%28 = OpLoad %v4float %27
%29 = OpCopyObject %v4float %28
OpStore %2 %28
OpReturn
OpFunctionEnd
)";

  SetAssembleOptions(SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS);
  SinglePassRunAndCheck<opt::LocalSingleStoreElimPass>(before, after, true,
                                                       true);
}

// TODO(greg-lunarg): Add tests to verify handling of these cases:
//
//    Other types
//    Others?

}  // anonymous namespace
