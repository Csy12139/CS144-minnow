#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include <map>
#include <queue>
#include <utility>

class RetransmissionTimeout
{
private:
  uint64_t m_value {};
  uint64_t m_init_value {};

public:
  enum RestransmissionEvent
  {
    TIMEOUT,
    SUCCESSFUL_RECEIPT,
  };

  explicit RetransmissionTimeout( uint64_t initial_RTO_ms );
  uint64_t value() const { return m_value; }
  void set_timeout( RestransmissionEvent event );
};

class RetransmissionTimer
{
private:
  uint64_t m_timer {};
  uint64_t m_timeout { UINT64_MAX };
  bool m_running {};

public:
  uint64_t value() const { return m_timer; }
  void stop();
  bool is_running() const { return m_running; }
  bool is_timeout() const { return m_running && m_timer >= m_timeout; }
  void restart( uint64_t ms_timeout );
  void elapse( uint64_t ms_time );
};

class TCPSender
{
private:
  std::map<uint64_t, TCPSenderMessage> m_outstanding_messages_ {}; // seqno -> TCPSenderMessage
  std::queue<TCPSenderMessage> m_send_queue_ {};

  bool m_syc_pushed {};
  bool m_fin_pushed {};

  uint64_t m_timer_ {};
  RetransmissionTimer m_retransmission_timer_ {};
  Wrap32 m_isn_ { 0 };
  RetransmissionTimeout m_RTO_ms_ { 0 };
  uint64_t m_consecutive_retransmissions {};
  uint64_t m_window_left { 0 };
  uint64_t m_window_size { 1 }; // for syn

  uint64_t get_absolute_seqno() const;
  void push_message( std::string payload, bool syn = false, bool fin = false );

public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn );

  /* Push bytes from the outbound stream */
  void push( Reader& outbound_stream );

  /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
  std::optional<TCPSenderMessage> maybe_send();

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage send_empty_message() const;

  /* Receive an act on a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called. */
  void tick( const uint64_t ms_since_last_tick );

  /* Accessors for use in testing */
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
};
