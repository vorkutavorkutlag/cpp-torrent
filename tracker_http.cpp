#include <curl/curl.h>

#include <iostream>

#include "tracker.h"

size_t write_cb(void* contents, size_t size, size_t nmemb, void* userp) {
  size_t total = size * nmemb;
  auto* buf = static_cast<std::vector<char>*>(userp);
  buf->insert(buf->end(), (char*)contents, (char*)contents + total);
  return total;
}


std::vector<char> _http_announce(const std::shared_ptr<TrackerParams>& params, CURL* curl) {
  std::vector<char> response;

  curl_easy_setopt(curl, CURLOPT_URL, params.get()->url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "VORKUTORRENT/0.1");
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");  // allow gzip

  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    // handle error
  }

  for (const auto& c : response) {
    std::cout << c;
  }

  return response;
}

void http_life(const std::shared_ptr<TrackerParams>& params) {
  CURL* curl = curl_easy_init();

  
}