// Copyright (c) 2015-2016 The Khronos Group Inc.
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

#include "source/val/validate.h"

#include <cassert>
#include <cstdio>

#include <algorithm>
#include <functional>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "source/binary.h"
#include "source/diagnostic.h"
#include "source/enum_string_mapping.h"
#include "source/extensions.h"
#include "source/instruction.h"
#include "source/opcode.h"
#include "source/operand.h"
#include "source/spirv_constant.h"
#include "source/spirv_endian.h"
#include "source/spirv_target_env.h"
#include "source/spirv_validator_options.h"
#include "source/val/construct.h"
#include "source/val/function.h"
#include "source/val/instruction.h"
#include "source/val/validation_state.h"
#include "spirv-tools/libspirv.h"

namespace spvtools {
namespace val {
namespace {

spv_result_t spvValidateIDs(const spv_instruction_t* pInsts,
                            const uint64_t count,
                            const ValidationState_t& state,
                            spv_position position) {
  position->index = SPV_INDEX_INSTRUCTION;
  if (auto error = spvValidateInstructionIDs(pInsts, count, state, position))
    return error;
  return SPV_SUCCESS;
}

// TODO(umar): Validate header
// TODO(umar): The binary parser validates the magic word, and the length of the
// header, but nothing else.
spv_result_t setHeader(void* user_data, spv_endianness_t, uint32_t,
                       uint32_t version, uint32_t generator, uint32_t id_bound,
                       uint32_t) {
  // Record the ID bound so that the validator can ensure no ID is out of bound.
  ValidationState_t& _ = *(reinterpret_cast<ValidationState_t*>(user_data));
  _.setIdBound(id_bound);
  _.setGenerator(generator);
  _.setVersion(version);

  return SPV_SUCCESS;
}

// Parses OpExtension instruction and registers extension.
void RegisterExtension(ValidationState_t& _,
                       const spv_parsed_instruction_t* inst) {
  const std::string extension_str = spvtools::GetExtensionString(inst);
  Extension extension;
  if (!GetExtensionFromString(extension_str.c_str(), &extension)) {
    // The error will be logged in the ProcessInstruction pass.
    return;
  }

  _.RegisterExtension(extension);
}

// Parses the beginning of the module searching for OpExtension instructions.
// Registers extensions if recognized. Returns SPV_REQUESTED_TERMINATION
// once an instruction which is not SpvOpCapability and SpvOpExtension is
// encountered. According to the SPIR-V spec extensions are declared after
// capabilities and before everything else.
spv_result_t ProcessExtensions(void* user_data,
                               const spv_parsed_instruction_t* inst) {
  const SpvOp opcode = static_cast<SpvOp>(inst->opcode);
  if (opcode == SpvOpCapability) return SPV_SUCCESS;

  if (opcode == SpvOpExtension) {
    ValidationState_t& _ = *(reinterpret_cast<ValidationState_t*>(user_data));
    RegisterExtension(_, inst);
    return SPV_SUCCESS;
  }

  // OpExtension block is finished, requesting termination.
  return SPV_REQUESTED_TERMINATION;
}

spv_result_t ProcessInstruction(void* user_data,
                                const spv_parsed_instruction_t* inst) {
  ValidationState_t& _ = *(reinterpret_cast<ValidationState_t*>(user_data));

  auto* instruction = _.AddOrderedInstruction(inst);
  _.RegisterDebugInstruction(instruction);

  return SPV_SUCCESS;
}

void printDot(const ValidationState_t& _, const BasicBlock& other) {
  std::string block_string;
  if (other.successors()->empty()) {
    block_string += "end ";
  } else {
    for (auto block : *other.successors()) {
      block_string += _.getIdOrName(block->id()) + " ";
    }
  }
  printf("%10s -> {%s\b}\n", _.getIdOrName(other.id()).c_str(),
         block_string.c_str());
}

void PrintBlocks(ValidationState_t& _, Function func) {
  assert(func.first_block());

  printf("%10s -> %s\n", _.getIdOrName(func.id()).c_str(),
         _.getIdOrName(func.first_block()->id()).c_str());
  for (const auto& block : func.ordered_blocks()) {
    printDot(_, *block);
  }
}

#ifdef __clang__
#define UNUSED(func) [[gnu::unused]] func
#elif defined(__GNUC__)
#define UNUSED(func)            \
  func __attribute__((unused)); \
  func
#elif defined(_MSC_VER)
#define UNUSED(func) func
#endif

UNUSED(void PrintDotGraph(ValidationState_t& _, Function func)) {
  if (func.first_block()) {
    std::string func_name(_.getIdOrName(func.id()));
    printf("digraph %s {\n", func_name.c_str());
    PrintBlocks(_, func);
    printf("}\n");
  }
}

spv_result_t ValidateForwardDecls(ValidationState_t& _) {
  if (_.unresolved_forward_id_count() == 0) return SPV_SUCCESS;

  std::stringstream ss;
  std::vector<uint32_t> ids = _.UnresolvedForwardIds();

  std::transform(
      std::begin(ids), std::end(ids),
      std::ostream_iterator<std::string>(ss, " "),
      bind(&ValidationState_t::getIdName, std::ref(_), std::placeholders::_1));

  auto id_str = ss.str();
  return _.diag(SPV_ERROR_INVALID_ID, nullptr)
         << "The following forward referenced IDs have not been defined:\n"
         << id_str.substr(0, id_str.size() - 1);
}

// Entry point validation. Based on 2.16.1 (Universal Validation Rules) of the
// SPIRV spec:
// * There is at least one OpEntryPoint instruction, unless the Linkage
//   capability is being used.
// * No function can be targeted by both an OpEntryPoint instruction and an
//   OpFunctionCall instruction.
spv_result_t ValidateEntryPoints(ValidationState_t& _) {
  _.ComputeFunctionToEntryPointMapping();

  if (_.entry_points().empty() && !_.HasCapability(SpvCapabilityLinkage)) {
    return _.diag(SPV_ERROR_INVALID_BINARY, nullptr)
           << "No OpEntryPoint instruction was found. This is only allowed if "
              "the Linkage capability is being used.";
  }
  for (const auto& entry_point : _.entry_points()) {
    if (_.IsFunctionCallTarget(entry_point)) {
      return _.diag(SPV_ERROR_INVALID_BINARY, _.FindDef(entry_point))
             << "A function (" << entry_point
             << ") may not be targeted by both an OpEntryPoint instruction and "
                "an OpFunctionCall instruction.";
    }
  }

  return SPV_SUCCESS;
}

spv_result_t ValidateBinaryUsingContextAndValidationState(
    const spv_context_t& context, const uint32_t* words, const size_t num_words,
    spv_diagnostic* pDiagnostic, ValidationState_t* vstate) {
  auto binary = std::unique_ptr<spv_const_binary_t>(
      new spv_const_binary_t{words, num_words});

  spv_endianness_t endian;
  spv_position_t position = {};
  if (spvBinaryEndianness(binary.get(), &endian)) {
    return DiagnosticStream(position, context.consumer, "",
                            SPV_ERROR_INVALID_BINARY)
           << "Invalid SPIR-V magic number.";
  }

  spv_header_t header;
  if (spvBinaryHeaderGet(binary.get(), endian, &header)) {
    return DiagnosticStream(position, context.consumer, "",
                            SPV_ERROR_INVALID_BINARY)
           << "Invalid SPIR-V header.";
  }

  if (header.version > spvVersionForTargetEnv(context.target_env)) {
    return DiagnosticStream(position, context.consumer, "",
                            SPV_ERROR_WRONG_VERSION)
           << "Invalid SPIR-V binary version "
           << SPV_SPIRV_VERSION_MAJOR_PART(header.version) << "."
           << SPV_SPIRV_VERSION_MINOR_PART(header.version)
           << " for target environment "
           << spvTargetEnvDescription(context.target_env) << ".";
  }

  // Look for OpExtension instructions and register extensions.
  spvBinaryParse(&context, vstate, words, num_words,
                 /* parsed_header = */ nullptr, ProcessExtensions,
                 /* diagnostic = */ nullptr);

  // Parse the module and perform inline validation checks. These checks do
  // not require the the knowledge of the whole module.
  if (auto error = spvBinaryParse(&context, vstate, words, num_words, setHeader,
                                  ProcessInstruction, pDiagnostic)) {
    return error;
  }

  for (auto& instruction : vstate->ordered_instructions()) {
    {
      // In order to do this work outside of Process Instruction we need to be
      // able to, briefly, de-const the instruction.
      Instruction* inst = const_cast<Instruction*>(&instruction);

      if (inst->opcode() == SpvOpEntryPoint) {
        const auto entry_point = inst->GetOperandAs<uint32_t>(1);
        const auto execution_model = inst->GetOperandAs<SpvExecutionModel>(0);
        const char* str = reinterpret_cast<const char*>(
            inst->words().data() + inst->operand(2).offset);

        ValidationState_t::EntryPointDescription desc;
        desc.name = str;

        std::vector<uint32_t> interfaces;
        for (size_t j = 3; j < inst->operands().size(); ++j)
          desc.interfaces.push_back(inst->word(inst->operand(j).offset));

        vstate->RegisterEntryPoint(entry_point, execution_model,
                                   std::move(desc));
      }
      if (inst->opcode() == SpvOpFunctionCall) {
        if (!vstate->in_function_body()) {
          return vstate->diag(SPV_ERROR_INVALID_LAYOUT, &instruction)
                 << "A FunctionCall must happen within a function body.";
        }

        vstate->AddFunctionCallTarget(inst->GetOperandAs<uint32_t>(2));
      }

      if (vstate->in_function_body()) {
        inst->set_function(&(vstate->current_function()));
        inst->set_block(vstate->current_function().current_block());

        if (vstate->in_block() && spvOpcodeIsBlockTerminator(inst->opcode())) {
          vstate->current_function().current_block()->set_terminator(inst);
        }
      }

      if (auto error = IdPass(*vstate, inst)) return error;
    }

    if (auto error = CapabilityPass(*vstate, &instruction)) return error;
    if (auto error = DataRulesPass(*vstate, &instruction)) return error;
    if (auto error = ModuleLayoutPass(*vstate, &instruction)) return error;
    if (auto error = CfgPass(*vstate, &instruction)) return error;
    if (auto error = InstructionPass(*vstate, &instruction)) return error;
    if (auto error = UpdateIdUse(*vstate, &instruction)) return error;
  }

  if (!vstate->has_memory_model_specified())
    return vstate->diag(SPV_ERROR_INVALID_LAYOUT, nullptr)
           << "Missing required OpMemoryModel instruction.";

  if (vstate->in_function_body())
    return vstate->diag(SPV_ERROR_INVALID_LAYOUT, nullptr)
           << "Missing OpFunctionEnd at end of module.";

  // Catch undefined forward references before performing further checks.
  if (auto error = ValidateForwardDecls(*vstate)) return error;

  // Validate individual opcodes.
  for (size_t i = 0; i < vstate->ordered_instructions().size(); ++i) {
    auto& instruction = vstate->ordered_instructions()[i];

    // Keep these passes in the order they appear in the SPIR-V specification
    // sections to maintain test consistency.
    // Miscellaneous
    if (auto error = DebugPass(*vstate, &instruction)) return error;
    if (auto error = AnnotationPass(*vstate, &instruction)) return error;
    if (auto error = ExtInstPass(*vstate, &instruction)) return error;
    if (auto error = ModeSettingPass(*vstate, &instruction)) return error;
    if (auto error = TypePass(*vstate, &instruction)) return error;
    // Constants
    if (auto error = ValidateMemoryInstructions(*vstate, &instruction))
      return error;
    // Functions
    if (auto error = ImagePass(*vstate, &instruction)) return error;
    if (auto error = ConversionPass(*vstate, &instruction)) return error;
    if (auto error = CompositesPass(*vstate, &instruction)) return error;
    if (auto error = ArithmeticsPass(*vstate, &instruction)) return error;
    if (auto error = BitwisePass(*vstate, &instruction)) return error;
    if (auto error = LogicalsPass(*vstate, &instruction)) return error;
    if (auto error = ControlFlowPass(*vstate, &instruction)) return error;
    if (auto error = DerivativesPass(*vstate, &instruction)) return error;
    if (auto error = AtomicsPass(*vstate, &instruction)) return error;
    if (auto error = PrimitivesPass(*vstate, &instruction)) return error;
    if (auto error = BarriersPass(*vstate, &instruction)) return error;
    // Group
    // Device-Side Enqueue
    // Pipe
    if (auto error = NonUniformPass(*vstate, &instruction)) return error;

    if (auto error = LiteralsPass(*vstate, &instruction)) return error;
    // Validate the preconditions involving adjacent instructions. e.g. SpvOpPhi
    // must only be preceeded by SpvOpLabel, SpvOpPhi, or SpvOpLine.
    if (auto error = ValidateAdjacency(*vstate, i)) return error;
  }

  if (auto error = ValidateEntryPoints(*vstate)) return error;
  // CFG checks are performed after the binary has been parsed
  // and the CFGPass has collected information about the control flow
  if (auto error = PerformCfgChecks(*vstate)) return error;
  if (auto error = CheckIdDefinitionDominateUse(*vstate)) return error;
  if (auto error = ValidateDecorations(*vstate)) return error;
  if (auto error = ValidateInterfaces(*vstate)) return error;
  // TODO(dsinclair): Restructure ValidateBuiltins so we can move into the
  // for() above as it loops over all ordered_instructions internally.
  if (auto error = ValidateBuiltIns(*vstate)) return error;

  // NOTE: Copy each instruction for easier processing
  std::vector<spv_instruction_t> instructions;
  // Expect average instruction length to be a bit over 2 words.
  instructions.reserve(binary->wordCount / 2);
  uint64_t index = SPV_INDEX_INSTRUCTION;
  while (index < binary->wordCount) {
    uint16_t wordCount;
    uint16_t opcode;
    spvOpcodeSplit(spvFixWord(binary->code[index], endian), &wordCount,
                   &opcode);
    spv_instruction_t inst;
    spvInstructionCopy(&binary->code[index], static_cast<SpvOp>(opcode),
                       wordCount, endian, &inst);
    instructions.emplace_back(std::move(inst));
    index += wordCount;
  }

  position.index = SPV_INDEX_INSTRUCTION;
  if (auto error = spvValidateIDs(instructions.data(), instructions.size(),
                                  *vstate, &position))
    return error;

  return SPV_SUCCESS;
}

}  // namespace

spv_result_t ValidateBinaryAndKeepValidationState(
    const spv_const_context context, spv_const_validator_options options,
    const uint32_t* words, const size_t num_words, spv_diagnostic* pDiagnostic,
    std::unique_ptr<ValidationState_t>* vstate) {
  spv_context_t hijack_context = *context;
  if (pDiagnostic) {
    *pDiagnostic = nullptr;
    UseDiagnosticAsMessageConsumer(&hijack_context, pDiagnostic);
  }

  vstate->reset(
      new ValidationState_t(&hijack_context, options, words, num_words));

  return ValidateBinaryUsingContextAndValidationState(
      hijack_context, words, num_words, pDiagnostic, vstate->get());
}

}  // namespace val
}  // namespace spvtools

spv_result_t spvValidate(const spv_const_context context,
                         const spv_const_binary binary,
                         spv_diagnostic* pDiagnostic) {
  return spvValidateBinary(context, binary->code, binary->wordCount,
                           pDiagnostic);
}

spv_result_t spvValidateBinary(const spv_const_context context,
                               const uint32_t* words, const size_t num_words,
                               spv_diagnostic* pDiagnostic) {
  spv_context_t hijack_context = *context;
  if (pDiagnostic) {
    *pDiagnostic = nullptr;
    spvtools::UseDiagnosticAsMessageConsumer(&hijack_context, pDiagnostic);
  }

  // This interface is used for default command line options.
  spv_validator_options default_options = spvValidatorOptionsCreate();

  // Create the ValidationState using the context and default options.
  spvtools::val::ValidationState_t vstate(&hijack_context, default_options,
                                          words, num_words);

  spv_result_t result =
      spvtools::val::ValidateBinaryUsingContextAndValidationState(
          hijack_context, words, num_words, pDiagnostic, &vstate);

  spvValidatorOptionsDestroy(default_options);
  return result;
}

spv_result_t spvValidateWithOptions(const spv_const_context context,
                                    spv_const_validator_options options,
                                    const spv_const_binary binary,
                                    spv_diagnostic* pDiagnostic) {
  spv_context_t hijack_context = *context;
  if (pDiagnostic) {
    *pDiagnostic = nullptr;
    spvtools::UseDiagnosticAsMessageConsumer(&hijack_context, pDiagnostic);
  }

  // Create the ValidationState using the context.
  spvtools::val::ValidationState_t vstate(&hijack_context, options,
                                          binary->code, binary->wordCount);

  return spvtools::val::ValidateBinaryUsingContextAndValidationState(
      hijack_context, binary->code, binary->wordCount, pDiagnostic, &vstate);
}
