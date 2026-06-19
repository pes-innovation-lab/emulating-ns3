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

	err := netlink.LinkAdd(netkit)
	if err != nil {
		return fmt.Errorf("error creating netkit pair: %w", err)
	}
	slog.Debug("createNetkitPair: pair created, bringing up host side", "hostName", hostName)

	err = netlink.LinkSetUp(netkit)
	if err != nil {
		err2 := netlink.LinkDel(netkit)
		if err2 != nil {
			return errors.Join(fmt.Errorf("error with link set up: %w", err), fmt.Errorf("error with link down on failed link: %w", err2))
		}
		return fmt.Errorf("error with link set up: %w", err)
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

func delLinkInHost(interfaceName string) error {
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
