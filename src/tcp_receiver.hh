#pragma once

#include "reassembler.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

class TCPReceiver
{
protected:
  enum TCORecriverState
  {
    LISTEN,
    SYN_RCVD,
    ESTB_CONN,
    CLOSED,
  };

  TCORecriverState m_state {LISTEN};

  // recv ack
  Wrap32 m_recv_zero_point {0};

public:
  /*
   * The TCPReceiver receives TCPSenderMessages, inserting their payload into the Reassembler
   * at the correct stream index.
   */
  void receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream );

  /* The TCPReceiver sends TCPReceiverMessages back to the TCPSender. */
  TCPReceiverMessage send( const Writer& inbound_stream ) const;
};
