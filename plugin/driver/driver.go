package driver

import (
	"fmt"
	"log/slog"
	"net"

	nw_sdk "github.com/docker/go-plugins-helpers/network"
)

func (n *NetlinkPairDriver) GetCapabilities() (*nw_sdk.CapabilitiesResponse, error) {
	return &nw_sdk.CapabilitiesResponse{
		Scope:             "local",
		ConnectivityScope: "local",
	}, nil
}

func (n *NetlinkPairDriver) CreateNetwork(req *nw_sdk.CreateNetworkRequest) error {
	n.Lock()
	defer n.Unlock()

	slog.Debug("CreateNetwork", "networkID", req.NetworkID)

	_, ok := n.Networks[req.NetworkID]
	if ok {
		err := fmt.Errorf("duplicate network id: %s", req.NetworkID)
		slog.Error("CreateNetwork failed", "networkID", req.NetworkID, "error", err)
		return err
	}

	networkName := req.NetworkID[:16]
	metadata, ok := req.Options["com.docker.network.generic"].(map[string]any)
	if ok {
		name, ok := metadata["net-name"].(string)
		if ok && name != "" {
			networkName = name
		}
	}

	n.Networks[req.NetworkID] = &NetkitNetwork{
		NetworkID:   req.NetworkID,
		NetworkName: networkName,
		IPAMv4:      nil,
		Pair:        nil,
	}
	if len(req.IPv4Data) > 0 {
		n.Networks[req.NetworkID].IPAMv4 = req.IPv4Data[0]
		slog.Info("CreateNetwork", "networkID", req.NetworkID, "networkName", networkName, "subnet", req.IPv4Data[0].Pool, "gateway", req.IPv4Data[0].Gateway)
	} else {
		slog.Info("CreateNetwork", "networkID", req.NetworkID, "networkName", networkName)
	}
	return nil
}

func (n *NetlinkPairDriver) CreateEndpoint(req *nw_sdk.CreateEndpointRequest) (*nw_sdk.CreateEndpointResponse, error) {
	n.Lock()
	defer n.Unlock()

	slog.Debug("CreateEndpoint", "networkID", req.NetworkID, "endpointID", req.EndpointID)

	network, ok := n.Networks[req.NetworkID]
	if !ok {
		err := fmt.Errorf("invalid network id: %s", req.NetworkID)
		slog.Error("CreateEndpoint failed", "networkID", req.NetworkID, "endpointID", req.EndpointID, "error", err)
		return nil, err
	}

	if network.Pair == nil {
		ifNameA := fmt.Sprintf("nk-%s%s-a", req.NetworkID[:4], req.EndpointID[:4])
		ifNameB := fmt.Sprintf("nk-%s%s-b", req.NetworkID[:4], req.EndpointID[:4])

		slog.Debug("CreateEndpoint: creating netkit pair", "networkName", network.NetworkName, "ifNameA", ifNameA, "ifNameB", ifNameB)
		err := createNetkitPair(ifNameA, ifNameB)
		if err != nil {
			slog.Error("CreateEndpoint: failed to create netkit pair", "networkName", network.NetworkName, "ifNameA", ifNameA, "ifNameB", ifNameB, "error", err)
			return nil, fmt.Errorf("failed to create netkit pair (%s, %s): %w", ifNameA, ifNameB, err)
		}

		ep := &NetkitEndpoint{
			EndpointID:    req.EndpointID,
			InterfaceName: ifNameA,
		}
		if req.Interface != nil {
			ep.IPAddress = req.Interface.Address
			ep.MACAddress = req.Interface.MacAddress
		}
		network.Pair = &NetkitPair{SideA: ep}

		slog.Info("CreateEndpoint: SideA created", "networkName", network.NetworkName, "endpointID", req.EndpointID, "interface", ifNameA, "ip", ep.IPAddress, "mac", ep.MACAddress)
		return &nw_sdk.CreateEndpointResponse{}, nil

	} else if network.Pair.SideA != nil && network.Pair.SideB == nil {
		ifNameB := fmt.Sprintf("nk-%s%s-b", req.NetworkID[:4], network.Pair.SideA.EndpointID[:4])
		ep := &NetkitEndpoint{
			EndpointID:    req.EndpointID,
			InterfaceName: ifNameB,
		}
		if req.Interface != nil {
			ep.IPAddress = req.Interface.Address
			ep.MACAddress = req.Interface.MacAddress
		}
		network.Pair.SideB = ep

		slog.Info("CreateEndpoint: SideB created", "networkName", network.NetworkName, "endpointID", req.EndpointID, "interface", ifNameB, "ip", ep.IPAddress, "mac", ep.MACAddress)
		return &nw_sdk.CreateEndpointResponse{}, nil

	} else {
		err := fmt.Errorf("network '%s' already has 2 endpoints", network.NetworkName)
		slog.Error("CreateEndpoint failed", "networkName", network.NetworkName, "endpointID", req.EndpointID, "error", err)
		return nil, err
	}
}

func (n *NetlinkPairDriver) Join(req *nw_sdk.JoinRequest) (*nw_sdk.JoinResponse, error) {
	n.Lock()
	defer n.Unlock()

	slog.Debug("Join", "networkID", req.NetworkID, "endpointID", req.EndpointID, "sandboxKey", req.SandboxKey)

	network, ok := n.Networks[req.NetworkID]
	if !ok {
		err := fmt.Errorf("invalid network id: %s", req.NetworkID)
		slog.Error("Join failed", "networkID", req.NetworkID, "endpointID", req.EndpointID, "error", err)
		return nil, err
	}

	gateway := ""
	if network.IPAMv4 != nil {
		ip, _, err := net.ParseCIDR(network.IPAMv4.Gateway)
		if err != nil {
			slog.Warn("Join: failed to parse gateway CIDR, using empty gateway", "networkName", network.NetworkName, "gateway", network.IPAMv4.Gateway, "error", err)
		} else {
			gateway = ip.String()
		}
	}

	if network.Pair == nil {
		err := fmt.Errorf("network '%s' not initialised", network.NetworkName)
		slog.Error("Join failed", "networkName", network.NetworkName, "endpointID", req.EndpointID, "error", err)
		return nil, err
	}

	ep := network.Pair.findEndpoint(req.EndpointID)
	if ep == nil {
		err := fmt.Errorf("endpoint '%s' is not on the network '%s'", req.EndpointID, network.NetworkID)
		slog.Error("Join failed", "networkName", network.NetworkName, "endpointID", req.EndpointID, "error", err)
		return nil, err
	}

	dstPrefix := sanitiseInterfaceName(network.NetworkName)

	switch ep.NamespacePath {
	case "":
		slog.Debug("Join: moving link to container netns", "networkName", network.NetworkName, "endpointID", req.EndpointID, "interface", ep.InterfaceName, "namespace", req.SandboxKey)
		err := moveToContainerNetns(ep.InterfaceName, req.SandboxKey)
		if err != nil {
			slog.Error("Join: failed to move link to namespace", "networkName", network.NetworkName, "endpointID", req.EndpointID, "interface", ep.InterfaceName, "namespace", req.SandboxKey, "error", err)
			return nil, fmt.Errorf("error moving link to namespace: %w", err)
		}
		ep.NamespacePath = req.SandboxKey

		if ep.IPAddress != "" && ep.MACAddress != "" {
			slog.Debug("Join: setting addr in netns", "networkName", network.NetworkName, "interface", ep.InterfaceName, "ip", ep.IPAddress, "mac", ep.MACAddress)
			err := setAddrInNetns(ep.InterfaceName, ep.NamespacePath, ep.IPAddress, ep.MACAddress)
			if err != nil {
				slog.Error("Join: failed to set addr on link", "networkName", network.NetworkName, "interface", ep.InterfaceName, "ip", ep.IPAddress, "mac", ep.MACAddress, "error", err)
				return nil, fmt.Errorf("error setting addr on link '%s': %w", ep.InterfaceName, err)
			}
		}

		slog.Info("Join: endpoint joined", "networkName", network.NetworkName, "endpointID", req.EndpointID, "interface", ep.InterfaceName, "dstPrefix", dstPrefix, "gateway", gateway)
		return &nw_sdk.JoinResponse{
			InterfaceName: nw_sdk.InterfaceName{
				SrcName:   ep.InterfaceName,
				DstPrefix: dstPrefix,
			},
			Gateway:               gateway,
			StaticRoutes:          []*nw_sdk.StaticRoute{},
			DisableGatewayService: false,
		}, nil

	case req.SandboxKey:
		slog.Info("Join: endpoint already in sandbox (idempotent)", "networkName", network.NetworkName, "endpointID", req.EndpointID, "interface", ep.InterfaceName)
		return &nw_sdk.JoinResponse{
			InterfaceName: nw_sdk.InterfaceName{
				SrcName:   ep.InterfaceName,
				DstPrefix: dstPrefix,
			},
			Gateway:               gateway,
			StaticRoutes:          []*nw_sdk.StaticRoute{},
			DisableGatewayService: false,
		}, nil

	default:
		err := fmt.Errorf("endpoint on '%s' is already connected to container", network.NetworkName)
		slog.Error("Join failed", "networkName", network.NetworkName, "endpointID", req.EndpointID, "currentNamespace", ep.NamespacePath, "requestedNamespace", req.SandboxKey, "error", err)
		return nil, err
	}
}

func (n *NetlinkPairDriver) Leave(req *nw_sdk.LeaveRequest) error {
	n.Lock()
	defer n.Unlock()

	slog.Debug("Leave", "networkID", req.NetworkID, "endpointID", req.EndpointID)

	network, ok := n.Networks[req.NetworkID]
	if !ok {
		err := fmt.Errorf("invalid network id: %s", req.NetworkID)
		slog.Error("Leave failed", "networkID", req.NetworkID, "endpointID", req.EndpointID, "error", err)
		return err
	}
	if network.Pair == nil {
		err := fmt.Errorf("network '%s' not initialised", network.NetworkName)
		slog.Error("Leave failed", "networkName", network.NetworkName, "endpointID", req.EndpointID, "error", err)
		return err
	}
	ep := network.Pair.findEndpoint(req.EndpointID)
	if ep == nil {
		err := fmt.Errorf("endpoint '%s' is not on the network '%s'", req.EndpointID, network.NetworkID)
		slog.Error("Leave failed", "networkName", network.NetworkName, "endpointID", req.EndpointID, "error", err)
		return err
	}
	if ep.NamespacePath == "" {
		slog.Debug("Leave: endpoint not in any namespace, nothing to do", "networkName", network.NetworkName, "endpointID", req.EndpointID)
		return nil
	}

	slog.Debug("Leave: removing addrs from link", "networkName", network.NetworkName, "endpointID", req.EndpointID, "interface", ep.InterfaceName, "namespace", ep.NamespacePath)
	err := leaveInNetns(ep.InterfaceName, ep.NamespacePath)
	if err != nil {
		slog.Error("Leave: failed to remove addrs from link", "networkName", network.NetworkName, "endpointID", req.EndpointID, "interface", ep.InterfaceName, "error", err)
		return fmt.Errorf("unable to remove addr from link '%s': %w", ep.InterfaceName, err)
	}

	ep.NamespacePath = ""
	slog.Info("Leave: endpoint left", "networkName", network.NetworkName, "endpointID", req.EndpointID, "interface", ep.InterfaceName)
	return nil
}

func (n *NetlinkPairDriver) DeleteEndpoint(req *nw_sdk.DeleteEndpointRequest) error {
	n.Lock()
	defer n.Unlock()

	slog.Debug("DeleteEndpoint", "networkID", req.NetworkID, "endpointID", req.EndpointID)

	network, ok := n.Networks[req.NetworkID]
	if !ok {
		err := fmt.Errorf("invalid network id: %s", req.NetworkID)
		slog.Error("DeleteEndpoint failed", "networkID", req.NetworkID, "endpointID", req.EndpointID, "error", err)
		return err
	}
	if network.Pair == nil {
		err := fmt.Errorf("network '%s' not initialised", network.NetworkName)
		slog.Error("DeleteEndpoint failed", "networkName", network.NetworkName, "endpointID", req.EndpointID, "error", err)
		return err
	}
	ep := network.Pair.findEndpoint(req.EndpointID)
	if ep == nil {
		err := fmt.Errorf("endpoint '%s' is not on the network '%s'", req.EndpointID, network.NetworkID)
		slog.Error("DeleteEndpoint failed", "networkName", network.NetworkName, "endpointID", req.EndpointID, "error", err)
		return err
	}

	if ep.NamespacePath != "" {
		slog.Debug("DeleteEndpoint: deleting link from netns", "networkName", network.NetworkName, "endpointID", req.EndpointID, "interface", ep.InterfaceName, "namespace", ep.NamespacePath)
		err := delLinkInNetns(ep.InterfaceName, ep.NamespacePath)
		if err != nil {
			slog.Error("DeleteEndpoint: failed to delete link", "networkName", network.NetworkName, "endpointID", req.EndpointID, "interface", ep.InterfaceName, "namespace", ep.NamespacePath, "error", err)
			return fmt.Errorf("unable to delete link '%s': %w", ep.InterfaceName, err)
		}
		ep.NamespacePath = ""
	}

	if network.Pair.SideA == ep {
		network.Pair.SideA = nil
	} else {
		network.Pair.SideB = nil
	}
	if network.Pair.SideA == nil && network.Pair.SideB == nil {
		network.Pair = nil
	}

	slog.Info("DeleteEndpoint: endpoint deleted", "networkName", network.NetworkName, "endpointID", req.EndpointID, "interface", ep.InterfaceName)
	return nil
}

func (n *NetlinkPairDriver) DeleteNetwork(req *nw_sdk.DeleteNetworkRequest) error {
	n.Lock()
	defer n.Unlock()

	slog.Debug("DeleteNetwork", "networkID", req.NetworkID)

	network, ok := n.Networks[req.NetworkID]
	if !ok {
		err := fmt.Errorf("invalid network id: %s", req.NetworkID)
		slog.Error("DeleteNetwork failed", "networkID", req.NetworkID, "error", err)
		return err
	}

	if network.Pair != nil {
		if network.Pair.SideA != nil {
			if network.Pair.SideA.NamespacePath != "" {
				slog.Debug("DeleteNetwork: deleting SideA from netns", "networkName", network.NetworkName, "interface", network.Pair.SideA.InterfaceName, "namespace", network.Pair.SideA.NamespacePath)
				err := delLinkInNetns(network.Pair.SideA.InterfaceName, network.Pair.SideA.NamespacePath)
				if err != nil {
					slog.Error("DeleteNetwork: failed to delete SideA from netns", "networkName", network.NetworkName, "interface", network.Pair.SideA.InterfaceName, "error", err)
					return fmt.Errorf("unable to delete link '%s': %w", network.Pair.SideA.InterfaceName, err)
				}
				network.Pair.SideA.NamespacePath = ""
			} else {
				slog.Debug("DeleteNetwork: deleting SideA from host", "networkName", network.NetworkName, "interface", network.Pair.SideA.InterfaceName)
				if err := delLinkInHost(network.Pair.SideA.InterfaceName); err != nil {
					slog.Warn("DeleteNetwork: failed to delete SideA from host namespace (interface leaked)", "networkName", network.NetworkName, "interface", network.Pair.SideA.InterfaceName, "error", err)
				}
			}
		}
		if network.Pair.SideB != nil {
			if network.Pair.SideB.NamespacePath != "" {
				slog.Debug("DeleteNetwork: deleting SideB from netns", "networkName", network.NetworkName, "interface", network.Pair.SideB.InterfaceName, "namespace", network.Pair.SideB.NamespacePath)
				err := delLinkInNetns(network.Pair.SideB.InterfaceName, network.Pair.SideB.NamespacePath)
				if err != nil {
					slog.Error("DeleteNetwork: failed to delete SideB from netns", "networkName", network.NetworkName, "interface", network.Pair.SideB.InterfaceName, "error", err)
					return fmt.Errorf("unable to delete link '%s': %w", network.Pair.SideB.InterfaceName, err)
				}
				network.Pair.SideB.NamespacePath = ""
			} else {
				slog.Debug("DeleteNetwork: deleting SideB from host", "networkName", network.NetworkName, "interface", network.Pair.SideB.InterfaceName)
				if err := delLinkInHost(network.Pair.SideB.InterfaceName); err != nil {
					slog.Warn("DeleteNetwork: failed to delete SideB from host namespace (interface leaked)", "networkName", network.NetworkName, "interface", network.Pair.SideB.InterfaceName, "error", err)
				}
			}
		}
		network.Pair = nil
	}

	delete(n.Networks, req.NetworkID)
	slog.Info("DeleteNetwork: network deleted", "networkID", req.NetworkID, "networkName", network.NetworkName)
	return nil
}

func (n *NetlinkPairDriver) EndpointInfo(req *nw_sdk.InfoRequest) (*nw_sdk.InfoResponse, error) {
	n.Lock()
	defer n.Unlock()

	network, ok := n.Networks[req.NetworkID]
	if !ok {
		return nil, fmt.Errorf("invalid network id: %s", req.NetworkID)
	}
	if network.Pair == nil {
		return nil, fmt.Errorf("network '%s' doesn't have any endpoints", network.NetworkName)
	}
	ep := network.Pair.findEndpoint(req.EndpointID)
	if ep == nil {
		return nil, fmt.Errorf("endpoint '%s' is not on the network '%s'", req.EndpointID, network.NetworkID)
	}
	return &nw_sdk.InfoResponse{
		Value: map[string]string{
			"ID":            ep.EndpointID,
			"InterfaceName": ep.InterfaceName,
			"NamespacePath": ep.NamespacePath,
			"IPAddress":     ep.IPAddress,
			"MACAddress":    ep.MACAddress,
			"Gateway":       ep.Gateway,
		},
	}, nil
}

// All the methods below are stubbed as they're not used in non-swarm networks

func (n *NetlinkPairDriver) AllocateNetwork(*nw_sdk.AllocateNetworkRequest) (*nw_sdk.AllocateNetworkResponse, error) {
	return &nw_sdk.AllocateNetworkResponse{}, nil
}

func (n *NetlinkPairDriver) FreeNetwork(*nw_sdk.FreeNetworkRequest) error {
	return nil
}

func (n *NetlinkPairDriver) ProgramExternalConnectivity(*nw_sdk.ProgramExternalConnectivityRequest) error {
	return nil
}

func (n *NetlinkPairDriver) RevokeExternalConnectivity(*nw_sdk.RevokeExternalConnectivityRequest) error {
	return nil
}

func (n *NetlinkPairDriver) DiscoverNew(*nw_sdk.DiscoveryNotification) error {
	return nil
}

func (n *NetlinkPairDriver) DiscoverDelete(*nw_sdk.DiscoveryNotification) error {
	return nil
}
