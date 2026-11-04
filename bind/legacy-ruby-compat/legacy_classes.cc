// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Admenri Adev <admenri0504@gmail.com>.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "bind/legacy-ruby-compat/legacy_classes.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/common/logger.h"
#include "ruby.h"
#include "ruby/encoding.h"
extern "C" {
#include "prism/prism.h"
}

namespace binding {
namespace {

struct RecoveryPlan {
  bool integer_alias = false;
  std::string name;
  std::string parent;
  std::vector<std::string> constants;
  std::string continuation;
};

std::string_view Text(const pm_node_t* node) {
  return {reinterpret_cast<const char*>(node->location.start),
          static_cast<size_t>(node->location.end - node->location.start)};
}

bool ConfigurationConstants(const pm_class_node_t* node,
                            std::vector<std::string>& constants) {
  if (!node->body || !PM_NODE_TYPE_P(node->body, PM_STATEMENTS_NODE))
    return false;
  const auto& body =
      reinterpret_cast<const pm_statements_node_t*>(node->body)->body;
  for (size_t i = 0; i < body.size; ++i) {
    if (!PM_NODE_TYPE_P(body.nodes[i], PM_CONSTANT_WRITE_NODE))
      return false;
    const auto* assignment =
        reinterpret_cast<const pm_constant_write_node_t*>(body.nodes[i]);
    switch (PM_NODE_TYPE(assignment->value)) {
      case PM_STRING_NODE:
      case PM_INTEGER_NODE:
      case PM_FLOAT_NODE:
      case PM_SYMBOL_NODE:
      case PM_TRUE_NODE:
      case PM_FALSE_NODE:
      case PM_NIL_NODE:
        break;
      default:
        return false;
    }
    constants.emplace_back(
        reinterpret_cast<const char*>(assignment->name_loc.start),
        assignment->name_loc.end - assignment->name_loc.start);
  }
  std::sort(constants.begin(), constants.end());
  constants.erase(std::unique(constants.begin(), constants.end()),
                  constants.end());
  return !constants.empty();
}

std::optional<RecoveryPlan> FindPlan(std::string_view source,
                                    std::string_view message,
                                    size_t line,
                                    bool configuration_classes) {
  constexpr std::string_view prefix = "superclass mismatch for class ";
  if (message.substr(0, prefix.size()) != prefix || !line ||
      source.find("shareable_constant_value") != std::string_view::npos)
    return std::nullopt;
  const auto name = message.substr(prefix.size());
  pm_parser_t parser;
  pm_parser_init(&parser, reinterpret_cast<const uint8_t*>(source.data()),
                 source.size(), nullptr);
  pm_node_t* root = pm_parse(&parser);
  std::optional<RecoveryPlan> result;
  if (root && !parser.error_list.head &&
      PM_NODE_TYPE_P(root, PM_PROGRAM_NODE)) {
    const auto* statements =
        reinterpret_cast<const pm_program_node_t*>(root)->statements;
    if (statements && statements->body.size &&
        std::none_of(statements->body.nodes,
                     statements->body.nodes + statements->body.size,
                     [](const pm_node_t* node) {
                       return PM_NODE_TYPE_P(node, PM_PRE_EXECUTION_NODE);
                     })) {
      // BEGIN executes at compile time, even after the failing class. Re-eval
      // would repeat it; leave those scripts to the normal error path.
      const auto& body = statements->body;
      const pm_class_node_t* first = nullptr;
      bool ambiguous = false;
      for (size_t i = 0; i < body.size; ++i) {
        if (!PM_NODE_TYPE_P(body.nodes[i], PM_CLASS_NODE))
          continue;
        const auto* node =
            reinterpret_cast<const pm_class_node_t*>(body.nodes[i]);
        if (!PM_NODE_TYPE_P(node->constant_path, PM_CONSTANT_READ_NODE) ||
            Text(node->constant_path) != name)
          continue;
        const size_t offset = node->base.location.start -
                              reinterpret_cast<const uint8_t*>(source.data());
        const size_t node_line =
            1 + std::count(source.begin(), source.begin() + offset, '\n');
        if (node_line < line) {
          if (first)
            ambiguous = true;
          first = node;
          continue;
        }
        if (node_line != line || !node->superclass ||
            !PM_NODE_TYPE_P(node->superclass, PM_CONSTANT_READ_NODE))
          break;
        // A standalone, single-line header identifies the failed statement;
        // never resume in the middle of an expression or a nested scope.
        size_t line_start = source.rfind('\n', offset);
        line_start = line_start == std::string_view::npos ? 0 : line_start + 1;
        const auto indentation = source.substr(line_start, offset - line_start);
        const auto header = std::string_view(
            reinterpret_cast<const char*>(node->base.location.start),
            node->superclass->location.end - node->base.location.start);
        if (indentation.find_first_not_of(" \t") != std::string_view::npos ||
            header.find_first_of("\r\n") != std::string_view::npos)
          break;
        RecoveryPlan plan;
        plan.integer_alias = (name == "Fixnum" || name == "Bignum") &&
                             Text(node->superclass) == "Integer";
        if (!plan.integer_alias &&
            (!configuration_classes || !first || ambiguous ||
             first->superclass ||
             !ConfigurationConstants(first, plan.constants)))
          break;
        plan.name = name;
        plan.parent = Text(node->superclass);
        plan.continuation = source;
        // Keep original line numbers, including CRLF input. No statement before
        // the failing class header is executed again.
        size_t preserved = source.substr(0, 3) == "\xEF\xBB\xBF" ? 3 : 0;
        while (preserved < offset) {
          size_t end = source.find('\n', preserved);
          end = end == std::string_view::npos ? source.size() : end + 1;
          const auto text = source.substr(preserved, end - preserved);
          const size_t nonspace = text.find_first_not_of(" \t\r\n");
          if (nonspace != std::string_view::npos && text[nonspace] != '#')
            break;
          preserved = end;
        }
        // Retain leading encoding/frozen-string magic comments as well.
        for (size_t j = preserved; j < offset; ++j) {
          if (plan.continuation[j] != '\n' && plan.continuation[j] != '\r')
            plan.continuation[j] = ' ';
        }
        if (plan.integer_alias) {
          // Integer aliases already denote the class being reopened. Remove
          // only the superclass clause; retain the class body and line layout.
          const size_t start = node->inheritance_operator_loc.start -
                               reinterpret_cast<const uint8_t*>(source.data());
          const size_t end = node->superclass->location.end -
                             reinterpret_cast<const uint8_t*>(source.data());
          std::fill(plan.continuation.begin() + start,
                    plan.continuation.begin() + end, ' ');
        }
        result = std::move(plan);
        break;
      }
    }
  }
  if (root)
    pm_node_destroy(&parser, root);
  pm_parser_free(&parser);
  return result;
}

VALUE Plan(VALUE,
           VALUE source,
           VALUE message,
           VALUE line,
           VALUE configuration_classes) {
  Check_Type(source, T_STRING);
  Check_Type(message, T_STRING);
  const size_t line_number = NUM2SIZET(line);
  int state = 0;
  VALUE result = Qnil;
  {
    const auto plan = FindPlan(
        {RSTRING_PTR(source), static_cast<size_t>(RSTRING_LEN(source))},
        {RSTRING_PTR(message), static_cast<size_t>(RSTRING_LEN(message))},
        line_number, RTEST(configuration_classes));
    if (!plan)
      return Qnil;
    struct Context {
      const RecoveryPlan* plan;
      VALUE source;
    } context{&*plan, source};
    // Finish all C++ parsing before Ruby allocation, and release owned strings
    // even if a Ruby allocation/encoding operation raises.
    result = rb_protect(
        [](VALUE value) -> VALUE {
          const auto* context = reinterpret_cast<const Context*>(value);
          const auto& plan = *context->plan;
          VALUE names = rb_ary_new();
          for (const auto& name : plan.constants)
            rb_ary_push(names, rb_str_new(name.data(), name.size()));
          VALUE tail =
              rb_str_new(plan.continuation.data(), plan.continuation.size());
          rb_enc_copy(tail, context->source);
          VALUE result = rb_ary_new();
          rb_ary_push(result, rb_str_new(plan.name.data(), plan.name.size()));
          rb_ary_push(result,
                      rb_str_new(plan.parent.data(), plan.parent.size()));
          rb_ary_push(result, names);
          rb_ary_push(result, tail);
          rb_ary_push(result, plan.integer_alias ? Qtrue : Qfalse);
          RB_GC_GUARD(names);
          RB_GC_GUARD(tail);
          return result;
        },
        reinterpret_cast<VALUE>(&context), &state);
  }
  if (state)
    rb_jump_tag(state);
  return result;
}

VALUE Applied(VALUE,
              VALUE name,
              VALUE filename,
              VALUE line,
              VALUE integer_alias) {
  LOGGER_INFO("[Ruby] Legacy {} fallback: {} ({}:{})",
              RTEST(integer_alias) ? "integer-alias" : "configuration-class",
              StringValueCStr(name), StringValueCStr(filename), NUM2LONG(line));
  return Qnil;
}

// TracePoint observes only exception raises. Its binding retains the failed
// eval's local variables without rewriting/recompiling successful scripts. Source analysis and recovery
// start only after an uncaught, matching TypeError reaches this evaluator.
constexpr const char* kEvaluator = R"RUBY(
module URGELegacyRubyCompat
  def self.recover(source, filename, error, observed, repaired)
    return unless error.instance_of?(TypeError) && observed &&
                  observed[0].equal?(error)
    location = error.backtrace_locations&.first
    return unless location && location.path == filename &&
                  location.lineno == observed[2]
    plan = self.plan(source, error.message, observed[2], @configuration_classes)
    return unless plan
    name, parent_name, names, tail, integer_alias = plan
    if integer_alias
      key = [name, observed[2]]
      return if repaired[key] || Object.autoload?(name) || Object.autoload?('Integer')
      return unless Object.const_defined?(name, false) &&
                    Object.const_defined?('Integer', false) &&
                    Object.const_get(name, false).equal?(@integer_class) &&
                    Object.const_get('Integer', false).equal?(@integer_class) &&
                    !@integer_class.frozen?
      repaired[key] = true
      applied(name, filename, observed[2], true)
      return [tail, observed[1]]
    end
    return if repaired[name] || Object.frozen? ||
              Object.autoload?(name) || Object.autoload?(parent_name)
    return unless Object.const_defined?(name, false) &&
                  Object.const_defined?(parent_name, false)
    holder = Object.const_get(name, false)
    parent = Object.const_get(parent_name, false)
    return unless holder.instance_of?(Class) && parent.instance_of?(Class) &&
                  holder.name == name && holder.superclass.equal?(Object) &&
                  !parent.equal?(Object) && !parent.equal?(holder) &&
                  !holder.frozen? && holder.subclasses.empty? &&
                  parent.method(:inherited).owner.equal?(Class)
    return unless holder.ancestors == [holder] + Object.ancestors &&
                  holder.singleton_class.ancestors ==
                    [holder.singleton_class] + Object.singleton_class.ancestors &&
                  holder.instance_methods(false).empty? &&
                  holder.private_instance_methods(false).empty? &&
                  holder.protected_instance_methods(false).empty? &&
                  holder.singleton_class.instance_methods(false).empty? &&
                  holder.singleton_class.private_instance_methods(false).empty? &&
                  holder.singleton_class.instance_variables.empty? &&
                  holder.instance_variables.empty? &&
                  holder.class_variables(false).empty? &&
                  holder.constants(false).map(&:to_s).sort == names
    return if ObjectSpace.each_object(holder).any?
    values = names.map do |key|
      return if holder.autoload?(key)
      value = holder.const_get(key, false)
      return unless [String, Integer, Float, Symbol, TrueClass, FalseClass,
                     NilClass].any? { |type| value.instance_of?(type) }
      [key, value]
    end
    replacement = Class.new(parent)
    values.each { |key, value| replacement.const_set(key, value) }
    # Reject reentrant hooks which changed the holder while creating the class.
    return unless Object.const_get(name, false).equal?(holder) && !Object.frozen?
    Object.send(:remove_const, name)
    Object.const_set(name, replacement)
    repaired[name] = true
    applied(name, filename, observed[2], false)
    [tail, observed[1]]
  end

  def self.evaluate(source, filename)
    __urge_original = source
    __urge_next_source = source
    __urge_filename = filename
    # Keep game constants at Object after moving this evaluator into a module.
    # Each script gets its own local scope; recovery reuses the failed Binding.
    __urge_context = TOPLEVEL_BINDING.dup
    __urge_observed = nil
    __urge_repaired = {}
    __urge_trace = TracePoint.new(:raise) do |event|
      __urge_event_error = event.raised_exception
      if __urge_event_error.instance_of?(TypeError) && event.path == __urge_filename
        __urge_observed = [__urge_event_error, event.binding, event.lineno]
      end
    end
    begin
      while true
        __urge_observed = nil
        __urge_trace.enable(target_thread: Thread.current)
        begin
          return eval(__urge_next_source, __urge_context, __urge_filename)
        rescue TypeError => __urge_error
          __urge_trace.disable
          raise unless __urge_error.instance_of?(TypeError) && __urge_observed &&
                       __urge_observed[0].equal?(__urge_error) &&
                       __urge_error.message.start_with?('superclass mismatch for class ')
          __urge_recovery = URGELegacyRubyCompat.recover(
            __urge_original, __urge_filename, __urge_error, __urge_observed, __urge_repaired)
          raise unless __urge_recovery
          __urge_next_source, __urge_context = __urge_recovery
        end
      end
    ensure
      __urge_trace.disable
    end
  end
end
)RUBY";
}  // namespace

void InitLegacyClasses(bool configuration_classes) {
  VALUE compatibility = rb_define_module("URGELegacyRubyCompat");
  rb_ivar_set(compatibility, rb_intern("@integer_class"), rb_cInteger);
  rb_ivar_set(compatibility, rb_intern("@configuration_classes"),
              configuration_classes ? Qtrue : Qfalse);
  rb_define_singleton_method(compatibility, "plan", RUBY_METHOD_FUNC(Plan), 4);
  rb_define_singleton_method(compatibility, "applied",
                             RUBY_METHOD_FUNC(Applied), 4);
  rb_eval_string(kEvaluator);
}
}  // namespace binding
