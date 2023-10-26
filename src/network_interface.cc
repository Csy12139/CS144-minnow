#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

using namespace std;

EthernetFrame NetworkInterface::create_ethernet_frame( uint16_t type,
                                                       std::vector<Buffer> payload,
                                                       const EthernetAddress& dst ) const
{
  EthernetFrame frame;
  frame.payload = std::move( payload );
  frame.header.type = type;
  frame.header.dst = dst;
  frame.header.src = ethernet_address_;

  return frame;
}

void NetworkInterface::push_datagram( const InternetDatagram& dgram, const EthernetAddress& dst )
{
  vector<Buffer> payload = serialize( dgram );
  EthernetFrame frame = create_ethernet_frame( EthernetHeader::TYPE_IPv4, std::move( payload ), dst );
  send_queue.push( frame );
}

void NetworkInterface::push_arp_request( uint32_t ipv4_numeric )
{
  ARPMessage message;
  message.opcode = ARPMessage::OPCODE_REQUEST;
  message.sender_ethernet_address = ethernet_address_;
  message.sender_ip_address = ip_address_.ipv4_numeric();
  message.target_ip_address = ipv4_numeric;

  vector<Buffer> payload = serialize( message );
  EthernetFrame frame = create_ethernet_frame( EthernetHeader::TYPE_ARP, std::move( payload ), ETHERNET_BROADCAST );

  arp_request_expire_timers[ipv4_numeric] = timer + NetworkInterface::ARP_REQUEST_TIMEOUT_MS;
  send_queue.push( std::move( frame ) );
}

// replying be like the host which ARP request is searching for(meaning the result should be set to "sender" fields)
// then, the network could cache sender fields eigher REPLY or REQUEST ARP message
void NetworkInterface::push_arp_reply( uint32_t sender_ipv4, uint32_t target_ipv4 )
{
  ARPMessage message;
  message.opcode = ARPMessage::OPCODE_REPLY;
  message.sender_ip_address = sender_ipv4;
  message.sender_ethernet_address = address_map[sender_ipv4].ethernet_address;
  message.target_ip_address = target_ipv4;
  message.target_ethernet_address = address_map[target_ipv4].ethernet_address;

  vector<Buffer> payload = serialize( message );
  EthernetFrame frame = create_ethernet_frame(
    EthernetHeader::TYPE_ARP, std::move( payload ), address_map[target_ipv4].ethernet_address );
  send_queue.push( std::move( frame ) );
}

bool NetworkInterface::is_equal( const EthernetAddress& lhs, const EthernetAddress& rhs ) const
{
  for ( uint64_t i = 0; i < lhs.size(); ++i )
    if ( lhs[i] != rhs[i] )
      return false;

  return true;
}

void NetworkInterface::handle_arp( const EthernetFrame& frame )
{
  ARPMessage message;

  if ( !parse( message, frame.payload ) )
    return;

  // cache address either REPLY or REQUEST ARP message
  uint32_t sedner_ipv4 = message.sender_ip_address;
  const EthernetAddress& sender_ethernet = message.sender_ethernet_address;

  address_map[sedner_ipv4] = AddressCache( sender_ethernet, timer + ADDRESS_CACHE_TIMEOUT_MS );

  if ( datagram_cache.contains( sedner_ipv4 ) ) {
    queue<InternetDatagram>& cache_queue = datagram_cache[sedner_ipv4];

    while ( !cache_queue.empty() ) {
      push_datagram( std::move( cache_queue.front() ), sender_ethernet );
      cache_queue.pop();
    }

    datagram_cache.erase( sedner_ipv4 );
  }

  if ( message.opcode == ARPMessage::OPCODE_REQUEST && address_map.contains( message.target_ip_address ) )
    push_arp_reply( message.target_ip_address, sedner_ipv4 );
}

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address ), ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";

  // add local address to the map
  address_map[ip_address_.ipv4_numeric()] = AddressCache( ethernet_address_, UINT64_MAX );
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  uint32_t ipv4_numeric = next_hop.ipv4_numeric();

  if ( address_map.contains( ipv4_numeric ) ) {
    push_datagram( dgram, address_map[ipv4_numeric].ethernet_address );
  } else {
    if ( !arp_request_expire_timers.contains( ipv4_numeric ) || timer > arp_request_expire_timers[ipv4_numeric] ) {
      push_arp_request( ipv4_numeric );

      if ( !datagram_cache.contains( ipv4_numeric ) )
        datagram_cache[ipv4_numeric] = queue<InternetDatagram> {};

      datagram_cache[ipv4_numeric].push( dgram );
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

    } break;

    case EthernetHeader::TYPE_ARP: {
      handle_arp( frame );
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
  for ( auto iter = address_map.begin(); iter != address_map.end(); ) {
    if ( timer < iter->second.expire_time_ms ) {
      ++iter;
      continue;
    }

    iter = address_map.erase( iter );
  }

  arp_timer += ms_since_last_tick;

  if ( arp_timer >= ARP_REQUEST_TIMEOUT_MS ) {
    arp_timer = 0;

    for ( auto iter = arp_request_expire_timers.begin(); iter != arp_request_expire_timers.end(); ) {
      if ( timer < iter->second ) {
        ++iter;
        continue;
      }

      iter = arp_request_expire_timers.erase( iter );
    }
  }
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  if ( send_queue.empty() )
    return {};

  EthernetFrame frame( std::move( send_queue.front() ) );
  send_queue.pop();

  return frame;
}
