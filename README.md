Tapasco (TPC)!
=========================
<img src="icon/tapasco_icon.png" alt="Tapasco logo"/>

System Requirements
-------------------
TPC is known to work in this environment:

* Intel x86_64 arch
* Fedora 20/22 / Ubuntu 14.04
* Bash Shell 4.2.x+

Other setups likely work as well, but are untested.

Prerequisites
-------------
To use TPC, you'll need working installations of

* Vivado Design Suite 2016.2 or newer
* Java SDK 7+
* sbt 0.13.x
* git

If you want to use the High-Level Synthesis flow for generating custom IP
cores, you'll also need:

* Vivado HLS 2016.2+

Check that at least the following are in your `$PATH`:

* `sbt`
* `vivado`
* `git`
* `bash`
* [`vivado_hls`]

Basic Setup
-------------------
1.  Open a terminal in the main directory of the repository and source the TPC
    setup script via `. setup.sh`.
    You need to do this every time you use TPC (or put it into your `~/.bashrc`).
2.  Build TPC: `sbt compile` (this may take a while, `sbt` needs to fetch all
    dependencies etc. once).
2.  Create the necessary jar files with `sbt assembly`.
4.  Run TPC unit tests: `sbt test`
5.  _Optional_: Generate sample configuration file: `tapasco -n config.json`
    TaPaSCo should exit immediately and `config.json` will include a full
    configuration that can be read with `--configFile`, including one example
    for each kind of job.

When everything completed successfully, TaPaSCo is ready to use!
Read on in [Getting Started](GETTINGSTARTED.md).
