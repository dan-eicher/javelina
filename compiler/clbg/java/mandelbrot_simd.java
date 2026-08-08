/* The Computer Language Benchmarks Game
   https://salsa.debian.org/benchmarksgame-team/benchmarksgame/

   line-by-line from Greg Buchholz's C program

   The published mandelbrot javaxint-8 program, with the escape iteration
   carried two pixels at a time in a javelina.simd f64x2 — the same shape the
   game's own C entries use with SSE2, which the rules permit ("with GCC SSE2
   intrinsics" is a published variant name). The ALGORITHM is unchanged: same
   iteration, same 50-iteration cap, same limit, same bit packing, same output.

   Why this task and not spectralnorm: the inner loop touches no arrays. Cr and
   Ci are computed from the pixel index into locals, so lane assembly is paid
   ONCE per pixel pair and amortised over up to 50 iterations of pure vector
   arithmetic. spectralnorm's inner loop reads a double[] every step, and there
   is no vector load from a Java array — its lanes must be rebuilt per element,
   which is where its win goes.

   The vector runs both lanes for the full 50 iterations unless BOTH have
   escaped, where the scalar stops each pixel at its own escape. That does not
   change the result: an escaped lane's Tr+Ti only grows (to infinity at worst,
   and inf <= 4.0 is false), so the final per-lane test answers the same as the
   scalar's test at its own exit point.
*/

import javelina.simd.V128;
import javelina.simd.F64x2;
import javelina.simd.I64x2;

class mandelbrot_simd {

public static void main(String[] args) {

    int w, h, bit_num = 0;
    int byte_acc = 0;
    int i, iter = 50;
    int x, y;
    double limit = 2.0;

    w = h = Integer.parseInt(args[0]);

    System.out.println("P4\n"+ w + " " + h);

    V128 vlimit = F64x2.splat(limit * limit);
    V128 vtwo   = F64x2.splat(2.0);

    for(y=0;y<h;y++)
    {
        V128 Ci = F64x2.splat(2.0*(double)y/h - 1.0);

        for(x=0;x<w;x+=2)
        {
            /* Cr for this pixel and its neighbour. The second lane is the
             * column x+1; when w is odd the final pair's high lane is a column
             * past the row, computed and then discarded by the bit loop. */
            double cr0 = 2.0*(double)x/w - 1.5;
            double cr1 = 2.0*(double)(x+1)/w - 1.5;
            V128 Cr = F64x2.replace_lane(F64x2.splat(cr0), cr1, 1);

            V128 Zr = F64x2.splat(0.0);
            V128 Zi = F64x2.splat(0.0);
            V128 Tr = F64x2.splat(0.0);
            V128 Ti = F64x2.splat(0.0);

            for (i=0;i<iter;++i)
            {
                /* Stop only when BOTH lanes have escaped. */
                if (V128.any_true(F64x2.le(F64x2.add(Tr, Ti), vlimit)) == 0) break;

                Zi = F64x2.add(F64x2.mul(F64x2.mul(vtwo, Zr), Zi), Ci);
                Zr = F64x2.add(F64x2.sub(Tr, Ti), Cr);
                Tr = F64x2.mul(Zr, Zr);
                Ti = F64x2.mul(Zi, Zi);
            }

            /* Per-lane "still inside": lane k of the mask is all-ones when
             * Tr+Ti <= 4.0, and bitmask gathers the lane sign bits, so bit k
             * is pixel x+k's result. */
            int inside = I64x2.bitmask(F64x2.le(F64x2.add(Tr, Ti), vlimit));

            for (int k = 0; k < 2 && x + k < w; ++k)
            {
                byte_acc <<= 1;
                if ((inside & (1 << k)) != 0) byte_acc |= 0x01;

                ++bit_num;

                if(bit_num == 8)
                {
                    System.out.write(byte_acc);
                    byte_acc = 0;
                    bit_num = 0;
                }
                else if(x + k == w-1)
                {
                    byte_acc = byte_acc << (8-w%8);
                    System.out.write(byte_acc);
                    byte_acc = 0;
                    bit_num = 0;
                }
            }
        }
    }
    System.out.flush();
}
}
