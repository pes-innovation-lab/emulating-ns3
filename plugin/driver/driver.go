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

	ifPrefix := req.NetworkID[:16]
	metadata, ok := req.Options["com.docker.network.generic"].(map[string]any)
	if ok {
		name, ok := metadata["if-prefix"].(string)
		if ok && name != "" {
			ifPrefix = name
		}
	}

	n.Networks[req.NetworkID] = &NetkitNetwork{
		NetworkID:       req.NetworkID,
		InterfacePrefix: ifPrefix,
		IPAMv4:          nil,
		Pair:            nil,
	}
	if len(req.IPv4Data) > 0 {
		n.Networks[req.NetworkID].IPAMv4 = req.IPv4Data[0]
		slog.Info("CreateNetwork", "networkID", req.NetworkID, "ifPrefix", ifPrefix, "subnet", req.IPv4Data[0].Pool, "gateway", req.IPv4Data[0].Gateway)
	} else {
		slog.Info("CreateNetwork", "networkID", req.NetworkID, "ifPrefix", ifPrefix)
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

	makeEndpoint := func() *NetkitEndpoint {
		ep := &NetkitEndpoint{EndpointID: req.EndpointID}
		if req.Interface != nil {
			ep.IPAddress = req.Interface.Address
			ep.MACAddress = req.Interface.MacAddress
		}
		return ep
	}

	if network.Pair == nil || network.Pair.SideA == nil {
		ep := makeEndpoint()
		if network.Pair == nil {
			network.Pair = &NetkitPair{SideA: ep}
		} else {
			network.Pair.SideA = ep
		}
		slog.Info("CreateEndpoint: SideA created", "ifPrefix", network.InterfacePrefix, "endpointID", req.EndpointID, "ip", ep.IPAddress, "mac", ep.MACAddress)
		return &nw_sdk.CreateEndpointResponse{}, nil

	} else if network.Pair.SideB == nil {
		ep := makeEndpoint()
		if network.Pair.SideA.InterfaceName != "" {
			// pair already created by SideA's Join; pre-assign peer interface name
			ep.InterfaceName = fmt.Sprintf("nk-%s%s-b", req.NetworkID[:4], network.Pair.SideA.EndpointID[:4])
		}
		network.Pair.SideB = ep
		slog.Info("CreateEndpoint: SideB created", "ifPrefix", network.InterfacePrefix, "endpointID", req.EndpointID, "interface", ep.InterfaceName, "ip", ep.IPAddress, "mac", ep.MACAddress)
		return &nw_sdk.CreateEndpointResponse{}, nil

	} else {
		err := fmt.Errorf("network '%s' already has 2 endpoints", network.InterfacePrefix)
		slog.Error("CreateEndpoint failed", "ifPrefix", network.InterfacePrefix, "endpointID", req.EndpointID, "error", err)
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
			slog.Warn("Join: failed to parse gateway CIDR, using empty gateway", "ifPrefix", network.InterfacePrefix, "gateway", network.IPAMv4.Gateway, "error", err)
		} else {
			gateway = ip.String()
		}
	}

	if network.Pair == nil {
		err := fmt.Errorf("network '%s' not initialised", network.InterfacePrefix)
		slog.Error("Join failed", "ifPrefix", network.InterfacePrefix, "endpointID", req.EndpointID, "error", err)
		return nil, err
	}

	ep := network.Pair.findEndpoint(req.EndpointID)
	if ep == nil {
		err := fmt.Errorf("endpoint '%s' is not on the network '%s'", req.EndpointID, network.NetworkID)
		slog.Error("Join failed", "ifPrefix", network.InterfacePrefix, "endpointID", req.EndpointID, "error", err)
		return nil, err
	}

	dstPrefix := sanitiseInterfaceName(network.InterfacePrefix)

	if ep.Joined {
		err := fmt.Errorf("endpoint on '%s' is already connected to container", network.InterfacePrefix)
		slog.Error("Join failed", "ifPrefix", network.InterfacePrefix, "endpointID", req.EndpointID, "error", err)
		return nil, err
	}

	needsPair := ep.InterfaceName == ""
	if !needsPair {
		exists, err := linkExistsInHost(ep.InterfaceName)
		if err != nil {
			slog.Error("Join: failed to probe interface", "ifPrefix", network.InterfacePrefix, "interface", ep.InterfaceName, "error", err)
			return nil, fmt.Errorf("failed to probe interface '%s': %w", ep.InterfaceName, err)
		}
		if !exists {
			slog.Debug("Join: interface gone, recreating pair", "ifPrefix", network.InterfacePrefix, "interface", ep.InterfaceName)
			needsPair = true
		}
	}

	if needsPair {
		if network.Pair.SideA == nil {
			err := fmt.Errorf("cannot create pair on '%s': SideA not registered", network.InterfacePrefix)
			slog.Error("Join failed", "ifPrefix", network.InterfacePrefix, "endpointID", req.EndpointID, "error", err)
			return nil, err
		}
		ifNameA := fmt.Sprintf("nk-%s%s-a", req.NetworkID[:4], network.Pair.SideA.EndpointID[:4])
		ifNameB := fmt.Sprintf("nk-%s%s-b", req.NetworkID[:4], network.Pair.SideA.EndpointID[:4])
		slog.Debug("Join: creating netkit pair", "ifPrefix", network.InterfacePrefix, "ifNameA", ifNameA, "ifNameB", ifNameB)
		if err := createNetkitPair(ifNameA, ifNameB); err != nil {
			slog.Error("Join: failed to create netkit pair", "ifPrefix", network.InterfacePrefix, "error", err)
			return nil, fmt.Errorf("failed to create netkit pair: %w", err)
		}
		network.Pair.SideA.InterfaceName = ifNameA
		if network.Pair.SideB != nil {
			network.Pair.SideB.InterfaceName = ifNameB
		}
	}

	// Set promiscuous mode while the link is still in the host namespace.
	slog.Debug("Join: setting promiscuous mode", "ifPrefix", network.InterfacePrefix, "interface", ep.InterfaceName)
	if err := setLinkPromiscuous(ep.InterfaceName); err != nil {
		slog.Error("Join: failed to set promiscuous", "ifPrefix", network.InterfacePrefix, "interface", ep.InterfaceName, "error", err)
		return nil, fmt.Errorf("error setting promiscuous on link '%s': %w", ep.InterfaceName, err)
	}
	ep.Joined = true

	slog.Info("Join: endpoint ready", "ifPrefix", network.InterfacePrefix, "endpointID", req.EndpointID, "srcName", ep.InterfaceName, "dstPrefix", dstPrefix, "gateway", gateway)
	return &nw_sdk.JoinResponse{
		InterfaceName: nw_sdk.InterfaceName{
			SrcName:   ep.InterfaceName,
			DstPrefix: dstPrefix,
		},
		Gateway:               gateway,
		StaticRoutes:          []*nw_sdk.StaticRoute{},
		DisableGatewayService: false,
	}, nil
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
		err := fmt.Errorf("network '%s' not initialised", network.InterfacePrefix)
		slog.Error("Leave failed", "ifPrefix", network.InterfacePrefix, "endpointID", req.EndpointID, "error", err)
		return err
	}
	ep := network.Pair.findEndpoint(req.EndpointID)
	if ep == nil {
		err := fmt.Errorf("endpoint '%s' is not on the network '%s'", req.EndpointID, network.NetworkID)
		slog.Error("Leave failed", "ifPrefix", network.InterfacePrefix, "endpointID", req.EndpointID, "error", err)
		return err
	}

	ep.Joined = false
	if peer := network.Pair.findPeer(ep); peer != nil {
		peer.Joined = false
	}
	slog.Info("Leave: endpoint left", "ifPrefix", network.InterfacePrefix, "endpointID", req.EndpointID, "interface", ep.InterfaceName)
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
		err := fmt.Errorf("network '%s' not initialised", network.InterfacePrefix)
		slog.Error("DeleteEndpoint failed", "ifPrefix", network.InterfacePrefix, "endpointID", req.EndpointID, "error", err)
		return err
	}
	ep := network.Pair.findEndpoint(req.EndpointID)
	if ep == nil {
		err := fmt.Errorf("endpoint '%s' is not on the network '%s'", req.EndpointID, network.NetworkID)
		slog.Error("DeleteEndpoint failed", "ifPrefix", network.InterfacePrefix, "endpointID", req.EndpointID, "error", err)
		return err
	}

	if ep.InterfaceName != "" {
		// attempt to delete the host-side interface
		slog.Debug("DeleteEndpoint: deleting link from host namespace", "ifPrefix", network.InterfacePrefix, "endpointID", req.EndpointID, "interface", ep.InterfaceName)
		if err := delLinkInHost(ep.InterfaceName); err != nil {
			slog.Error("DeleteEndpoint: failed to delete link", "ifPrefix", network.InterfacePrefix, "endpointID", req.EndpointID, "interface", ep.InterfaceName, "error", err)
			return fmt.Errorf("unable to delete link '%s': %w", ep.InterfaceName, err)
		}
	}

	if network.Pair.SideA == ep {
		network.Pair.SideA = nil
	} else {
		network.Pair.SideB = nil
	}
	if network.Pair.SideA == nil && network.Pair.SideB == nil {
		network.Pair = nil
	}

	slog.Info("DeleteEndpoint: endpoint deleted", "ifPrefix", network.InterfacePrefix, "endpointID", req.EndpointID, "interface", ep.InterfaceName)
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
		// Safety net: endpoints should have been cleaned up via DeleteEndpoint
		if network.Pair.SideA != nil {
			slog.Warn("DeleteNetwork: SideA endpoint not cleaned up, forcing delete", "ifPrefix", network.InterfacePrefix, "interface", network.Pair.SideA.InterfaceName)
			if err := delLinkInHost(network.Pair.SideA.InterfaceName); err != nil {
				slog.Warn("DeleteNetwork: failed to delete SideA (likely in container, will clean up on sandbox teardown)", "ifPrefix", network.InterfacePrefix, "interface", network.Pair.SideA.InterfaceName, "error", err)
			}
		}
		if network.Pair.SideB != nil {
			slog.Warn("DeleteNetwork: SideB endpoint not cleaned up, forcing delete", "ifPrefix", network.InterfacePrefix, "interface", network.Pair.SideB.InterfaceName)
			if err := delLinkInHost(network.Pair.SideB.InterfaceName); err != nil {
				slog.Warn("DeleteNetwork: failed to delete SideB (likely in container, will clean up on sandbox teardown)", "ifPrefix", network.InterfacePrefix, "interface", network.Pair.SideB.InterfaceName, "error", err)
			}
		}
		network.Pair = nil
	}

	delete(n.Networks, req.NetworkID)
	slog.Info("DeleteNetwork: network deleted", "networkID", req.NetworkID, "ifPrefix", network.InterfacePrefix)
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
		return nil, fmt.Errorf("network '%s' doesn't have any endpoints", network.InterfacePrefix)
	}
	ep := network.Pair.findEndpoint(req.EndpointID)
	if ep == nil {
		return nil, fmt.Errorf("endpoint '%s' is not on the network '%s'", req.EndpointID, network.NetworkID)
	}
	return &nw_sdk.InfoResponse{
		Value: map[string]string{
			"ID":            ep.EndpointID,
			"InterfaceName": ep.InterfaceName,
			"Joined":        fmt.Sprintf("%v", ep.Joined),
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
