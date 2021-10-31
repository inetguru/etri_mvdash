/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef ETRI_MVDASH_HELPER_H
#define ETRI_MVDASH_HELPER_H

#include <stdint.h>
#include "ns3/application-container.h"
#include "ns3/node-container.h"
#include "ns3/object-factory.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"

namespace ns3 {

/**
 * \ingroup etri_mvdash
 * \brief Create an ETRI Multiview DASH server application.
 */
class etri_mvdashServerHelper
{
public:
  /**
   * Create etri_mvdashServerHelper which will make life easier for people trying
   * to set up simulations with etri_mvdashServer application.
   *
   */
  etri_mvdashServerHelper ();
  /**
   * Create etri_mvdashServerHelper which will make life easier for people trying
   * to set up simulations with etri_mvdashServer application.
   *
   * \param port The port the server will wait on for incoming packets
   */
  etri_mvdashServerHelper (uint16_t port, uint16_t bUseQuic);

  /**
   * Record an attribute to be set in each Application after it is is created.
   *
   * \param name the name of the attribute to set
   * \param value the value of the attribute to set
   */
  void SetAttribute (std::string name, const AttributeValue &value);

  /**
   * Create an etri_mvdashServerApplication on the specified Node.
   *
   * \param node The node on which to create the Application.  The node is
   *             specified by a Ptr<Node>.
   *
   * \returns An ApplicationContainer holding the Application created,
   */
  ApplicationContainer Install (Ptr<Node> node) const;

  /**
   * Create an etri_mvdashServerApplication on specified node
   *
   * \param nodeName The node on which to create the application.  The node
   *                 is specified by a node name previously registered with
   *                 the Object Name Service.
   *
   * \returns An ApplicationContainer holding the Application created.
   */
  ApplicationContainer Install (std::string nodeName) const;

  /**
   * \param c The nodes on which to create the Applications.  The nodes
   *          are specified by a NodeContainer.
   *
   * Create one tcp stream server application on each of the Nodes in the
   * NodeContainer.
   *
   * \returns The applications created, one Application per Node in the 
   *          NodeContainer.
   */
  ApplicationContainer Install (NodeContainer c) const;

private:
  /**
   * Install an ns3::etri_mvdashServer on the node configured with all the
   * attributes set with SetAttribute.
   *
   * \param node The node on which an etri_mvdashServer will be installed.
   * \returns Ptr to the application installed.
   */
  Ptr<Application> InstallPriv (Ptr<Node> node) const;

  ObjectFactory m_factory; //!< Object factory.
};

/**
 * \ingroup etri_mvdash
 * \brief Create an ETRI Multiview DASH client application.
 */
class etri_mvdashClientHelper
{
public:
  /**
   * Create etri_mvdashClientHelper which will make life easier for people trying
   * to set up simulations with etri_mvdashServer application.
   *
   */
  etri_mvdashClientHelper ();

  /**
   * Create etri_mvdashClientHelper which will make life easier for people trying
   * to set up simulations with echos.
   *
   * \param ip The IP address of the remote tcp stream server
   * \param port The port number of the remote tcp stream server
   */
  etri_mvdashClientHelper (Address ip, uint16_t port, uint16_t bUseQuic);
  /**
   * Create etri_mvdashClientHelper which will make life easier for people trying
   * to set up simulations with echos.
   *
   * \param ip The IPv4 address of the remote tcp stream server
   * \param port The port number of the remote tcp stream server
   */
  etri_mvdashClientHelper (Ipv4Address ip, uint16_t port, uint16_t bUseQuic);
  /**
   * Create etri_mvdashClientHelper which will make life easier for people trying
   * to set up simulations with echos.
   *
   * \param ip The IPv6 address of the remote tcp stream server
   * \param port The port number of the remote tcp stream server
   */
  etri_mvdashClientHelper (Ipv6Address ip, uint16_t port, uint16_t bUseQuic);

  /**
   * Record an attribute to be set in each Application after it is is created.
   *
   * \param name the name of the attribute to set
   * \param value the value of the attribute to set
   */
  void SetAttribute (std::string name, const AttributeValue &value);

  /**
   * \param clients the nodes with the name of the adaptation algorithm to be used
   *
   * Create one tcp stream client application on each of the input nodes and
   * instantiate an adaptation algorithm on each of the tcp stream client according
   * to the given string.
   *
   * \returns the applications created, one application per input node.
   */
  ApplicationContainer Install (std::vector <std::pair <Ptr<Node>, std::string> > clients) const;

private:
  /**
   * Install an ns3::etri_mvdashClient on the node configured with all the
   * attributes set with SetAttribute.
   *
   * \param node The node on which an etri_mvdashClient will be installed.
   * \param algo A string containing the name of the adaptation algorithm to be used on this client
   * \param clientId distinguish this client object from other parallel running clients, for logging purposes
   * \param simulationId distinguish this simulation from other subsequently started simulations, for logging purposes
   * \returns Ptr to the application installed.
   */
  Ptr<Application> InstallPriv (Ptr<Node> node, std::string algo, uint16_t clientId) const;
  ObjectFactory m_factory; //!< Object factory.
};

}

#endif /* ETRI_MVDASH_HELPER_H */
