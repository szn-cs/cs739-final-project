# Distributed Lock-service

-   [Architecture design documentation](./documentation/architecture.md)
-   [Resouces & Notes](./documentation/resources.md)


# BUILD

-   `(source ./script/provision_local.sh)`
-   `(source ./script/build.sh && build)`
-   check ./script/run.sh for run examples & use `--help` for documentation.

_(build tested on Linux OS)_

### Ebay NuRaft library documentation:

-   https://github.com/eBay/NuRaft/blob/master/docs/how_to_use.md#modules
-   https://github.com/eBay/NuRaft/blob/master/docs/quick_tutorial.md
-   https://github.com/eBay/NuRaft/tree/master/examples/calculator


# Demonstration 
_Interactive Mode: open session → acquire lock → write file → read file → Jeopardy period → close session._

![alt text](./documentation/demonstration_rw.gif "Demonstration")


### TODO:

-   [build process] expose client header functions once build moves to a library rather than an executable.
-   Fix issues with segmentation error. 
-   Fix issues with multithread conflicts. 

---

## 🔑 COPY RIGHT NOTICE:

Code for using NuRaft library was modified over from the examples in their repo (Github @ ebay/nuraft).
