/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
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
 *
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */
#include "topk-query-helper.h"
#include "ns3/topk-query-server.h"
#include "ns3/topk-query-client.h"
#include "ns3/uinteger.h"
#include "ns3/names.h"

namespace ns3 {

TopkQueryServerHelper::TopkQueryServerHelper (uint16_t port)
{
  m_factory.SetTypeId (TopkQueryServer::GetTypeId ());
  SetAttribute ("Port", UintegerValue (port));
}

void 
TopkQueryServerHelper::SetAttribute (
  std::string name, 
  const AttributeValue &value)
{
  m_factory.Set (name, value);
}

ApplicationContainer
TopkQueryServerHelper::Install (Ptr<Node> node) const
{
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
TopkQueryServerHelper::Install (std::string nodeName) const
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
TopkQueryServerHelper::Install (NodeContainer c) const
{
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      apps.Add (InstallPriv (*i));
    }

  return apps;
}

Ptr<Application>
TopkQueryServerHelper::InstallPriv (Ptr<Node> node) const
{
  Ptr<Application> app = m_factory.Create<TopkQueryServer> ();
  node->AddApplication (app);

  return app;
}

TopkQueryClientHelper::TopkQueryClientHelper (uint16_t port, uint32_t num_nodes)
{
  m_factory.SetTypeId (TopkQueryClient::GetTypeId ());
  SetAttribute ("RemotePort", UintegerValue (port));
  SetAttribute ("NumNodes", UintegerValue(num_nodes));
}

TopkQueryClientHelper::TopkQueryClientHelper (Address address, uint16_t port, uint32_t num_nodes)
{
  m_factory.SetTypeId (TopkQueryClient::GetTypeId ());
  SetAttribute ("RemoteAddress", AddressValue (address));
  SetAttribute ("RemotePort", UintegerValue (port));
  SetAttribute ("NumNodes", UintegerValue(num_nodes));
}

TopkQueryClientHelper::TopkQueryClientHelper (Ipv4Address address, uint16_t port, uint32_t num_nodes)
{
  m_factory.SetTypeId (TopkQueryClient::GetTypeId ());
  SetAttribute ("RemoteAddress", AddressValue (Address(address)));
  SetAttribute ("RemotePort", UintegerValue (port));
  SetAttribute ("NumNodes", UintegerValue(num_nodes));
}

TopkQueryClientHelper::TopkQueryClientHelper (Ipv6Address address, uint16_t port, uint32_t num_nodes)
{
  m_factory.SetTypeId (TopkQueryClient::GetTypeId ());
  SetAttribute ("RemoteAddress", AddressValue (Address(address)));
  SetAttribute ("RemotePort", UintegerValue (port));
  SetAttribute ("NumNodes", UintegerValue(num_nodes));
}

void 
TopkQueryClientHelper::SetAttribute (
  std::string name, 
  const AttributeValue &value)
{
  m_factory.Set (name, value);
}

ApplicationContainer
TopkQueryClientHelper::Install (Ptr<Node> node) const
{
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
TopkQueryClientHelper::Install (std::string nodeName) const
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
TopkQueryClientHelper::Install (NodeContainer c) const
{
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      apps.Add (InstallPriv (*i));
    }

  return apps;
}

Ptr<Application>
TopkQueryClientHelper::InstallPriv (Ptr<Node> node) const
{
  Ptr<Application> app = m_factory.Create<TopkQueryClient> ();
  node->AddApplication (app);

  return app;
}

} // namespace ns3
