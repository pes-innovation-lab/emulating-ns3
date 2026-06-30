# pair-ns-3

Docker network plugin that creates a paired netkit/veth device between two containers with one end per container.

## Install

```bash
./install_plugin.sh  # requires sudo
```

## Network options

| Option | Default | Description |
|--------|---------|-------------|
| `if-prefix` | first 16 chars of network UUID | Interface name inside containers: `{if-prefix}0` |
| `type` | `netkit-l2` | See types below |

| `type` | Behaviour |
|--------|-----------|
| `netkit-l2` | L2 netkit pair. Promiscuous mode set automatically. Use with `EmuFdNetDevice`. |
| `veth` | Standard veth pair. |
| `netkit-l3` | L3 netkit pair (no ARP, zero MAC). No known ns-3 integration path yet. |

**Limit:** exactly 2 containers per network.

## Compose example

```yaml
services:
  app:
    image: myapp
    networks: [simnet]

  ns3:
    image: myns3
    networks: [simnet]

networks:
  simnet:
    driver: pair-ns-3
    driver_opts:
      if-prefix: simnet
      type: netkit-l2
```

Both containers get interface `simnet0`. Point `EmuFdNetDevice` at `simnet0` in the ns-3 container.

## Star topology

One ns-3 container connected to multiple app containers via separate networks:

```yaml
services:
  hub:
    image: myns3
    networks: [net-a, net-b, net-c]

  node-a:
    image: myapp
    networks: [net-a]

  node-b:
    image: myapp
    networks: [net-b]

  node-c:
    image: myapp
    networks: [net-c]

networks:
  net-a:
    driver: pair-ns-3
    driver_opts:
      if-prefix: neta
  net-b:
    driver: pair-ns-3
    driver_opts:
      if-prefix: netb
  net-c:
    driver: pair-ns-3
    driver_opts:
      if-prefix: netc
```

`hub` gets `neta0`, `netb0`, `netc0`; one interface per network, identifiable by prefix.

> [!NOTE]
> Docker's plugin API doesn't expose the network name, so `if-prefix` is the only way to get an easily identifiable interface name inside the container.
