/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "etri_mvdash-helper.h"
#include "ns3/etri_mvdash-server.h"
#include "ns3/etri_mvdash-client.h"
#include "ns3/uinteger.h"
#include "ns3/names.h"

namespace ns3 {

etri_mvdashServerHelper::etri_mvdashServerHelper ()
{
  m_factory.SetTypeId (etri_mvdashServer::GetTypeId ());
}

etri_mvdashServerHelper::etri_mvdashServerHelper (uint16_t port, uint16_t bUseQuic)
{
  m_factory.SetTypeId (etri_mvdashServer::GetTypeId ());
  SetAttribute ("Port", UintegerValue (port));
  SetAttribute ("UseQuic", UintegerValue (bUseQuic));  
}

void
etri_mvdashServerHelper::SetAttribute (
  std::string name,
  const AttributeValue &value)
{
  m_factory.Set (name, value);
}

ApplicationContainer
etri_mvdashServerHelper::Install (Ptr<Node> node) const
{
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
etri_mvdashServerHelper::Install (std::string nodeName) const
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
etri_mvdashServerHelper::Install (NodeContainer c) const
{
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      apps.Add (InstallPriv (*i));
    }

  return apps;
}

Ptr<Application>
etri_mvdashServerHelper::InstallPriv (Ptr<Node> node) const
{
  Ptr<Application> app = m_factory.Create<etri_mvdashServer> ();
  node->AddApplication (app);

  return app;
}

etri_mvdashClientHelper::etri_mvdashClientHelper ()
{
  m_factory.SetTypeId (etri_mvdashClient::GetTypeId ());
}

etri_mvdashClientHelper::etri_mvdashClientHelper (Address address, uint16_t port, uint16_t bUseQuic)
{
  m_factory.SetTypeId (etri_mvdashClient::GetTypeId ());
  SetAttribute ("RemoteAddress", AddressValue (address));
  SetAttribute ("RemotePort", UintegerValue (port));
  SetAttribute ("UseQuic", UintegerValue (bUseQuic));  
}

etri_mvdashClientHelper::etri_mvdashClientHelper (Ipv4Address address, uint16_t port, uint16_t bUseQuic)
{
  m_factory.SetTypeId (etri_mvdashClient::GetTypeId ());
  SetAttribute ("RemoteAddress", AddressValue (Address(address)));
  SetAttribute ("RemotePort", UintegerValue (port));
  SetAttribute ("UseQuic", UintegerValue (bUseQuic));  
}

etri_mvdashClientHelper::etri_mvdashClientHelper (Ipv6Address address, uint16_t port, uint16_t bUseQuic)
{
  m_factory.SetTypeId (etri_mvdashClient::GetTypeId ());
  SetAttribute ("RemoteAddress", AddressValue (Address(address)));
  SetAttribute ("RemotePort", UintegerValue (port));
  SetAttribute ("UseQuic", UintegerValue (bUseQuic));  
}

void
etri_mvdashClientHelper::SetAttribute (std::string name, const AttributeValue &value)
{
  m_factory.Set (name, value);
}

ApplicationContainer
etri_mvdashClientHelper::Install (std::vector <std::pair <Ptr<Node>, std::string> > clients) const
{
  ApplicationContainer apps;
  for (uint i = 0; i < clients.size (); i++)
    {
      apps.Add (InstallPriv (clients.at (i).first, clients.at (i).second, i));
    }

  return apps;
}

Ptr<Application>
etri_mvdashClientHelper::InstallPriv (Ptr<Node> node, std::string algo, uint16_t clientId) const
{
  Ptr<Application> app = m_factory.Create<etri_mvdashClient> ();
  app->GetObject<etri_mvdashClient> ()->SetAttribute ("ClientId", UintegerValue (clientId));
  app->GetObject<etri_mvdashClient> ()->Initialise (algo, clientId);
  node->AddApplication (app);
  return app;
}


}

