// Copyright (c) 2021 Mostafa Ashraf
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

#ifndef SOURCE_FUZZ_FUZZER_PASS_CHANGING_MEMORY_SEMANTICS_H_
#define SOURCE_FUZZ_FUZZER_PASS_CHANGING_MEMORY_SEMANTICS_H_

#include <algorithm>

#include "source/fuzz/fuzzer_pass.h"

namespace spvtools {
namespace fuzz {

// This fuzzer pass changes the memory semantics value for specific atomic
// operations with a new one.
class FuzzerPassChangingMemorySemantics : public FuzzerPass {
 public:
  FuzzerPassChangingMemorySemantics(
      opt::IRContext* ir_context, TransformationContext* transformation_context,
      FuzzerContext* fuzzer_context,
      protobufs::TransformationSequence* transformations,
      bool ignore_inapplicable_transformations);

  static std::vector<SpvMemorySemanticsMask>
  GetSuitableNewMemorySemanticsLowerBitValues(
      opt::IRContext* ir_context,
      spvtools::opt::Instruction* needed_instruction,
      SpvMemorySemanticsMask lower_bits_old_memory_semantics_value,
      uint32_t memory_semantics_operand_position, SpvMemoryModel memory_model);

  void Apply() override;
};

}  // namespace fuzz
}  // namespace spvtools

#endif  // SOURCE_FUZZ_FUZZER_PASS_CHANGING_MEMORY_SEMANTICS_H_
