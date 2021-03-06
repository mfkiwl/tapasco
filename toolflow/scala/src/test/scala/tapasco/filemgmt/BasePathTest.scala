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
  * @file BasePathTest.scala
  * @brief Unit tests for BasePath.
  * @authors J. Korinth, TU Darmstadt (jk@esa.cs.tu-darmstadt.de)
  **/
package tapasco.filemgmt

import java.nio.file._

import org.scalatest._
import tapasco.TaPaSCoSpec
import tapasco.util._

class BasePathSpec extends TaPaSCoSpec with Matchers {
  "Setting a new path" should "change the path" in {
    val p = Paths.get(".").resolve("test")
    val bp = new BasePath(p.getParent, false)
    assert(bp.get equals p.getParent)
    bp.set(p)
    assert(bp.get equals p)
  }

  "A path change" should "result in an event" in {
    val p = Paths.get(".")
    val np = p.resolve("test")
    val bp = new BasePath(p, false)
    var ok = false
    bp += new Listener[BasePath.Event] {
      def update(e: BasePath.Event): Unit = e match {
        case BasePath.BasePathChanged(`np`) => ok = true
        case _ => {}
      }
    }
    bp.set(np)
    assert(ok)
  }

  "Setting the same path" should "not result in an event" in {
    val p = Paths.get(".")
    val bp = new BasePath(p, false)
    var ok = true
    bp += new Listener[BasePath.Event] {
      def update(e: BasePath.Event): Unit = ok = false
    }
    bp.set(p)
    assert(ok)
  }

  "Parallel changes with unique paths" should "generate an event for each change" in {
    import java.util.concurrent.atomic.AtomicInteger

    import scala.concurrent.ExecutionContext.Implicits.global
    import scala.concurrent.Future

    val tests = 1000000
    val count = new AtomicInteger(tests)
    val check = new Array[Boolean](tests)
    val p = Paths.get(".")
    val bp = new BasePath(p, false)

    0 until tests foreach { i => check(i) = false }

    bp += new Listener[BasePath.Event] {
      def update(e: BasePath.Event): Unit = e match {
        case BasePath.BasePathChanged(np) => check(np.getFileName().toString.toInt) = true
      }
    }

    val futures = for {i <- 0 until count.get()} yield Future {
      var i = count.getAndDecrement()
      while (i >= 0) {
        val newpath = p.resolve("%d".format(i))
        bp.set(newpath)
        i = count.getAndDecrement()
      }
    }

    futures foreach {
      scala.concurrent.Await.ready(_, scala.concurrent.duration.Duration.Inf)
    }

    assert(0 until tests map { i => check(i) } reduce (_ && _))
  }
}

