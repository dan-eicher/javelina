/* The Computer Language Benchmarks Game
   https://salsa.debian.org/benchmarksgame-team/benchmarksgame/
   contributed by Mike Pall
   java port by Stefan Krause

   PORT of the published pidigits javaxint-2 program. That entry computes with
   GMP through JNI -- System.loadLibrary("jgmplib"), mpz_mul_si, mpz_add,
   mpz_tdiv_q -- which is not available here, and the other Java entry uses
   java.math.BigInteger, which this runtime does not ship. The ALGORITHM is
   unchanged: the same unbounded spigot, the same 2x2 matrix, the same
   compose_l / compose_r / extract, the same output format.

   What replaces GMP is `Big` below, and it is deliberately no bigger than the
   published program's own bignum surface. Reading the GmpInteger wrapper it
   uses exactly five operations:

       set(int) | mul(Big, int) | add(Big, Big) | div(Big, Big) | intValue()

   so that is what `Big` provides. The C entry links GMP and other entries use
   their standard library; the arbitrary-precision arithmetic is infrastructure
   under the algorithm, not the algorithm.
*/

public class pidigits {

   Big q = new Big(), r = new Big(), s = new Big(), t = new Big();
   Big u = new Big(), v = new Big(), w = new Big();

   int i, k, c;
   int digit;
   int d;
   StringBuffer strBuf = new StringBuffer(20);
   int n;

   private pidigits(int n) {
      this.n = n;
   }

   private void compose_r(int bq, int br, int bs, int bt) {
      u.mul(r, bs);
      r.mul(r, bq);
      v.mul(t, br);
      r.add(r, v);
      t.mul(t, bt);
      t.add(t, u);
      s.mul(s, bt);
      u.mul(q, bs);
      s.add(s, u);
      q.mul(q, bq);
   }

   /* Compose matrix with numbers on the left. */
   private void compose_l(int bq, int br, int bs, int bt) {
      r.mul(r, bt);
      u.mul(q, br);
      r.add(r, u);
      u.mul(t, bs);
      t.mul(t, bt);
      v.mul(s, br);
      t.add(t, v);
      s.mul(s, bq);
      s.add(s, u);
      q.mul(q, bq);
   }

   /* Extract one digit. */
   private int extract(int j) {
      u.mul(q, j);
      u.add(u, r);
      v.mul(s, j);
      v.add(v, t);
      w.div(u, v);
      return w.intValue();
   }

   /* Print one digit. Returns 1 for the last digit. */
   private boolean prdigit(int y) {
      strBuf.append(y);
      if (++i % 10 == 0 || i == n) {
         if (i % 10 != 0) for (int j = 10 - (i % 10); j > 0; j--) { strBuf.append(" "); }
         strBuf.append("\t:");
         strBuf.append(i);
         System.out.println(strBuf);
         strBuf = new StringBuffer(20);
      }
      return i == n;
   }

   /* Generate successive digits of PI. */
   void pidigits() {
      int k = 1;
      d = 0;
      i = 0;
      q.set(1);
      r.set(0);
      s.set(0);
      t.set(1);
      for (;;) {
         int y = extract(3);
         if (y == extract(4)) {
            if (prdigit(y)) return;
            compose_r(10, -10 * y, 0, 1);
         } else {
            compose_l(k, 4 * k + 2, 0, 2 * k + 1);
            k++;
         }
      }
   }

   public static void main(String[] args) {
      pidigits m = new pidigits(Integer.parseInt(args[0]));
      m.pidigits();
   }
}

/* A signed arbitrary-precision integer, base 10^4, little-endian limbs, with
   long intermediates so a limb times a small multiplier cannot overflow. Only
   the five operations the spigot performs are here; every one of them may have
   its destination alias a source (`r.mul(r, bq)` is how the composition is
   written), so each computes into scratch and then adopts it. */
class Big {
   static final int BASE = 10000;

   int[] d = new int[4];
   int len = 1;        // limbs in use; a zero value is len 1, d[0] == 0
   int sign = 1;       // +1 or -1; zero is +1

   void set(int v) {
      sign = (v < 0) ? -1 : 1;
      long m = (v < 0) ? -(long) v : (long) v;
      len = 0;
      if (m == 0) { grow(1); d[0] = 0; len = 1; return; }
      grow(8);
      while (m > 0) {
         if (len == d.length) grow(len * 2);
         d[len++] = (int) (m % BASE);
         m = m / BASE;
      }
   }

   void grow(int want) {
      if (d.length >= want) return;
      int[] nd = new int[want];
      for (int i = 0; i < len; i++) nd[i] = d[i];
      d = nd;
   }

   void adopt(int[] nd, int nlen, int nsign) {
      d = nd;
      len = nlen;
      while (len > 1 && d[len - 1] == 0) len--;
      sign = (len == 1 && d[0] == 0) ? 1 : nsign;
   }

   boolean isZero() { return len == 1 && d[0] == 0; }

   /* dst = src * m, m a small signed int. */
   void mul(Big src, int m) {
      if (m == 0 || src.isZero()) { set(0); return; }
      int ms = (m < 0) ? -1 : 1;
      long mm = (m < 0) ? -(long) m : (long) m;
      int[] nd = new int[src.len + 8];
      long carry = 0;
      int nlen = 0;
      for (int i = 0; i < src.len; i++) {
         long cur = (long) src.d[i] * mm + carry;
         nd[nlen++] = (int) (cur % BASE);
         carry = cur / BASE;
      }
      while (carry > 0) {
         if (nlen == nd.length) { int[] g = new int[nlen * 2]; for (int i = 0; i < nlen; i++) g[i] = nd[i]; nd = g; }
         nd[nlen++] = (int) (carry % BASE);
         carry = carry / BASE;
      }
      adopt(nd, nlen, src.sign * ms);
   }

   /* dst = a + b, signs honoured. */
   void add(Big a, Big b) {
      if (a.sign == b.sign) {
         adopt(addMag(a, b), Math.max(a.len, b.len) + 1, a.sign);
         return;
      }
      int c = cmpMag(a, b);
      if (c == 0) { set(0); return; }
      if (c > 0) adopt(subMag(a, b), a.len, a.sign);
      else       adopt(subMag(b, a), b.len, b.sign);
   }

   static int[] addMag(Big a, Big b) {
      int n = Math.max(a.len, b.len) + 1;
      int[] nd = new int[n];
      int carry = 0;
      for (int i = 0; i < n; i++) {
         int x = (i < a.len) ? a.d[i] : 0;
         int y = (i < b.len) ? b.d[i] : 0;
         int cur = x + y + carry;
         nd[i] = cur % BASE;
         carry = cur / BASE;
      }
      return nd;
   }

   /* |a| - |b|, requiring |a| >= |b|. */
   static int[] subMag(Big a, Big b) {
      int[] nd = new int[a.len];
      int borrow = 0;
      for (int i = 0; i < a.len; i++) {
         int y = (i < b.len) ? b.d[i] : 0;
         int cur = a.d[i] - y - borrow;
         if (cur < 0) { cur += BASE; borrow = 1; } else borrow = 0;
         nd[i] = cur;
      }
      return nd;
   }

   static int cmpMag(Big a, Big b) {
      if (a.len != b.len) return (a.len > b.len) ? 1 : -1;
      for (int i = a.len - 1; i >= 0; i--) {
         if (a.d[i] != b.d[i]) return (a.d[i] > b.d[i]) ? 1 : -1;
      }
      return 0;
   }

   /* dst = a / b, truncated. The spigot only ever extracts a single decimal
      digit, so the quotient is small and repeated subtraction is the whole
      algorithm; the guard turns a violated assumption into a stop rather than a
      hang. */
   void div(Big a, Big b) {
      int[] rem = new int[a.len];
      for (int i = 0; i < a.len; i++) rem[i] = a.d[i];
      Big acc = new Big();
      acc.adopt(rem, a.len, 1);
      int qd = 0;
      while (cmpMag(acc, b) >= 0) {
         acc.adopt(subMag(acc, b), acc.len, 1);
         qd++;
         if (qd > 1000000) throw new ArithmeticException("pidigits: quotient not small");
      }
      set(qd * a.sign * b.sign);
   }

   int intValue() {
      long v = 0;
      for (int i = len - 1; i >= 0; i--) v = v * BASE + d[i];
      return (int) (v * sign);
   }
}
