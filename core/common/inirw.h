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

#pragma once

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <istream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ini {

class Section {
 public:
  const std::string& Get(const std::string& key) const {
    static const std::string empty;
    auto it = values_.find(key);
    return it != values_.end() ? it->second : empty;
  }

  bool Set(const std::string& key, std::string value) {
    auto it = values_.find(key);
    if (it == values_.end()) {
      keys_.push_back(key);
      values_.emplace(key, std::move(value));
      return true;
    }
    if (it->second == value)
      return false;
    it->second = std::move(value);
    return true;
  }

  bool Remove(const std::string& key) {
    if (!values_.erase(key))
      return false;

    auto it = std::find(keys_.begin(), keys_.end(), key);
    if (it != keys_.end())
      keys_.erase(it);
    return true;
  }

  bool empty() const { return keys_.empty(); }
  bool Has(const std::string& key) const {
    return values_.find(key) != values_.end();
  }

  const std::vector<std::string>& keys() const { return keys_; }

  bool operator==(const Section& other) const {
    return keys_ == other.keys_ && values_ == other.values_;
  }

 private:
  std::unordered_map<std::string, std::string> values_;
  std::vector<std::string> keys_;
};

class IniFile {
 public:
  explicit IniFile(const std::string& filename) { LoadFromFile(filename); }
  explicit IniFile(std::string_view content, bool) { Parse(content); }
  IniFile() = default;

  void LoadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
      return;
    }
    std::ostringstream oss;
    oss << file.rdbuf();
    Parse(oss.str());
  }

  void LoadFromString(std::string_view content) { Parse(content); }

  std::string Get(const std::string& section,
                  const std::string& key,
                  const std::string& defaultVal = {}) const {
    const Section* found = GetSection(section);
    if (!found || !found->Has(key))
      return defaultVal;
    return found->Get(key);
  }

  int32_t GetInt(const std::string& section,
                 const std::string& key,
                 int32_t defaultVal = 0) const {
    auto s = Get(section, key);
    if (s.empty())
      return defaultVal;
    int32_t value = defaultVal;
    const auto parsed = std::from_chars(s.data(), s.data() + s.size(), value);
    if (parsed.ec != std::errc() || parsed.ptr != s.data() + s.size())
      return defaultVal;
    return value;
  }

  float GetFloat(const std::string& section,
                 const std::string& key,
                 float defaultVal = 0.0f) const {
    auto s = Get(section, key);
    if (s.empty())
      return defaultVal;
    float value = defaultVal;
    const auto parsed = std::from_chars(s.data(), s.data() + s.size(), value);
    if (parsed.ec != std::errc() || parsed.ptr != s.data() + s.size())
      return defaultVal;
    return value;
  }

  bool GetBool(const std::string& section,
               const std::string& key,
               bool defaultVal = false) const {
    auto s = Get(section, key);
    if (s.empty())
      return defaultVal;
    auto ieq = [](const std::string& a, const std::string& b) {
      if (a.size() != b.size())
        return false;
      for (size_t i = 0; i < a.size(); ++i)
        if (std::tolower(static_cast<uint8_t>(a[i])) !=
            std::tolower(static_cast<uint8_t>(b[i])))
          return false;
      return true;
    };
    if (ieq(s, "true") || s == "1" || ieq(s, "yes"))
      return true;
    return false;
  }

  const Section* GetSection(const std::string& section) const {
    auto it = sections_.find(section);
    return (it != sections_.end()) ? &it->second : nullptr;
  }

  bool HasSection(const std::string& section) const {
    return sections_.find(section) != sections_.end();
  }

  bool HasKey(const std::string& section, const std::string& key) const {
    const Section* found = GetSection(section);
    return found && found->Has(key);
  }

  std::vector<std::string> GetKeys(const std::string& section) const {
    const Section* found = GetSection(section);
    return found ? found->keys() : std::vector<std::string>();
  }

  const std::vector<std::string>& GetSectionNames() const {
    return sectionOrder_;
  }

  void Set(const std::string& section,
           const std::string& key,
           const std::string& value) {
    auto& sec = sections_[section];

    if (std::find(sectionOrder_.begin(), sectionOrder_.end(), section) ==
        sectionOrder_.end())
      sectionOrder_.push_back(section);

    auto removed =
        std::find(removed_sections_.begin(), removed_sections_.end(), section);
    if (removed != removed_sections_.end())
      removed_sections_.erase(removed);

    if (sec.Set(key, value))
      Remember(&changed_, section, key);
  }

  void Set(const std::string& section, const std::string& key, int32_t value) {
    Set(section, key, std::to_string(value));
  }

  void Set(const std::string& section, const std::string& key, float value) {
    Set(section, key, std::to_string(value));
  }

  void Set(const std::string& section, const std::string& key, bool value) {
    Set(section, key, value ? std::string("true") : std::string("false"));
  }

  void RemoveKey(const std::string& section, const std::string& key) {
    auto sit = sections_.find(section);
    if (sit == sections_.end() || !sit->second.Remove(key))
      return;
    Forget(&changed_, section, key);
    Remember(&removed_keys_, section, key);
  }

  void RemoveSection(const std::string& section) {
    auto sit = sections_.find(section);
    if (sit == sections_.end())
      return;

    sections_.erase(sit);
    auto it = std::find(sectionOrder_.begin(), sectionOrder_.end(), section);
    if (it != sectionOrder_.end())
      sectionOrder_.erase(it);

    auto match = [&section](const Change& change) {
      return change.section == section;
    };
    changed_.erase(std::remove_if(changed_.begin(), changed_.end(), match),
                   changed_.end());
    removed_keys_.erase(
        std::remove_if(removed_keys_.begin(), removed_keys_.end(), match),
        removed_keys_.end());

    if (std::find(removed_sections_.begin(), removed_sections_.end(),
                  section) == removed_sections_.end())
      removed_sections_.push_back(section);
  }

  bool HasChanges() const {
    return !changed_.empty() || !removed_keys_.empty() ||
           !removed_sections_.empty();
  }

  std::string ToString() const {
    std::ostringstream oss;

    for (const auto& secname : sectionOrder_) {
      auto it = sections_.find(secname);
      if (it == sections_.end() || it->second.empty())
        continue;

      oss << '[' << secname << "]\n";
      for (const auto& key : it->second.keys())
        oss << key << '=' << it->second.Get(key) << '\n';
      oss << '\n';
    }
    return oss.str();
  }

  std::string RenderPatched() const {
    if (source_.empty())
      return ToString();

    const std::string terminator = DominantTerminator();
    std::string output;
    size_t cursor = 0;
    if (source_.size() >= 3 && static_cast<uint8_t>(source_[0]) == 0xEF &&
        static_cast<uint8_t>(source_[1]) == 0xBB &&
        static_cast<uint8_t>(source_[2]) == 0xBF) {
      output.append(source_, 0, 3);
      cursor = 3;
    }

    std::vector<Change> handled;
    std::vector<std::string> seen;
    std::string current;
    std::string trailing;
    bool dropping = false;

    while (cursor < source_.size()) {
      const size_t newline = source_.find('\n', cursor);
      const size_t stop =
          newline == std::string::npos ? source_.size() : newline + 1;
      const std::string line = source_.substr(cursor, stop - cursor);
      cursor = stop;

      std::string_view text(line);
      if (!text.empty() && text.back() == '\n')
        text.remove_suffix(1);
      if (!text.empty() && text.back() == '\r')
        text.remove_suffix(1);
      const std::string trimmed = Trim(text);

      if (!trimmed.empty() && trimmed.front() == '[') {
        FlushSection(current, handled, terminator, &output);
        output += trailing;
        trailing.clear();

        const size_t close = trimmed.find(']');
        current = (close != std::string::npos && close > 1)
                      ? Trim(trimmed.substr(1, close - 1))
                      : std::string();
        dropping = sections_.find(current) == sections_.end();
        if (dropping)
          continue;
        output += line;
        if (std::find(seen.begin(), seen.end(), current) == seen.end())
          seen.push_back(current);
        continue;
      }

      if (dropping)
        continue;

      if (trimmed.empty()) {
        trailing += line;
        continue;
      }

      if (trimmed.front() == ';' || trimmed.front() == '#') {
        output += trailing;
        trailing.clear();
        output += line;
        continue;
      }

      output += trailing;
      trailing.clear();

      const size_t equals = trimmed.find('=');
      if (equals == std::string_view::npos) {
        output += line;
        continue;
      }

      const std::string key = Trim(trimmed.substr(0, equals));
      if (key.empty()) {
        output += line;
        continue;
      }

      if (Contains(removed_keys_, current, key))
        continue;

      if (!Contains(changed_, current, key)) {
        output += line;
        continue;
      }

      output += key;
      output += '=';
      output += ValueOf(current, key);
      output += TerminatorOf(line);
      Remember(&handled, current, key);
    }

    FlushSection(current, handled, terminator, &output);
    output += trailing;

    for (const auto& name : sectionOrder_) {
      if (std::find(seen.begin(), seen.end(), name) != seen.end())
        continue;

      std::string block;
      for (const auto& change : changed_) {
        if (change.section != name || Contains(handled, name, change.key))
          continue;
        block += change.key;
        block += '=';
        block += ValueOf(name, change.key);
        block += terminator;
        Remember(&handled, name, change.key);
      }
      if (block.empty())
        continue;

      EnsureLineEnd(&output, terminator);
      output += '[';
      output += name;
      output += ']';
      output += terminator;
      output += block;
    }

    return output;
  }

  void Save(const std::string& filename) const {
    std::ofstream file(filename, std::ios::out | std::ios::trunc);
    if (!file.is_open())
      return;
    file << ToString();
  }

 private:
  struct Change {
    std::string section;
    std::string key;
  };

  static std::string Trim(std::string_view s) {
    constexpr const char* ws = " \t\r\n";
    auto start = s.find_first_not_of(ws);
    if (start == std::string_view::npos)
      return {};
    auto end = s.find_last_not_of(ws);
    return std::string(s.substr(start, end - start + 1));
  }

  static bool Contains(const std::vector<Change>& list,
                       const std::string& section,
                       const std::string& key) {
    return std::any_of(list.begin(), list.end(), [&](const Change& change) {
      return change.section == section && change.key == key;
    });
  }

  static void Remember(std::vector<Change>* list,
                       const std::string& section,
                       const std::string& key) {
    if (!Contains(*list, section, key))
      list->push_back(Change{section, key});
  }

  static void Forget(std::vector<Change>* list,
                     const std::string& section,
                     const std::string& key) {
    list->erase(std::remove_if(list->begin(), list->end(),
                               [&](const Change& change) {
                                 return change.section == section &&
                                        change.key == key;
                               }),
                list->end());
  }

  static std::string TerminatorOf(const std::string& line) {
    if (line.size() >= 2 && line[line.size() - 2] == '\r' &&
        line.back() == '\n')
      return "\r\n";
    if (!line.empty() && line.back() == '\n')
      return "\n";
    return {};
  }

  static void EnsureLineEnd(std::string* output,
                            const std::string& terminator) {
    if (!output->empty() && output->back() != '\n')
      *output += terminator;
  }

  std::string DominantTerminator() const {
    size_t crlf = 0;
    size_t lf = 0;
    for (size_t i = 0; i < source_.size(); ++i) {
      if (source_[i] != '\n')
        continue;
      if (i > 0 && source_[i - 1] == '\r')
        ++crlf;
      else
        ++lf;
    }
    return crlf > lf ? "\r\n" : "\n";
  }

  const std::string& ValueOf(const std::string& section,
                             const std::string& key) const {
    static const std::string empty;
    const Section* found = GetSection(section);
    return found ? found->Get(key) : empty;
  }

  void FlushSection(const std::string& section,
                    std::vector<Change>& handled,
                    const std::string& terminator,
                    std::string* output) const {
    if (section.empty())
      return;

    for (const auto& change : changed_) {
      if (change.section != section || Contains(handled, section, change.key))
        continue;
      if (!HasKey(section, change.key))
        continue;

      EnsureLineEnd(output, terminator);
      *output += change.key;
      *output += '=';
      *output += ValueOf(section, change.key);
      *output += terminator;
      Remember(&handled, section, change.key);
    }
  }

  void Parse(std::string_view content) {
    source_ = std::string(content);
    changed_.clear();
    removed_keys_.clear();
    removed_sections_.clear();
    sections_.clear();
    sectionOrder_.clear();

    if (content.size() >= 3 && static_cast<uint8_t>(content[0]) == 0xEF &&
        static_cast<uint8_t>(content[1]) == 0xBB &&
        static_cast<uint8_t>(content[2]) == 0xBF) {
      content.remove_prefix(3);
    }

    std::string currentSection;
    std::string contentStr(content);
    std::istringstream stream(contentStr);
    std::string line;

    while (std::getline(stream, line)) {
      std::string sv = Trim(line);

      if (sv.empty() || sv.front() == ';' || sv.front() == '#')
        continue;

      if (sv.front() == '[') {
        auto end = sv.find(']');
        if (end != std::string_view::npos && end > 1) {
          currentSection = Trim(sv.substr(1, end - 1));
          if (sections_.find(currentSection) == sections_.end()) {
            sectionOrder_.push_back(currentSection);
          }
        }
        continue;
      }

      auto eq = sv.find('=');
      if (eq != std::string_view::npos) {
        auto key = Trim(sv.substr(0, eq));
        auto val = Trim(sv.substr(eq + 1));
        if (!key.empty()) {
          sections_[currentSection].Set(key, val);
        }
      }
    }
  }

  std::string source_;
  std::unordered_map<std::string, Section> sections_;
  std::vector<std::string> sectionOrder_;
  std::vector<Change> changed_;
  std::vector<Change> removed_keys_;
  std::vector<std::string> removed_sections_;
};

}
