// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// https://opensource.org/licenses/BSD-3-Clause
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "pass_level1.h"

#include "../utils.h"

namespace pnnx {

class LeakyReLU : public FuseModulePass {
 public:
  const char* match_type_str() const {
    return "__torch__.torch.nn.modules.activation.LeakyReLU";
  }

  const char* type_str() const {
    return "nn.LeakyReLU";
  }

  void write(Operator* op, const std::shared_ptr<torch::jit::Graph>& graph) const {
    const torch::jit::Node* leaky_relu = find_node_by_kind(graph, "aten::leaky_relu");
    const torch::jit::Node* leaky_relu_ = find_node_by_kind(graph, "aten::leaky_relu_");

    if (leaky_relu_) {
      leaky_relu = leaky_relu_;
    }

    op->params["negative_slope"] = leaky_relu->namedInput("negative_slope");
  }
};

REGISTER_GLOBAL_PNNX_FUSE_MODULE_PASS(LeakyReLU)

} // namespace pnnx