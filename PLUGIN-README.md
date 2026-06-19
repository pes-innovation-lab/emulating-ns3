# netkit-ns-3: Docker Network Plugin for simulation interfacing

## How it works:

This plugin creates a netkit L2 pair for the workload of interfacing simulations to existing applications in order to test these simulations.

## Usage:

Run the following to install the plugin from the repository root:

```bash
./install_plugin.sh # requires sudo
```

When creating a `docker-compose.yml` file, use it to create the network between the app<->ns-3 pair:

```yaml
networks:
  <network_name>:
    driver: netkit-ns-3
    driver_opts:
      if-prefix: <interface_prefix> # used to identify the interface in the ns-3 container
```
