#include "org_caf_Mandelbrot.h"

#include <QColor>

#include <atomic>
#include <cstddef>

#include "config.hpp"
#include "calculate_fractal.hpp"

namespace {

std::atomic_flag lock = ATOMIC_FLAG_INIT;
std::vector<QColor> palette;

} // namespace <anonymous>

class array_list_writer {
public:
  array_list_writer(JNIEnv* env, jobject ptr) : env_(env), data_(ptr) {
    array_list_class_ = env->FindClass("java/util/ArrayList");
    byte_class_ = env->FindClass("java/lang/Byte");
    byte_constructor_ = env->GetMethodID(byte_class_, "<init>", "(B)V");
    add_method_ = env->GetMethodID(array_list_class_, "add", "(Ljava/lang/Object;)Z");
  }

  void push_back(char value) {
    jobject arg = env_->NewObject(byte_class_, byte_constructor_, (jbyte) value);
    env_->CallBooleanMethod(data_, add_method_, arg);
  }

private:
  JNIEnv* env_;
  jobject data_;
  jclass array_list_class_;
  jclass byte_class_;
  jmethodID byte_constructor_;
  jmethodID add_method_;
};

void JNICALL Java_org_caf_Mandelbrot_calculate(JNIEnv* env, jclass, jobject result,
                                               jint width, jint height, jint iterations,
                                               jfloat min_re, jfloat max_re,
                                               jfloat min_im, jfloat max_im,
                                               jboolean fracs_changed) {
  array_list_writer storage{env, result};
  {
    while (lock.test_and_set(std::memory_order_acquire))  // acquire lock
             ; // spin
    if (palette.empty())
      calculate_palette_mandelbrot(palette, iterations);
    lock.clear(std::memory_order_release);               // release lock
  }
  calculate_mandelbrot(storage, palette, (uint32_t) width, (uint32_t) height,
                       (uint32_t) iterations, min_re, max_re, min_im, max_im,
                       fracs_changed);
}
