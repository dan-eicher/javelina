/* The Computer Language Benchmarks Game
   https://salsa.debian.org/benchmarksgame-team/benchmarksgame/

 contributed by Java novice Jarkko Miettinen
 modified ~3 lines of the original C#-version
 by Isaac Gouy

   PORT of the published spectralnorm javaxint-1 program. One change: the
   DecimalFormat("#.000000000") that prints the result is java.text, which is
   Java 1.1, so `fmt9` below does the same fixed nine decimal places. The
   arithmetic is untouched, and the four separate functions the task
   description requires (A, MultiplyAv, MultiplyAtv, MultiplyAtAv) are as
   published.
*/

public class spectralnorm
{

   public static void main(String[] args) {
      int n = 100;
      if (args.length > 0) n = Integer.parseInt(args[0]);

      System.out.println(fmt9(new spectralnorm().Approximate(n)));
   }

   /* DecimalFormat("#.000000000") — nine decimals, no grouping. Scale, round
      half-up, then place the point by hand: the value here is ~1.27, far from
      any range where the scaling loses a digit. */
   private static String fmt9(double x) {
      boolean neg = x < 0;
      if (neg) x = -x;
      long scaled = (long) Math.floor(x * 1000000000.0 + 0.5);
      long whole = scaled / 1000000000L;
      long frac = scaled % 1000000000L;
      String f = Long.toString(frac);
      StringBuffer sb = new StringBuffer();
      if (neg) sb.append('-');
      sb.append(Long.toString(whole));
      sb.append('.');
      for (int i = f.length(); i < 9; i++) sb.append('0');
      sb.append(f);
      return sb.toString();
   }

   private final double Approximate(int n) {
      // create unit vector
      double[] u = new double[n];
      for (int i=0; i<n; i++) u[i] =  1;

      // 20 steps of the power method
      double[] v = new double[n];
      for (int i=0; i<n; i++) v[i] = 0;

      for (int i=0; i<10; i++) {
         MultiplyAtAv(n,u,v);
         MultiplyAtAv(n,v,u);
      }

      // B=AtA         A multiplied by A transposed
      // v.Bv /(v.v)   eigenvalue of v
      double vBv = 0, vv = 0;
      for (int i=0; i<n; i++) {
         vBv += u[i]*v[i];
         vv  += v[i]*v[i];
      }

      return Math.sqrt(vBv/vv);
   }


   /* return element i,j of infinite matrix A */
   private final double A(int i, int j){
      return 1.0/((i+j)*(i+j+1)/2 +i+1);
   }

   /* multiply vector v by matrix A */
   private final void MultiplyAv(int n, double[] v, double[] Av){
      for (int i=0; i<n; i++){
         Av[i] = 0;
         for (int j=0; j<n; j++) Av[i] += A(i,j)*v[j];
      }
   }

   /* multiply vector v by matrix A transposed */
   private final void MultiplyAtv(int n, double[] v, double[] Atv){
      for (int i=0;i<n;i++){
         Atv[i] = 0;
         for (int j=0; j<n; j++) Atv[i] += A(j,i)*v[j];
      }
   }

   /* multiply vector v by matrix A and then by matrix A transposed */
   private final void MultiplyAtAv(int n, double[] v, double[] AtAv){
      double[] u = new double[n];
      MultiplyAv(n,v,u);
      MultiplyAtv(n,u,AtAv);
   }
}
