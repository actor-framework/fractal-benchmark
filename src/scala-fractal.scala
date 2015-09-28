package org.caf

// import org.caf.utility._

import akka.actor._
import com.typesafe.config.ConfigFactory

import scala.annotation.tailrec

import scala.collection.mutable
import scala.collection.JavaConversions._

import scala.concurrent.duration._

import Console.println

static final Int MAX_PENDING_WORKER_SENDS = 3
static final Int MAX_PENDING_TASKS_PER_WORKER = 3
static final Int MAX_IMAGES = 3000

static final Int DEFAULT_WIDTH = 1920;
static final Int DEFAULT_HEIGHT = 1080;
static final Int DEFAULT_ITERATIONS = 1000;

static final Float DEFAULT_MIN_RE = -1.9;  // must be <= 0.0
static final Float DEFAULT_MAX_RE =  1.0;  // must be >= 0.0
static final Float DEFAULT_MIN_IM = -0.9;  // must be <= 0.0
static final Float DEFAULT_MAX_IM = DEFAULT_MIN_IM
                                  + (DEFAULT_MAX_RE - DEFAULT_MIN_RE)
                                  * DEFAULT_HEIGHT
                                  / DEFAULT_WIDTH;
static final Float DEFAULT_ZOOM   =  0.9;  // must be >= 0.0


case object Done
case class NewWorker(worker: ActorRef)
case class Job(width: Float, height: Float,
               minRe: Float, maxRe: Float,
               minIm: Float, maxIm: Float,
               iterations: Int, id: Int);
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
    if (! requestStream.next()) // todo: get fractal request stream
      Stop
    ++sentImages
    val req = requestStream.request()
    worker ! Job(req.getWidth(), req.getHeight(),
                 req.getMinRe(), req.getMaxRe(),
                 req.getMinIm(), req.getMaxIm(),
                 req.getIterations(), req.getId())
  }
}

/*
class PingActor(pongs: List[ActorRef]) extends Actor {

    import context.become

    private var parent: ActorRef = null
    private var left = pongs.length

    private final def recvLoop: Receive = {
        case Pong(0) => {
            parent ! Done
            //println(parent.toString + " ! Done")
            if (left > 1) left -= 1
            else context.stop(self)
        }
        case Pong(value) => sender ! Ping(value - 1)
    }

    def receive = {
        case SetParent(p) => parent = p
        case KickOff(value) => pongs.foreach(_ ! Ping(value)); become(recvLoop)
    }
}
*/

/*
class ServerActor(system: ActorSystem) extends Actor {

    import context.become

    private final def reply(what: Any): Unit = sender ! what

    private final def kickOff(peers: Peers, value: Int): Unit = {
        val ping = context.actorOf(Props(new PingActor(peers.connected map (_.channel))))
        ping ! SetParent(sender)
        ping ! KickOff(value)
        //println("[" + peers + "]: KickOff(" + value + ")")
    }

    private final def connectionEstablished(peers: Peers, x: Any): Peers = x match {
        case PendingPeer(path, channel, client, token) => {
            client ! Ok(token)
            //println("connected to " + path)
            Peers(Peer(path, channel) :: peers.connected, peers.pending filterNot (_.clientToken == token))
        }
    }

    private final def newPending(peers: Peers, path: String, token: String) : Peers = {
        import context.dispatcher // Use this Actors' Dispatcher as ExecutionContext
        val channel = system.actorFor(path)
        channel ! Hello(token)
        system.scheduler.scheduleOnce(5 seconds, self, AddPongTimeout(path, token))
        //println("[" + peers + "]: sent 'Hello' to " + path)
        Peers(peers.connected, PendingPeer(path, channel, sender, token) :: peers.pending)
    }

    private final def handleAddPongTimeout(peers: Peers, path: String, token: String) = {
        peers.pending find (x => x.path == path && x.clientToken == token) match {
            case Some(PendingPeer(_, channel, client, _)) => {
                client ! Error(path + " did not respond", token)
                //println(path + " did not respond")
                become(bhvr(Peers(peers.connected, peers.pending filterNot (x => x.path == path && x.clientToken == token))))
            }
            case None => Unit
        }
    }

    def bhvr(peers: Peers): Receive = {
        case Ping(value) => reply(Pong(value))
        case Hello(token) => reply(Olleh(token))
        case Olleh(token) => peers.pending.find (_.clientToken == token) match {
            case Some(x) => connectionEstablished(peers, x); become(bhvr(peers))
            case None => Unit
        }
        case AddPong(path, token) => {
            //println("received AddPong(" + path + ", " + token + ")")
            if (peers.connected exists (_.path == path)) {
                reply(Ok(token))
                //println("recv[" + peers + "]: " + path + " cached (replied 'Ok')")
            }
            else {
                try { become(bhvr(newPending(peers, path, token))) }
                catch {
                    // catches match error and integer conversion failure
                    case e : Exception => reply(Error(e.toString, token))
                }
            }
        }
        case KickOff(value) => kickOff(peers, value)
        case AddPongTimeout(path, token) => handleAddPongTimeout(peers, path, token)
    }

    def receive = bhvr(Peers(Nil, Nil))

}
*/

// case class TokenTimeout(token: String)
// case class RunClient(paths: List[String], numPings: Int)

/*
class ClientActor(system: ActorSystem) extends Actor {

    import context.become

    def collectDoneMessages(left: Int): Receive = {
        case Done => {
//println("Done")
            if (left == 1) {
                global_latch.countDown
                context.stop(self)
            } else {
                become(collectDoneMessages(left - 1))
            }
        }
        case _ => {
            // ignore any other message
        }
    }

    def collectOkMessages(pongs: List[ActorRef], left: Int, receivedTokens: List[String], numPings: Int): Receive = {
        case Ok(token) => {
//println("Ok")
            if (left == 1) {
                //println("collected all Ok messages (wait for Done messages)")
                pongs foreach (_ ! KickOff(numPings))
                become(collectDoneMessages(pongs.length * (pongs.length - 1)))
            }
            else {
                become(collectOkMessages(pongs, left - 1, token :: receivedTokens, numPings))
            }
        }
        case TokenTimeout(token) => {
            if (!receivedTokens.contains(token)) {
                println("Error: " + token + " did not reply within 10 seconds")
                global_latch.countDown
                context.stop(self)
            }
        }
        case Error(what, token) => {
            println("Error [from " + token+ "]: " + what)
            global_latch.countDown
            context.stop(self)
        }
    }

    def receive = {
        case RunClient(paths, numPings) => {
//println("RunClient(" + paths.toString + ", " + numPings + ")")
            val pongs = paths map (x => {
                val pong = system.actorFor(x)
                paths foreach (y => if (x != y) {
                    val token = x + " -> " + y
                    pong ! AddPong(y, token)
//println(x + " ! AddPong(" + y + ", " + token + ")")
                    import context.dispatcher // Use this Actors' Dispatcher as ExecutionContext
                    system.scheduler.scheduleOnce(10 seconds, self, TokenTimeout(token))
                })
                pong
            })
            become(collectOkMessages(pongs, pongs.length * (pongs.length - 1), Nil, numPings))
        }
    }
}
*/


object distributed {

/*
    def runServer(args: List[String]) {
        val system = ActorSystem("fractalServer", ConfigFactory.load.getConfig("fractalServer"))
        //system.actorOf(Props(new ServerActor(system)), "pong")
        val selection = system.actorSelection("akka.tcp://")
    }
*/
    def runServer(args: List[String]) {

    }

    //private val NumPings = "num_pings=([0-9]+)".r
    private val SimpleUri = "([0-9a-zA-Z\\.]+):([0-9]+)".r

/*
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
*/

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
    def runWorker(args: List[String]) {

    }

    def runMaster(args: List[String]) {

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
