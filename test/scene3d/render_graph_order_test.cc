// SPDX-License-Identifier: MIT
#include "core/scene3d/render_graph_order3d.h"

#include <cstdlib>
#include <iostream>

using urge::RenderGraphAccess3D;

void Expect(const std::vector<RenderGraphAccess3D>& passes,
            const std::vector<size_t>& expected) {
  std::string error;
  if (urge::OrderRenderGraph3D(passes, error) != expected || !error.empty()) {
    std::cerr << "Unexpected graph order: " << error << '\n';
    std::exit(1);
  }
}

void Reject(const std::vector<RenderGraphAccess3D>& passes,
            const std::string& expected) {
  std::string error;
  if (!urge::OrderRenderGraph3D(passes, error).empty() ||
      error.find(expected) == std::string::npos) {
    std::cerr << "Expected " << expected << ", got " << error << '\n';
    std::exit(1);
  }
}

int main() {
  Expect({}, {});
  Expect({{"a", {}, {"screen"}}, {"b", {}, {"screen"}}}, {0, 1});
  Expect({{"consumer", {"a"}, {"screen"}}, {"producer", {}, {"a"}}}, {1, 0});
  // WAR: the reader uses the first version, even when it waits on another
  // input.
  Expect({{"v1", {}, {"a"}},
          {"reader", {"a", "b"}, {"screen"}},
          {"v2", {}, {"a"}},
          {"other", {}, {"b"}}},
         {0, 3, 1, 2});
  // A forward consumer must also precede the second writer of that resource.
  Expect({{"reader", {"a"}, {"screen"}}, {"v1", {}, {"a"}}, {"v2", {}, {"a"}}},
         {1, 0, 2});
  // Explicit read/write uses the preceding version; never its own output.
  Expect({{"init", {}, {"a"}},
          {"update", {"a"}, {"a"}},
          {"reader", {"a"}, {"screen"}}},
         {0, 1, 2});
  Expect({{"off", {"missing"}, {"a"}, false}, {"valid", {}, {"screen"}}}, {1});
  Reject({{"reader", {"a"}, {"screen"}}, {"off", {}, {"a"}, false}},
         "enabled producer");
  Reject({{"a", {"b"}, {"a"}}, {"b", {"a"}, {"b"}}}, "cycle");
  Reject({{"reader", {"missing"}, {"screen"}}}, "missing");
  Reject({{"self", {"a"}, {"a"}}}, "enabled producer");
  std::cout << "11 render graph ordering checks passed\n";
}
