package driver

import (
	"sync"

	nw_sdk "github.com/docker/go-plugins-helpers/network"
)

type NetlinkPairDriver struct {
	sync.Mutex
	Networks map[string]*PairNetwork
}

type PairNetwork struct {
	NetworkID       string
	InterfacePrefix string
	Type            DeviceType
	IPAMv4          *nw_sdk.IPAMData
	Pair            *DevicePair
}

type DevicePair struct {
	SideA *PairEndpoint
	SideB *PairEndpoint
}

type PairEndpoint struct {
	EndpointID    string
	InterfaceName string

	Joined bool

	IPAddress  string
	MACAddress string
	Gateway    string
}

type DeviceType int

const (
	NetkitL2 = DeviceType(iota)
	NetkitL3
	Veth
)
