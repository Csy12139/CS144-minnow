#include "tcp_receiver.hh"
#include <iostream>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  if ( message.SYN ) {
    m_state = ESTB_CONN;
    m_recv_zero_point = message.seqno;
  }

  if ( m_state != ESTB_CONN )
    return;

  uint64_t first_index = message.seqno.unwrap( m_recv_zero_point, inbound_stream.bytes_pushed() ) - 1 + message.SYN;
  reassembler.insert( first_index, std::move( message.payload ), message.FIN, inbound_stream );

  if (inbound_stream.is_closed())
    m_state = CLOSED;
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  TCPReceiverMessage message;

  if ( m_state != LISTEN ) {
    uint64_t bytes_pushed = inbound_stream.bytes_pushed();
    uint64_t absolute_ackno = inbound_stream.is_closed() ? bytes_pushed + 2 : bytes_pushed + 1;  // syn + fin : syn
    message.ackno = Wrap32::wrap( absolute_ackno, m_recv_zero_point );
  }

  uint64_t window_size = inbound_stream.available_capacity();
  window_size = window_size > 0xFFFF ? 0xFFFF : window_size;
  message.window_size = window_size;

  return message;
}
