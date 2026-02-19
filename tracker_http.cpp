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
        // handle error
    }

    // std::cout << "Response size: " << response.size() << std::endl;
    // std::cout.write(response.data(), response.size());
    // d8:completei0e10:downloadedi0e10:incompletei1e8:intervali1819e12:min
    // intervali909e5:peers6:Y#g9e

    std::string data(response.begin(), response.end());
    std::istringstream iss(data);
    std::string __ri;
    bool __ihc;
    BencodeValue decoded = bdecode(iss, __ri, __ihc);

    if (!std::holds_alternative<BencodeDict>(decoded)) {
        // handle error
    }

    // bencode_dump(decoded);

    BencodeDict decoded_dict = std::get<BencodeDict>(decoded);
    const BencodeValue peers_be =
        decoded_dict[PEERS];  // ugly.. should maybe change dict keys
                              // to c_str
    const std::string& peers = std::get<std::string>(peers_be);

    std::cout << "Here it comes!" << std::endl;
    for (const auto& i : peers) {
        std::cout << +i << ' ';
    }
    std::cout << "Done!" << std::endl;

    return response;
}

void http_life(const std::shared_ptr<TrackerParams>& params) {
    // curl_global_init(0L);
    CURL* curl = curl_easy_init();
    _http_announce(params, curl);
    curl_easy_cleanup(curl);
    // curl_global_cleanup();
}