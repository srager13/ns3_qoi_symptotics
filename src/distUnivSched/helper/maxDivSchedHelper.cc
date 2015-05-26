/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "maxDivSchedHelper.h"
#include "ns3/maxDivSched.h"
#include "ns3/node-list.h"
#include "ns3/names.h"
#include "ns3/ipv4-list-routing.h"

namespace ns3 {

/* ... */

  MaxDivSchedHelper::MaxDivSchedHelper () :
    DistUnivSchedHelper()
  {
    m_agentFactory.SetTypeId ("ns3::dus::MaxDivSched");
  }

  MaxDivSchedHelper*
  MaxDivSchedHelper::Copy (void) const
  {
    return new MaxDivSchedHelper (*this);
  }

  Ptr<Ipv4RoutingProtocol> 
  MaxDivSchedHelper::Create (Ptr<Node> node) const
  {
    Ptr<dus::MaxDivSched> agent = m_agentFactory.Create<dus::MaxDivSched> ();
    node->AggregateObject (agent);
    return agent;
  }

  void 
  MaxDivSchedHelper::Set (std::string name, const AttributeValue &value)
  {
    m_agentFactory.Set (name, value);
  }

  void 
  DummyMethod( )
  {
    std::cout<<"This does nothing\n";
  }

}
