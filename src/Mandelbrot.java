import java.util.ArrayList;
import java.io.FileOutputStream;
import java.io.IOException;

public class Mandelbrot {
  static {
    System.loadLibrary("fractal-benchmark");
  }
  // NOT thread-safe
  public static native void calculate(ArrayList<Byte> storage, int width,
                                      int height, int iterations, float min_re,
                                      float max_re, float min_im, float max_im,
                                      boolean fracs_changed);
/*
  public static void main(String[] args) throws IOException {
    long startTime = System.currentTimeMillis();
    ArrayList<Byte> stuff = new ArrayList<Byte>();
    calculate(stuff, 1920, 1080, 1000, -1.05973f, 1.28927f, -0.665529f, 0.655783f, true);
    long estimatedTime = System.currentTimeMillis() - startTime;
    System.out.println("" + estimatedTime);
    // FileOutputStream fos = new FileOutputStream("javafrac.png");
    // byte[] arr = new byte[stuff.size()];
    // for (int i = 0; i < stuff.size(); ++i)
    //   arr[i] = stuff.get(i);
    // fos.write(arr);
    // fos.close();
  }
*/
}
