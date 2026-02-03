#include "tracker.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>

#include <ctime>
#include <map>
#include <set>
#include <variant>

#include "bencode.h"
#include "constants.h"
#include "sys/socket.h"

std::set<std::string> extract_trackers(BencodeDict torrent_dict) {
  std::set<std::string> trackers;

  trackers.insert(std::get<std::string>(torrent_dict[TF_String[ANNOUNCE]]));

  for (const auto& i :
       std::get<BencodeList>(torrent_dict[TF_String[ANNOUNCE_LIST]])) {
    for (const auto& j : std::get<BencodeList>(i)) {
      trackers.insert(std::get<std::string>(j));
    }
  }

  return trackers;
}

int determine_tracker_type(std::string url) {
  if (url.rfind("udp", 0) == 0) return 1;
  if (url.rfind("http", 0) == 0 || url.rfind("https", 0)) return 2;
  return -1;
}

int32_t generate_rand_transaction_id() {
  srand(time(NULL) * getpid());
  return rand();
}

void _send_conreq_udp(int sockfd, UDP_ConnectRequest conreq,
                      sockaddr_in* servaddr) {
  std::string payload;
  payload += conreq.protocol_id + conreq.action + conreq.transaction_id;
  const char* payload_cstr = payload.c_str();

  sendto(sockfd, payload_cstr, strlen(payload_cstr), MSG_CONFIRM,
         (const struct sockaddr*)servaddr, sizeof(servaddr));
}

void connect_udp(const char* ip_address, const int port) {
  int sockfd;
  struct sockaddr_in servaddr;

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  memset(&servaddr, 0, sizeof(servaddr));

  servaddr.sin_family = AF_INET;  // IPv4
  servaddr.sin_port = htons(port);
  servaddr.sin_addr.s_addr = inet_addr(ip_address);

  socklen_t sock_len = sizeof(servaddr);

  struct UDP_ConnectRequest conreq = {PROTOCOL_ID,
                                          static_cast<int32_t>(UDP::ACTION),
                                          generate_rand_transaction_id()};
  _send_conreq_udp(sockfd, conreq, &servaddr);
}