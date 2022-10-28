/*
 * main.cc
 *
 *  Created on: 04.05.2020
 *      Author: Kevin Küchler
 */

#include "ns3/config.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/energy-module.h"
#include "ns3/wifi-net-device.h"
#include "ns3/fifo-queue-disc.h"
#include "ns3/traffic-control-layer.h"
#include "ns3/wifi-radio-energy-model-helper.h"

#include "ns3/csma-helper.h"

#include "GameState.h"
#include "EEBTPHeader.h"
#include "EEBTProtocol.h"
#include "EEBTProtocolHelper.h"

#include "SimpleBroadcastHeader.h"
#include "SimpleBroadcastProtocol.h"
#include "SimpleBroadcastProtocolHelper.h"

#include "ns3/ptr.h"
#include "float.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("BroadcastTest");

int iMax = 1, skipTo = 0;
int wifi_stations = 100;
bool eebtp = true;
bool enable_logging = false;
bool enable_logging_verbose = false;
bool enable_pcap = false;
bool use_rts_cts = false;
bool udp_test_brdcst = false;
bool use_linear_energy_model = false;
uint64_t rndSeed = 1001;
uint16_t maxHopCount = 3;
uint64_t gameID = 0;
std::string c_cpm = "CYCLE_TEST_ASYNC";
CYCLE_PREV_METHOD cpm = CYCLE_TEST_ASYNC;

double sizeX = 501.0;
double sizeY = 501.0;

void initSimpleBroadcast(Ptr<NetDevice> initiator, uint16_t hopCount, uint32_t seqNo)
{
	Ptr<SimpleBroadcastProtocol> sbp = initiator->GetObject<SimpleBroadcastProtocol>();
	if (sbp != 0)
		sbp->Send(hopCount, seqNo);
}

void initEEBroadcast(Ptr<NetDevice> initiator)
{
	Ptr<EEBTProtocol> eebtp = initiator->GetObject<EEBTProtocol>();
	if (eebtp != 0)
	{
		uint64_t gid = 0;
		Mac48Address sender;
		sender.ConvertFrom(initiator->GetAddress());
		uint8_t addr[6];
		sender.CopyTo(addr);
		uint arrSize = (sizeof(addr) / sizeof(addr[0]));
		for (uint i = arrSize - 1; i < arrSize; i--)
		{
			gid <<= 8;
			gid |= addr[i];
		}

		Ptr<GameState> gs = eebtp->initGameState(gid);
		eebtp->Send(gs, FRAME_TYPE::NEIGHBOR_DISCOVERY, 20.0);
	}
}

/*
 * Simulation Setup
 */
void SetupSimpleBroadcast(NetDeviceContainer wifiStations, SimpleBroadcastProtocolHelper sbph)
{
	sbph.Install(wifiStations);
}

void SetupEEBroadcast(NetDeviceContainer wifiStations, EEBTProtocolHelper eebtph)
{
	Ptr<CycleWatchDog> cwd = Create<CycleWatchDog>();
	cwd->setNetDeviceContainer(wifiStations);
	eebtph.setCycleWatchDogCallback(cwd);
	eebtph.setCyclePreventionMethod(cpm);
	eebtph.Install(wifiStations);
}

std::pair<NetDeviceContainer, EnergySourceContainer> SetupSimulation()
{
	//Create nodes
	NodeContainer nodes;
	nodes.Create(wifi_stations);

	//Create mobility model
	MobilityHelper mobility;
	mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX", DoubleValue(0.0), "MinY", DoubleValue(0.0),
								  "DeltaX", DoubleValue(sizeX), "DeltaY", DoubleValue(sizeY));
	//Set a constant position model for the initiator
	mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
	mobility.Install(nodes);

	EnergySourceContainer esc;
	Ptr<UniformRandomVariable> random = CreateObject<UniformRandomVariable>();
	for (NodeContainer::Iterator i = nodes.Begin(); i != nodes.End(); i++)
	{
		//Create one energy source for every node
		BasicEnergySourceHelper energySource;
		energySource.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(10000000.0)); // => J = Ws = V*As
		esc.Add(energySource.Install(*i));

		//Set random position
		Vector3D pos = Vector3D(random->GetInteger(0, sizeX - 1), random->GetInteger(0, sizeY - 1), 0);
		(*i)->GetObject<MobilityModel>()->SetPosition(pos);
	}

	/*
	 * PHY Layer
	 */
	YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
	YansWifiChannelHelper channel = YansWifiChannelHelper::Default();

	channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
	phy.SetChannel(channel.Create());
	phy.SetPreambleDetectionModel("ns3::CustomThresholdPreambleDetectionModel");

	//Enable PCAP-Logging
	if (enable_pcap)
		phy.EnablePcapAll("brdcst-test");

	WifiHelper wifi;
	if (enable_logging_verbose)
		wifi.EnableLogComponents();
	wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager");
	wifi.SetStandard(WIFI_PHY_STANDARD_80211a);

	/*
	 * Data Link Layer (MAC)
	 */
	WifiMacHelper mac;
	mac.SetType("ns3::AdhocWifiMac");

	//Install WiFi on all nodes
	NetDeviceContainer wifiStations = wifi.Install(phy, mac, nodes);

	for (NetDeviceContainer::Iterator i = wifiStations.Begin(); i != wifiStations.End(); i++)
	{
		//Create a traffic control layer object for every node
		Ptr<TrafficControlLayer> tcl = Create<TrafficControlLayer>();
		tcl->SetRootQueueDiscOnDevice((*i), Create<FifoQueueDisc>());
		(*i)->GetNode()->AggregateObject(tcl);
	}

	/*
	 * Radio Energy Model
	 * Calculates the used energy and maintains the energy source
	 */
	WifiRadioEnergyModelHelper radioEnergyModel;
	if (use_linear_energy_model)
		radioEnergyModel.SetTxCurrentModel("ns3::LinearWifiTxCurrentModel", "Voltage", DoubleValue(3.0), "IdleCurrent", DoubleValue(0.028),
										   "Eta", DoubleValue(0.75));
	else
		radioEnergyModel.SetTxCurrentModel("ns3::CustomWifiTxCurrentModel", "Voltage", DoubleValue(3.0), "IdleCurrent", DoubleValue(0.028),
										   "Eta", DoubleValue(0.75), "Chip", StringValue("MAX2831"));
	radioEnergyModel.Set("RxCurrentA", DoubleValue(0.062));
	radioEnergyModel.Set("IdleCurrentA", DoubleValue(0.028));
	radioEnergyModel.Install(wifiStations, esc);

	NS_LOG_UNCOND("Energy: " << esc.Get(0)->GetRemainingEnergy() << "J");
	if (eebtp)
	{
		EEBTProtocolHelper eebtph;
		cpm = CYCLE_TEST_ASYNC;
		if (c_cpm == "1" || c_cpm == "MUTEX")
		{
			cpm = MUTEX;
			NS_LOG_UNCOND("Using cycle prevention method MUTEX with" << (!use_rts_cts ? "out" : "") << " RTS/CTS");
		}
		else if (c_cpm == "2" || c_cpm == "PATH_TO_SRC")
		{
			cpm = PATH_TO_SRC;
			NS_LOG_UNCOND("Using cycle prevention method PATH_TO_SRC with" << (!use_rts_cts ? "out" : "") << " RTS/CTS");
		}
		else if (!(c_cpm == "0" || c_cpm == "CYCLE_TEST_ASYNC"))
		{
			NS_LOG_UNCOND("Unknown cycle prevention method: " << c_cpm << ". Using CYCLE_TEST_ASYNC with" << (!use_rts_cts ? "out" : "") << " RTS/CTS");
		}
		else
			NS_LOG_UNCOND("Using cycle prevention method CYCLE_TEST_ASYNC with" << (!use_rts_cts ? "out" : "") << " RTS/CTS");
		SetupEEBroadcast(wifiStations, eebtph);
	}
	else
	{
		SimpleBroadcastProtocolHelper sbph;
		SetupSimpleBroadcast(wifiStations, sbph);
	}

	return std::pair<NetDeviceContainer, EnergySourceContainer>(wifiStations, esc);
}

/*
 * Helper methods
 */
int getTreeDepth(Mac48Address node, std::map<Mac48Address, std::vector<Mac48Address>> tree, std::map<Mac48Address, int> *nodeDepth, int depth)
{
	int d = depth;
	(*nodeDepth)[node] = d;

	for (Mac48Address n : tree[node])
	{
		int x = getTreeDepth(n, tree, nodeDepth, depth + 1);
		if (x > d)
			d = x;
	}

	return d;
}

/*
 * Print method to print simulation results
 */
void PrintResult(NetDeviceContainer wifiStations, EnergySourceContainer esc)
{
	int treeDepth = 0;
	uint32_t unconNodes = 0;
	uint32_t cycles = 0, cyclesLasted = 0;
	Time timeToBuildInitiator, maxTimeToBuild;
	double totalTxPower = 0.0, totalEnergy = 0.0;
	double totalConstructionEnergy = 0.0, totalApplicationEnergy = 0.0;

	double energyPerFrameRecv[8]{0, 0, 0, 0, 0, 0, 0, 0};
	double energyPerFrameSent[8]{0, 0, 0, 0, 0, 0, 0, 0};
	uint32_t dataPerFrameRecv[8]{0, 0, 0, 0, 0, 0, 0, 0};
	uint32_t dataPerFrameSent[8]{0, 0, 0, 0, 0, 0, 0, 0};
	uint32_t packetsPerFrameRecv[8]{0, 0, 0, 0, 0, 0, 0, 0};
	uint32_t packetsPerFrameSent[8]{0, 0, 0, 0, 0, 0, 0, 0};

	int maxPackets;
	std::map<int, int> packetLostPerDepth;
	std::map<int, int> maxPacketsPerDepth;

	if (eebtp)
	{
		switch (cpm)
		{
		case MUTEX:
			NS_LOG_INFO("CYCLE PREVENTION METHOD: MUTEX");
			break;
		case PATH_TO_SRC:
			NS_LOG_INFO("CYCLE PREVENTION METHOD: PATH_TO_SRC");
			break;
		case CYCLE_TEST_ASYNC:
		default:
			NS_LOG_INFO("CYCLE PREVENTION METHOD: CYCLE_TEST_ASYNC");
			break;
		}
		NS_LOG_INFO("MAX_UNCHANGED_ROUNDS: " << EEBTProtocol::MAX_UNCHANGED_ROUNDS << "\n");

		Mac48Address source;
		std::vector<Mac48Address> nodeList;
		std::vector<Mac48Address> cycledNodes;

		std::map<Mac48Address, int> nodeDepth;
		std::map<Mac48Address, int> packetLossPerNode;
		std::map<Mac48Address, std::vector<Mac48Address>> tree;
		for (NetDeviceContainer::Iterator i = wifiStations.Begin(); i != wifiStations.End(); i++)
		{
			Ptr<WifiNetDevice> dev = DynamicCast<WifiNetDevice>(*i);
			Ptr<Node> node = dev->GetNode();
			Ptr<EEBTProtocol> proto = dev->GetObject<EEBTProtocol>();
			if (proto != 0)
			{
				Ptr<GameState> gs = proto->getGameState(gameID);
				Ptr<EEBTPPacketManager> pm = proto->getPacketManager();
				Ptr<CycleWatchDog> cwd = proto->getCycleWatchDog();

				if (i == wifiStations.Begin())
				{
					source = gs->getMyAddress();
					maxPackets = proto->maxPackets;
				}
				nodeList.push_back(gs->getMyAddress());

				if (gs->getParent() != 0)
				{
					NS_LOG_INFO("The parent of node [" << gs->getMyAddress() << "] is [" << gs->getParent()->getAddress() << "]");
					tree[gs->getParent()->getAddress()].push_back(gs->getMyAddress());
				}
				else
				{
					unconNodes++;
					NS_LOG_INFO("The parent of node [" << Mac48Address::ConvertFrom(dev->GetAddress()) << "] is [" << Mac48Address::GetBroadcast() << "]");
				}

				totalConstructionEnergy += proto->getEnergyForConstruction(gameID);
				totalApplicationEnergy += proto->getEnergyByFrameType(gameID, APPLICATION_DATA);

				if (gs->getHighestTxPower() > -FLT_MAX)
					totalTxPower += DbmToW(gs->getHighestTxPower());

				if (gs->isInitiator())
					timeToBuildInitiator = gs->getTimeFinished();
				if (gs->getTimeFinished() > maxTimeToBuild)
					maxTimeToBuild = gs->getTimeFinished();

				std::vector<Ptr<CycleInfo>> ci = cwd->getCycles(gameID, node->GetId());
				for (std::vector<Ptr<CycleInfo>>::iterator it = ci.begin(); it != ci.end(); it++)
				{
					Ptr<CycleInfo> c = *it;
					if (c->getEndTime().GetNanoSeconds() == 0 && (*c->getNodes().begin()) == proto->GetDevice())
					{
						NS_LOG_INFO("Cycle started at " << c->getStartTime() << " and ended at " << c->getEndTime());
						cyclesLasted++;
						unconNodes += c->getNodes().size();

						for (Ptr<NetDevice> dev : c->getNodes())
							cycledNodes.push_back(Mac48Address::ConvertFrom(dev->GetAddress()));
					}
				}
				cycles = cwd->getUniqueCycles();

				for (uint8_t i = 0; i < 8; i++)
				{
					energyPerFrameRecv[i] += pm->getEnergyByRecvFrame(gameID, i);
					energyPerFrameSent[i] += pm->getEnergyBySentFrame(gameID, i);
					dataPerFrameRecv[i] += pm->getDataRecvByFrame(gameID, i);
					dataPerFrameSent[i] += pm->getDataSentByFrame(gameID, i);
					packetsPerFrameRecv[i] += pm->getFrameTypeRecv(gameID, i);
					packetsPerFrameSent[i] += pm->getFrameTypeSent(gameID, i);
				}

				packetLossPerNode[gs->getMyAddress()] = proto->maxPackets - gs->getApplicationDataHandler()->getPacketCount();
			}
			else
				NS_LOG_INFO("Node " << node->GetId() << " has no EEBTProtocol installed!");
		}

		//Subtract on since the source node is seen as unconnected
		unconNodes--;

		//Remove cycled node from tree
		for (Mac48Address addr : cycledNodes)
			tree[addr].clear();

		//Calculate tree depth
		treeDepth = getTreeDepth(source, tree, &nodeDepth, 0);

		//Calculate packetloss on every tree depth level and the total amount of to expect packets on these levels
		for (Mac48Address node : nodeList)
		{
			NS_LOG_UNCOND("Node " << node << " is " << nodeDepth[node] << " deep");
			packetLostPerDepth[nodeDepth[node]] += packetLossPerNode[node];
			maxPacketsPerDepth[nodeDepth[node]]++;
		}
	} //end if(eebtp)
	else
	{
		totalTxPower = DbmToW(23.0) * wifi_stations;
		for (NetDeviceContainer::Iterator i = wifiStations.Begin(); i != wifiStations.End(); i++)
		{
			Ptr<WifiNetDevice> dev = DynamicCast<WifiNetDevice>(*i);
			Ptr<Node> node = dev->GetNode();
			Ptr<SimpleBroadcastProtocol> proto = dev->GetObject<SimpleBroadcastProtocol>();

			if (proto != 0)
			{
				totalEnergy += proto->getSentEnergy();
				totalApplicationEnergy += proto->getSentEnergy();
				energyPerFrameRecv[7] += proto->getRecvEnergy();
				energyPerFrameSent[7] += proto->getSentEnergy();
				dataPerFrameRecv[7] += proto->getRecvData();
				dataPerFrameSent[7] += proto->getSentData();
				packetsPerFrameRecv[7] += proto->getRecvPackets();
				packetsPerFrameSent[7] += proto->getSentPackets();
				NS_LOG_UNCOND("Node " << node->GetId() << " has sent " << proto->getSentPackets() << " packets and used " << proto->getSentEnergy() << "J");
			}
			else
				NS_LOG_INFO("Node " << node->GetId() << " has no SimpleBroadcastProtocol installed!");
		}

		treeDepth = 1;
		maxPackets = 1000;
		maxPacketsPerDepth[1] = wifi_stations;
		packetLostPerDepth[1] = (maxPacketsPerDepth[1] * maxPackets) - packetsPerFrameSent[7];
	}

	for (EnergySourceContainer::Iterator i = esc.Begin(); i != esc.End(); i++)
		totalEnergy += ((*i)->GetInitialEnergy() - (*i)->GetRemainingEnergy());

	NS_LOG_INFO("Total energy consumed: " << totalEnergy << "J");
	NS_LOG_INFO("Total configured TX power: " << WToDbm(totalTxPower) << "dBm / " << totalTxPower << "W");

	std::stringstream str;
	for (int i = 1; i <= treeDepth; i++)
	{
		NS_LOG_INFO("Packets[" << i << "]: " << maxPacketsPerDepth[i] << "; loss: " << packetLostPerDepth[i]);
		str << ((double)packetLostPerDepth[i] / (double)(maxPacketsPerDepth[i] * maxPackets));
		str << ";";
	}
	std::string s = str.str();
	if (s.length() > 0)
		s.erase(s.end() - 1);

	NS_LOG_INFO("CODE: " << totalEnergy << ";" << totalConstructionEnergy << ";" << totalApplicationEnergy << ";" << totalTxPower << ";" << timeToBuildInitiator.GetNanoSeconds() << ";"
						 << maxTimeToBuild.GetNanoSeconds() << ";" << treeDepth << ";" << unconNodes << ";" << cycles << ";" << cyclesLasted << ";"
						 << energyPerFrameRecv[0] << ";" << energyPerFrameRecv[1] << ";" << energyPerFrameRecv[2] << ";" << energyPerFrameRecv[3] << ";" << energyPerFrameRecv[4] << ";" << energyPerFrameRecv[5] << ";" << energyPerFrameRecv[6] << ";" << energyPerFrameRecv[7] << ";"
						 << energyPerFrameSent[0] << ";" << energyPerFrameSent[1] << ";" << energyPerFrameSent[2] << ";" << energyPerFrameSent[3] << ";" << energyPerFrameSent[4] << ";" << energyPerFrameSent[5] << ";" << energyPerFrameSent[6] << ";" << energyPerFrameSent[7] << ";"
						 << dataPerFrameRecv[0] << ";" << dataPerFrameRecv[1] << ";" << dataPerFrameRecv[2] << ";" << dataPerFrameRecv[3] << ";" << dataPerFrameRecv[4] << ";" << dataPerFrameRecv[5] << ";" << dataPerFrameRecv[6] << ";" << dataPerFrameRecv[7] << ";"
						 << dataPerFrameSent[0] << ";" << dataPerFrameSent[1] << ";" << dataPerFrameSent[2] << ";" << dataPerFrameSent[3] << ";" << dataPerFrameSent[4] << ";" << dataPerFrameSent[5] << ";" << dataPerFrameSent[6] << ";" << dataPerFrameSent[7] << ";"
						 << packetsPerFrameRecv[0] << ";" << packetsPerFrameRecv[1] << ";" << packetsPerFrameRecv[2] << ";" << packetsPerFrameRecv[3] << ";" << packetsPerFrameRecv[4] << ";" << packetsPerFrameRecv[5] << ";" << packetsPerFrameRecv[6] << ";" << packetsPerFrameRecv[7] << ";"
						 << packetsPerFrameSent[0] << ";" << packetsPerFrameSent[1] << ";" << packetsPerFrameSent[2] << ";" << packetsPerFrameSent[3] << ";" << packetsPerFrameSent[4] << ";" << packetsPerFrameSent[5] << ";" << packetsPerFrameSent[6] << ";" << packetsPerFrameSent[7] << ";"
						 << s);

	if (eebtp)
	{
		for (NetDeviceContainer::Iterator i = wifiStations.Begin(); i != wifiStations.End(); i++)
		{
			Ptr<NetDevice> dev = *i;
			Ptr<Node> node = dev->GetNode();
			Ptr<EEBTProtocol> proto = dev->GetObject<EEBTProtocol>();
			if (proto != 0)
				NS_LOG_INFO(*proto);
		}
	}
}

void DoSimulation(NetDeviceContainer wifiStations)
{
	//Set time limit for simulation (20 seconds)
	Simulator::Stop(Seconds(20));

	Ptr<NetDevice> sourceNode = wifiStations.Get(0);

	if (eebtp)
		Simulator::ScheduleWithContext(sourceNode->GetNode()->GetId(), Seconds(0), &initEEBroadcast, sourceNode);
	else
	{
		for (int i = 0; i < 1000; i++)
			Simulator::ScheduleWithContext(sourceNode->GetNode()->GetId(), MilliSeconds(i * 10), &initSimpleBroadcast, sourceNode, maxHopCount, i);
	}

	//Start simulation
	Simulator::Run();
}

/*
 * Set the commandline arguments
 */
CommandLine ProcessCommandLineArgs()
{
	CommandLine cmd;
	cmd.AddValue("nWifi", "Number of WiFi stations", wifi_stations);
	cmd.AddValue("log", "If set to true, enable logging", enable_logging);
	cmd.AddValue("verbose", "If set to true, enable verbose logging", enable_logging_verbose);
	cmd.AddValue("tracing", "If set to true, enable PCAP-Tracing", enable_pcap);
	cmd.AddValue("hopCount", "The max hop count a broadcast message can travel", maxHopCount);
	cmd.AddValue("udpTestBrdcst", "Test simulation with an UDP broadcast", udp_test_brdcst);
	cmd.AddValue("eebtp", "Use the EEBT protocol", eebtp);
	cmd.AddValue("rtsCts", "Use RTS/CTS during the simulation", use_rts_cts);
	cmd.AddValue("rndSeed", "Seed for randomness", rndSeed);
	cmd.AddValue("iMax", "Number of simulations", iMax);
	cmd.AddValue("skipTo", "Number of simulations to skip", skipTo);
	cmd.AddValue("linearEnergyModel", "Run simulation with the LinearWifiTxCurrentModel instead of the CustomWifiTxCurrentModel", use_linear_energy_model);
	cmd.AddValue("cpm", "The cycle prevention method to use", c_cpm);
	cmd.AddValue("width", "Width (X) of the simulated area", sizeX);
	cmd.AddValue("height", "Height (Y) of the simulated area", sizeY);
	return cmd;
}

/*
 * main
 */

int main(int argc, char *argv[])
{
	//Pares incoming arguments
	ProcessCommandLineArgs().Parse(argc, argv);

	//Enabled logging
	if (enable_logging)
		LogComponentEnable("BroadcastTest", LOG_LEVEL_LOGIC);
	else if (enable_logging_verbose)
		LogComponentEnable("BroadcastTest", LOG_LEVEL_DEBUG);

	if (udp_test_brdcst && eebtp)
		NS_FATAL_ERROR("Can't run simulation with two different protocols!");
	if (use_rts_cts)
		Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(1));

	RngSeedManager::SetSeed(rndSeed);
	for (int i = 0; i < iMax; i++)
	{
		RngSeedManager::SetRun(i);

		std::pair<NetDeviceContainer, EnergySourceContainer> pair = SetupSimulation();

		if (skipTo == 0 || (skipTo > 0 && skipTo == i))
		{
			DoSimulation(pair.first);

			NS_LOG_INFO("<========== END OF SIMULATION ==========>");
			NS_LOG_INFO("SIMULATION SEED: " << rndSeed << " + " << i);
			NS_LOG_INFO("SIMULATION TIME: " << Simulator::Now());

			PrintResult(pair.first, pair.second);

			NS_LOG_INFO("<X=======================================X>");

			NS_LOG_INFO("\n\n\n\n");
		}

		Simulator::Destroy();

		if (skipTo > 0 && i >= skipTo)
			break;
	}

	return 0;
}
