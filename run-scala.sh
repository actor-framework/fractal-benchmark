#!/bin/bash

AKKA_HOME="$HOME/akka-2.4.0"
JAVA_HOME="$HOME/jdk1.8.0_60"

if [ -z "$AKKA_HOME" ] ; then
  echo "AKKA_HOME is not set"
  exit 1
fi

AKKA_LIB="$AKKA_HOME/lib"

classpath=$(find $AKKA_HOME/lib/ -name "*.jar" | tr '\n' ':')

#export JAVA_HOME=$HOME/jdk1.8.0_60
#export JAVA_CMD="$JAVA_HOME/bin/java"
if [ -z "$JAVA_HOME" ] ; then
  JAVA_CMD="java"
else
  JAVA_CMD="$JAVA_HOME/bin/java"
fi
JVM_TUNING="-Xmx10240M -Xms32M -cp ."
SCALA_TUNING="-Xbootclasspath/a:$classpath -Dscala.usejavacp=true"
if [ "$(uname)" = "Darwin" ] ; then
  ipaddr=$(ifconfig en0 | grep "inet " | awk '{print $2}' | sed 's/addr\://')
else
  ipaddr=$(ifconfig | grep "inet addr" | grep -v '127.0.0.1' | awk '{print $2}' | awk -F: '{print $2}')
  #ipaddr=$(ifconfig -a eth1 | grep "inet addr:" | awk '{print $2}' | sed 's/addr\://')
fi
AKKA_TUNING="-Dakka.remote.netty.tcp.hostname=$ipaddr"

case "$1" in
    --master)
        slaves=$(cat "$2" | tr '\n' ',')
        $JAVA_CMD $JVM_TUNING $SCALA_TUNING $AKKA_TUNING org.caf.distributed -n $slaves
        ;;
    --worker)
        $JAVA_CMD $JVM_TUNING $SCALA_TUNING $AKKA_TUNING -Djava.library.path=../ org.caf.distributed -w
        ;;
esac

