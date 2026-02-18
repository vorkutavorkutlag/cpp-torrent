#include <curl/curl.h>

#include <iostream>
#include <sstream>

#include "tracker.h"

size_t write_cb(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    auto* buf = static_cast<std::vector<char>*>(userp);
    buf->insert(buf->end(), (char*)contents, (char*)contents + total);
    return total;
}

static std::string url_encode_bytes(CURL* curl, const uint8_t* data,
                                    size_t len) {
    char* encoded = curl_easy_escape(curl, reinterpret_cast<const char*>(data),
                                     static_cast<int>(len));

    if (!encoded) {
        throw std::runtime_error("curl_easy_escape failed");
    }

    std::string result(encoded);
    curl_free(encoded);
    return result;
}

std::string build_http_announce_url(const TrackerParams& params, CURL* curl,
                                    uint16_t listen_port, bool compact = true,
                                    const std::string& event = "started") {
    std::ostringstream oss;

    oss << params.url;

    oss << "info_hash="
        << url_encode_bytes(curl, params.info_hash.data(),
                            params.info_hash.size());

    oss << "&peer_id="
        << url_encode_bytes(curl, params.peer_id.data(), params.peer_id.size());

    oss << "&port=" << listen_port;

    oss << "&uploaded=0";  // I AM A LEECH

    {
        std::lock_guard<std::mutex> lock(params.d_mut);
        oss << "&downloaded=" << params.downloaded;
    }

    oss << "&left=" << params.size;

    oss << "&compact=" << (compact ? 1 : 0);

    if (!event.empty()) {
        oss << "&event=" << event;
    }

    return oss.str();
}

std::vector<char> _http_announce(const std::shared_ptr<TrackerParams>& params,
                                 CURL* curl) {
    std::vector<char> response;

    std::string announce_url = build_http_announce_url(*params, curl, 6881);

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