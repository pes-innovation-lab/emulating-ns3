package driver

import (
	"sync"

	nw_sdk "github.com/docker/go-plugins-helpers/network"
)

type NetlinkPairDriver struct {
	sync.Mutex
	Networks map[string]*NetkitNetwork
}

type NetkitNetwork struct {
	NetworkID   string
	NetworkName string
	IPAMv4      *nw_sdk.IPAMData
	Pair        *NetkitPair
}

type NetkitPair struct {
	SideA *NetkitEndpoint
	SideB *NetkitEndpoint
}

type NetkitEndpoint struct {
	EndpointID    string
	InterfaceName string
	NamespacePath string
	IPAddress     string
	MACAddress    string
	Gateway       string
	// IPv6Address   string
}
