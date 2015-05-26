/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "distUnivSched-helper.h"
#include "ns3/dus-routing-protocol.h"
#include "ns3/node-list.h"
#include "ns3/names.h"
#include "ns3/ipv4-list-routing.h"

namespace ns3 
{
//NS_OBJECT_ENSURE_REGISTERED (DistUnivSchedHelper);

DistUnivSchedHelper::DistUnivSchedHelper () :
  Ipv4RoutingHelper ()
{
  m_agentFactory.SetTypeId ("ns3::dus::RoutingProtocol");
} 

DistUnivSchedHelper*
DistUnivSchedHelper::Copy (void) const
{
  return new DistUnivSchedHelper (*this);
}

Ptr<Ipv4RoutingProtocol> 
DistUnivSchedHelper::Create (Ptr<Node> node) const
{
  Ptr<dus::RoutingProtocol> agent = m_agentFactory.Create<dus::RoutingProtocol> ();
  node->AggregateObject (agent);
  return agent;
}

void 
DistUnivSchedHelper::Set (std::string name, const AttributeValue &value)
{
  m_agentFactory.Set (name, value);
}


}


