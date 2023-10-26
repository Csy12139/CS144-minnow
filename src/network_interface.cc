#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

using namespace std;

EthernetFrame NetworkInterface::create_ethernet_frame(uint16_t type, std::vector<Buffer> payload, const EthernetAddress& dst) const
{
  // mayebe std::move
  EthernetFrame frame;
  frame.payload = payload;
  frame.header.type = type;
  frame.header.dst = dst;
  frame.header.src = ethernet_address_;

  return frame;
}

void NetworkInterface::push_datagram(const InternetDatagram& dgram, const EthernetAddress& dst)
{
  // mayebe std::move
  vector<Buffer> payload = serialize(dgram);
  EthernetFrame frame = create_ethernet_frame(EthernetHeader::TYPE_IPv4, payload, dst);
  send_queue.push(frame);
}

void NetworkInterface::push_arp_request(uint32_t ipv4_numeric)
{
  ARPMessage message;
  message.opcode = ARPMessage::OPCODE_REQUEST;
  message.sender_ethernet_address = ethernet_address_;
  message.sender_ip_address = ip_address_.ipv4_numeric();
  message.target_ip_address = ipv4_numeric;

  // maybe std::move
  vector<Buffer> payload = serialize(message);
  EthernetFrame frame = create_ethernet_frame(EthernetHeader::TYPE_ARP, payload, ETHERNET_BROADCAST);

  arp_request_expire_timers[ipv4_numeric] = timer + NetworkInterface::ARP_REQUEST_TIMEOUT_MS ;
  send_queue.push(frame); 
}

void NetworkInterface::push_arp_reply(uint32_t query_ipv4, const EthernetAddress& query_ethernet, const EthernetAddress& dst)
{
  ARPMessage message;
  message.opcode = ARPMessage::OPCODE_REPLY;
  message.sender_ethernet_address = ethernet_address_;
  message.sender_ip_address = ip_address_.ipv4_numeric();
  message.target_ip_address = query_ipv4;
  message.target_ethernet_address = query_ethernet;

  // maybe std::move
  vector<Buffer> payload = serialize(message);
  EthernetFrame frame = create_ethernet_frame(EthernetHeader::TYPE_ARP, payload, dst);
  send_queue.push(frame); 
}

bool NetworkInterface::is_equal(const EthernetAddress& lhs, const EthernetAddress& rhs) const
{
  for ( uint64_t i = 0; i < lhs.size(); ++i)
    if (lhs[i] != rhs[i]) return false;

  return true;
}

void NetworkInterface::handle_arp_reply(const EthernetFrame& frame)
{
  ARPMessage message;

  if (!parse(message, frame.payload))
    return;

  if (message.opcode == ARPMessage::OPCODE_REPLY)
  {
    uint32_t ipv4_numeric = message.target_ip_address;
    EthernetAddress& ethernet_address = message.target_ethernet_address;

    address_expire_timers[ipv4_numeric] = timer + NetworkInterface::ADDRESS_CACHE_TIMEOUT_MS;
    address_cache[ipv4_numeric] = ethernet_address;

    if (datagram_cache.contains(ipv4_numeric))
    {
      queue<InternetDatagram>& cache_queue = datagram_cache[ipv4_numeric];

      while (!cache_queue.empty())
      {
        push_datagram(cache_queue.front(), ethernet_address);
        cache_queue.pop();
      }

      datagram_cache.erase(ipv4_numeric); 
    }
  }
  else if (message.opcode == ARPMessage::OPCODE_REQUEST)
  {
    uint32_t src_ipv4_numeric = message.sender_ip_address;
    uint32_t target_ipv4_numeric = message.target_ip_address;

    if (address_cache.contains(target_ipv4_numeric))
      push_arp_reply(src_ipv4_numeric, address_cache[target_ipv4_numeric], message.sender_ethernet_address);
  }

}

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address ), ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  uint32_t ipv4_numeric = next_hop.ipv4_numeric();

  if (address_cache.contains(ipv4_numeric))
  {
    push_datagram(dgram, address_cache[ipv4_numeric]);
  }
  else
  {
    if (!arp_request_expire_timers.contains(ipv4_numeric) || timer > arp_request_expire_timers[ipv4_numeric])
    {
      push_arp_request(ipv4_numeric);

      if (!datagram_cache.contains(ipv4_numeric))
        datagram_cache[ipv4_numeric] = queue<InternetDatagram> {};

      datagram_cache[ipv4_numeric].push(dgram);
    }
  }
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{

  if ( !is_equal( frame.header.dst, ethernet_address_ ) && !is_equal( frame.header.dst, ETHERNET_BROADCAST ) )
    return {};

  switch ( frame.header.type ) {
    case EthernetHeader::TYPE_IPv4: {
      InternetDatagram dgram;

      if ( parse( dgram, frame.payload ) )
        return dgram;
      else
        return {};
    } break;

    case EthernetHeader::TYPE_ARP: {
      handle_arp_reply( frame );
    } break;

    default:
      break;
  }

  return {};
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  timer += ms_since_last_tick;
  
  // expire address map cache
  for (auto iter = address_expire_timers.begin(); iter != address_expire_timers.end();)
  {
    if (timer < iter->second)
    {
      ++iter;
      continue;
    }

    address_cache.erase(iter->first);
    iter = address_expire_timers.erase(iter);
  }

  // expire ARP requests
  for (auto iter = arp_request_expire_timers.begin(); iter != arp_request_expire_timers.end();)
  {
    if (timer < iter->second)
    {
      ++iter;
      continue;
    }

    iter = arp_request_expire_timers.erase(iter);
  }
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  if (send_queue.empty())
    return {};

  EthernetFrame frame(std::move(send_queue.front()));
  send_queue.pop();

  return frame;
}
