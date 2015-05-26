/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef __DISTUNIVSCHED_HELPER_H__
#define __DISTUNIVSCHED_HELPER_H__

#include "ns3/object-factory.h"
#include "ns3/node.h"
#include "ns3/node-container.h"
#include "ns3/ipv4-routing-helper.h"


namespace ns3 
{

/* ... */

class DistUnivSchedHelper : public Ipv4RoutingHelper
{
  public:
    DistUnivSchedHelper();

    DistUnivSchedHelper* Copy (void) const;

    virtual Ptr<Ipv4RoutingProtocol> Create (Ptr<Node> node) const;

    void Set (std::string name, const AttributeValue &value);

  private:
    ObjectFactory m_agentFactory;
};

}
#endif /* __DISTUNIVSCHED_HELPER_H__ */

