export AKKA_LIB=$AKKA_HOME/lib
export SCALA_LIB=$SCALA_HOME/lib

classpath=$(find $AKKA_HOME/lib/ -name "*.jar" | tr '\n' ':')

export JVM_TUNING="-Xmx10240M -Xms32M"
export SCALA_TUNING="-Xbootclasspath/a:$classpath -Dscala.home=$SCALA_HOME -Dscala.usejavacp=true"

case "$1" in
    --master)
        echo "start master"
        java $JVM_TUNING $SCALA_TUNING org.caf.distributed -n "akka.tcp://FractalWorkerSystem@127.0.0.1:2552/user/worker0, akka.tcp://FractalWorkerSystem@127.0.0.1:2552/user/worker1, akka.tcp://FractalWorkerSystem@127.0.0.1:2552/user/worker2, akka.tcp://FractalWorkerSystem@127.0.0.1:2552/user/worker3"
        ;;
    --worker)
        echo "start worker"
        java $JVM_TUNING $SCALA_TUNING -Djava.library.path=../ org.caf.distributed -w
        ;;
esac

