/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2021 ETRI (Electronic and Telecommunication Research Institute) KOREA
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef MVDASH_SERVER_H
#define MVDASH_SERVER_H

#include "ns3/application.h"
#include "ns3/ptr.h"
#include "ns3/address.h"
#include "ns3/inet-socket-address.h"
#include "ns3/traced-callback.h"
#include <map>
#include <queue>
#include "mvdash.h"

namespace ns3 {

class Address;
class Socket;
class Packet;

class mvdashServer : public Application 
{
public:
  static TypeId GetTypeId (void);
  mvdashServer ();
  virtual ~mvdashServer ();

protected:
  virtual void DoDispose (void);

private:
  // inherited from Application base class.
  virtual void StartApplication (void);    // Called at time specified by Start
  virtual void StopApplication (void);     // Called at time specified by Stop

  /**
   * \brief Handle a packet received by the application
   * \param socket the receiving socket
   */
  void HandleRead (Ptr<Socket> socket);
  /**
   * \brief Send more data as soon as some has been transmitted.
   * \param socket the sending socket.
   */
  void HandleSend (Ptr<Socket> socket, unsigned int unused);
  /**
   * \brief Handle an incoming connection
   * \param socket the incoming connection socket
   * \param from the address the connection is from
   */
  void HandleAccept (Ptr<Socket> socket, const Address& from);
  /**
   * \brief Handle an connection close
   * \param socket the connected socket
   */
  void HandlePeerClose (Ptr<Socket> socket);
  /**
   * \brief Handle an connection error
   * \param socket the connected socket
   */
  void HandlePeerError (Ptr<Socket> socket);

  bool ParseRequest(Ptr<Packet> packet, const Address &from);
  void SendResponse(Ptr<Socket> socket, const Address &from);

  Ptr<Socket>     m_socket;   //!< Listening socket
  Address m_localAddress;     //!< Local Address on which we listen for incoming packets.
  std::list<Ptr<Socket> > m_connectedClients; //!< the accepted sockets
  std::map <Address, std::queue <st_mvdashRequest> > m_requests;
  std::map <Address, Ptr<Packet>> m_unsentPacket; //!< Variable to cache unsent packet
  std::map <Address, int64_t> m_bytesSent;  

  /// Traced Callback: received packets, source address.
  TracedCallback<Ptr<const Packet>, const Address &> m_rxTrace;
  /// Traced Callback: sent packets
  TracedCallback<Ptr<const Packet>, const Address &> m_txTrace;
};

} // namespace ns3

#endif /* MVDASH_SERVER_H */