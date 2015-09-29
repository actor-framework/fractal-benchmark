package org.caf

import Array._

class FractalRequests {
  final val WIDTH = 1920;
  final val HEIGHT = 1080;
  final val ITERATIONS = 1000;
  final val MIN_RE = -1.9;  // must be <= 0.0
  final val MAX_RE =  1.0;  // must be >= 0.0
  final val MIN_IM = -0.9;  // must be <= 0.0
  final val MAX_IM = MIN_IM + (MAX_RE - MIN_RE) * HEIGHT / WIDTH;
  private var position = 0
  private val configs: Array[(Int, Int, Float, Float, Float, Float, Int, Int)]
    = new Array(
      (WIDTH, HEIGHT, MIN_RE, MAX_RE, MIN_IM, MAX_IM, ITERATIONS, 0),
      (WIDTH, HEIGHT, MIN_RE, MAX_RE, MIN_IM, MAX_IM, ITERATIONS, 1)
    )
  private val max = configs.size()

  def atEnd() = { position >= max }
  def next() = {
    if (! atEnd()) {
      position += 1
    }
    atEnd()
  }
  def request() = { configs(position) }
}
