#include "org_caf_Mandelbrot.h"

#include <QColor>

#include <atomic>
#include <cstddef>

#include "config.hpp"
#include "calculate_fractal.hpp"

namespace {

using color_vector = std::vector<QColor>;

__thread std::vector<QColor>* palette = nullptr;

} // namespace <anonymous>

class array_list_writer {
public:
  array_list_writer(JNIEnv* env) : env_(env) {
    buf_.reserve(102400); // reserve 100kb upfront
    /*
    array_list_class_ = env->FindClass("java/util/ArrayList");
    byte_class_ = env->FindClass("java/lang/Byte");
    byte_constructor_ = env->GetMethodID(byte_class_, "<init>", "(B)V");
    add_method_ = env->GetMethodID(array_list_class_, "add", "(Ljava/lang/Object;)Z");
    */
  }

  inline void push_back(char value) {
    buf_.push_back(value);
    /*
    jobject arg = env_->NewObject(byte_class_, byte_constructor_, (jbyte) value);
    env_->CallBooleanMethod(data_, add_method_, arg);
    */
  }

  jbyteArray toJavaArray() {
    auto result = env_->NewByteArray(buf_.size());
    env_->SetByteArrayRegion(result, 0, buf_.size(), buf_.data());
    return result;
  }

private:
  std::vector<jbyte> buf_;
  JNIEnv* env_;
};

jbyteArray JNICALL
Java_org_caf_Mandelbrot_calculate(JNIEnv* env, jclass, jint width, jint height,
                                  jint iterations, jfloat min_re, jfloat max_re,
                                  jfloat min_im, jfloat max_im,
                                  jboolean fracs_changed) {
  if (! palette)
    palette = new color_vector;
  array_list_writer storage{env};
  calculate_mandelbrot(storage, *palette, (uint32_t) width, (uint32_t) height,
                       (uint32_t) iterations, min_re, max_re, min_im, max_im,
                       fracs_changed);
  return storage.toJavaArray();
}
