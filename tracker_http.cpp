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

std::vector<char> _http_announce(const std::shared_ptr<TrackerParams>& params,
                                 CURL* curl) {
    std::vector<char> response;

    std::string announce_url = build_http_announce_url(*params, curl, 6969);

    std::cout << announce_url.c_str() << '\n';

    curl_easy_setopt(curl, CURLOPT_URL, announce_url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "VORKUTORRENT/0.1");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");  // allow gzip

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        response.clear();  // erasing error messages is bad but i don't care
        return response;
    }

    return response;
}

void http_life(const std::shared_ptr<TrackerParams>& params) {
    // curl_global_init(0L);
    CURL* curl = curl_easy_init();

    uint32_t interval = 180;  // reasonable default interval
    for (;; std::this_thread::sleep_for(std::chrono::seconds(interval))) {
        const std::vector<char> resp = _http_announce(params, curl);
        if (resp.empty()) continue;

        BencodeDict resp_dict = _decode_http_annresp(resp);
        if (resp_dict.empty()) continue;

        if (!std::holds_alternative<std::string>(resp_dict[PEERS_STR]))
            continue;

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