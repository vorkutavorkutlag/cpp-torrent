#include "tracker.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

#include <cstring>
#include <ctime>
#include <iostream>
#include <map>
#include <set>
#include <variant>

#include "bencode.h"
#include "constants.h"
#include "sys/socket.h"

struct HostPortPair {
  const std::string host;
  const std::string port;
};

struct SocketConnectionUDP {
  uint64_t connection_id;
  const int sockfd;
  sockaddr_in servaddr;
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

uint32_t generate_rand_transaction_id() {
  srand(time(NULL) * getpid());
  return rand();
}

int _send_announce_udp(SocketConnectionUDP& conn,
                       const UDP_IPv4_AnnounceRequest& req) {
  uint8_t buffer[(size_t)(UDP_BUFFER::ANNOUNCE_REQUEST)];

  uint64_t connection_id = htonll(req.connection_id);
  uint32_t action = htonl(req.action);
  uint32_t transaction_id = htonl(req.transaction_id);

  uint64_t downloaded = htonll(req.downloaded);
  uint64_t left = htonll(req.left);
  uint64_t uploaded = htonll(req.uploaded);

  uint32_t event = htonl(req.event);
  uint32_t ip_address = htonl(req.ip_address);
  uint32_t key = htonl(req.key);
  uint32_t num_want = htonl(req.num_want);

  uint16_t port = htons(req.port);

  memcpy(buffer + 0, &connection_id, 8);
  memcpy(buffer + 8, &action, 4);
  memcpy(buffer + 12, &transaction_id, 4);

  memcpy(buffer + 16, req.info_hash, 20);
  memcpy(buffer + 36, req.peer_id, 20);

  memcpy(buffer + 56, &downloaded, 8);
  memcpy(buffer + 64, &left, 8);
  memcpy(buffer + 72, &uploaded, 8);

  memcpy(buffer + 80, &event, 4);
  memcpy(buffer + 84, &ip_address, 4);
  memcpy(buffer + 88, &key, 4);
  memcpy(buffer + 92, &num_want, 4);
  memcpy(buffer + 96, &port, 2);

  socklen_t len = sizeof(&conn.servaddr);
  return sendto(conn.sockfd, buffer, sizeof(buffer), 0,
                reinterpret_cast<sockaddr*>(&conn.servaddr), len);
}

int _send_conreq_udp(SocketConnectionUDP& conn,
                     const UDP_ConnectRequest& conreq) {
  uint8_t buffer[(size_t)(UDP_BUFFER::CONNECT_REQUEST)];

  uint64_t protocol_id = htonll(conreq.protocol_id);
  uint32_t action = htonl(conreq.action);
  uint32_t transaction_id = htonl(conreq.transaction_id);

  std::memcpy(buffer + 0, &protocol_id, 8);
  std::memcpy(buffer + 8, &action, 4);
  std::memcpy(buffer + 12, &transaction_id, 4);

  socklen_t len = sizeof(conn.servaddr);
  return sendto(conn.sockfd, buffer, sizeof(buffer), 0,
                reinterpret_cast<sockaddr*>(&conn.servaddr), len);
}

UDP_IPv4_AnnounceResponse _recv_annreq_udp(SocketConnectionUDP conn) {
  uint8_t head_buffer[(size_t)UDP_BUFFER::ANNOUNCE_RESPONSE_HEAD];

  uint32_t action;
  uint32_t transaction_id;
  uint32_t interval;
  uint32_t leechers;
  uint32_t seeders;

  socklen_t socklen = sizeof(conn.servaddr);
  ssize_t head_recv_len = recvfrom(
      conn.sockfd, head_buffer, (size_t)(UDP_BUFFER::ANNOUNCE_RESPONSE_HEAD),
      MSG_WAITALL, reinterpret_cast<sockaddr*>(&conn.servaddr), &socklen);

  if (head_recv_len < (size_t)(UDP_BUFFER::ANNOUNCE_RESPONSE_HEAD))
    return (UDP_IPv4_AnnounceResponse){};

  action = ntohl(action);
  transaction_id = ntohl(transaction_id);
  interval = ntohl(interval);
  leechers = ntohl(leechers);
  seeders = ntohl(seeders);

  /* len(IP) == 32 bits == 4 bytes
      len(PORT) == 16 bits == 2 bytes
      total == (4+2) * seeders */
  constexpr size_t ip_len = 4;
  constexpr size_t port_len = 2;
  constexpr size_t pair_len = 6;

  size_t remaining_size = pair_len * seeders;
  uint8_t tail_buffer[remaining_size];

  std::vector<uint32_t> ip_addresses;
  std::vector<uint16_t> ports;

  ssize_t tail_recv_len =
      recvfrom(conn.sockfd, tail_buffer, remaining_size, MSG_WAITALL,
               reinterpret_cast<sockaddr*>(&conn.servaddr), &socklen);

  for (size_t i = 0; i < seeders; i++) {
    uint32_t ip;
    uint16_t port;

    std::memcpy(&ip, tail_buffer + i * pair_len, ip_len);
    std::memcpy(&port, tail_buffer + i * pair_len, port_len);

    ip = htonl(ip);
    port = htons(port);

    ip_addresses.push_back(ip);
    ports.push_back(port);
  }

  return (UDP_IPv4_AnnounceResponse){
      action, transaction_id, interval, leechers, seeders, ip_addresses, ports};
}

UDP_ConnectResponse _recv_conresp_udp(SocketConnectionUDP& conn) {
  uint8_t buffer[(size_t)(UDP_BUFFER::CONNECT_RESPONSE)];

  uint32_t action;
  uint32_t transaction_id;
  uint64_t connection_id;

  socklen_t socklen = sizeof(conn.servaddr);
  ssize_t recv_len = recvfrom(
      conn.sockfd, buffer, (size_t)(UDP_BUFFER::CONNECT_RESPONSE), MSG_WAITALL,
      reinterpret_cast<sockaddr*>(&conn.servaddr), &socklen);

  if (recv_len < (size_t)(UDP_BUFFER::CONNECT_RESPONSE))
    return (UDP_ConnectResponse){};

  std::memcpy(&action, buffer + 0, 4);
  std::memcpy(&transaction_id, buffer + 4, 4);
  std::memcpy(&connection_id, buffer + 8, 8);

  action = ntohl(action);
  transaction_id = ntohl(transaction_id);
  connection_id = ntohll(connection_id);

  return (UDP_ConnectResponse){action, transaction_id, connection_id};
}

HostPortPair parse_udp(std::string url) {
  auto host_start = url.find("://") + 3;  // len(udp) == 3
  auto port_start = url.find(':', host_start);
  auto path_start = url.find('/', port_start);

  std::string host = url.substr(host_start, port_start - host_start);
  std::string port = url.substr(port_start + 1, path_start - port_start - 1);

  return {host, port};
}

SocketConnectionUDP connect_udp(std::string url) {
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

  sockaddr_in servaddr{};
  servaddr.sin_family = AF_INET;

  addrinfo hints{}, *res;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;

  HostPortPair hp = parse_udp(url);

  int addr_err = getaddrinfo(hp.host.c_str(), hp.port.c_str(), &hints, &res);
  if (addr_err != 0) {
    close(sockfd);
    return (SocketConnectionUDP){0, -1, servaddr};
  }

  servaddr = *reinterpret_cast<sockaddr_in*>(res->ai_addr);

  auto transaction_id = generate_rand_transaction_id();

  UDP_ConnectRequest conreq = {
      PROTOCOL_ID, static_cast<int32_t>(UDP_ACTION::CONNECT), transaction_id};

  SocketConnectionUDP conn = {0, sockfd, servaddr};

  int send_err = _send_conreq_udp(conn, conreq);

  if (send_err == -1) {
    close(sockfd);
    return (SocketConnectionUDP){0, -1, servaddr};
  }
  // std::cout << "Sent:\n"
  //           << PROTOCOL_ID << "\n"
  //           << static_cast<int32_t>(UDP_ACTION::CONNECT) << "\n"
  //           << transaction_id << "\n"
  //           << std::endl;

  UDP_ConnectResponse conresp = _recv_conresp_udp(conn);

  if (transaction_id != conresp.transaction_id ||
      (!conresp.action && !conresp.connection_id && !conresp.transaction_id)) {
    close(sockfd);
    return (SocketConnectionUDP){0, -1, servaddr};
  }

  // std::cout << "Received:\n"
  //           << conresp.action << "\n"
  //           << conresp.transaction_id << "\n"
  //           << conresp.connection_id << std::endl;
  conn.connection_id = conresp.connection_id;
  return (SocketConnectionUDP){conresp.connection_id, sockfd, servaddr};
}

UDP_IPv4_AnnounceResponse udp_announce(SocketConnectionUDP conn,
                                       UDP_IPv4_AnnounceRequest& annreq) {
  annreq.transaction_id = generate_rand_transaction_id();
}

int main(int argc, char* argv[]) {
  connect_udp(argv[1]);
  return 0;
}