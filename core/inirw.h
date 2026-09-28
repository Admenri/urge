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
#include <fstream>
#include <istream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ini {

class IniFile {
 public:
  using Section = std::unordered_map<std::string, std::string>;

  explicit IniFile(const std::string& filename) { LoadFromFile(filename); }
  explicit IniFile(std::string_view content, bool /*tag*/) { Parse(content); }
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
    auto sit = sections_.find(section);
    if (sit == sections_.end())
      return defaultVal;
    auto kit = sit->second.find(key);
    return (kit != sit->second.end()) ? kit->second : defaultVal;
  }

  int32_t GetInt(const std::string& section,
                 const std::string& key,
                 int32_t defaultVal = 0) const {
    auto s = Get(section, key);
    if (s.empty())
      return defaultVal;
    try {
      return std::stoi(s);
    } catch (...) {
      return defaultVal;
    }
  }

  float GetFloat(const std::string& section,
                 const std::string& key,
                 float defaultVal = 0.0f) const {
    auto s = Get(section, key);
    if (s.empty())
      return defaultVal;
    try {
      return std::stof(s);
    } catch (...) {
      return defaultVal;
    }
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
    auto sit = sections_.find(section);
    if (sit == sections_.end())
      return false;
    return sit->second.find(key) != sit->second.end();
  }

  const std::vector<std::string>& GetSectionNames() const {
    return sectionOrder_;
  }

  void Set(const std::string& section,
           const std::string& key,
           const std::string& value) {
    auto& sec = sections_[section];
    if (sec.empty()) {
      sectionOrder_.push_back(section);
    }
    sec[key] = value;
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
    if (sit == sections_.end())
      return;
    sit->second.erase(key);
  }

  void RemoveSection(const std::string& section) {
    sections_.erase(section);
    auto it = std::find(sectionOrder_.begin(), sectionOrder_.end(), section);
    if (it != sectionOrder_.end())
      sectionOrder_.erase(it);
  }

  std::string ToString() const {
    std::ostringstream oss;

    for (const auto& secname : sectionOrder_) {
      auto it = sections_.find(secname);
      if (it == sections_.end() || it->second.empty())
        continue;

      oss << '[' << secname << "]\n";
      for (const auto& [key, val] : it->second) {
        oss << key << '=' << val << '\n';
      }
      oss << '\n';
    }
    return oss.str();
  }

  void Save(const std::string& filename) const {
    std::ofstream file(filename, std::ios::out | std::ios::trunc);
    if (!file.is_open())
      return;
    file << ToString();
  }

 private:
  static std::string Trim(std::string_view s) {
    constexpr const char* ws = " \t\r\n";
    auto start = s.find_first_not_of(ws);
    if (start == std::string_view::npos)
      return {};
    auto end = s.find_last_not_of(ws);
    return std::string(s.substr(start, end - start + 1));
  }

  void Parse(std::string_view content) {
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
          sections_[currentSection][key] = val;
        }
      }
    }
  }

  std::unordered_map<std::string, Section> sections_;
  std::vector<std::string> sectionOrder_;
};

}  // namespace ini
