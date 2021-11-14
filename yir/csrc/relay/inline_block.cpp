#include "inline_block.h"
#include "../jit.h"

#include <set>

#include <torch/csrc/jit/passes/quantization/helper.h>

namespace pnnx {

static void inlineCallTo(torch::jit::Node* to_replace, torch::jit::Function* callee) {
  torch::jit::WithInsertPoint guard(to_replace);

  std::unordered_map<torch::jit::Value*, torch::jit::Value*> value_map;
  std::vector<torch::jit::Value*> new_outputs = torch::jit::insertGraph(
      *to_replace->owningGraph(), *(callee->graph()), to_replace->inputs(), value_map);

  const auto& old_outputs = to_replace->outputs();
  for (size_t i = 0; i < old_outputs.size(); ++i) {
    new_outputs[i]->copyMetadata(old_outputs[i]);

    old_outputs[i]->replaceAllUsesWith(new_outputs[i]);
  }

  to_replace->destroy();
}

static void inlineCalls(
    torch::jit::Block* block,
    const std::vector<std::string>& module_operators,
    std::set<std::string>& inlined_modules) {
  for (auto it = block->nodes().begin(), end = block->nodes().end(); it != end;) {
    torch::jit::Node* n = *it++;
    if (n->kind() == c10::prim::CallFunction) {
      auto function_constant = n->input(0)->node();
      auto fun_type = function_constant->output()->type()->expect<torch::jit::FunctionType>();
      if (!fun_type->function()->isGraphFunction())
        continue;

      inlineCalls(fun_type->function()->graph()->block(), module_operators, inlined_modules);

      n->removeInput(0);

      fprintf(stderr, "inline funtion %s\n", fun_type->function()->name().c_str());

      pnnx::inlineCallTo(n, fun_type->function());
    } else if (n->kind() == c10::prim::CallMethod) {
      auto class_type = n->input(0)->type()->cast<torch::jit::ClassType>();
      if (!class_type)
        continue;

      const std::string& function_name = n->s(torch::jit::attr::name);
      torch::jit::Function& function = class_type->getMethod(function_name);
      if (!function.isGraphFunction())
        continue;

      std::string class_type_str = torch::jit::removeTorchMangle(class_type->str());

      bool skip_inline = false;
      for (const auto& ow : get_global_pnnx_fuse_module_passes()) {
        if (class_type_str == ow->match_type_str()) {
          skip_inline = true;
          break;
        }
      }

      if (skip_inline)
        continue;

      std::string class_type_str_no_torch_prefix = class_type_str.substr(10);

      if (std::find(
              module_operators.begin(), module_operators.end(), class_type_str_no_torch_prefix) !=
          module_operators.end()) {
        continue;
      }

      inlineCalls(function.graph()->block(), module_operators, inlined_modules);

      inlined_modules.insert(class_type_str_no_torch_prefix);

      pnnx::inlineCallTo(n, &function);
    } else {
      for (auto b : n->blocks()) {
        inlineCalls(b, module_operators, inlined_modules);
      }
    }
  }
}

void inline_block(
    std::shared_ptr<torch::jit::Graph>& graph,
    const std::vector<std::string>& module_operators) {
  std::set<std::string> inlined_modules;

  inlineCalls(graph->block(), module_operators, inlined_modules);

  for (const auto& x : inlined_modules) {
    if (x == "torch.nn.modules.container.Sequential")
      continue;

    fprintf(stderr, "inline module = %s\n", x.c_str());
  }
}

} // namespace pnnx
