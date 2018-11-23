#include "ns3/lte-helper.h"
#include "ns3/epc-helper.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"

namespace ns3
{
class MobileApplicationHelper
{
    NodeContainer &m_enbNodes;
    NodeContainer &m_ueNodes;
    u_int16_t m_numberOfNodes;
    Ipv4InterfaceContainer ueIpIface;

  public:
    MobileApplicationHelper(NodeContainer &enbNodes, NodeContainer &ueNodes, u_int16_t numberOfNodes);
    void SetupMobilityModule(double distance);
    void SetupDevices(Ptr<LteHelper> &lteHelper, Ptr<PointToPointEpcHelper> &epcHelper, Ipv4StaticRoutingHelper ipv4RoutingHelper);
    void SetupApplications(ApplicationContainer &serverApps, ApplicationContainer &clientApps, Ptr<Node> &remoteHost, Ipv4Address &remoteHostAddr, int16_t ulPort, int16_t dlPort, int16_t otherPort, double interPacketInterval);
};

MobileApplicationHelper::MobileApplicationHelper(NodeContainer &enbNodes, NodeContainer &ueNodes, u_int16_t numberOfNodes) : m_enbNodes(enbNodes), m_ueNodes(ueNodes), m_numberOfNodes(numberOfNodes)
{
}

void MobileApplicationHelper::SetupMobilityModule(double distance)
{
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (uint16_t i = 0; i < m_numberOfNodes; i++)
    {
        positionAlloc->Add(Vector(distance * i, 100, 100));
    }
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(m_enbNodes);
    mobility.Install(m_ueNodes);
}

void MobileApplicationHelper::SetupDevices(Ptr<LteHelper> &lteHelper, Ptr<PointToPointEpcHelper> &epcHelper, Ipv4StaticRoutingHelper ipv4RoutingHelper)
{
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(m_enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(m_ueNodes);
    // Install the IP stack on the UEs
    InternetStackHelper internet;
    internet.Install(m_ueNodes);
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));
    // Assign IP address to UEs, and install applications
    for (uint32_t u = 0; u < m_ueNodes.GetN(); ++u)
    {
        Ptr<Node> ueNode = m_ueNodes.Get(u);
        // Set the default gateway for the UE
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNode->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }
    // Attach one UE per eNodeB
    for (uint16_t i = 0; i < m_numberOfNodes; i++)
    {
        lteHelper->Attach(ueLteDevs.Get(i), enbLteDevs.Get(i));
        // side effect: the default EPS bearer will be activated
    }
}

void MobileApplicationHelper::SetupApplications(ApplicationContainer &serverApps, ApplicationContainer &clientApps, Ptr<Node> &remoteHost, Ipv4Address &remoteHostAddr, int16_t ulPort, int16_t dlPort, int16_t otherPort, double interPacketInterval)
{
    for (uint32_t u = 0; u < m_ueNodes.GetN(); ++u)
    {
        ++ulPort;
        ++otherPort;
        PacketSinkHelper dlPacketSinkHelper("ns3::UdpSocketFactory",
                                            InetSocketAddress(Ipv4Address::GetAny(), dlPort));
        PacketSinkHelper ulPacketSinkHelper("ns3::UdpSocketFactory",
                                            InetSocketAddress(Ipv4Address::GetAny(), ulPort));
        PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory",
                                          InetSocketAddress(Ipv4Address::GetAny(), otherPort));
        serverApps.Add(dlPacketSinkHelper.Install(m_ueNodes.Get(u)));
        serverApps.Add(ulPacketSinkHelper.Install(remoteHost));
        serverApps.Add(packetSinkHelper.Install(m_ueNodes.Get(u)));
        UdpClientHelper dlClient(ueIpIface.GetAddress(u), dlPort);
        dlClient.SetAttribute("Interval", TimeValue(MilliSeconds(interPacketInterval)));
        dlClient.SetAttribute("MaxPackets", UintegerValue(1000000));
        UdpClientHelper ulClient(remoteHostAddr, ulPort);
        ulClient.SetAttribute("Interval", TimeValue(MilliSeconds(interPacketInterval)));
        ulClient.SetAttribute("MaxPackets", UintegerValue(1000000));
        UdpClientHelper client(ueIpIface.GetAddress(u), otherPort);
        client.SetAttribute("Interval", TimeValue(MilliSeconds(interPacketInterval)));
        client.SetAttribute("MaxPackets", UintegerValue(1000000));
        clientApps.Add(dlClient.Install(remoteHost));
        clientApps.Add(ulClient.Install(m_ueNodes.Get(u)));
        if (u + 1 < m_ueNodes.GetN())
        {
            clientApps.Add(client.Install(m_ueNodes.Get(u + 1)));
        }
        else
        {
            clientApps.Add(client.Install(m_ueNodes.Get(0)));
        }
    }
}

} // namespace ns3