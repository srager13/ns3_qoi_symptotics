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
#include "qoi-query-flood-helper.h"
#include "ns3/qoi-query-flood-server.h"
#include "ns3/qoi-query-flood-client.h"
#include "ns3/uinteger.h"
#include "ns3/names.h"

namespace ns3 {

QoiQueryFloodServerHelper::QoiQueryFloodServerHelper (uint16_t port, uint32_t num_nodes)
{
  m_factory.SetTypeId (QoiQueryFloodServer::GetTypeId ());
  SetAttribute ("Port", UintegerValue (port));
  SetAttribute ("NumNodes", UintegerValue(num_nodes));
}

void 
QoiQueryFloodServerHelper::SetAttribute (
  std::string name, 
  const AttributeValue &value)
{
  m_factory.Set (name, value);
}

ApplicationContainer
QoiQueryFloodServerHelper::Install (Ptr<Node> node) const
{
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
QoiQueryFloodServerHelper::Install (std::string nodeName) const
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
QoiQueryFloodServerHelper::Install (NodeContainer c) const
{
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      apps.Add (InstallPriv (*i));
    }

  return apps;
}

Ptr<Application>
QoiQueryFloodServerHelper::InstallPriv (Ptr<Node> node) const
{
  Ptr<Application> app = m_factory.Create<QoiQueryFloodServer> ();
  node->AddApplication (app);

  return app;
}

QoiQueryFloodClientHelper::QoiQueryFloodClientHelper (uint16_t port, uint32_t num_nodes)
{
  m_factory.SetTypeId (QoiQueryFloodClient::GetTypeId ());
  SetAttribute ("RemotePort", UintegerValue (port));
  SetAttribute ("NumNodes", UintegerValue(num_nodes));
}

QoiQueryFloodClientHelper::QoiQueryFloodClientHelper (Address address, uint16_t port, uint32_t num_nodes)
{
  m_factory.SetTypeId (QoiQueryFloodClient::GetTypeId ());
  SetAttribute ("RemoteAddress", AddressValue (address));
  SetAttribute ("RemotePort", UintegerValue (port));
  SetAttribute ("NumNodes", UintegerValue(num_nodes));
}

QoiQueryFloodClientHelper::QoiQueryFloodClientHelper (Ipv4Address address, uint16_t port, uint32_t num_nodes)
{
  m_factory.SetTypeId (QoiQueryFloodClient::GetTypeId ());
  SetAttribute ("RemoteAddress", AddressValue (Address(address)));
  SetAttribute ("RemotePort", UintegerValue (port));
  SetAttribute ("NumNodes", UintegerValue(num_nodes));
}

QoiQueryFloodClientHelper::QoiQueryFloodClientHelper (Ipv6Address address, uint16_t port, uint32_t num_nodes)
{
  m_factory.SetTypeId (QoiQueryFloodClient::GetTypeId ());
  SetAttribute ("RemoteAddress", AddressValue (Address(address)));
  SetAttribute ("RemotePort", UintegerValue (port));
  SetAttribute ("NumNodes", UintegerValue(num_nodes));
}

void 
QoiQueryFloodClientHelper::SetAttribute (
  std::string name, 
  const AttributeValue &value)
{
  m_factory.Set (name, value);
}

void
QoiQueryFloodClientHelper::SetFill (Ptr<Application> app, std::string fill)
{
  app->GetObject<QoiQueryFloodClient>()->SetFill (fill);
}

void
QoiQueryFloodClientHelper::SetFill (Ptr<Application> app, uint8_t fill, uint32_t dataLength)
{
  app->GetObject<QoiQueryFloodClient>()->SetFill (fill, dataLength);
}

void
QoiQueryFloodClientHelper::SetFill (Ptr<Application> app, uint8_t *fill, uint32_t fillLength, uint32_t dataLength)
{
  app->GetObject<QoiQueryFloodClient>()->SetFill (fill, fillLength, dataLength);
}

ApplicationContainer
QoiQueryFloodClientHelper::Install (Ptr<Node> node) const
{
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
QoiQueryFloodClientHelper::Install (std::string nodeName) const
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
QoiQueryFloodClientHelper::Install (NodeContainer c) const
{
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      apps.Add (InstallPriv (*i));
    }

  return apps;
}

Ptr<Application>
QoiQueryFloodClientHelper::InstallPriv (Ptr<Node> node) const
{
  Ptr<Application> app = m_factory.Create<QoiQueryFloodClient> ();
  node->AddApplication (app);

  return app;
}

} // namespace ns3
