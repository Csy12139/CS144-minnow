#pragma once

#include "address.hh"
#include "ethernet_frame.hh"
#include "ipv4_datagram.hh"

#include <iostream>
#include <map>
#include <optional>
#include <queue>
#include <unordered_map>
#include <utility>
#include <vector>

// A "network interface" that connects IP (the internet layer, or network layer)
// with Ethernet (the network access layer, or link layer).

// This module is the lowest layer of a TCP/IP stack
// (connecting IP with the lower-layer network protocol,
// e.g. Ethernet). But the same module is also used repeatedly
// as part of a router: a router generally has many network
// interfaces, and the router's job is to route Internet datagrams
// between the different interfaces.

// The network interface translates datagrams (coming from the
// "customer," e.g. a TCP/IP stack or router) into Ethernet
// frames. To fill in the Ethernet destination address, it looks up
// the Ethernet address of the next IP hop of each datagram, making
// requests with the [Address Resolution Protocol](\ref rfc::rfc826).
// In the opposite direction, the network interface accepts Ethernet
// frames, checks if they are intended for it, and if so, processes
// the the payload depending on its type. If it's an IPv4 datagram,
// the network interface passes it up the stack. If it's an ARP
// request or reply, the network interface processes the frame
// and learns or replies as necessary.
class NetworkInterface
{
private:
  struct AddressCache
  {
    EthernetAddress ethernet_address {};
    uint64_t expire_time_ms {};
    AddressCache() = default;
    AddressCache(const EthernetAddress& ethernet_addr, uint64_t time_ms): ethernet_address(ethernet_addr), expire_time_ms(time_ms) {}
  };

  // Ethernet (known as hardware, network-access, or link-layer) address of the interface
  EthernetAddress ethernet_address_;

  // IP (known as Internet-layer or network-layer) address of the interface
  Address ip_address_;

  // TIMEOUT CONFIG
  static constexpr uint32_t ADDRESS_CACHE_TIMEOUT_MS = 30000;
  static constexpr uint32_t ARP_REQUEST_TIMEOUT_MS = 5000;

  // total number of milliseconds the NetworkInterface has been alive
  uint64_t timer {};

  // a timer for clean up expire arp requests
  uint64_t arp_timer {};

  // ip_address(numeric) -> ethernet_address
  std::unordered_map<uint32_t, AddressCache> address_map {};

  // ip_address(numeric) -> expire_time
  std::unordered_map<uint32_t, uint64_t> arp_request_expire_timers {};

  // datagram cache waiting for ARP reply. ip_address(numeric) -> queue<InternetDatagram>
  std::unordered_map<uint32_t, std::queue<InternetDatagram>> datagram_cache {};

  // send queue for EthernetFrame
  std::queue<EthernetFrame> send_queue {};

private:
  // internal helper to create a ethernet frame
  EthernetFrame create_ethernet_frame( uint16_t type,
                                       std::vector<Buffer> payload,
                                       const EthernetAddress& dst ) const;

  // internal helper to contrast EthernetAddress
  bool is_equal( const EthernetAddress& lhs, const EthernetAddress& rhs ) const;

  // push Internet datagram to the send queue
  void push_datagram( const InternetDatagram& dgram, const EthernetAddress& dst );

  // push ARP message to the send queue
  void push_arp_request( uint32_t ipv4_numeric );
  void push_arp_reply( uint32_t sender_ipv4, uint32_t target_ipv4 );

  // handlers
  void handle_arp( const EthernetFrame& frame );

public:
  // Construct a network interface with given Ethernet (network-access-layer) and IP (internet-layer)
  // addresses
  NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address );

  // Access queue of Ethernet frames awaiting transmission
  std::optional<EthernetFrame> maybe_send();

  // Sends an IPv4 datagram, encapsulated in an Ethernet frame (if it knows the Ethernet destination
  // address). Will need to use [ARP](\ref rfc::rfc826) to look up the Ethernet destination address
  // for the next hop.
  // ("Sending" is accomplished by making sure maybe_send() will release the frame when next called,
  // but please consider the frame sent as soon as it is generated.)
  void send_datagram( const InternetDatagram& dgram, const Address& next_hop );

  // Receives an Ethernet frame and responds appropriately.
  // If type is IPv4, returns the datagram.
  // If type is ARP request, learn a mapping from the "sender" fields, and send an ARP reply.
  // If type is ARP reply, learn a mapping from the "sender" fields.
  std::optional<InternetDatagram> recv_frame( const EthernetFrame& frame );

  // Called periodically when time elapses
  void tick( size_t ms_since_last_tick );
};
