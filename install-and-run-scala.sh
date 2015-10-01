#!/bin/bash

## configuration for worker host, port and JAVA
## worker will get their configuration by themselves
HOST="141.22.28.46" #"mobi36.cpt.haw-hamburg.de"
PORT="2552"
JAVA_8_HOME=/usr/lib/jvm/java-8-oracle/


echo "### Exporting JAVA_HOME ###"
export JAVA_HOME=$JAVA_8_HOME
echo "export JAVA_HOME=$JAVA_HOME"


SCALA_URL=http://downloads.typesafe.com/scala/2.11.7/scala-2.11.7.tgz
echo "### Checking Scala ($(pwd)/scala-2.11.7) ###"
if [ ! -d "./scala-2.11.7" ]; then
    echo "Downloading Scala ($SCALA_URL)"
    wget -qO- $SCALA_URL | tar xvz >/dev/null
else
    echo "Scala found"
fi
echo "### Exporting SCALA_HOME and SCALA_LIB ###"
export SCALA_HOME=$(pwd)/scala-2.11.7
export SCALA_LIB=$SCALA_HOME/lib
echo "export SCALA_HOME=$SCALA_HOME"
echo "export SCALA_LIB=$SCALA_LIB"
echo "### Adding Scala compiler to PATH ###"
export PATH=$PATH:$SCALA_HOME/bin
echo "export PATH=$PATH:$SCALA_HOME/bin"


AKKA_URL=http://downloads.typesafe.com/akka/akka_2.11-2.3.14.zip
echo "### Checking Akka ###"
if [ ! -d "./akka-2.3.14" ]; then
    echo "Downloading Akka ($AKKA_URL)"
    wget -q $AKKA_URL -O temp.zip ; unzip temp.zip >/dev/null; rm temp.zip
else
    echo "Akka found"
fi
echo "### Exporting AKKA_HOME and AKKA_LIB ###"
export AKKA_HOME=$(pwd)/akka-2.3.14
export AKKA_LIB=$AKKA_HOME/lib
echo "export AKKA_HOME=$AKKA_HOME"
echo "export AKKA_LIB=$AKKA_LIB"

echo "### Tuning JVM ###"
export JVM_TUNING="-Xmx10240M -Xms32M"
echo "export JVM_TUNING=$JVM_TUNING"

echo "### Tuning SCALA ###"
export SCALA_TUNING=-Xbootclasspath/a:$SCALA_LIB/akka-actor_2.11-2.3.10.jar:$SCALA_LIB/jline-2.12.1.jar:$SCALA_LIB/scala-actors-migration_2.11-1.1.0.jar:$SCALA_LIB/scala-actors-2.11.0.jar:$SCALA_LIB/scala-compiler.jar:$SCALA_LIB/scala-library.jar:$SCALA_LIB/scala-reflect.jar:$SCALA_LIB/scala-swing_2.11-1.0.2.jar:$SCALA_LIB/scalap.jar:$AKKA_LIB/akka/config-1.2.1.jar:$AKKA_LIB/akka/akka-remote_2.11-2.3.14.jar:$AKKA_LIB/akka/protobuf-java-2.5.0.jar:$AKKA_LIB/akka/netty-3.8.0.Final.jar -classpath "" -Dscala.home=$SCALA_HOME -Dscala.usejavacp=true
echo "export SCALA_TUNING=\"$SCALA_TUNING\""

echo "### Checking for build dir ###"
if [ ! -d "build/bin" ]; then
    echo "Configuring"
    bash ./configure
    echo "Building"
    make -j 4
else
    echo "Build dir found, not building again"
fi

current_dir=$(pwd)
echo "### Switching into build/bin ###"
cd build/bin

while [ $# -ne 0 ]; do
    case "$1" in
        -*=*) optarg=`echo "$1" | sed 's/[-_a-zA-Z0-9]*=//'` ;;
        *) optarg= ;;
    esac

    case $1 in
        --worker)
            echo ""
            echo "### Executing worker ###"
            echo "java $JVM_TUNING $SCALA_TUNING -cp \"\" -Djava.library.path=../ org.caf.distributed --worker"
            java $JVM_TUNING $SCALA_TUNING -cp "" -Djava.library.path=../ -Dakka.remote.netty.tcp.hostname=$(ifconfig -a eth1 | grep "inet addr:" | awk '{print $2}' | sed 's/addr\://') org.caf.distributed --worker
            ;;
        --master=*)
            addresses="akka.tcp://FractalWorkerSystem@$HOST:$PORT/user/worker0"
            added=1
            while [ $added -lt $optarg ]; do
                addresses="$addresses,akka.tcp://FractalWorkerSystem@$HOST:$PORT/user/worker$added"
                added=$[ $added + 1]
            done
            echo ""
            echo "### Executing master ###"
            echo "java $JVM_TUNING $SCALA_TUNING -cp \"\" org.caf.distributed -n $addresses"
            java $JVM_TUNING $SCALA_TUNING -cp "" org.caf.distributed -n "$addresses"
            ;;
        *)
            echo "only available options: --worker | --master=NUM_WORKERS"
            ;;
    esac
    shift
done

cd $current_dir

# rm -rf akka-2.3.14
# rm -rf scala-2.11.7

