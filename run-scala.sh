#!/bin/bash

if [ -z "$AKKA_HOME" ] ; then
  echo "AKKA_HOME is not set"
  exit 1
fi

AKKA_LIB="$AKKA_HOME/lib"

classpath=$(find $AKKA_HOME/lib/ -name "*.jar" | tr '\n' ':')

#export JAVA_HOME=$HOME/jdk1.8.0_60
#export JAVA_CMD="$JAVA_HOME/bin/java"
if [ -z "$JAVA_HOME" ] ; then
  JAVA_CMD="$JAVA_HOME/bin/java"
else
  JAVA_CMD="java"
fi
JVM_TUNING="-Xmx10240M -Xms32M"
SCALA_TUNING="-Xbootclasspath/a:$classpath -Dscala.usejavacp=true"
AKKA_TUNING="-Dakka.remote.netty.tcp.hostname=$(ifconfig -a eth1 | grep "inet addr:" | awk '{print $2}' | sed 's/addr\://')"

cd $HOME/fractal-benchmark/build/bin
case "$1" in
    --master)
        echo "start master"
        shift
        $JAVA_CMD $JVM_TUNING $SCALA_TUNING $AKKA_TUNING org.caf.distributed -n $@
        ;;
    --worker)
        echo "$JAVA_CMD $JVM_TUNING $SCALA_TUNING $AKKA_TUNING -Djava.library.path=../ org.caf.distributed -w"
        ;;
esac

