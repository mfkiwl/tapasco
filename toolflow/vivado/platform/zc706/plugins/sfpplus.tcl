# Copyright (c) 2014-2020 Embedded Systems and Applications, TU Darmstadt.
#
# This file is part of TaPaSCo
# (see https://github.com/esa-tu-darmstadt/tapasco).
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
#

namespace eval sfpplus {

  proc is_sfpplus_supported {} {
    return true
  }

  variable available_ports 1
  variable rx_ports       {"Y6"}
  variable tx_ports       {"W4"}
  variable disable_pins   {"AA18"}
  variable refclk_pins    {"AC8"}
  variable disable_pins_voltages {"LVCMOS25"}

  proc num_available_ports {} {
    variable available_ports
    return $available_ports
  }

  proc generate_cores {ports} {
    variable refclk_pins

    set num_streams [dict size $ports]

    create_network_config_master

    set constraints_fn "[get_property DIRECTORY [current_project]]/sfpplus.xdc"
    set constraints_file [open $constraints_fn w+]

    #Setup CLK-Ports for Ethernet-Subsystem
    set gt_refclk_n [create_bd_port -dir I gt_refclk_n]
    set gt_refclk_p [create_bd_port -dir I gt_refclk_p]
    puts $constraints_file [format {set_property PACKAGE_PIN %s [get_ports %s]} [lindex $refclk_pins 0] $gt_refclk_p]

    # AXI Interconnect for Configuration
    set axi_config [tapasco::ip::create_axi_ic axi_config 1 $num_streams]

    set dclk_wiz [tapasco::ip::create_clk_wiz dclk_wiz]
    set_property -dict [list CONFIG.USE_SAFE_CLOCK_STARTUP {true} CONFIG.CLKOUT1_REQUESTED_OUT_FREQ 100 CONFIG.USE_LOCKED {false} CONFIG.USE_RESET {false}] $dclk_wiz

    set dclk_reset [tapasco::ip::create_rst_gen dclk_reset]

    connect_bd_net [get_bd_pins $dclk_wiz/clk_out1] [get_bd_pins $dclk_reset/slowest_sync_clk]
    connect_bd_net [get_bd_pins design_peripheral_aresetn] [get_bd_pins $dclk_reset/ext_reset_in]
    connect_bd_net [get_bd_pins design_clk] [get_bd_pins $dclk_wiz/clk_in1]
    connect_bd_net [get_bd_pins $axi_config/M*_ACLK] [get_bd_pins $dclk_wiz/clk_out1]
    connect_bd_net [get_bd_pins $axi_config/M*_ARESETN] [get_bd_pins $dclk_reset/peripheral_aresetn]

    connect_bd_intf_net [get_bd_intf_pins $axi_config/S00_AXI] [get_bd_intf_pins S_NETWORK]
    connect_bd_net [get_bd_pins $axi_config/S00_ACLK] [get_bd_pins design_clk]
    connect_bd_net [get_bd_pins $axi_config/S00_ARESETN] [get_bd_pins design_interconnect_aresetn]
    connect_bd_net [get_bd_pins $axi_config/ACLK] [get_bd_pins design_clk]
    connect_bd_net [get_bd_pins $axi_config/ARESETN] [get_bd_pins design_interconnect_aresetn]


    set out_inv [create_inverter out_inv]


    set keys [dict keys $ports]

    variable port_number [lindex $keys 0]
    variable port_name [dict get $ports 0]
    set main_core [create_main_core $port_name $port_number 0 $constraints_file]

    for {set i 1} {$i < $num_streams} {incr i} {
      variable port_number [lindex $keys $i]
      variable port_name [dict get $ports $port_number]
      create_secondary_core $port_name $port_number $i $main_core $constraints_file
    }
    close $constraints_file
    read_xdc $constraints_fn
    set_property PROCESSING_ORDER NORMAL [get_files $constraints_fn]
  }

  # Creates the main SFP+Core (with shared logic)
  # @param port_name the name of the port for this core
  # @param port_number the physical port number
  # @param axi_index the index to connect to on the configuration interconnect
  # @param constraints_file the the file used for constraints
  # @return the created ip core
  proc create_main_core {port_name port_number axi_index constraints_file} {

    # Create the 10G Network Subsystem for the Port
    set core [tapasco::ip::create_10g_mac ethernet_${port_number}]
    set_property -dict [list CONFIG.base_kr {BASE-R} CONFIG.SupportLevel {1} CONFIG.autonegotiation {0} CONFIG.fec {0} CONFIG.Statistics_Gathering {0} CONFIG.Statistics_Gathering {false} CONFIG.TransceiverControl {true} CONFIG.DRP {false}] $core
    
    create_connect_ports $port_number $core $constraints_file

    connect_bd_net [get_bd_ports /gt_refclk_p] [get_bd_pins $core/refclk_p]
    connect_bd_net [get_bd_ports /gt_refclk_n] [get_bd_pins $core/refclk_n]
    connect_bd_net [get_bd_pins $core/reset] [get_bd_pins design_peripheral_areset]

    connect_bd_net [get_bd_pins $core/coreclk_out] [get_bd_pins sfp_tx_clock_${port_name}] [get_bd_pins sfp_rx_clock_${port_name}]
    connect_bd_net [get_bd_pins $core/areset_datapathclk_out] [get_bd_pins out_inv/Op1]
    connect_bd_net [get_bd_pins out_inv/Res] [get_bd_pins sfp_rx_resetn_${port_name}] [get_bd_pins sfp_tx_resetn_${port_name}]
    

    connect_core $core $port_name $axi_index

    return $core
  }

  # Creates the a secondary SFP+-Core (without shared logic)
  # @param port_name the name of the port for this core
  # @param port_number the physical port number
  # @param axi_index the index to connect to on the configuration interconnect
  # @param main_core the main core which provides shared logic
  # @param constraints_file the file used for constraints
  proc create_secondary_core {port_name port_number axi_index main_core constraints_file} {

    # Create the 10G Network Subsystem for the Port
    set core [tapasco::ip::create_10g_mac ethernet_${port_number}]
    set_property -dict [list CONFIG.base_kr {BASE-R} CONFIG.SupportLevel {0} CONFIG.autonegotiation {0} CONFIG.fec {0} CONFIG.Statistics_Gathering {0} CONFIG.Statistics_Gathering {false} CONFIG.TransceiverControl {true} CONFIG.DRP {false}] $core
    

    create_connect_ports $port_number $core $constraints_file

    connect_bd_net [get_bd_pins $main_core/qplllock_out]           [get_bd_pins $core/qplllock]
    connect_bd_net [get_bd_pins $main_core/qplloutclk_out]         [get_bd_pins $core/qplloutclk]
    connect_bd_net [get_bd_pins $main_core/qplloutrefclk_out]      [get_bd_pins $core/qplloutrefclk]
    connect_bd_net [get_bd_pins $main_core/reset_counter_done_out] [get_bd_pins $core/reset_counter_done]
    connect_bd_net [get_bd_pins $main_core/txusrclk_out]           [get_bd_pins $core/txusrclk]
    connect_bd_net [get_bd_pins $main_core/txusrclk2_out]          [get_bd_pins $core/txusrclk2]
    connect_bd_net [get_bd_pins $main_core/txuserrdy_out]          [get_bd_pins $core/txuserrdy]
    connect_bd_net [get_bd_pins $main_core/coreclk_out]            [get_bd_pins $core/coreclk]
    connect_bd_net [get_bd_pins $main_core/gttxreset_out]          [get_bd_pins $core/gttxreset]
    connect_bd_net [get_bd_pins $main_core/gtrxreset_out]          [get_bd_pins $core/gtrxreset]
    connect_bd_net [get_bd_pins $main_core/gttxreset_out]          [get_bd_pins $core/areset_coreclk]
    connect_bd_net [get_bd_pins design_peripheral_areset]          [get_bd_pins $core/areset]

    connect_bd_net [get_bd_pins $main_core/coreclk_out] [get_bd_pins sfp_tx_clock_${port_name}] [get_bd_pins sfp_rx_clock_${port_name}]
    connect_bd_net [get_bd_pins out_inv/Res] [get_bd_pins sfp_rx_resetn_${port_name}] [get_bd_pins sfp_tx_resetn_${port_name}]

    connect_core $core $port_name $axi_index
  }

  # Creates and connects the bd ports for the core
  # @param port_number the physical port number
  # @param core the ip core
  # @param constraints_file the file used for constraints
  proc create_connect_ports {port_number core constraints_file} {
    variable rx_ports
    variable tx_ports
    variable disable_pins
    variable disable_pins_voltages

    puts $constraints_file [format {# SFP-Port %d} $port_number]
    set txp [create_bd_port -dir O txp_${port_number}]
    set txn [create_bd_port -dir O txn_${port_number}]
    puts $constraints_file [format {set_property PACKAGE_PIN %s [get_ports %s]} [lindex $tx_ports $port_number] $txp]
    set tx_disable [create_bd_port -dir O tx_disable_${port_number}]
    puts $constraints_file [format {set_property PACKAGE_PIN %s [get_ports %s]} [lindex $disable_pins $port_number] $tx_disable]
    puts $constraints_file [format {set_property IOSTANDARD %s [get_ports %s]} [lindex $disable_pins_voltages $port_number] $tx_disable]
    set rxp [create_bd_port -dir I rxp_${port_number}]
    set rxn [create_bd_port -dir I rxn_${port_number}]
    puts $constraints_file [format {set_property PACKAGE_PIN %s [get_ports %s]} [lindex $rx_ports $port_number] $rxp]

    connect_bd_net [get_bd_pins $core/txp] $txp
    connect_bd_net [get_bd_pins $core/txn] $txn
    connect_bd_net [get_bd_pins $core/rxp] $rxp
    connect_bd_net [get_bd_pins $core/rxn] $rxn
    connect_bd_net [get_bd_pins $core/tx_disable] $tx_disable
  }

  # Creates the connections which are common to main and secondary cores
  # @param core the ip core
  # @param port_name the name of the port
  # @param axi_index the index to connect to on the configuration interconnect
  proc connect_core {core port_name axi_index} {
    connect_bd_net [get_bd_pins $core/tx_axis_aresetn] [get_bd_pins out_inv/Res]
    connect_bd_net [get_bd_pins $core/rx_axis_aresetn] [get_bd_pins out_inv/Res]
    connect_bd_intf_net [get_bd_intf_pins $core/m_axis_rx] [get_bd_intf_pins AXIS_RX_${port_name}]
    connect_bd_intf_net [get_bd_intf_pins $core/s_axis_tx] [get_bd_intf_pins AXIS_TX_${port_name}]
    connect_bd_intf_net [get_bd_intf_pins $core/s_axi] [get_bd_intf_pins axi_config/M[format %02d $axi_index]_AXI]
    connect_bd_net [get_bd_pins $core/dclk] [get_bd_pins design_clk]
    connect_bd_net [get_bd_pins $core/s_axi_aclk] [get_bd_pins dclk_wiz/clk_out1]
    connect_bd_net [get_bd_pins $core/s_axi_aresetn] [get_bd_pins dclk_reset/peripheral_aresetn]
  }

  proc create_inverter {name} {
    variable ret [create_bd_cell -type ip -vlnv xilinx.com:ip:util_vector_logic:2.0 $name]
    set_property -dict [list CONFIG.C_SIZE {1} CONFIG.C_OPERATION {not} CONFIG.LOGO_FILE {data/sym_notgate.png}] [get_bd_cells $name]
    return $ret
  }

  # Create AXI connection to Host interconnect for network configuration interfaces
  proc create_network_config_master {} {
    create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:aximm_rtl:1.0 S_NETWORK
  }

  proc addressmap {{args {}}} {
    if {[tapasco::is_feature_enabled "SFPPLUS"]} {
          set args [lappend args "M_NETWORK" [list 0x82500000 0 0 ""]]
      }
      return $args
  }


}

tapasco::register_plugin "platform::sfpplus::addressmap" "post-address-map"

