#include <curl/curl.h>

#include <iomanip>
#include <iostream>
#include <sstream>

#include "tracker.h"

const std::string hexify_peer_id(
    const std::array<uint8_t, PEERID_SIZE> peerid) {
    std::stringstream ss;
    for (int i = 0; i < PEERID_SIZE; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(peerid[i]);
    }

    return ss.str().substr(0, PEERID_SIZE);
}

uint16_t extract_port(const std::string& url) {
    auto scheme_pos = url.find("://");
    if (scheme_pos == std::string::npos) return 0;

    size_t host_start = scheme_pos + 3;
    size_t host_end = url.find('/', host_start);
    if (host_end == std::string::npos) host_end = url.length();

    std::string authority = url.substr(host_start, host_end - host_start);

    auto colon_pos = authority.find(':');
    if (colon_pos == std::string::npos) return 0;

    try {
        int port = std::stoi(authority.substr(colon_pos + 1));
        if (port < 0 || port > 65535) return 0;
        return static_cast<uint16_t>(port);
    } catch (...) {
        return 0;
    }
}

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

    std::string result(encoded);
    curl_free(encoded);
    return result;
}

std::string build_http_announce_url(const TrackerParams& params, CURL* curl,
                                    uint16_t listen_port, bool compact = true,
                                    const std::string& event = "started") {
    std::ostringstream oss;

    oss << params.url;

    oss << "?info_hash=" << params.hex_ih;

    oss << "&peer_id=" << hexify_peer_id(params.peer_id);

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

BencodeDict _decode_http_annresp(const std::vector<char>& resp) {
    std::string data(resp.begin(), resp.end());
    std::istringstream iss(data);
    std::string __ri;
    bool __ihc;
    BencodeValue decoded = bdecode(iss, __ri, __ihc);

    if (!std::holds_alternative<BencodeDict>(decoded)) {
        return BencodeDict{};
    }

    return std::get<BencodeDict>(decoded);
}

struct HttpResult {
    std::vector<char> response;
    CURLcode code;
};

HttpResult _http_announce(const std::shared_ptr<TrackerParams>& params,
                          CURL* curl, uint16_t port) {
    std::vector<char> response;

    std::string announce_url = build_http_announce_url(*params, curl, port);

    curl_easy_setopt(curl, CURLOPT_URL, announce_url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "VORKUTORRENT/0.1");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

    CURLcode res = curl_easy_perform(curl);

    return {std::move(response), res};
}

void http_life(const std::shared_ptr<TrackerParams>& params) {
    // curl_global_init(0L);
    CURL* curl = curl_easy_init();

    uint16_t port = extract_port(params.get()->url);
    if (!port) port = 6881;

    uint32_t interval = 60;  // reasonable default interval
    for (;; std::this_thread::sleep_for(std::chrono::seconds(interval))) {
        HttpResult res = _http_announce(params, curl, port);

        if (res.code == CURLE_OPERATION_TIMEDOUT) {
            if (port < 6889) {
                ++port;
                continue;
            } else {
                port = 6881;  // reset range
                std::this_thread::sleep_for(std::chrono::seconds(interval));
                continue;
            }
        }

        const std::vector<char> resp = res.response;

        if (resp.empty()) {
            std::this_thread::sleep_for(std::chrono::seconds(interval));
            continue;
        }

        BencodeDict resp_dict = _decode_http_annresp(resp);
        if (resp_dict.empty()) {
            std::this_thread::sleep_for(std::chrono::seconds(interval));
            continue;
        }

        if (!std::holds_alternative<std::string>(resp_dict[PEERS_STR])) {
            std::this_thread::sleep_for(std::chrono::seconds(interval));
            continue;
        }

        if (std::holds_alternative<uint32_t>(resp_dict[INTERVAL_STR]))
            interval = std::get<uint32_t>(resp_dict[INTERVAL_STR]);

        const std::string& peers = std::get<std::string>(resp_dict[PEERS_STR]);

        for (size_t i = 0; i + 6 <= peers.size(); i += 6) {
            uint32_t ip;
            uint16_t port;

            memcpy(&ip, &peers[i], 4);
            memcpy(&port, &peers[i + 4], 2);

            const std::lock_guard<std::mutex> lock(params.get()->ps_mut);
            params.get()->peer_set.emplace(ip, port);
        }
    }

    curl_easy_cleanup(curl);
    // curl_global_cleanup();
}