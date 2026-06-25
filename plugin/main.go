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

package main

import (
	"log/slog"
	"os"

	nw_sdk "github.com/docker/go-plugins-helpers/network"
	"github.com/pes-innovation-lab/ns-3-docker-network/driver"
)

func main() {
	level := slog.LevelInfo
	switch os.Getenv("LOG_LEVEL") {
	case "debug":
		level = slog.LevelDebug
	case "warn":
		level = slog.LevelWarn
	case "error":
		level = slog.LevelError
	}

	slog.SetDefault(slog.New(slog.NewTextHandler(os.Stdout, &slog.HandlerOptions{Level: level})))

	d := driver.NetlinkPairDriver{
		Networks: make(map[string]*driver.PairNetwork),
	}
	handler := nw_sdk.NewHandler(&d)
	slog.Info("netkit driver starting")
	handler.ServeUnix("netkit", 0)
}
