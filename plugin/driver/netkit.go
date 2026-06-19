package driver

import (
	"errors"
	"fmt"
	"log/slog"
	"regexp"
	"github.com/vishvananda/netlink"
)

func (p *NetkitPair) findEndpoint(endpointID string) *NetkitEndpoint {
	if p.SideA != nil && p.SideA.EndpointID == endpointID {
		return p.SideA
	}
	if p.SideB != nil && p.SideB.EndpointID == endpointID {
		return p.SideB
	}
	return nil
}

func (p *NetkitPair) findPeer(ep *NetkitEndpoint) *NetkitEndpoint {
	if p.SideA == ep {
		return p.SideB
	}
	return p.SideA
}

func sanitiseInterfaceName(ifname string) string {
	name := regexp.MustCompile("[^a-zA-Z0-9]+").ReplaceAllString(ifname, "_")
	if len(name) > 14 {
		return name[:14]
	}
	return name
}

func createNetkitPair(hostName, peerName string) error {
	slog.Debug("createNetkitPair", "hostName", hostName, "peerName", peerName)

	attrs := netlink.NewLinkAttrs()
	attrs.Name = hostName

	netkit := &netlink.Netkit{
		LinkAttrs:  attrs,
		Mode:       netlink.NETKIT_MODE_L2,
		Policy:     netlink.NETKIT_POLICY_FORWARD,
		PeerPolicy: netlink.NETKIT_POLICY_FORWARD,
	}

	peerAttrs := netlink.NewLinkAttrs()
	peerAttrs.Name = peerName
	netkit.SetPeerAttrs(&peerAttrs)

	if existing, lerr := netlink.LinkByName(hostName); lerr == nil {
		slog.Warn("createNetkitPair: stale interface, deleting", "name", hostName)
		netlink.LinkDel(existing)
	} else if existing, lerr := netlink.LinkByName(peerName); lerr == nil {
		// peer returned to host namespace on disconnect; deleting it destroys both sides
		slog.Warn("createNetkitPair: stale peer, deleting", "name", peerName)
		netlink.LinkDel(existing)
	}

	if err := netlink.LinkAdd(netkit); err != nil {
		return fmt.Errorf("error creating netkit pair: %w", err)
	}

	peer, err := netlink.LinkByName(peerName)
	if err != nil {
		netlink.LinkDel(netkit)
		return fmt.Errorf("error finding peer %s: %w", peerName, err)
	}

	if err = netlink.LinkSetUp(netkit); err != nil {
		netlink.LinkDel(netkit)
		return fmt.Errorf("error bringing up %s: %w", hostName, err)
	}
	if err = netlink.LinkSetUp(peer); err != nil {
		netlink.LinkDel(netkit)
		return fmt.Errorf("error bringing up %s: %w", peerName, err)
	}

	slog.Debug("createNetkitPair: done", "hostName", hostName, "peerName", peerName)
	return nil
}

// required for EmuFdNetDeviceHelper to receive all frames
// persists when Docker moves the link
func setLinkPromiscuous(interfaceName string) error {
	link, err := netlink.LinkByName(interfaceName)
	if err != nil {
		return fmt.Errorf("error finding link %s: %w", interfaceName, err)
	}
	if err := netlink.SetPromiscOn(link); err != nil {
		return fmt.Errorf("error setting promiscuous on %s: %w", interfaceName, err)
	}
	return nil
}

func linkExistsInHost(interfaceName string) (bool, error) {
	_, err := netlink.LinkByName(interfaceName)
	if err == nil {
		return true, nil
	}
	if errors.As(err, &netlink.LinkNotFoundError{}) {
		return false, nil
	}
	return false, err
}

func delLinkInHost(interfaceName string) error {
	if interfaceName == "" {
		return nil
	}
	slog.Debug("delLinkInHost", "interface", interfaceName)

	link, err := netlink.LinkByName(interfaceName)
	if err != nil {
		if errors.As(err, &netlink.LinkNotFoundError{}) {
			slog.Debug("delLinkInHost: link already gone", "interface", interfaceName)
			return nil
		}
		return fmt.Errorf("error finding link %s in host namespace: %w", interfaceName, err)
	}

	err = netlink.LinkDel(link)
	if err != nil {
		return fmt.Errorf("error deleting link %s: %w", interfaceName, err)
	}

	slog.Debug("delLinkInHost: done", "interface", interfaceName)
	return nil
}
