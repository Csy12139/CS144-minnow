#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

bool Router::RouteTable::match( const uint32_t ipv4_address, const uint64_t concat )
{
  // This ensure the safety to shift 64-bit uint of any valid (up to 32) prefix_length
  const uint64_t prefix_length = concat & 0x3F;
  const uint64_t route_prefix_mask = 0xFFFFFFFF - ( ( (uint64_t)1 << ( 32 - prefix_length ) ) - 1 );

  return ( (concat >> 32) & route_prefix_mask ) == ( ipv4_address & route_prefix_mask );
}

void Router::RouteTable::insert( const uint64_t concat, const size_t num, const optional<Address>& next_hop )
{
  entries[concat] = num;

  if ( next_hop.has_value() )
    next_hops[concat] = next_hop.value().ipv4_numeric();
}

bool Router::RouteTable::look_up( const uint32_t ipv4_address, size_t& interface_num, optional<uint32_t>& next_hop ) const
{
  next_hop.reset();
  const uint64_t prefix_length_mask = 0xFFFFFFFF;

  uint64_t concat = 0;
  uint8_t longest_prefix_length = 0;
  size_t num = 0;
  bool matched = false;

  for ( const auto pair : entries ) {
    if ( match( ipv4_address, pair.first ) && ( pair.first & prefix_length_mask ) >= longest_prefix_length ) {
      matched = true;

      num = pair.second;
      longest_prefix_length = pair.first & prefix_length_mask;
      concat = pair.first;
    }
  }

  interface_num = matched ? num : interface_num;

  if ( matched && next_hops.contains( concat ) )
    next_hop = next_hops.find( concat )->second;

  return matched;
}

void Router::route_datagram( InternetDatagram dgram )
{
  if ( dgram.header.ttl == 1 || dgram.header.ttl == 0 )
    return;

  --dgram.header.ttl;

  size_t interface_num = 0;
  optional<uint32_t> next_hop;

  if ( !route_table_.look_up( dgram.header.dst, interface_num, next_hop ) )
    return;

  if ( !next_hop.has_value() )
    next_hop = dgram.header.dst;

  auto& interface = interfaces_[interface_num];

  dgram.header.compute_checksum();
  interface.send_datagram( dgram, Address::from_ipv4_numeric( next_hop.value() ) );
}

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
      << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
      << " on interface " << interface_num << "\n";

  const uint64_t concat = ( static_cast<uint64_t>( route_prefix ) << 32 ) + static_cast<uint64_t>( prefix_length );
  route_table_.insert( concat, interface_num, next_hop );
}

void Router::route()
{
  for ( auto& interface : interfaces_ ) {
    while ( true ) {
      auto dgram = interface.maybe_receive();

      if ( !dgram.has_value() )
        break;

      route_datagram( std::move( dgram.value() ) );
    }
  }
}
