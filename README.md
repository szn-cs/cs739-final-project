# Distributed Lock-service

-   [Architecture design documentation](./documentation/architecture.md)
-   <https://docs.google.com/document/d/1gX3OVmCk0FP5KAkVj-eKInxStUYDQ7L4AN5ZVq-RjAU/edit?usp=share_link>

### Ebay NuRaft library documentation:

-   https://github.com/eBay/NuRaft/blob/master/docs/how_to_use.md#modules
-   https://github.com/eBay/NuRaft/blob/master/docs/quick_tutorial.md
-   https://github.com/eBay/NuRaft/tree/master/examples/calculator

# BUILD

-   `(source ./script/provision_local.sh)`
-   `(source ./script/build.sh && build)`
-   check ./script/run.sh for run examples & use `--help` for documentation.

_(build tested on Linux OS)_

### TODO:

-   [build process] expose client header functions once build moves to a library rather than an executable.
