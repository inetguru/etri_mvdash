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
#include "mvdash_server.h"
#include <ns3/core-module.h>
#include "ns3/tcp-socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("mvdashServer");

NS_OBJECT_ENSURE_REGISTERED (mvdashServer);

TypeId mvdashServer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::mvdashServer")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<mvdashServer> ()
    .AddAttribute ("LocalAddress",
                   "The local Address",
                   AddressValue (),
                   MakeAddressAccessor (&mvdashServer::m_localAddress),
                   MakeAddressChecker ())
    .AddTraceSource ("Rx", "A packet has been received",
                     MakeTraceSourceAccessor (&mvdashServer::m_rxTrace),
                     "ns3::Packet::AddressTracedCallback") 
    .AddTraceSource ("Tx", "A new packet is sent",
                     MakeTraceSourceAccessor (&mvdashServer::m_txTrace),
                     "ns3::Packet::AddressTracedCallback")
  ;
  return tid;
}

mvdashServer::mvdashServer ()
{
    NS_LOG_FUNCTION (this);
    m_socket = 0;
}

mvdashServer::~mvdashServer ()
{
    NS_LOG_FUNCTION (this);
}

void mvdashServer::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_connectedClients.clear ();

  // chain up
  Application::DoDispose ();
}

// Application Methods
void mvdashServer::StartApplication ()    // Called at time specified by Start
{
  NS_LOG_FUNCTION (this);
  // Create the socket if not already
  if (!m_socket)
    {
      TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);

      if (m_socket->Bind (m_localAddress) == -1)
        {
          NS_FATAL_ERROR ("Failed to bind socket");
        }
      m_socket->Listen ();      
    }

  m_socket->SetRecvCallback (MakeCallback (&mvdashServer::HandleRead, this));
  m_socket->SetSendCallback (MakeCallback (&mvdashServer::HandleSend, this));
  m_socket->SetAcceptCallback (
    MakeNullCallback<bool, Ptr<Socket>, const Address &> (),
    MakeCallback (&mvdashServer::HandleAccept, this));
  m_socket->SetCloseCallbacks (
    MakeCallback (&mvdashServer::HandlePeerClose, this),
    MakeCallback (&mvdashServer::HandlePeerError, this));
}

void mvdashServer::StopApplication ()      // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);
  while(!m_connectedClients.empty ()) //these are accepted sockets, close them
    {
      Ptr<Socket> acceptedSocket = m_connectedClients.front ();
      m_connectedClients.pop_front ();
      acceptedSocket->Close ();
    }
  if (m_socket) 
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
}

void mvdashServer::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Address from;
  Ptr<Packet> packet = socket->RecvFrom (from);
  //NS_LOG_INFO (packet->GetSize () << " bytes at time " << Simulator::Now ().As (Time::S));
  m_rxTrace(packet,from);

  if (ParseRequest (packet, from))
    SendResponse(socket, from);
}

bool mvdashServer::ParseRequest(Ptr<Packet> packet, const Address &from)
{
    NS_LOG_FUNCTION (this);

    uint8_t *buffer = new uint8_t [packet->GetSize ()];
    packet->CopyData (buffer, packet->GetSize ());
    st_mvdashRequest * pReq = (st_mvdashRequest *) buffer;
    int nRequests = packet->GetSize() / sizeof(st_mvdashRequest);

    if (m_requests[from].empty()) {
      m_unsentPacket[from] = 0;
      m_bytesSent[from] = 0;
    }

    for (int nR = 0; nR < nRequests; nR++) {
      m_requests[from].push(pReq[nR]);
      //NS_LOG_INFO("Viewpoint " << pReq[nR].viewpoint << "  time" << pReq[nR].timeIndex << " quality " << pReq[nR].qualityIndex);
    }
    //int64_t packetSizeToReturn = *(int64_t *)(buffer);
    //NS_LOG_INFO("Parse Request Called : " << packetSizeToReturn);

    return 1;
}

void mvdashServer::SendResponse(Ptr<Socket> socket, const Address &from)
{
    NS_LOG_FUNCTION (this);

    if (m_requests[from].empty()) return;
    st_mvdashRequest curReq = m_requests[from].front();

    while (m_bytesSent[from] < curReq.segmentSize) 
    {
      Ptr<Packet> packet;

      int64_t toSend = std::min ((int64_t)1446, curReq.segmentSize - m_bytesSent[from]);

      if (m_unsentPacket[from])
      {
          packet = m_unsentPacket[from];
          toSend = packet->GetSize ();
      }
      else
      {
          packet = Create<Packet> (toSend);
      }

      int actual = socket->Send (packet);
      if (actual == -1)
      {
          //NS_LOG_DEBUG ("[SERVER] Send Error - Caching for later attempt");
          m_unsentPacket[from] = packet;
          break;
      }
      else if (actual == toSend)
      {
          m_txTrace (packet, from);
          m_unsentPacket[from] = 0;
          m_bytesSent[from] += actual;
      }
      else if (actual > 0 && actual < toSend)
      {
          // A Linux socket (non-blocking, such as in DCE) may return
          // a quantity less than the packet size.  Split the packet
          // into two, trace the sent packet, save the unsent packet
          NS_LOG_DEBUG ("Packet size: " << packet->GetSize () << "; sent: " << actual << "; fragment saved: " << toSend - (unsigned) actual);
          Ptr<Packet> sent = packet->CreateFragment (0, actual);
          Ptr<Packet> unsent = packet->CreateFragment (actual, (toSend - (unsigned) actual));
          m_txTrace (sent, from);
          m_unsentPacket[from] = unsent;
          m_bytesSent[from] += actual;
          break;
      }
      else
      {
            NS_FATAL_ERROR ("[SERVER] Unexpected return value from m_socket->Send ()");
      }

      if (m_bytesSent[from] == curReq.segmentSize)
      {
/*        NS_LOG_INFO("Response to the Request <" << curReq.viewpoint
                    << "," << curReq.timeIndex
                    << "," << curReq.qualityIndex
                    << "," << curReq.segmentSize <<"> is completely Sent");*/
        m_bytesSent[from] = 0;
        m_requests[from].pop();
        if (m_requests[from].empty())
          break;
        curReq = m_requests[from].front();
      }
    }
}

void mvdashServer::HandleSend (Ptr<Socket> socket, unsigned int unused)
{
    NS_LOG_FUNCTION (this);
    
    Address from;
    socket->GetPeerName (from);
    if (m_unsentPacket[from]) {
      //NS_LOG_DEBUG("[SERVER] HANDLE SEND - THERE IS An UNSENT PACKET");
      SendResponse (socket, from);    
    }
}

void mvdashServer::HandleAccept (Ptr<Socket> socket, const Address& from)
{
    NS_LOG_FUNCTION (this << socket << from);

    m_unsentPacket[from] = 0;
    m_connectedClients.push_back (socket);
    socket->SetRecvCallback (MakeCallback (&mvdashServer::HandleRead, this));
    socket->SetSendCallback (MakeCallback (&mvdashServer::HandleSend, this));
}

void mvdashServer::HandlePeerClose (Ptr<Socket> socket)
{
    NS_LOG_FUNCTION (this << socket);

//  Address from;
//  socket->GetPeerName (from);
  for (std::list<Ptr<Socket>>::iterator it = m_connectedClients.begin (); it != m_connectedClients.end (); ++it)
    {
      if (*it == socket)
        {
          m_connectedClients.erase (it);
          // No more clients left in m_connectedClients, simulation is done.
          if (m_connectedClients.size () == 0)
            {
              Simulator::Stop ();
            }
          return;
        }
    }    
}
 
void mvdashServer::HandlePeerError (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
}

}// Namespace ns3