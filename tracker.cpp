#include "tracker.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

#include <cstring>
#include <ctime>
#include <map>
#include <set>
#include <variant>

#include "bencode.h"
#include "constants.h"
#include "sys/socket.h"

struct HostPortPair {
  std::string host;
  std::string port;
};

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

void _send_conreq_udp(int sockfd, const UDP_ConnectRequest& conreq,
                      sockaddr_in* servaddr) {
  std::uint8_t buffer[16];

  std::uint64_t protocol_id = htonll(conreq.protocol_id);
  std::uint32_t action = htonl(conreq.action);
  std::uint32_t transaction_id = htonl(conreq.transaction_id);

  std::memcpy(buffer + 0, &protocol_id, 8);
  std::memcpy(buffer + 8, &action, 4);
  std::memcpy(buffer + 12, &transaction_id, 4);

  socklen_t len = sizeof(*servaddr);
  sendto(sockfd, buffer, sizeof(buffer), 0,
         reinterpret_cast<sockaddr*>(servaddr), len);
}

UDP_ConnectResponse _recv_conresp_udp(int sockfd, sockaddr_in* servaddr) {
  uint8_t buffer[static_cast<size_t>(UDP_BUFFER::CONNECT_RESPONSE)];
  std::uint32_t action;
  std::uint32_t transaction_id;
  std::uint64_t connection_id;

  socklen_t len = sizeof(*servaddr);
  ssize_t n = recvfrom(sockfd, buffer,
                       static_cast<size_t>(UDP_BUFFER::CONNECT_RESPONSE),
                       MSG_WAITALL, (struct sockaddr*)servaddr, &len);

  std::memcpy(&action, buffer + 0, 4);
  std::memcpy(&transaction_id, buffer + 4, 4);
  std::memcpy(&connection_id, buffer + 8, 8);

  action = ntohl(action);
  transaction_id = ntohl(transaction_id);
  connection_id = ntohll(connection_id);

  return (UDP_ConnectResponse){action, transaction_id, connection_id};
}

HostPortPair parse_udp(std::string url) {
  auto host_start = url.find("://") + 3;
  auto port_start = url.find(':', host_start);
  auto path_start = url.find('/', port_start);

  std::string host = url.substr(host_start, port_start - host_start);
  std::string port = url.substr(port_start + 1, path_start - port_start - 1);

  return {host, port};
}

int connect_udp(std::string url) {
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

  sockaddr_in servaddr{};
  servaddr.sin_family = AF_INET;

  addrinfo hints{}, *res;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;

  HostPortPair hp = parse_udp(url);

  int err = getaddrinfo(hp.host.c_str(), hp.port.c_str(), &hints, &res);
  if (err != 0) return -1;

  servaddr = *reinterpret_cast<sockaddr_in*>(res->ai_addr);

  struct UDP_ConnectRequest conreq = {PROTOCOL_ID,
                                      static_cast<int32_t>(UDP_ACTION::CONNECT),
                                      generate_rand_transaction_id()};

  _send_conreq_udp(sockfd, conreq, &servaddr);
}