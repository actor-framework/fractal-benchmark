package org.caf

import org.caf.Mandelbrot._
import org.caf.Requests._

import akka.actor._

import com.typesafe.config.ConfigFactory

import scala.annotation.tailrec
import scala.collection.JavaConversions._
import scala.collection.mutable.ArrayBuffer
import scala.concurrent.duration._

import Console.println

object conf {
  final val MAX_PENDING_WORKER_SENDS     =    3
  final val MAX_PENDING_TASKS_PER_WORKER =    3
  final val MAX_IMAGES                   = 3000
  var start: Long = 0
  var end: Long = 0
}


case object Done
case class WorkerAddresses(paths: Array[String])
case class Job(width: Int, height: Int, minRe: Float, maxRe: Float,
               minIm: Float, maxIm: Float, iterations: Int);
case class Image(img: java.util.ArrayList[java.lang.Byte])


class WorkerActor() extends Actor {
  def receive = {
    case Job(width, height, minRe, maxRe, minIm, maxIm, iterations) => {
      var buffer = new java.util.ArrayList[java.lang.Byte](102400)
      println(s"Job($width, $height, $minRe, $maxRe, $minIm, $maxIm, $iterations)")
      calculate(buffer, width, height, iterations,
                minRe, maxRe, minIm, maxIm, false)
      sender ! Image(buffer)
    }
    case _ => println("Unexpected message")
  }
}


class MasterActor() extends Actor {
  import conf._

  private val requests = new FractalRequests("scala-values.txt")
  private var sentImages     = 0
  private var receivedImages = 0
  private var workers = ArrayBuffer[ActorRef]()
  private var expectedWorkers = 0

  def manage(): Receive = {
    case Image(img: java.util.ArrayList[java.lang.Byte]) =>
      receivedImages += 1
      if (receivedImages == MAX_IMAGES) {
        conf.end = System.nanoTime
        println(s"${conf.end - conf.start}")
        distributed.global_latch.countDown
        context.stop(self)
      } else {
        sendJob(sender)
      }
    case Terminated(w) => println(s"Worker $w died!")
    case ReceiveTimeout =>
    case _ => println("Unexpected message")
  }

  def init(): Receive = {
    case WorkerAddresses(addresses) =>
      expectedWorkers = addresses.size
      for (a <- addresses) {
        context.actorSelection(a) ! Identify(a)
      }
      import context.dispatcher
      context.system.scheduler.scheduleOnce(3.seconds, self, ReceiveTimeout)
    case ActorIdentity(path, Some(worker)) =>
      context.watch(worker)
      workers += worker
      if (workers.size == expectedWorkers) {
        conf.start = System.nanoTime
        for (i <- 1 to MAX_PENDING_TASKS_PER_WORKER) {
          for (w <- workers) {
            sendJob(w)
          }
        }
        context.become(manage())
      }
    case ActorIdentity(path, None) =>
      println(s"Remote actor not available: $path")
    case ReceiveTimeout => println("Timeout")
    case _ => println("Unexpected Message")
  }

  def receive = init

  private def sendJob(worker: ActorRef) = {
    if (sentImages != MAX_IMAGES) {
      if (requests.atEnd())
        context.system.shutdown()
      sentImages += 1
      val (w, h, miR, maR, miI, maI, itr) = requests.request()
      worker ! Job(w, h, miR, maR, miI, maI, itr)
      requests.next()
    }
  }
}

object distributed {
  val global_latch = new java.util.concurrent.CountDownLatch(1)

  val workerConf = ConfigFactory.parseString("""
    akka {
      actor {
        provider = "akka.remote.RemoteActorRefProvider"
        serialize-messages = on
        serializers {
          proto = "akka.remote.serialization.ProtobufSerializer"
        }
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
        serialize-messages = on
        serializers {
          proto = "akka.remote.serialization.ProtobufSerializer"
        }
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
    val addresses = nodes.replace(" ","").split(",")
    val system = ActorSystem("FractalMasterSystem",
                               ConfigFactory.load(masterConf))
    system.actorOf(Props[MasterActor], "master") ! WorkerAddresses(addresses)
    global_latch.await
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
