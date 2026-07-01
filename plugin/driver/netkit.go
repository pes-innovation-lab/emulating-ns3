package driver

import (
	"errors"
	"fmt"
	"log/slog"
	"regexp"

	"github.com/vishvananda/netlink"
)

var netRegex = regexp.MustCompile("[^a-zA-Z0-9]+")

func (p *DevicePair) findEndpoint(endpointID string) *PairEndpoint {
	if p.SideA != nil && p.SideA.EndpointID == endpointID {
		return p.SideA
	}
	if p.SideB != nil && p.SideB.EndpointID == endpointID {
		return p.SideB
	}
	return nil
}

func sanitiseInterfaceName(ifname string) string {
	// 14 characters + Docker appends interface index (e.g. "0") + NUL terminator = 16
	s := netRegex.ReplaceAllString(ifname, "_")
	return s[:min(len(s), 14)]
}

func createPair(hostName, peerName string, devtype DeviceType) error {
	slog.Debug("createPair", "hostName", hostName, "peerName", peerName)

	attrs := netlink.NewLinkAttrs()
	attrs.Name = hostName

	var device netlink.Link
	switch devtype {
	case NetkitL3:
		netkit := &netlink.Netkit{
			LinkAttrs:  attrs,
			Mode:       netlink.NETKIT_MODE_L3,
			Policy:     netlink.NETKIT_POLICY_FORWARD,
			PeerPolicy: netlink.NETKIT_POLICY_FORWARD,
		}
		peerAttrs := netlink.NewLinkAttrs()
		peerAttrs.Name = peerName
		netkit.SetPeerAttrs(&peerAttrs)
		device = netkit

	case Veth:
		device = &netlink.Veth{
			LinkAttrs: attrs,
			PeerName:  peerName,
		}
	default:
		netkit := &netlink.Netkit{
			LinkAttrs:  attrs,
			Mode:       netlink.NETKIT_MODE_L2,
			Policy:     netlink.NETKIT_POLICY_FORWARD,
			PeerPolicy: netlink.NETKIT_POLICY_FORWARD,
		}
		peerAttrs := netlink.NewLinkAttrs()
		peerAttrs.Name = peerName
		netkit.SetPeerAttrs(&peerAttrs)
		device = netkit
	}

	if existing, lerr := netlink.LinkByName(hostName); lerr == nil {
		slog.Warn("createPair: stale interface, deleting", "name", hostName)
		if err := netlink.LinkDel(existing); err != nil {
			return fmt.Errorf("error deleting stale interface %s: %w", hostName, err)
		}
	}
	if existing, lerr := netlink.LinkByName(peerName); lerr == nil {
		// peer returned to host namespace on disconnect; deleting it destroys both sides
		slog.Warn("createPair: stale peer, deleting", "name", peerName)
		if err := netlink.LinkDel(existing); err != nil {
			return fmt.Errorf("error deleting stale peer interface %s: %w", peerName, err)
		}
	}

	if err := netlink.LinkAdd(device); err != nil {
		return fmt.Errorf("error creating pair: %w", err)
	}

	peer, err := netlink.LinkByName(peerName)
	if err != nil {
		if err := netlink.LinkDel(device); err != nil {
			slog.Warn("createPair: cleanup failed after peer lookup error", "name", hostName, "error", err)
		}
		return fmt.Errorf("error finding peer %s: %w", peerName, err)
	}

	if err = netlink.LinkSetUp(device); err != nil {
		if err := netlink.LinkDel(device); err != nil {
			slog.Warn("createPair: cleanup failed after LinkSetUp error", "name", hostName, "error", err)
		}
		return fmt.Errorf("error bringing up %s: %w", hostName, err)
	}
	if err = netlink.LinkSetUp(peer); err != nil {
		if err := netlink.LinkDel(device); err != nil {
			slog.Warn("createPair: cleanup failed after peer LinkSetUp error", "name", hostName, "error", err)
		}
		return fmt.Errorf("error bringing up %s: %w", peerName, err)
	}

	slog.Debug("createPair: done", "hostName", hostName, "peerName", peerName)
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
