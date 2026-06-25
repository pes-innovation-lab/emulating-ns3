// Copyright 2026 PES Innovation Lab
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

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
