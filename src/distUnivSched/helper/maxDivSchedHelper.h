/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef __MAX_DIV_SCHED_HELPER_H__
#define __MAX_DIV_SCHED_HELPER_H__

#include "ns3/object-factory.h"
#include "ns3/node.h"
#include "ns3/node-container.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/distUnivSched-helper.h"

namespace ns3 {

/* ... */

  class MaxDivSchedHelper : public DistUnivSchedHelper
  {
    public:
      MaxDivSchedHelper();

      //virtual ~MaxDivSchedHelper();

      MaxDivSchedHelper* Copy (void) const;

      virtual Ptr<Ipv4RoutingProtocol> Create (Ptr<Node> node) const;

      void Set (std::string name, const AttributeValue &value);

      void DummyMethod();

    private:
      ObjectFactory m_agentFactory;
  };
}

#endif /* __MAX-DIV-SCHED_HELPER_H__ */

