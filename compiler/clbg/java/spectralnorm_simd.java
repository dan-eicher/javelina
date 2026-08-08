/* The Computer Language Benchmarks Game
   https://salsa.debian.org/benchmarksgame-team/benchmarksgame/

   Hand-vectorised variant of the published spectralnorm javaxint-1 program,
   using javelina.simd's v128 (f64x2). The same algorithm and the same four
   functions the task description requires — only the inner loops differ.

   This is the same move the fastest published C entry makes: spectralnorm
   gcc-5 is titled "with GCC SSE2 intrinsics" and runs `_mm_div_pd` over
   `__m128d` pairs. The measured comparison this exists for is our v128 through
   the copy-and-patch JIT against that.

   WHAT IS AND IS NOT VECTORISED, stated plainly so the number is readable:
   the ARITHMETIC is — two reciprocals per f64x2 divide, two multiply-adds per
   instruction — but the LOADS are not. There is no vector load from a Java
   double[], and putting the vectors in linear memory (where Mem.v128_load
   would apply) shares an address space with the jre's I/O staging, so it needs
   a memory map this file has no business assuming. Lane assembly via
   splat/replace_lane therefore costs two scalar loads per pair. If the win is
   small, that is the reason, and it is the honest measurement of what the v128
   path buys without a memory redesign.
*/

import javelina.simd.F64x2;
import javelina.simd.V128;

public class spectralnorm_simd
{
   public static void main(String[] args) {
      int n = 100;
      if (args.length > 0) n = Integer.parseInt(args[0]);

      System.out.println(fmt9(new spectralnorm_simd().Approximate(n)));
   }

   /* DecimalFormat("#.000000000") — java.text is 1.1; nine decimals by hand. */
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
      double[] u = new double[n];
      for (int i=0; i<n; i++) u[i] =  1;

      double[] v = new double[n];
      for (int i=0; i<n; i++) v[i] = 0;

      for (int i=0; i<10; i++) {
         MultiplyAtAv(n,u,v);
         MultiplyAtAv(n,v,u);
      }

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

   /* The two denominators for columns j and j+1, reciprocated together: one
      f64x2 divide where the scalar form does two. Integer overflow behaves
      exactly as the scalar A() does — the same int expression, evaluated in
      int, then widened. */
   private static V128 A2(int i, int j) {
      double d0 = (double)((i+j)*(i+j+1)/2 + i+1);
      double d1 = (double)((i+j+1)*(i+j+2)/2 + i+1);
      V128 den = F64x2.replace_lane(F64x2.splat(d0), d1, 1);
      return F64x2.div(F64x2.splat(1.0), den);
   }

   /* Same, for A transposed: rows j and j+1 of column i. */
   private static V128 At2(int i, int j) {
      double d0 = (double)((j+i)*(j+i+1)/2 + j+1);
      double d1 = (double)((j+1+i)*(j+1+i+1)/2 + j+2);
      V128 den = F64x2.replace_lane(F64x2.splat(d0), d1, 1);
      return F64x2.div(F64x2.splat(1.0), den);
   }

   private static double hadd(V128 acc) {
      return F64x2.extract_lane(acc, 0) + F64x2.extract_lane(acc, 1);
   }

   /* multiply vector v by matrix A */
   private final void MultiplyAv(int n, double[] v, double[] Av){
      int pairs = n & ~1;
      for (int i=0; i<n; i++){
         V128 acc = F64x2.splat(0.0);
         int j = 0;
         for (; j<pairs; j+=2) {
            V128 vv = F64x2.replace_lane(F64x2.splat(v[j]), v[j+1], 1);
            acc = F64x2.add(acc, F64x2.mul(A2(i,j), vv));
         }
         double s = hadd(acc);
         for (; j<n; j++) s += A(i,j)*v[j];
         Av[i] = s;
      }
   }

   /* multiply vector v by matrix A transposed */
   private final void MultiplyAtv(int n, double[] v, double[] Atv){
      int pairs = n & ~1;
      for (int i=0;i<n;i++){
         V128 acc = F64x2.splat(0.0);
         int j = 0;
         for (; j<pairs; j+=2) {
            V128 vv = F64x2.replace_lane(F64x2.splat(v[j]), v[j+1], 1);
            acc = F64x2.add(acc, F64x2.mul(At2(i,j), vv));
         }
         double s = hadd(acc);
         for (; j<n; j++) s += A(j,i)*v[j];
         Atv[i] = s;
      }
   }

   /* multiply vector v by matrix A and then by matrix A transposed */
   private final void MultiplyAtAv(int n, double[] v, double[] AtAv){
      double[] u = new double[n];
      MultiplyAv(n,v,u);
      MultiplyAtv(n,u,AtAv);
   }
}
