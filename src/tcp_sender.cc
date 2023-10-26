#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>

using namespace std;

void RetransmissionTimer::restart( uint64_t ms_timeout )
{
  m_timer = 0;
  m_timeout = ms_timeout;
  m_running = true;
}

void RetransmissionTimer::elapse( uint64_t ms_time )
{
  m_timer = m_running ? m_timer + ms_time : m_timer;
}

void RetransmissionTimer::stop()
{
  m_running = false;
  m_timeout = UINT64_MAX;
  m_timer = 0; 
}

RetransmissionTimeout::RetransmissionTimeout( uint64_t initial_RTO_ms )
  : m_value( initial_RTO_ms ), m_init_value( initial_RTO_ms )
{}

void RetransmissionTimeout::set_timeout( RestransmissionEvent event )
{
  switch ( event ) {
    case TIMEOUT:
      m_value += m_value;
      break;

    case SUCCESSFUL_RECEIPT:
      m_value = m_init_value;
      break;

    default:
      break;
  }
}

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : m_isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ), m_RTO_ms_( initial_RTO_ms )
{}

uint64_t TCPSender::get_absolute_seqno() const
{
  return m_outstanding_messages_.empty()
           ? m_window_left
           : m_outstanding_messages_.rbegin()->first + m_outstanding_messages_.rbegin()->second.sequence_length();
}

void TCPSender::push_message( std::string payload, bool syn, bool fin )
{
  uint64_t absolute_seqno = get_absolute_seqno();

  TCPSenderMessage message;
  message.seqno = Wrap32::wrap( absolute_seqno, m_isn_ );
  message.payload = Buffer( std::move( payload ) );
  message.SYN = syn;
  message.FIN = fin;

  m_outstanding_messages_.insert( std::make_pair( absolute_seqno, message ) );
  m_send_queue_.push( std::move( message ) );
}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return get_absolute_seqno() - m_window_left;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return m_consecutive_retransmissions;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  if ( m_send_queue_.empty() )
    return {};

  TCPSenderMessage message = std::move( m_send_queue_.front() );
  m_send_queue_.pop();

  if ( !m_retransmission_timer_.is_running() )
    m_retransmission_timer_.restart( m_RTO_ms_.value() );

  return message;
}

void TCPSender::push( Reader& outbound_stream )
{
  if ( !m_syc_pushed ) {
    m_fin_pushed = outbound_stream.bytes_buffered() == 0 && outbound_stream.is_finished();
    m_syc_pushed = true;
    push_message( {}, m_syc_pushed, m_fin_pushed );
  }

  if ( m_fin_pushed )
    return;

  string payload;

  uint64_t window_right = m_window_size == 0 ? m_window_left + 1 : m_window_left + m_window_size;

  while (!m_fin_pushed && get_absolute_seqno() + TCPConfig::MAX_PAYLOAD_SIZE <= window_right) {
    read( outbound_stream, TCPConfig::MAX_PAYLOAD_SIZE, payload );
    bool is_fin_msg = payload.size() + get_absolute_seqno() < window_right && outbound_stream.bytes_buffered() == 0 && outbound_stream.is_finished();

    if (payload.empty() && !is_fin_msg)
      break;
    
    m_fin_pushed = m_fin_pushed || is_fin_msg;
    push_message( std::move( payload ) , false, is_fin_msg);
  }

  if ( !m_fin_pushed && get_absolute_seqno() < window_right ) {
    read( outbound_stream, window_right - get_absolute_seqno(), payload );
    bool is_fin_msg = payload.size() + get_absolute_seqno() < window_right && outbound_stream.bytes_buffered() == 0 && outbound_stream.is_finished();

    if (!payload.empty() || is_fin_msg)
    {
      m_fin_pushed = m_fin_pushed || is_fin_msg;
      push_message( std::move( payload ) , false, is_fin_msg);
    }
  }
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  TCPSenderMessage message;
  message.seqno = Wrap32::wrap( get_absolute_seqno(), m_isn_ );
  return message;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if ( !msg.ackno.has_value() )
    return;

  uint64_t absolute_ackno = msg.ackno.value().unwrap( m_isn_, m_window_left );

  bool sucessful_recipt = false;

  while ( !m_outstanding_messages_.empty() ) {
    uint64_t wait_for_ackno
      = m_outstanding_messages_.begin()->first + m_outstanding_messages_.begin()->second.sequence_length();

    if ( absolute_ackno < wait_for_ackno || absolute_ackno > get_absolute_seqno())
      break;

    // sucessful receipt
    m_outstanding_messages_.erase( m_outstanding_messages_.begin() );
    sucessful_recipt = true;
  }

  if ( sucessful_recipt ) {
    m_RTO_ms_.set_timeout( RetransmissionTimeout::SUCCESSFUL_RECEIPT );
    m_consecutive_retransmissions = 0;
    m_window_left = absolute_ackno;

    if ( !m_outstanding_messages_.empty() )
      m_retransmission_timer_.restart( m_RTO_ms_.value() );
    else
      m_retransmission_timer_.stop();
  }

  m_window_size = msg.window_size;
}

void TCPSender::tick( const uint64_t ms_since_last_tick )
{
  m_timer_ += ms_since_last_tick;
  m_retransmission_timer_.elapse( ms_since_last_tick );

  if ( m_retransmission_timer_.is_timeout() ) {
    // resend the earliest outstanding message
    m_send_queue_.push( m_outstanding_messages_.begin()->second );

    if ( m_window_size > 0 ) {
      m_RTO_ms_.set_timeout( RetransmissionTimeout::TIMEOUT );
      m_consecutive_retransmissions++;
    }

    m_retransmission_timer_.restart( m_RTO_ms_.value() );
  }
}
