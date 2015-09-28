package org.caf

import org.caf.FractalRequests._

import akka.actor._

import com.typesafe.config.ConfigFactory

import scala.annotation.tailrec
import scala.collection.mutable
import scala.collection.JavaConversions._
import scala.concurrent.duration._

import Console.println


static final val MAX_PENDING_WORKER_SENDS     =    3
static final val MAX_PENDING_TASKS_PER_WORKER =    3
static final val MAX_IMAGES                   = 3000

static final val DEFAULT_WIDTH      = 1920;
static final val DEFAULT_HEIGHT     = 1080;
static final val DEFAULT_ITERATIONS = 1000;

static final val DEFAULT_MIN_RE = -1.9;  // must be <= 0.0
static final val DEFAULT_MAX_RE =  1.0;  // must be >= 0.0
static final val DEFAULT_MIN_IM = -0.9;  // must be <= 0.0
static final val DEFAULT_MAX_IM = DEFAULT_MIN_IM
                                  + (DEFAULT_MAX_RE - DEFAULT_MIN_RE)
                                  * DEFAULT_HEIGHT
                                  / DEFAULT_WIDTH;
static final val DEFAULT_ZOOM   =  0.9;  // must be >= 0.0


case object Done
case class NewWorker(worker: ActorRef)
case class Job(width: Int, height: Int, minRe: Float, maxRe: Float,
               minIm: Float, maxIm: Float, iterations: Int, id: Int);
case class Image(img: ArryList<Byte>)


class WorkerActor() extends Actor {
  private var buffer = new ArryList<Byte>()

  def receive = {
    case Job(width, height, minRe, maxRe, minIm, maxIm, iterations, id) =>
      calculate(buffer, width, height, iterations, minRe, maxRe, minIm, maxIm, False); sender ! Image(buffer)
    case Done => Stop
  }
}


class MasterActor() extends Actor {
  private var sentImages = 0
  private var receivedImages = 0
  private var workers = new ArrayBuffer<ActorRef>()
  private val requests = new FractalRequests()

  def receive = {
    case NewWorker(worker) =>
      workers += worker
      for (i <- 1 to MAX_PENDING_TASKS_PER_WORKER) {
        sendJob(worker)
      }
    case Image(img) =>
      if (++receivedImages == MAX_IMAGES) {
        for (w <- workers) { w ! Done }
        STOP
      } else {
        sendJob(sender)
      }
  }

  private def sendJob(worker: ActorRef) = {
    if (sentImages == MAX_IMAGES)
      return
    if (requests.atEnd())
      Stop
    ++sentImages
    val (w, h, miR, maR, miI, maI, itr, id) = requests.request()
    println("Job(${w}, ${h}, ${miR}, ${maR}, ${miI}, ${maI}, ${itr}, ${id}")
    worker ! Job(w, h, miR, maR, miI, maI, itr, id)
    requests.next();
  }
}

object distributed {

/*
    def runServer(args: List[String]) {
        val system = ActorSystem("fractalServer", ConfigFactory.load.getConfig("fractalServer"))
        //system.actorOf(Props(new ServerActor(system)), "pong")
        val selection = system.actorSelection("akka.tcp://")
    }
*/

    //private val NumPings = "num_pings=([0-9]+)".r
    private val SimpleUri = "([0-9a-zA-Z\\.]+):([0-9]+)".r


  @tailrec private final def run(args: List[String], paths: List[String], numPings: Option[Int], finalizer: (List[String], Int) => Unit): Unit = args match {
    case KeyValuePair("num_pings", IntStr(num)) :: tail => numPings match {
      case Some(x) => throw new IllegalArgumentException("\"num_pings\" already defined, first value = " + x + ", second value = " + num)
      case None => run(tail, paths, Some(num), finalizer)
    }
    case arg :: tail => run(tail, arg :: paths, numPings, finalizer)
    case Nil => numPings match {
      case Some(x) => {
        if (paths.length < 2) throw new RuntimeException("at least two hosts required")
        finalizer(paths, x)
      }
      case None => throw new RuntimeException("no \"num_pings\" found")
    }
  }

/*
  def runBenchmark(args: List[String]) {
    run(args, Nil, None, ((paths, x) => {
      val system = ActorSystem("benchmark", ConfigFactory.load.getConfig("benchmark"))
      system.actorOf(Props(new ClientActor(system))) ! RunClient(paths, x)
      global_latch.await
      system.shutdown
      System.exit(0)
    }))
  }
*/

    def runMaster(args: List[String]) {

    }

    def runWorker(args: List[String]) {
      val system = ActorSystem("FractalWorkerSystem",
                               ConfigFactory.load.getConfig("fractalWorker"))
      system.actorOf(Props[WorkerActor], "worker")
    }

  def main(args: Array[String]): Unit = args match {
    case Array("mode=server")     => runMaster(args.toList.drop(2))
    case Array("mode=master")     => runMaster(args.toList.drop(2))
    case Array("mode=client", _*) => runClient(args.toList.drop(2))
    case Array("mode=worker", _*) => runClient(args.toList.drop(2))
    // error, print help
    case _ => {
      println("Running a master:\n"                                    +
              "  mode=master\n"                                        +
              "  remote_actors PORT *or* akka\n"                       +
              "\n"                                                     +
              "Running a worker:\n"                                    +
              "  mode=worker\n"                                        +
              "  remote_actors ... *or* akka\n"                         );
    }
  }
}
