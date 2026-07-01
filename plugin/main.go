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
