// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Admenri Adev <admenri0504@gmail.com>.

#pragma once

#include <algorithm>
#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace urge {

struct RenderGraphAccess3D {
  std::string name;
  std::set<std::string> reads;
  std::set<std::string> writes;
  bool enabled = true;
};

// Reads use the last preceding writer, or the first following writer when a
// producer was added after its consumer. Preserve WAW and WAR as well as RAW:
// a consumer of one version must execute before the next version is written.
inline std::vector<size_t> OrderRenderGraph3D(
    const std::vector<RenderGraphAccess3D>& passes,
    std::string& error) {
  error.clear();
  std::map<std::string, std::vector<size_t>> writers;
  std::vector<std::set<size_t>> edges(passes.size());
  std::vector<size_t> indegree(passes.size(), 0);
  size_t enabled_count = 0;
  const auto add_edge = [&](size_t before, size_t after) {
    if (before != after && edges[before].insert(after).second)
      ++indegree[after];
  };
  for (size_t i = 0; i < passes.size(); ++i) {
    if (!passes[i].enabled)
      continue;
    ++enabled_count;
    for (const std::string& write : passes[i].writes)
      writers[write].push_back(i);
  }
  for (const auto& [resource, list] : writers)
    for (size_t i = 1; i < list.size(); ++i)
      add_edge(list[i - 1], list[i]);

  for (size_t i = 0; i < passes.size(); ++i) {
    if (!passes[i].enabled)
      continue;
    for (const std::string& read : passes[i].reads) {
      const auto found = writers.find(read);
      size_t producer = passes.size();
      if (found != writers.end()) {
        for (size_t writer : found->second) {
          if (writer < i)
            producer = writer;
          else
            break;
        }
        if (producer == passes.size()) {
          for (size_t writer : found->second) {
            if (writer != i) {
              producer = writer;
              break;
            }
          }
        }
      }
      if (producer == passes.size()) {
        error = "pass '" + passes[i].name + "' reads '" + read +
                "' without an enabled producer";
        return {};
      }
      add_edge(producer, i);
      for (size_t writer : found->second) {
        if (writer > producer && writer != i) {
          add_edge(i, writer);
          break;
        }
      }
    }
  }

  std::vector<size_t> order;
  std::vector<bool> emitted(passes.size(), false);
  while (order.size() < enabled_count) {
    size_t next = passes.size();
    for (size_t i = 0; i < passes.size(); ++i) {
      if (passes[i].enabled && !emitted[i] && indegree[i] == 0) {
        next = i;
        break;
      }
    }
    if (next == passes.size()) {
      error = "dependency cycle involving";
      for (size_t i = 0; i < passes.size(); ++i)
        if (passes[i].enabled && !emitted[i])
          error += " '" + passes[i].name + "'";
      return {};
    }
    emitted[next] = true;
    order.push_back(next);
    for (size_t after : edges[next])
      --indegree[after];
  }
  return order;
}

}  // namespace urge
