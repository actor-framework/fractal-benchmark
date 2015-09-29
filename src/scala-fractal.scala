package org.caf

import org.caf.Mandelbrot.calculate
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
}

case object Done
case class NewWorker(worker: ActorRef)
case class Job(width: Int, height: Int, minRe: Float, maxRe: Float,
               minIm: Float, maxIm: Float, iterations: Int, id: Int);
case class Image(img: java.util.ArrayList[java.lang.Byte])


class WorkerActor() extends Actor {
  private var buffer = new java.util.ArrayList[java.lang.Byte]()

  def receive = {
    case Job(width, height, minRe, maxRe, minIm, maxIm, iterations, id) => {
      calculate(buffer, width, height, iterations, minRe, maxRe, minIm, maxIm, false)
      sender ! Image(buffer)
    }
    case Done => context.system.shutdown()
  }
}


class MasterActor() extends Actor {
  import conf._

  private var sentImages = 0
  private var receivedImages = 0
  private var workers = ArrayBuffer[ActorRef]()
  private val requests = new FractalRequests()

  def receive = {
    case NewWorker(worker) =>
      workers += worker
      for (i <- 1 to MAX_PENDING_TASKS_PER_WORKER) {
        sendJob(worker)
      }
    case Image(img) =>
      receivedImages += 1
      if (receivedImages == MAX_IMAGES) {
        for (w <- workers) { w ! Done }
        context.system.shutdown()
      } else {
        sendJob(sender)
      }
  }

  private def sendJob(worker: ActorRef) = {
    if (sentImages != MAX_IMAGES) {
      if (requests.atEnd())
        context.system.shutdown()
      sentImages += 1
      val (w, h, miR, maR, miI, maI, itr, id) = requests.request()
      println("Job(${w}, ${h}, ${miR}, ${maR}, ${miI}, ${maI}, ${itr}, ${id}")
      worker ! Job(w, h, miR, maR, miI, maI, itr, id)
      requests.next();
    }
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
      actor {
        provider = "akka.remote.RemoteActorRefProvider"
      }
      remote {
        enabled-transports = ["akka.remote.netty.tcp"]
        netty.tcp {
          hostname = "127.0.0.1"
          port = 2552
        }
     }
    }
    """)

  //private val NumPings = "num_pings=([0-9]+)".r
  private val SimpleUri = "([0-9a-zA-Z\\.]+):([0-9]+)".r

  // @tailrec private final def run(args: List[String], paths: List[String], numPings: Option[Int], finalizer: (List[String], Int) => Unit): Unit = args match {
  //   case KeyValuePair("num_pings", IntStr(num)) :: tail => numPings match {
  //     case Some(x) => throw new IllegalArgumentException("\"num_pings\" already defined, first value = " + x + ", second value = " + num)
  //     case None => run(tail, paths, Some(num), finalizer)
  //   }
  //   case arg :: tail => run(tail, arg :: paths, numPings, finalizer)
  //   case Nil => numPings match {
  //     case Some(x) => {
  //       if (paths.length < 2) throw new RuntimeException("at least two hosts required")
  //       finalizer(paths, x)
  //     }
  //     case None => throw new RuntimeException("no \"num_pings\" found")
  //   }
  // }

  def runMaster(args: List[String]) {
    // run(args, Nil, None, ((paths, x) => {
    //   val system = ActorSystem("FractalMasterSystem",
    //                            ConfigFactory.load(masterConf))
    //   system.actorOf(Props(new ClientActor(system))) ! RunClient(paths, x)
    //   global_latch.await
    //   system.shutdown
    //   System.exit(0)
    // }))
  }

  def runWorker(args: List[String]) {
    val system = ActorSystem("FractalWorkerSystem",
                             ConfigFactory.load(workerConf))
    system.actorOf(Props[WorkerActor], "worker")
    val selection = system.actorSelection("akka.tcp://")
  }

  def main(args: Array[String]): Unit = args match {
    case Array("mode=server")     => runMaster(args.toList.drop(2))
    case Array("mode=master")     => runMaster(args.toList.drop(2))
    case Array("mode=client", _*) => runWorker(args.toList.drop(2))
    case Array("mode=worker", _*) => runWorker(args.toList.drop(2))
    // error, print help
    case _ => {
      println("Running a master:\n"                                            +
              "  mode=master\n"                                                +
              "  remote_actors PORT *or* akka\n"                               +
              "\n"                                                             +
              "Running a worker:\n"                                            +
              "  mode=worker\n"                                                +
              "  remote_actors ... *or* akka\n"
      );
    }
  }
}
