package driver

import (
	"errors"
	"fmt"
	"log/slog"
	"net"
	"regexp"
	"runtime"

	"github.com/vishvananda/netlink"
	"github.com/vishvananda/netns"
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

func moveToContainerNetns(interfaceName, namespacePath string) error {
	slog.Debug("moveToContainerNetns", "interface", interfaceName, "namespace", namespacePath)

	containerNamespace, err := netns.GetFromPath(namespacePath)
	if err != nil {
		return fmt.Errorf("error opening container namespace %s: %w", namespacePath, err)
	}
	defer containerNamespace.Close()

	link, err := netlink.LinkByName(interfaceName)
	if err != nil {
		return fmt.Errorf("error finding link %s in host namespace: %w", interfaceName, err)
	}

	slog.Debug("moveToContainerNetns: moving link to container namespace", "interface", interfaceName)
	if err := netlink.LinkSetNsFd(link, int(containerNamespace)); err != nil {
		return fmt.Errorf("error moving link %s to container namespace: %w", interfaceName, err)
	}

	runtime.LockOSThread()
	defer runtime.UnlockOSThread()

	originalNamespace, err := netns.Get()
	if err != nil {
		return fmt.Errorf("error getting host namespace: %w", err)
	}
	defer originalNamespace.Close()

	err = netns.Set(containerNamespace)
	if err != nil {
		return fmt.Errorf("error switching to container namespace %s: %w", namespacePath, err)
	}
	defer netns.Set(originalNamespace)

	link, err = netlink.LinkByName(interfaceName)
	if err != nil {
		return fmt.Errorf("error finding link %s in container namespace: %w", interfaceName, err)
	}

	slog.Debug("moveToContainerNetns: bringing link up", "interface", interfaceName)
	err = netlink.LinkSetUp(link)
	if err != nil {
		return fmt.Errorf("error setting up link %s: %w", interfaceName, err)
	}

	// accept all frames regardless of destination MAC, required by EmuFdNetDeviceHelper
	slog.Debug("moveToContainerNetns: setting promiscuous mode", "interface", interfaceName)
	err = netlink.SetPromiscOn(link)
	if err != nil {
		return fmt.Errorf("error setting link %s promiscuous: %w", interfaceName, err)
	}

	slog.Debug("moveToContainerNetns: done", "interface", interfaceName, "namespace", namespacePath)
	return nil
}

func delLinkInNetns(interfaceName, namespacePath string) error {
	slog.Debug("delLinkInNetns", "interface", interfaceName, "namespace", namespacePath)

	containerNamespace, err := netns.GetFromPath(namespacePath)
	if err != nil {
		return fmt.Errorf("error opening container namespace %s: %w", namespacePath, err)
	}
	defer containerNamespace.Close()

	runtime.LockOSThread()
	defer runtime.UnlockOSThread()

	originalNamespace, err := netns.Get()
	if err != nil {
		return fmt.Errorf("error getting host namespace: %w", err)
	}
	defer originalNamespace.Close()

	err = netns.Set(containerNamespace)
	if err != nil {
		return fmt.Errorf("error switching to container namespace %s: %w", namespacePath, err)
	}
	defer netns.Set(originalNamespace)

	link, err := netlink.LinkByName(interfaceName)
	if err != nil {
		if errors.As(err, &netlink.LinkNotFoundError{}) {
			slog.Debug("delLinkInNetns: link already gone", "interface", interfaceName)
			return nil
		}
		return fmt.Errorf("error finding link %s in container namespace: %w", interfaceName, err)
	}

	err = netlink.LinkDel(link)
	if err != nil {
		return fmt.Errorf("error deleting link %s: %w", interfaceName, err)
	}

	slog.Debug("delLinkInNetns: done", "interface", interfaceName)
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

func setAddrInNetns(interfaceName, namespacePath, ipAddr, macAddr string) error {
	slog.Debug("setAddrInNetns", "interface", interfaceName, "namespace", namespacePath, "ip", ipAddr, "mac", macAddr)

	addr, err := netlink.ParseAddr(ipAddr)
	if err != nil {
		return fmt.Errorf("failed to parse IP addr '%s': %w", ipAddr, err)
	}

	hwaddr, err := net.ParseMAC(macAddr)
	if err != nil {
		return fmt.Errorf("failed to parse MAC addr '%s': %w", macAddr, err)
	}

	containerNamespace, err := netns.GetFromPath(namespacePath)
	if err != nil {
		return fmt.Errorf("error opening container namespace %s: %w", namespacePath, err)
	}
	defer containerNamespace.Close()

	runtime.LockOSThread()
	defer runtime.UnlockOSThread()

	originalNamespace, err := netns.Get()
	if err != nil {
		return fmt.Errorf("error getting host namespace: %w", err)
	}
	defer originalNamespace.Close()

	err = netns.Set(containerNamespace)
	if err != nil {
		return fmt.Errorf("error switching to container namespace %s: %w", namespacePath, err)
	}
	defer netns.Set(originalNamespace)

	link, err := netlink.LinkByName(interfaceName)
	if err != nil {
		return fmt.Errorf("error finding link %s in container namespace: %w", interfaceName, err)
	}

	err = netlink.AddrAdd(link, addr)
	if err != nil {
		return fmt.Errorf("error adding IP addr %s to link %s: %w", ipAddr, interfaceName, err)
	}

	err = netlink.LinkSetHardwareAddr(link, hwaddr)
	if err != nil {
		return fmt.Errorf("error setting MAC addr %s on link %s: %w", macAddr, interfaceName, err)
	}

	slog.Debug("setAddrInNetns: done", "interface", interfaceName, "ip", ipAddr, "mac", macAddr)
	return nil
}

func leaveInNetns(interfaceName, namespacePath string) error {
	slog.Debug("leaveInNetns", "interface", interfaceName, "namespace", namespacePath)

	containerNamespace, err := netns.GetFromPath(namespacePath)
	if err != nil {
		return fmt.Errorf("error opening container namespace %s: %w", namespacePath, err)
	}
	defer containerNamespace.Close()

	runtime.LockOSThread()
	defer runtime.UnlockOSThread()

	originalNamespace, err := netns.Get()
	if err != nil {
		return fmt.Errorf("error getting host namespace: %w", err)
	}
	defer originalNamespace.Close()

	err = netns.Set(containerNamespace)
	if err != nil {
		return fmt.Errorf("error switching to container namespace %s: %w", namespacePath, err)
	}
	defer netns.Set(originalNamespace)

	link, err := netlink.LinkByName(interfaceName)
	if err != nil {
		return fmt.Errorf("error finding link %s in container namespace: %w", interfaceName, err)
	}

	addrs, err := netlink.AddrList(link, netlink.FAMILY_V4)
	if err != nil {
		return fmt.Errorf("error listing addrs on link %s: %w", interfaceName, err)
	}

	slog.Debug("leaveInNetns: removing addrs", "interface", interfaceName, "count", len(addrs))
	for _, addr := range addrs {
		err := netlink.AddrDel(link, &addr)
		if err != nil {
			return fmt.Errorf("error removing IP addr %s from link %s: %w", addr.IP, interfaceName, err)
		}
	}

	slog.Debug("leaveInNetns: done", "interface", interfaceName)
	return nil
}
