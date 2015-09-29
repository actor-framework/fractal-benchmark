#include "org_caf_Mandelbrot.h"

#include <QColor>

#include <cstddef>

#include "config.hpp"
#include "calculate_fractal.hpp"

namespace {

bool initialized = false;

jclass array_list_class;
jclass byte_class;
jmethodID byte_constructor;
jmethodID add_method;
std::vector<QColor> palette;

void init_jni(JNIEnv* env) {
  array_list_class = env->FindClass("java/util/ArrayList");
  byte_class = env->FindClass("java/lang/Byte");
  byte_constructor = env->GetMethodID(byte_class, "<init>", "(B)V");
  add_method = env->GetMethodID(array_list_class, "add", "(Ljava/lang/Object;)Z");
  initialized = true;
}

} // namespace

class array_list_writer {
public:
  array_list_writer(JNIEnv* env, jobject ptr) : env_(env), data_(ptr) {
    // nop
  }

  void push_back(char value) {
    jobject arg = env_->NewObject(byte_class, byte_constructor, (jbyte) value);
    env_->CallBooleanMethod(data_, add_method, arg);
  }

private:
  JNIEnv* env_;
  jobject data_;
};

void JNICALL Java_org_caf_Mandelbrot_calculate(JNIEnv* env, jclass, jobject result,
                                               jint width, jint height, jint iterations,
                                               jfloat min_re, jfloat max_re,
                                               jfloat min_im, jfloat max_im,
                                               jboolean fracs_changed) {
  if (! initialized)
    init_jni(env);
  array_list_writer storage{env, result};
  calculate_mandelbrot(storage, palette, (uint32_t) width, (uint32_t) height,
                       (uint32_t) iterations, min_re, max_re, min_im, max_im,
                       fracs_changed);
}
