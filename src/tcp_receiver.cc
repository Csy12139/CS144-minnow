#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  m_syn_rcvd = m_syn_rcvd || message.SYN;

  if ( !m_syn_rcvd )
    return;

  m_fin_rcvd = m_fin_rcvd || message.FIN;
  m_recv_zero_point = message.SYN ? message.seqno : m_recv_zero_point;

  uint64_t first_index = message.seqno.unwrap( m_recv_zero_point, inbound_stream.bytes_pushed() ) - 1 + message.SYN;
  reassembler.insert( first_index, std::move( message.payload ), message.FIN, inbound_stream );
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  TCPReceiverMessage message;

  if ( m_syn_rcvd ) {
    uint64_t bytes_pushed = inbound_stream.bytes_pushed();
    uint64_t absolute_ackno = inbound_stream.is_closed() ? bytes_pushed + 2 : bytes_pushed + 1;
    message.ackno = Wrap32::wrap( absolute_ackno, m_recv_zero_point );
  }

  message.window_size = inbound_stream.available_capacity() > 0xFFFF ? 0xFFFF : inbound_stream.available_capacity();

  return message;
}
