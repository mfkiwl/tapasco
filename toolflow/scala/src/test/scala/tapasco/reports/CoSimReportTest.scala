/*
 *
 * Copyright (c) 2014-2020 Embedded Systems and Applications, TU Darmstadt.
 *
 * This file is part of TaPaSCo
 * (see https://github.com/esa-tu-darmstadt/tapasco).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */
/**
  * @file CoSimReportTest.scala
  * @brief Unit tests for CoSimReport model.
  * @authors J. Korinth, TU Darmstadt (jk@esa.cs.tu-darmstadt.de)
  **/
package tapasco.reports

import org.scalatest._
import tapasco.TaPaSCoSpec

class CoSimReportSpec extends TaPaSCoSpec with Matchers {

  "A missing CoSimReport file" should "result in None" in {
    CoSimReport(reportPath.resolve("missing.rpt")) shouldBe empty
  }

  "An invalid CoSimReport file" should "result in None" in {
    CoSimReport(reportPath.resolve("correct-timing.rpt")) shouldBe empty
    CoSimReport(reportPath.resolve("invalid-cosim1.rpt")) shouldBe empty
  }

  "A correct CoSimReport file" should "be parsed correctly" in {
    val oc = CoSimReport(reportPath.resolve("correct-cosim1.rpt"))
    lazy val r = oc.get
    oc should not be empty
    r.latency.min should be(279)
    r.latency.avg should be(280)
    r.latency.max should be(281)
    r.interval.min should be(282)
    r.interval.avg should be(283)
    r.interval.max should be(284)
    val oc2 = CoSimReport(reportPath.resolve("correct-cosim2.rpt"))
    lazy val r2 = oc2.get
    oc2 should not be empty
    r2.latency.min should be(1279)
    r2.latency.avg should be(2279)
    r2.latency.max should be(3279)
    r2.interval.min should be(4279)
    r2.interval.avg should be(5279)
    r2.interval.max should be(6279)
  }
}
