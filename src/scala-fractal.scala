package org.caf

import akka.actor._

import com.typesafe.config.ConfigFactory

import scala.annotation.tailrec
import scala.collection.JavaConversions._
import scala.collection.mutable.ArrayBuffer
import scala.concurrent.duration.Duration
import java.util.concurrent.TimeUnit
import Console.println

import java.lang.{Byte => JByte}

case class WorkerAddresses(paths: Array[String])

case class Job(width: Int, height: Int, minRe: Float, maxRe: Float,
               minIm: Float, maxIm: Float, iterations: Int);

object global {
  final val MAX_PENDING_TASKS_PER_WORKER =    3
  type Image = java.util.ArrayList[java.lang.Byte]
  val latch = new java.util.concurrent.CountDownLatch(1)
}

class Requests(file: String) {
  private var position = 0

  private val configs = {
    val f = scala.io.Source.fromFile(file)
    val res = f.getLines.map{
      s =>
        val values = s.split(',')
        Job(values(0).toInt, values(1).toInt,
            values(2).toFloat, values(3).toFloat,
            values(4).toFloat, values(5).toFloat,
            values(6).toInt)
    }.toArray
    f.close
    res
  }

  def num() = configs.size

  def atEnd() = position >= num

  def next() = {
    val res = configs(position)
    position += 1
    res
  }
}

class WorkerActor() extends Actor {
  def receive = {
    case Job(width, height, minRe, maxRe, minIm, maxIm, iterations) => {
      var buf = new java.util.ArrayList[java.lang.Byte](102400)
      org.caf.Mandelbrot.calculate(buf, width, height, iterations,
                                   minRe, maxRe, minIm, maxIm, false)
      sender ! buf
    }
  }
}

class MasterActor(workerRefs: List[ActorRef], requests: Requests) extends Actor {
  private var sentImages     = 0
  private var receivedImages = 0
  private var expectedWorkers = 0

  private def sendJob(worker: ActorRef) = {
    if (! requests.atEnd()) {
      sentImages += 1
      worker ! requests.next()
    }
  }

  // send first wave of jobs
  for (worker <- workerRefs) {
    context watch worker
    for (i <- 1 to global.MAX_PENDING_TASKS_PER_WORKER)
      sendJob(worker)
  }

  def receive = {
    case img: global.Image =>
      receivedImages += 1
      if (receivedImages == requests.num) {
        context.stop(self)
        global.latch.countDown
      } else {
        sendJob(sender)
      }
    case Terminated(w) => println(s"Worker $w died!")
  }
}

object distributed {
  val workerConf = ConfigFactory.parseString("""
    akka {
      actor {
        provider = "akka.remote.RemoteActorRefProvider"
      }
      remote {
        enabled-transports = ["akka.remote.netty.tcp"]
        netty.tcp {
          hostname = "127.0.0.1"
          port = 2552
          send-buffer-size = 0b
          receive-buffer-size = 0b
          maximum-frame-size = 1000000000b
        }
      }
      actor {
        deployment {
          "/WorkerActor/*" {
            remote = "akka.tcp://FractalWorkerSystem@127.0.0.1:2552"
          }
        }
      }
    }
    """)

  val masterConf = ConfigFactory.parseString("""
    akka {
      loglevel = "OFF"
      actor {
        provider = "akka.remote.RemoteActorRefProvider"
      }
      remote {
        enabled-transports = ["akka.remote.netty.tcp"]
        netty.tcp {
          hostnames = ["127.0.0.1"]
          port = 2553
          send-buffer-size = 0b
          receive-buffer-size = 0b
          maximum-frame-size = 1000000000b
        }
     }
    }
    """)

  def runMaster(nodes: String) {
    val system = ActorSystem("FractalMasterSystem", ConfigFactory.load(masterConf))
    val ibox = Inbox.create(system)
    val refs = nodes.replace(" ", "").split(",").map {
      x =>
        val tout = Duration.create(1, TimeUnit.SECONDS)
        scala.concurrent.Await.result(system.actorSelection(x).resolveOne(tout), tout)
    }.toList
    val requests = new Requests("scala-values.txt")
    val start = System.nanoTime
    system.actorOf(Props(new MasterActor(refs, requests)), "master")
    global.latch.await
    val end = System.nanoTime
    println("" + ((end - start) / 1000000))
    system.shutdown
    System.exit(0)
  }

  def runWorker(args: List[String]) = {
    val system = ActorSystem("FractalWorkerSystem",
                             ConfigFactory.load(workerConf))
    system.actorOf(Props[WorkerActor], "worker0")
    system.actorOf(Props[WorkerActor], "worker1")
    system.actorOf(Props[WorkerActor], "worker2")
    system.actorOf(Props[WorkerActor], "worker3")
    println("Started FractalWorkerSystem - waiting for master")
  }

  def main(args: Array[String]): Unit = args match {
    case Array("-w",       _*) => runWorker(args.toList.drop(1))
    case Array("--worker", _*) => runWorker(args.toList.drop(1))
    case Array("-n", nodes)    => runMaster(nodes)
    // error, print help
    case _ => {
      println("-w,--worker     run in worker mode          \n"                 +
              "-n NODES        set worker nodes (HOST:PORT)\n");
    }
  }
}
