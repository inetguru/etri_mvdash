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

#include "mvdash-helper.h"
#include "ns3/mvdash_server.h"
#include "ns3/mvdash_client.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/names.h"

namespace ns3 {

mvdashServerHelper::mvdashServerHelper ()
{
  m_factory.SetTypeId (mvdashServer::GetTypeId ());
}

mvdashServerHelper::mvdashServerHelper (Address address, uint16_t bUseQuic)
{
  m_factory.SetTypeId (mvdashServer::GetTypeId ());
  SetAttribute ("LocalAddress", AddressValue (address));
//  SetAttribute ("Port", UintegerValue (port));
//  SetAttribute ("UseQuic", UintegerValue (bUseQuic));  
}

mvdashServerHelper::mvdashServerHelper (Ipv4Address ip, uint16_t port, uint16_t bUseQuic)
{
  m_factory.SetTypeId (mvdashServer::GetTypeId ());
  SetAttribute ("LocalAddress", AddressValue (InetSocketAddress(ip, port)));
//  SetAttribute ("Port", UintegerValue (port));
//  SetAttribute ("UseQuic", UintegerValue (bUseQuic));  
}

void
mvdashServerHelper::SetAttribute (
  std::string name,
  const AttributeValue &value)
{
  m_factory.Set (name, value);
}

ApplicationContainer
mvdashServerHelper::Install (Ptr<Node> node) const
{
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
mvdashServerHelper::Install (std::string nodeName) const
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
mvdashServerHelper::Install (NodeContainer c) const
{
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      apps.Add (InstallPriv (*i));
    }

  return apps;
}

Ptr<Application>
mvdashServerHelper::InstallPriv (Ptr<Node> node) const
{
  Ptr<Application> app = m_factory.Create<mvdashServer> ();
  node->AddApplication (app);

  return app;
}

mvdashClientHelper::mvdashClientHelper ()
{
  m_factory.SetTypeId (mvdashClient::GetTypeId ());
}

mvdashClientHelper::mvdashClientHelper (Address serverAddress, uint16_t bUseQuic)
{
  m_factory.SetTypeId (mvdashClient::GetTypeId ());
  SetAttribute ("ServerAddress", AddressValue (serverAddress));
//  SetAttribute ("UseQuic", UintegerValue (bUseQuic));  
}

mvdashClientHelper::mvdashClientHelper (Ipv4Address serverIP, uint16_t serverPort, uint16_t bUseQuic)
{
  m_factory.SetTypeId (mvdashClient::GetTypeId ());
  SetAttribute ("ServerAddress", AddressValue (InetSocketAddress(serverIP, serverPort)));
//  SetAttribute ("UseQuic", UintegerValue (bUseQuic));  
}

void
mvdashClientHelper::SetAttribute (std::string name, const AttributeValue &value)
{
  m_factory.Set (name, value);
}

ApplicationContainer
mvdashClientHelper::Install (NodeContainer c, std::string algo) const
{
  ApplicationContainer apps;
  int j=0;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i, ++j)
    {
      apps.Add (InstallPriv (*i, j));
    }

  return apps;
}

Ptr<Application>
mvdashClientHelper::InstallPriv (Ptr<Node> node, uint16_t clientId) const
{
  Ptr<Application> app = m_factory.Create<mvdashClient> ();
  app->GetObject<mvdashClient> ()->SetAttribute ("ClientId", UintegerValue (clientId));
  //app->GetObject<mvdashClient> ()->Initialise (algo, clientId);
  node->AddApplication (app);
  return app;
}

}
