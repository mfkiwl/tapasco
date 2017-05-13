#
# Copyright (C) 2017 Jens Korinth, TU Darmstadt
#
# This file is part of Tapasco (TPC).
#
# Tapasco is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Tapasco is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with Tapasco.  If not, see <http://www.gnu.org/licenses/>.
#
# @file   clock_constraint.tcl
# @brief  Plugin to constraint the sys_clk to the right pin on PyNQ.
#         Workaround: PyNQ does not have a Vivado board definition fil
# @author J. Korinth, TU Darmstadt (jk@esa.cs.tu-darmstadt.de)
#
namespace eval clock_constraint {
  # Constraints the input pins called 'sys_clk'
  proc create_clock_constraint {} {
    set clk [get_ports "sys_clk"]
    set_property -dict { PACKAGE_PIN H16 IOSTANDARD LVCMOS33 } $clk
    create_clock -add -name sys_clk_pin -period 10.00 -waveform {0 5} $clk
  }
}

tapasco::register_plugin "platform::clock_constraint::create_clock_constraint" "post-synth"
