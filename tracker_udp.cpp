#include "tracker.h"

// #include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <ctime>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <variant>

#include "bencode.h"
#include "constants.h"

struct HostPortPair {
  const std::string host;
  const std::string port;
};

struct SocketConnectionUDP {
  uint64_t connection_id;
  const int sockfd;
  sockaddr_in servaddr;
  uint16_t port;
};

size_t write_u16(uint8_t* buffer, const size_t offset, uint16_t& v) {
  memcpy(buffer + offset, &v, 2);
  return offset + 2;
}

size_t write_u32(uint8_t* buffer, const size_t offset, uint32_t& v) {
  memcpy(buffer + offset, &v, 4);
  return offset + 4;
}

size_t write_u64(uint8_t* buffer, const size_t offset, uint64_t& v) {
  memcpy(buffer + offset, &v, 8);
  return offset + 8;
}

size_t write_20B(uint8_t* buffer, const size_t offset, const uint8_t v[]) {
  memcpy(buffer + offset, v, 20);
  return offset + 20;
}

size_t read_u16(uint8_t* buffer, const size_t offset, uint16_t& v) {
  memcpy(&v, buffer + offset, 2);
  return offset + 2;
}

size_t read_u32(uint8_t* buffer, const size_t offset, uint32_t& v) {
  memcpy(&v, buffer + offset, 4);
  return offset + 4;
}

size_t read_u64(uint8_t* buffer, const size_t offset, uint64_t& v) {
  memcpy(&v, buffer + offset, 8);
  return offset + 9;
}

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

uint32_t generate_rand_transaction_id() {
  srand(time(NULL) * getpid());
  return rand();
}

int _send_announce_udp(SocketConnectionUDP& conn,
                       const IPv4_AnnounceRequest& annreq) {
  uint8_t buffer[(size_t)(UDP_BUFFER::ANNOUNCE_REQUEST)];

  uint64_t connection_id = htonll(annreq.connection_id);
  uint32_t action = htonl(annreq.action);
  uint32_t transaction_id = htonl(annreq.transaction_id);

  uint64_t downloaded = htonll(annreq.downloaded);
  uint64_t left = htonll(annreq.left);
  uint64_t uploaded = htonll(annreq.uploaded);

  uint32_t event = htonl(annreq.event);
  uint32_t ip_address = htonl(annreq.ip_address);
  uint32_t key = htonl(annreq.key);
  uint32_t num_want = htonl(annreq.num_want);

  uint16_t port = htons(annreq.port);

  size_t offset = 0;
  offset = write_u64(buffer, offset, connection_id);
  offset = write_u32(buffer, offset, action);
  offset = write_u32(buffer, offset, transaction_id);

  offset = write_20B(buffer, offset, annreq.info_hash);
  offset = write_20B(buffer, offset, annreq.peer_id);

  offset = write_u64(buffer, offset, downloaded);
  offset = write_u64(buffer, offset, left);
  offset = write_u64(buffer, offset, uploaded);

  offset = write_u32(buffer, offset, event);
  offset = write_u32(buffer, offset, ip_address);
  offset = write_u32(buffer, offset, key);
  offset = write_u32(buffer, offset, num_want);
  offset = write_u16(buffer, offset, port);

  socklen_t len = sizeof(conn.servaddr);
  return sendto(conn.sockfd, buffer, offset, 0,
                reinterpret_cast<sockaddr*>(&conn.servaddr), len);
}

int _send_conreq_udp(SocketConnectionUDP& conn,
                     const UDP_ConnectRequest& conreq) {
  uint8_t buffer[(size_t)(UDP_BUFFER::CONNECT_REQUEST)];

  uint64_t protocol_id = htonll(conreq.protocol_id);
  uint32_t action = htonl(conreq.action);
  uint32_t transaction_id = htonl(conreq.transaction_id);
  
  size_t offset = 0;
  offset = write_u64(buffer, offset, protocol_id);
  offset = write_u32(buffer, offset, action);
  offset = write_u32(buffer, offset, transaction_id);

  socklen_t len = sizeof(conn.servaddr);
  return sendto(conn.sockfd, buffer, sizeof(buffer), 0,
                reinterpret_cast<sockaddr*>(&conn.servaddr), len);
}

IPv4_AnnounceResponse _recv_annreq_udp(SocketConnectionUDP conn) {
  uint8_t buffer[(size_t)UDP_BUFFER::ANNOUNCE_RESPONSE];

  uint32_t action;
  uint32_t transaction_id;
  uint32_t interval;
  uint32_t leechers;
  uint32_t seeders;

  socklen_t socklen = sizeof(conn.servaddr);
  ssize_t recv_len =
      recvfrom(conn.sockfd, buffer, sizeof(buffer), 0,
               reinterpret_cast<sockaddr*>(&conn.servaddr), &socklen);

  if (recv_len < (size_t)(UDP_BUFFER::ANNOUNCE_HEAD))
    return (IPv4_AnnounceResponse){};

  size_t offset = 0;
  offset = read_u32(buffer, offset, action);
  offset = read_u32(buffer, offset, transaction_id);
  offset = read_u32(buffer, offset, interval);
  offset = read_u32(buffer, offset, leechers);
  offset = read_u32(buffer, offset, seeders);

  action = ntohl(action);
  transaction_id = ntohl(transaction_id);
  interval = ntohl(interval);
  leechers = ntohl(leechers);
  seeders = ntohl(seeders);

  // std::cout << "action:" << action << " tid:" << transaction_id
  //           << " int:" << interval << " le:" << leechers << " se:" << seeders
  //           << std::endl;

  /* len(IP) == 32 bits == 4 bytes
      len(PORT) == 16 bits == 2 bytes */
  constexpr size_t ip_len = 4;
  constexpr size_t port_len = 2;
  constexpr size_t pair_len = 6;

  size_t remaining_size = pair_len * seeders;

  std::vector<uint32_t> ip_addresses;
  std::vector<uint16_t> ports;

  while (offset <= recv_len) {
    uint32_t ip;
    uint16_t port;

    offset = read_u32(buffer, offset, ip);
    offset = read_u16(buffer, offset, port);

    // no conversion, will be using big endian later anyway
    ip_addresses.push_back(ip);
    ports.push_back(port);
  }

  return (IPv4_AnnounceResponse){action,  transaction_id, interval, leechers,
                                 seeders, ip_addresses,   ports};
}

UDP_ConnectResponse _recv_conresp_udp(SocketConnectionUDP& conn) {
  uint8_t buffer[(size_t)(UDP_BUFFER::CONNECT_RESPONSE)];

  uint32_t action;
  uint32_t transaction_id;
  uint64_t connection_id;

  socklen_t socklen = sizeof(conn.servaddr);
  ssize_t recv_len =
      recvfrom(conn.sockfd, buffer, (size_t)(UDP_BUFFER::CONNECT_RESPONSE), 0,
               reinterpret_cast<sockaddr*>(&conn.servaddr), &socklen);

  if (recv_len < (size_t)(UDP_BUFFER::CONNECT_RESPONSE))
    return (UDP_ConnectResponse){};

  size_t offset = 0;
  offset = read_u32(buffer, offset, action);
  offset = read_u32(buffer, offset, transaction_id);
  offset = read_u64(buffer, offset, connection_id);

  action = ntohl(action);
  transaction_id = ntohl(transaction_id);
  connection_id = ntohll(connection_id);

  return (UDP_ConnectResponse){action, transaction_id, connection_id};
}

HostPortPair _parse_udp(std::string url) {
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

  HostPortPair hp = _parse_udp(url);

  int addr_err = getaddrinfo(hp.host.c_str(), hp.port.c_str(), &hints, &res);
  if (addr_err != 0) {
    close(sockfd);
    return (SocketConnectionUDP){0, -1, servaddr, 0};
  }

  servaddr = *reinterpret_cast<sockaddr_in*>(res->ai_addr);

  auto transaction_id = generate_rand_transaction_id();

  UDP_ConnectRequest conreq = {
      PROTOCOL_ID, static_cast<int32_t>(UDP_ACTION::CONNECT), transaction_id};

  SocketConnectionUDP conn = {0, sockfd, servaddr};

  int send_err = _send_conreq_udp(conn, conreq);

  if (send_err == -1) {
    close(sockfd);
    return (SocketConnectionUDP){0, -1, servaddr, 0};
  }
  // std::cout << "Sent:\n"
  //           << PROTOCOL_ID << "\n"
  //           << static_cast<int32_t>(UDP_ACTION::CONNECT) << "\n"
  //           << transaction_id << "\n"
  //           << std::endl;

  UDP_ConnectResponse conresp = _recv_conresp_udp(conn);

  // std::cout << "Received:\n"
  //           << conresp.action << "\n"
  //           << conresp.transaction_id << "\n"
  //           << conresp.connection_id << std::endl;

  if (transaction_id != conresp.transaction_id ||
      (!conresp.action && !conresp.connection_id && !conresp.transaction_id)) {
    close(sockfd);
    return (SocketConnectionUDP){0, -1, servaddr, 0};
  }

  conn.connection_id = conresp.connection_id;
  return (SocketConnectionUDP){conresp.connection_id, sockfd, servaddr,
                               (uint16_t)atoi(hp.port.c_str())};
}

IPv4_AnnounceResponse announce_udp(std::shared_ptr<TrackerParams> params,
                                   SocketConnectionUDP conn, uint32_t event,
                                   uint16_t port) {
  uint32_t real_downloaded;
  {
    const std::lock_guard<std::mutex> lock(params.get()->d_mut);
    real_downloaded = params.get()->downloaded;
  }

  IPv4_AnnounceRequest annreq = {
      conn.connection_id,
      static_cast<uint32_t>(UDP_ACTION::ANNOUNCE),
      generate_rand_transaction_id(),
      params.get()->info_hash.data(),
      params.get()->peer_id.data(),
      real_downloaded,
      params.get()->size - real_downloaded,
      (uint32_t)0,  // uploaded...? I'M A LEECH!
      event,
      static_cast<uint32_t>(ANNOUNCE_DEFAULTS::IP_ADDRESS),
      (uint32_t)0,  // no idea what a key does.
      static_cast<uint32_t>(ANNOUNCE_DEFAULTS::NUM_WANT),
      port,
  };

  // std::cout << "Sent Announce:\n"
  //           << annreq.connection_id << "\n"
  //           << annreq.action << "\n"
  //           << annreq.info_hash << "\n"
  //           << annreq.peer_id << "\n"
  //           << annreq.downloaded << "\n"
  //           << annreq.left << "\n"
  //           << annreq.uploaded << "\n"
  //           << annreq.event << "\n"
  //           << annreq.ip_address << "\n"
  //           << annreq.key << "\n"
  //           << annreq.num_want << "\n"
  //           << annreq.port << std::endl;

  int err = _send_announce_udp(conn, annreq);
  if (err == -1) return IPv4_AnnounceResponse{};

  const IPv4_AnnounceResponse& annresp = _recv_annreq_udp(conn);
  // std::cout << "Received Announce:\n"
  //           << annresp.action << "\n"
  //           << annresp.transaction_id << "\n"
  //           << annresp.interval << "\n"
  //           << annresp.leechers << "\n"
  //           << annresp.seeders << "\n"
  //           << std::endl;

  if (annresp.transaction_id != annreq.transaction_id)
    return IPv4_AnnounceResponse{};

  return annresp;
}

void udp_life(const std::shared_ptr<TrackerParams> & params) {
  // std::cout << "Attempting connection to " << params.get()->url << std::endl;

  // connect
  SocketConnectionUDP conn = connect_udp(params.get()->url);

  // std::cout << "Connected to " << params.get()->url << std::endl;

  // indefinite announce loop
  size_t iter = 0;
  for (;; iter++) {
    uint32_t event = iter == 0
                         ? static_cast<uint32_t>(ANNOUNCE_DEFAULTS::STARTED)
                         : static_cast<uint32_t>(ANNOUNCE_DEFAULTS::NONE);

    IPv4_AnnounceResponse resp = announce_udp(params, conn, event, conn.port);

    const std::lock_guard<std::mutex> lock(params.get()->ps_mut);
    for (size_t i = 0; i < resp.ip_addresses.size(); i++) {
      params.get()->peer_set.emplace(resp.ip_addresses[i], resp.ports[i]);
    }

    std::this_thread::sleep_for(
        std::chrono::seconds(resp.interval)); 
  }
}

