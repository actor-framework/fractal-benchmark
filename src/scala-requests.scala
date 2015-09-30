package org.caf

import scala.collection.mutable.ArrayBuffer
import scala.io.Source._

object Requests {
  class FractalRequests(file: String) {
    private var position = 0
    private val configs: List[(Int, Int, Float, Float, Float, Float, Int)] = init(file)
    private val max = configs.size

    def init(path: String): List[(Int, Int, Float, Float, Float, Float, Int)] = {
      val f = fromFile(file)
      val lines = f.getLines.toList
      f.close
      lines.map{
        s => val values = s.split(','); (values(0).toInt, values(1).toInt,
                                         values(2).toFloat, values(3).toFloat,
                                         values(4).toFloat, values(5).toFloat,
                                         values(6).toInt)
      }
    }

    def atEnd() = { position >= max }
    def next() = {
      if (! atEnd()) {
        position += 1
      }
      atEnd()
    }
    def request(): (Int, Int, Float, Float, Float, Float, Int) = { configs(position) }
  }

}
