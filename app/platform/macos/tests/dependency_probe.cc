// Exercises the same static libcurl archive that the engine links.
#include <brotli/encode.h>
#include <curl/curl.h>
#include <zstd.h>

#include <fstream>
#include <clocale>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

static size_t append(char* data, size_t size, size_t count, void* out) {
  static_cast<std::string*>(out)->append(data, size * count);
  return size * count;
}

int main(int argc, char** argv) {
  // Like curl's CLI and CRuby, initialize the locale before converting IDNs.
  std::setlocale(LC_CTYPE, "en_US.UTF-8");
  if (argc < 2 || curl_global_init(CURL_GLOBAL_DEFAULT)) return 1;
  auto* version = curl_version_info(CURLVERSION_NOW);
  const int required = CURL_VERSION_SSL | CURL_VERSION_LIBZ | CURL_VERSION_BROTLI |
                       CURL_VERSION_ZSTD | CURL_VERSION_HTTP2 | CURL_VERSION_IDN;
  if ((version->features & required) != required) return 2;
  std::string mode = argv[1];
  if (mode == "features") {
    CURLU* url = curl_url();
    char* host = nullptr;
    if (!url || curl_url_set(url, CURLUPART_URL, "https://例子.测试/", 0) ||
        curl_url_get(url, CURLUPART_HOST, &host, CURLU_PUNYCODE)) return 10;
    bool idn_ok = std::string(host) == "xn--fsqu00a.xn--0zwm56d";
    curl_free(host);
    curl_url_cleanup(url);
    if (!idn_ok) return 11;
    std::cout << "PASS: " << curl_version() << std::endl;
  } else if (mode == "encode" && argc == 3) {
    std::string path = argv[2];
    std::ifstream file(path, std::ios::binary);
    if (!file) return 3;
    std::string input((std::istreambuf_iterator<char>(file)), {});
    size_t length = BrotliEncoderMaxCompressedSize(input.size());
    std::vector<uint8_t> output(length);
    if (!BrotliEncoderCompress(5, BROTLI_DEFAULT_WINDOW, BROTLI_MODE_GENERIC,
                              input.size(), reinterpret_cast<const uint8_t*>(input.data()),
                              &length, output.data())) return 4;
    std::ofstream(path + ".br", std::ios::binary).write(reinterpret_cast<char*>(output.data()), length);
    output.resize(ZSTD_compressBound(input.size()));
    length = ZSTD_compress(output.data(), output.size(), input.data(), input.size(), 3);
    if (ZSTD_isError(length)) return 5;
    std::ofstream(path + ".zstd", std::ios::binary).write(reinterpret_cast<char*>(output.data()), length);
  } else if (mode == "request" && argc == 5) {
    CURL* curl = curl_easy_init();
    if (!curl) return 6;
    std::string body;
    curl_easy_setopt(curl, CURLOPT_URL, argv[2]);
    if (std::string(argv[3]) != "-") curl_easy_setopt(curl, CURLOPT_CAINFO, argv[3]);
    curl_easy_setopt(curl, CURLOPT_NOPROXY, "*");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, append);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    auto result = curl_easy_perform(curl);
    long http = 0;
    curl_easy_getinfo(curl, CURLINFO_HTTP_VERSION, &http);
    curl_easy_cleanup(curl);
    if (result != CURLE_OK) {
      std::cerr << curl_easy_strerror(result) << std::endl;
      return 7;
    }
    if (std::stoi(argv[4]) != 0 && http != std::stoi(argv[4])) return 8;
    std::cout << body;
  } else {
    return 9;
  }
  curl_global_cleanup();
  return 0;
}
