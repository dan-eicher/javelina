package java.lang;

public final class Math {
    public static final double E = 2.7182818284590452354;
    public static final double PI = 3.14159265358979323846;
    // ── §20.11 sin/cos/tan — fdlibm s_sin/s_cos/s_tan + k_sin/k_cos/k_tan + argument reduction
    //    (e_rem_pio2 medium range + k_rem_pio2 Payne–Hanek for huge |x|). ──
    private static final int[]    trig_two_over_pi = {
        0xA2F983, 0x6E4E44, 0x1529FC, 0x2757D1, 0xF534DD, 0xC0DB62, 0x95993C, 0x439041, 0xFE5163, 0xABDEBB,
        0xC561B7, 0x246E3A, 0x424DD2, 0xE00649, 0x2EEA09, 0xD1921C, 0xFE1DEB, 0x1CB129, 0xA73EE8, 0x8235F5,
        0x2EBB44, 0x84E99C, 0x7026B4, 0x5F7E41, 0x3991D6, 0x398353, 0x39F49C, 0x845F8B, 0xBDF928, 0x3B1FF8,
        0x97FFDE, 0x05980F, 0xEF2F11, 0x8B5A0A, 0x6D1F6D, 0x367ECF, 0x27CB09, 0xB74F46, 0x3F669E, 0x5FEA2D,
        0x7527BA, 0xC7EBE5, 0xF17B3D, 0x0739F7, 0x8A5292, 0xEA6BFB, 0x5FB11F, 0x8D5D08, 0x560330, 0x46FC7B,
        0x6BABF0, 0xCFBC20, 0x9AF436, 0x1DA9E3, 0x91615E, 0xE61B08, 0x659985, 0x5F14A0, 0x68408D, 0xFFD880,
        0x4D7327, 0x310606, 0x1556CA, 0x73A8C9, 0x60E27B, 0xC08C6B };
    private static final double[] trig_PIo2 = {
        1.57079625129699707031e+00, 7.54978941586159635335e-08, 5.39030252995776476554e-15, 3.28200341580791294123e-22,
        1.27065575308067607349e-29, 1.22933308981111328932e-36, 2.73370053816464559624e-44, 2.16741683877804819444e-51 };
    private static final int[] trig_init_jk = {2, 3, 4, 6};
    private static final int[] trig_npio2_hw = {
        0x3FF921FB, 0x400921FB, 0x4012D97C, 0x401921FB, 0x401F6A7A, 0x4022D97C, 0x4025FDBB, 0x402921FB,
        0x402C463A, 0x402F6A7A, 0x4031475C, 0x4032D97C, 0x40346B9C, 0x4035FDBB, 0x40378FDB, 0x403921FB,
        0x403AB41B, 0x403C463A, 0x403DD85A, 0x403F6A7A, 0x40407E4C, 0x4041475C, 0x4042106C, 0x4042D97C,
        0x4043A28C, 0x40446B9C, 0x404534AC, 0x4045FDBB, 0x4046C6CB, 0x40478FDB, 0x404858EB, 0x404921FB };

    // fdlibm __kernel_rem_pio2 (Payne–Hanek). ipio2 is trig_two_over_pi; the C `goto recompute` is the while-loop.
    private static int kernel_rem_pio2(double[] x, double[] y, int e0, int nx, int prec) {
        double zero = 0.0, one = 1.0, two24 = 1.67772160000000000000e+07, twon24 = 5.96046447753906250000e-08;
        int jz, jx, jv, jp, jk, carry, n, i, j, k, m, q0, ih;
        int[] iq = new int[20];
        double z, fw;
        double[] f = new double[20], fq = new double[20], q = new double[20];
        jk = trig_init_jk[prec];
        jp = jk;
        jx = nx - 1;
        jv = (e0 - 3) / 24; if (jv < 0) jv = 0;
        q0 = e0 - 24 * (jv + 1);
        j = jv - jx; m = jx + jk;
        for (i = 0; i <= m; i++, j++) f[i] = (j < 0) ? zero : (double) trig_two_over_pi[j];
        for (i = 0; i <= jk; i++) { fw = 0.0; for (j = 0; j <= jx; j++) fw += x[j] * f[jx + i - j]; q[i] = fw; }
        jz = jk;
        while (true) {                                   // recompute:
            for (i = 0, j = jz, z = q[jz]; j > 0; i++, j--) {
                fw = (double) ((int) (twon24 * z));
                iq[i] = (int) (z - two24 * fw);
                z = q[j - 1] + fw;
            }
            z = scalb(z, q0);
            z -= 8.0 * floor(z * 0.125);
            n = (int) z;
            z -= (double) n;
            ih = 0;
            if (q0 > 0) {
                i = (iq[jz - 1] >> (24 - q0)); n += i;
                iq[jz - 1] -= i << (24 - q0);
                ih = iq[jz - 1] >> (23 - q0);
            } else if (q0 == 0) ih = iq[jz - 1] >> 23;
            else if (z >= 0.5) ih = 2;
            if (ih > 0) {
                n += 1; carry = 0;
                for (i = 0; i < jz; i++) {
                    j = iq[i];
                    if (carry == 0) { if (j != 0) { carry = 1; iq[i] = 0x1000000 - j; } }
                    else iq[i] = 0xffffff - j;
                }
                if (q0 > 0) {
                    switch (q0) { case 1: iq[jz - 1] &= 0x7fffff; break; case 2: iq[jz - 1] &= 0x3fffff; break; }
                }
                if (ih == 2) { z = one - z; if (carry != 0) z -= scalb(one, q0); }
            }
            if (z == zero) {
                j = 0;
                for (i = jz - 1; i >= jk; i--) j |= iq[i];
                if (j == 0) {
                    for (k = 1; iq[jk - k] == 0; k++) ;
                    for (i = jz + 1; i <= jz + k; i++) {
                        f[jx + i] = (double) trig_two_over_pi[jv + i];
                        fw = 0.0; for (j = 0; j <= jx; j++) fw += x[j] * f[jx + i - j];
                        q[i] = fw;
                    }
                    jz += k;
                    continue;                            // goto recompute
                }
            }
            break;
        }
        if (z == 0.0) { jz -= 1; q0 -= 24; while (iq[jz] == 0) { jz--; q0 -= 24; } }
        else {
            z = scalb(z, -q0);
            if (z >= two24) { fw = (double) ((int) (twon24 * z)); iq[jz] = (int) (z - two24 * fw); jz += 1; q0 += 24; iq[jz] = (int) fw; }
            else iq[jz] = (int) z;
        }
        fw = scalb(one, q0);
        for (i = jz; i >= 0; i--) { q[i] = fw * (double) iq[i]; fw *= twon24; }
        for (i = jz; i >= 0; i--) { fw = 0.0; for (k = 0; k <= jp && k <= jz - i; k++) fw += trig_PIo2[k] * q[i + k]; fq[jz - i] = fw; }
        switch (prec) {
            case 0: fw = 0.0; for (i = jz; i >= 0; i--) fw += fq[i]; y[0] = (ih == 0) ? fw : -fw; break;
            case 1: case 2:
                fw = 0.0; for (i = jz; i >= 0; i--) fw += fq[i];
                y[0] = (ih == 0) ? fw : -fw;
                fw = fq[0] - fw; for (i = 1; i <= jz; i++) fw += fq[i];
                y[1] = (ih == 0) ? fw : -fw;
                break;
            default:                                     // case 3
                for (i = jz; i > 0; i--) { fw = fq[i - 1] + fq[i]; fq[i] += fq[i - 1] - fw; fq[i - 1] = fw; }
                for (i = jz; i > 1; i--) { fw = fq[i - 1] + fq[i]; fq[i] += fq[i - 1] - fw; fq[i - 1] = fw; }
                fw = 0.0; for (i = jz; i >= 2; i--) fw += fq[i];
                if (ih == 0) { y[0] = fq[0]; y[1] = fq[1]; y[2] = fw; } else { y[0] = -fq[0]; y[1] = -fq[1]; y[2] = -fw; }
        }
        return n & 7;
    }

    // fdlibm __ieee754_rem_pio2: reduce x to y[0]+y[1] in [-pi/4,pi/4], return quadrant n.
    private static int rem_pio2(double x, double[] y) {
        double zero = 0.0, half = 5.00000000000000000000e-01, two24 = 1.67772160000000000000e+07;
        double invpio2 = 6.36619772367581382433e-01;
        double pio2_1 = 1.57079632673412561417e+00, pio2_1t = 6.07710050650619224932e-11;
        double pio2_2 = 6.07710050630396597660e-11, pio2_2t = 2.02226624879595063154e-21;
        double pio2_3 = 2.02226624871116645580e-21, pio2_3t = 8.47842766036889956997e-32;
        double z, w, t, r, fn;
        double[] tx = new double[3];
        int e0, i, j, nx, n, ix, hx;
        hx = __HI(x); ix = hx & 0x7fffffff;
        if (ix <= 0x3fe921fb) { y[0] = x; y[1] = 0; return 0; }             // |x| <= pi/4
        if (ix < 0x4002d97c) {                                              // |x| < 3pi/4, n = +-1
            if (hx > 0) {
                z = x - pio2_1;
                if (ix != 0x3ff921fb) { y[0] = z - pio2_1t; y[1] = (z - y[0]) - pio2_1t; }
                else { z -= pio2_2; y[0] = z - pio2_2t; y[1] = (z - y[0]) - pio2_2t; }
                return 1;
            } else {
                z = x + pio2_1;
                if (ix != 0x3ff921fb) { y[0] = z + pio2_1t; y[1] = (z - y[0]) + pio2_1t; }
                else { z += pio2_2; y[0] = z + pio2_2t; y[1] = (z - y[0]) + pio2_2t; }
                return -1;
            }
        }
        if (ix <= 0x413921fb) {                                             // medium: |x| ~<= 2^19*(pi/2)
            t = abs(x);
            n = (int) (t * invpio2 + half);
            fn = (double) n;
            r = t - fn * pio2_1;
            w = fn * pio2_1t;
            if (n < 32 && ix != trig_npio2_hw[n - 1]) { y[0] = r - w; }
            else {
                j = ix >> 20;
                y[0] = r - w;
                i = j - (((__HI(y[0])) >> 20) & 0x7ff);
                if (i > 16) {
                    t = r; w = fn * pio2_2; r = t - w; w = fn * pio2_2t - ((t - r) - w); y[0] = r - w;
                    i = j - (((__HI(y[0])) >> 20) & 0x7ff);
                    if (i > 49) { t = r; w = fn * pio2_3; r = t - w; w = fn * pio2_3t - ((t - r) - w); y[0] = r - w; }
                }
            }
            y[1] = (r - y[0]) - w;
            if (hx < 0) { y[0] = -y[0]; y[1] = -y[1]; return -n; }
            return n;
        }
        if (ix >= 0x7ff00000) { y[0] = y[1] = x - x; return 0; }            // inf/NaN
        z = __LO(0.0, __LO(x));
        e0 = (ix >> 20) - 1046;
        z = __HI(z, ix - (e0 << 20));
        for (i = 0; i < 2; i++) { tx[i] = (double) ((int) (z)); z = (z - tx[i]) * two24; }
        tx[2] = z;
        nx = 3;
        while (tx[nx - 1] == zero) nx--;
        double[] ty = new double[3];
        n = kernel_rem_pio2(tx, ty, e0, nx, 2);
        y[0] = ty[0]; y[1] = ty[1];
        if (hx < 0) { y[0] = -y[0]; y[1] = -y[1]; return -n; }
        return n;
    }

    private static double kernel_sin(double x, double y, int iy) {
        double half = 5.00000000000000000000e-01;
        double S1 = -1.66666666666666324348e-01, S2 = 8.33333333332248946124e-03, S3 = -1.98412698298579493134e-04;
        double S4 = 2.75573137070700676789e-06, S5 = -2.50507602534068634195e-08, S6 = 1.58969099521155010221e-10;
        double z, r, v;
        int ix = __HI(x) & 0x7fffffff;
        if (ix < 0x3e400000) { if ((int) x == 0) return x; }
        z = x * x; v = z * x;
        r = S2 + z * (S3 + z * (S4 + z * (S5 + z * S6)));
        if (iy == 0) return x + v * (S1 + z * r);
        else return x - ((z * (half * y - v * r) - y) - v * S1);
    }
    private static double kernel_cos(double x, double y) {
        double one = 1.0;
        double C1 = 4.16666666666666019037e-02, C2 = -1.38888888888741095749e-03, C3 = 2.48015872894767294178e-05;
        double C4 = -2.75573143513906633035e-07, C5 = 2.08757232129817482790e-09, C6 = -1.13596475577881948265e-11;
        double a, hz, z, r, qx = 0.0;
        int ix = __HI(x) & 0x7fffffff;
        if (ix < 0x3e400000) { if ((int) x == 0) return one; }
        z = x * x;
        r = z * (C1 + z * (C2 + z * (C3 + z * (C4 + z * (C5 + z * C6)))));
        if (ix < 0x3FD33333) return one - (0.5 * z - (z * r - x * y));
        else {
            if (ix > 0x3fe90000) qx = 0.28125;
            else { qx = __HI(0.0, ix - 0x00200000); qx = __LO(qx, 0); }
            hz = 0.5 * z - qx; a = one - qx;
            return a - (hz - (z * r - x * y));
        }
    }
    public static double sin(double x) {
        double[] y = new double[2]; double z = 0.0;
        int n, ix = __HI(x) & 0x7fffffff;
        if (ix <= 0x3fe921fb) return kernel_sin(x, z, 0);
        else if (ix >= 0x7ff00000) return x - x;
        else {
            n = rem_pio2(x, y);
            switch (n & 3) {
                case 0: return kernel_sin(y[0], y[1], 1);
                case 1: return kernel_cos(y[0], y[1]);
                case 2: return -kernel_sin(y[0], y[1], 1);
                default: return -kernel_cos(y[0], y[1]);
            }
        }
    }
    public static double cos(double x) {
        double[] y = new double[2]; double z = 0.0;
        int n, ix = __HI(x) & 0x7fffffff;
        if (ix <= 0x3fe921fb) return kernel_cos(x, z);
        else if (ix >= 0x7ff00000) return x - x;
        else {
            n = rem_pio2(x, y);
            switch (n & 3) {
                case 0: return kernel_cos(y[0], y[1]);
                case 1: return -kernel_sin(y[0], y[1], 1);
                case 2: return -kernel_cos(y[0], y[1]);
                default: return kernel_sin(y[0], y[1], 1);
            }
        }
    }
    // §20.11 tan(x) — fdlibm s_tan + k_tan (reuses rem_pio2).
    private static final double[] tan_T = {
        3.33333333333334091986e-01, 1.33333333333201242699e-01, 5.39682539762260521377e-02, 2.18694882948595424599e-02,
        8.86323982359930005737e-03, 3.59207910759131235356e-03, 1.45620945432529025516e-03, 5.88041240820264096874e-04,
        2.46463134818469906812e-04, 7.81794442939557092300e-05, 7.14072491382608190305e-05, -1.85586374855275456654e-05,
        2.59073051863633712884e-05 };
    private static double kernel_tan(double x, double y, int iy) {
        double one = 1.0, pio4 = 7.85398163397448278999e-01, pio4lo = 3.06161699786838301793e-17;
        double z, r, v, w, s, a, t;
        int hx = __HI(x), ix = hx & 0x7fffffff;
        if (ix < 0x3e300000) {                                       // x < 2^-28
            if ((int) x == 0) {
                if (((ix | __LO(x)) | (iy + 1)) == 0) return one / abs(x);
                if (iy == 1) return x;
                z = w = x + y; z = __LO(z, 0);
                v = y - (z - x);
                t = a = -one / w; t = __LO(t, 0);
                s = one + t * z;
                return t + a * (s + t * v);
            }
        }
        if (ix >= 0x3FE59428) {                                      // |x| >= 0.6744
            if (hx < 0) { x = -x; y = -y; }
            z = pio4 - x; w = pio4lo - y;
            x = z + w; y = 0.0;
        }
        z = x * x; w = z * z;
        r = tan_T[1] + w * (tan_T[3] + w * (tan_T[5] + w * (tan_T[7] + w * (tan_T[9] + w * tan_T[11]))));
        v = z * (tan_T[2] + w * (tan_T[4] + w * (tan_T[6] + w * (tan_T[8] + w * (tan_T[10] + w * tan_T[12])))));
        s = z * x;
        r = y + z * (s * (r + v) + y);
        r += tan_T[0] * s;
        w = x + r;
        if (ix >= 0x3FE59428) {
            v = (double) iy;
            return (double) (1 - ((hx >> 30) & 2)) * (v - 2.0 * (x - (w * w / (w + v) - r)));
        }
        if (iy == 1) return w;
        z = w; z = __LO(z, 0);
        v = r - (z - x);
        t = a = -1.0 / w; t = __LO(t, 0);
        s = 1.0 + t * z;
        return t + a * (s + t * v);
    }
    public static double tan(double x) {
        double[] y = new double[2]; double z = 0.0;
        int n, ix = __HI(x) & 0x7fffffff;
        if (ix <= 0x3fe921fb) return kernel_tan(x, z, 1);
        else if (ix >= 0x7ff00000) return x - x;
        n = rem_pio2(x, y);
        return kernel_tan(y[0], y[1], 1 - ((n & 1) << 1));
    }
    // §20.11 asin(x) — fdlibm e_asin.
    public static double asin(double x) {
        double one = 1.0, huge = 1.0e300;
        double pio2_hi = 1.57079632679489655800e+00, pio2_lo = 6.12323399573676603587e-17;
        double pio4_hi = 7.85398163397448278999e-01;
        double pS0 = 1.66666666666666657415e-01, pS1 = -3.25565818622400915405e-01, pS2 = 2.01212532134862925881e-01;
        double pS3 = -4.00555345006794114027e-02, pS4 = 7.91534994289814532176e-04, pS5 = 3.47933107596021167570e-05;
        double qS1 = -2.40339491173441421878e+00, qS2 = 2.02094576023350569471e+00, qS3 = -6.88283971605453293030e-01, qS4 = 7.70381505559019352791e-02;
        double t = 0, w, p, q, c, r, s;
        int hx = __HI(x), ix = hx & 0x7fffffff;
        if (ix >= 0x3ff00000) {                                      // |x| >= 1
            if (((ix - 0x3ff00000) | __LO(x)) == 0) return x * pio2_hi + x * pio2_lo;
            return (x - x) / (x - x);                                // NaN
        } else if (ix < 0x3fe00000) {                                // |x| < 0.5
            if (ix < 0x3e400000) { if (huge + x > one) return x; }
            else t = x * x;
            p = t * (pS0 + t * (pS1 + t * (pS2 + t * (pS3 + t * (pS4 + t * pS5)))));
            q = one + t * (qS1 + t * (qS2 + t * (qS3 + t * qS4)));
            w = p / q;
            return x + x * w;
        }
        w = one - abs(x);
        t = w * 0.5;
        p = t * (pS0 + t * (pS1 + t * (pS2 + t * (pS3 + t * (pS4 + t * pS5)))));
        q = one + t * (qS1 + t * (qS2 + t * (qS3 + t * qS4)));
        s = sqrt(t);
        if (ix >= 0x3FEF3333) { w = p / q; t = pio2_hi - (2.0 * (s + s * w) - pio2_lo); }
        else {
            w = s; w = __LO(w, 0);
            c = (t - w * w) / (s + w);
            r = p / q;
            p = 2.0 * s * r - (pio2_lo - 2.0 * c);
            q = pio4_hi - 2.0 * w;
            t = pio4_hi - (p - q);
        }
        return (hx > 0) ? t : -t;
    }

    // §20.11 acos(x) — fdlibm e_acos (shares the pS/qS rational with asin).
    public static double acos(double x) {
        double one = 1.0;
        double pi = 3.14159265358979311600e+00;
        double pio2_hi = 1.57079632679489655800e+00, pio2_lo = 6.12323399573676603587e-17;
        double pS0 = 1.66666666666666657415e-01, pS1 = -3.25565818622400915405e-01, pS2 = 2.01212532134862925881e-01;
        double pS3 = -4.00555345006794114027e-02, pS4 = 7.91534994289814532176e-04, pS5 = 3.47933107596021167570e-05;
        double qS1 = -2.40339491173441421878e+00, qS2 = 2.02094576023350569471e+00, qS3 = -6.88283971605453293030e-01, qS4 = 7.70381505559019352791e-02;
        double z, p, q, r, w, s, c, df;
        int hx = __HI(x), ix = hx & 0x7fffffff;
        if (ix >= 0x3ff00000) {                                      // |x| >= 1
            if (((ix - 0x3ff00000) | __LO(x)) == 0) { if (hx > 0) return 0.0; else return pi + 2.0 * pio2_lo; }
            return (x - x) / (x - x);
        }
        if (ix < 0x3fe00000) {                                       // |x| < 0.5
            if (ix <= 0x3c600000) return pio2_hi + pio2_lo;
            z = x * x;
            p = z * (pS0 + z * (pS1 + z * (pS2 + z * (pS3 + z * (pS4 + z * pS5)))));
            q = one + z * (qS1 + z * (qS2 + z * (qS3 + z * qS4)));
            r = p / q;
            return pio2_hi - (x - (pio2_lo - x * r));
        } else if (hx < 0) {                                         // x < -0.5
            z = (one + x) * 0.5;
            p = z * (pS0 + z * (pS1 + z * (pS2 + z * (pS3 + z * (pS4 + z * pS5)))));
            q = one + z * (qS1 + z * (qS2 + z * (qS3 + z * qS4)));
            s = sqrt(z);
            r = p / q;
            w = r * s - pio2_lo;
            return pi - 2.0 * (s + w);
        } else {                                                     // x > 0.5
            z = (one - x) * 0.5;
            s = sqrt(z);
            df = s; df = __LO(df, 0);
            c = (z - df * df) / (s + df);
            p = z * (pS0 + z * (pS1 + z * (pS2 + z * (pS3 + z * (pS4 + z * pS5)))));
            q = one + z * (qS1 + z * (qS2 + z * (qS3 + z * qS4)));
            r = p / q;
            w = r * s + c;
            return 2.0 * (df + w);
        }
    }

    // §20.11 atan(x) — fdlibm s_atan.
    private static final double[] atan_hi = {4.63647609000806093515e-01, 7.85398163397448278999e-01, 9.82793723247329054082e-01, 1.57079632679489655800e+00};
    private static final double[] atan_lo = {2.26987774529616870924e-17, 3.06161699786838301793e-17, 1.39033110312309984516e-17, 6.12323399573676603587e-17};
    private static final double[] atan_T  = {3.33333333333329318027e-01, -1.99999999998764832476e-01, 1.42857142725034663711e-01, -1.11111104054623557880e-01, 9.09088713343650656196e-02, -7.69187620504482999495e-02, 6.66107313738753120669e-02, -5.83357013379057348645e-02, 4.97687799461593236017e-02, -3.65315727442169155270e-02, 1.62858201153657823623e-02};
    public static double atan(double x) {
        double one = 1.0, huge = 1.0e300;
        double w, s1, s2, z;
        int hx = __HI(x), ix = hx & 0x7fffffff, id;
        if (ix >= 0x44100000) {                                      // |x| >= 2^66
            if (ix > 0x7ff00000 || (ix == 0x7ff00000 && (__LO(x) != 0))) return x + x;
            if (hx > 0) return atan_hi[3] + atan_lo[3]; else return -atan_hi[3] - atan_lo[3];
        }
        if (ix < 0x3fdc0000) {                                       // |x| < 0.4375
            if (ix < 0x3e200000) { if (huge + x > one) return x; }
            id = -1;
        } else {
            x = abs(x);
            if (ix < 0x3ff30000) {                                   // |x| < 1.1875
                if (ix < 0x3fe60000) { id = 0; x = (2.0 * x - one) / (2.0 + x); }
                else { id = 1; x = (x - one) / (x + one); }
            } else {
                if (ix < 0x40038000) { id = 2; x = (x - 1.5) / (one + 1.5 * x); }
                else { id = 3; x = -1.0 / x; }
            }
        }
        z = x * x;
        w = z * z;
        s1 = z * (atan_T[0] + w * (atan_T[2] + w * (atan_T[4] + w * (atan_T[6] + w * (atan_T[8] + w * atan_T[10])))));
        s2 = w * (atan_T[1] + w * (atan_T[3] + w * (atan_T[5] + w * (atan_T[7] + w * atan_T[9]))));
        if (id < 0) return x - x * (s1 + s2);
        z = atan_hi[id] - ((x * (s1 + s2) - atan_lo[id]) - x);
        return (hx < 0) ? -z : z;
    }

    // §20.11 atan2(y,x) — fdlibm e_atan2 (uses atan). NB: C's unsigned `>>` → Java `>>>`.
    public static double atan2(double y, double x) {
        double tiny = 1.0e-300, zero = 0.0;
        double pi_o_4 = 7.8539816339744827900e-01, pi_o_2 = 1.5707963267948965580e+00;
        double pi = 3.1415926535897931160e+00, pi_lo = 1.2246467991473531772e-16;
        double z;
        int k, m, hx, hy, ix, iy, lx, ly;
        hx = __HI(x); ix = hx & 0x7fffffff; lx = __LO(x);
        hy = __HI(y); iy = hy & 0x7fffffff; ly = __LO(y);
        if (((ix | ((lx | -lx) >>> 31)) > 0x7ff00000) || ((iy | ((ly | -ly) >>> 31)) > 0x7ff00000)) return x + y;
        if (((hx - 0x3ff00000) | lx) == 0) return atan(y);          // x = 1.0
        m = ((hy >> 31) & 1) | ((hx >> 30) & 2);                    // 2*sign(x)+sign(y)
        if ((iy | ly) == 0) {
            switch (m) { case 0: case 1: return y; case 2: return pi + tiny; case 3: return -pi - tiny; }
        }
        if ((ix | lx) == 0) return (hy < 0) ? -pi_o_2 - tiny : pi_o_2 + tiny;
        if (ix == 0x7ff00000) {
            if (iy == 0x7ff00000) {
                switch (m) { case 0: return pi_o_4 + tiny; case 1: return -pi_o_4 - tiny; case 2: return 3.0 * pi_o_4 + tiny; case 3: return -3.0 * pi_o_4 - tiny; }
            } else {
                switch (m) { case 0: return zero; case 1: return -1.0 * zero; case 2: return pi + tiny; case 3: return -pi - tiny; }
            }
        }
        if (iy == 0x7ff00000) return (hy < 0) ? -pi_o_2 - tiny : pi_o_2 + tiny;
        k = (iy - ix) >> 20;
        if (k > 60) z = pi_o_2 + 0.5 * pi_lo;
        else if (hx < 0 && k < -60) z = 0.0;
        else z = atan(abs(y / x));
        switch (m) {
            case 0: return z;
            case 1: return __HI(z, __HI(z) ^ 0x80000000);
            case 2: return pi - (z - pi_lo);
            default: return (z - pi_lo) - pi;
        }
    }
    // ── fdlibm word-access helpers (OpenJDK FdLibm.java, via Double.*Bits reinterpret intrinsics) ──
    private static int __LO(double x) { return (int) Double.doubleToRawLongBits(x); }
    private static double __LO(double x, int low) {
        long t = Double.doubleToRawLongBits(x);
        return Double.longBitsToDouble((t & 0xFFFFFFFF00000000L) | (low & 0xFFFFFFFFL));
    }
    private static int __HI(double x) { return (int) (Double.doubleToRawLongBits(x) >> 32); }
    private static double __HI(double x, int high) {
        long t = Double.doubleToRawLongBits(x);
        return Double.longBitsToDouble((t & 0xFFFFFFFFL) | (((long) high) << 32));
    }

    // §20.11 exp(x) — fdlibm e_exp (OpenJDK FdLibm.Exp), exact decimal constants.
    public static double exp(double x) {
        double one = 1.0;
        double huge = 1.0e300;
        double twom1000 = 9.33263618503218878990e-302;
        double o_threshold = 7.09782712893383973096e+02;
        double u_threshold = -7.45133219101941108420e+02;
        double ln2HI0 = 6.93147180369123816490e-01, ln2HI1 = -6.93147180369123816490e-01;
        double ln2LO0 = 1.90821492927058770002e-10, ln2LO1 = -1.90821492927058770002e-10;
        double invln2 = 1.44269504088896338700e+00;
        double P1 = 1.66666666666666019037e-01;
        double P2 = -2.77777777770155933842e-03;
        double P3 = 6.61375632143793436117e-05;
        double P4 = -1.65339022054652515390e-06;
        double P5 = 4.13813679705723846039e-08;
        double y, hi = 0.0, lo = 0.0, c, t;
        int k = 0, xsb;
        int hx = __HI(x);
        xsb = (hx >> 31) & 1;
        hx &= 0x7fffffff;
        if (hx >= 0x40862E42) {                          // |x| >= 709.78
            if (hx >= 0x7ff00000) {
                if (((hx & 0xfffff) | __LO(x)) != 0) return x + x;   // NaN
                else return (xsb == 0) ? x : 0.0;                    // exp(+-inf)
            }
            if (x > o_threshold) return huge * huge;                 // overflow
            if (x < u_threshold) return twom1000 * twom1000;         // underflow
        }
        if (hx > 0x3fd62e42) {                           // |x| > 0.5 ln2
            if (hx < 0x3FF0A2B2) {                       // |x| < 1.5 ln2
                hi = x - (xsb == 0 ? ln2HI0 : ln2HI1);
                lo = (xsb == 0 ? ln2LO0 : ln2LO1);
                k = 1 - xsb - xsb;
            } else {
                k = (int) (invln2 * x + (xsb == 0 ? 0.5 : -0.5));
                t = k;
                hi = x - t * ln2HI0;                     // t*ln2HI is exact
                lo = t * ln2LO0;
            }
            x = hi - lo;
        } else if (hx < 0x3e300000) {                    // |x| < 2^-28
            if (huge + x > one) return one + x;
        } else {
            k = 0;
        }
        t = x * x;
        c = x - t * (P1 + t * (P2 + t * (P3 + t * (P4 + t * P5))));
        if (k == 0) return one - ((x * c) / (c - 2.0) - x);
        else y = one - ((lo - (x * c) / (2.0 - c)) - hi);
        if (k >= -1021) {
            return __HI(y, __HI(y) + (k << 20));
        } else {
            y = __HI(y, __HI(y) + ((k + 1000) << 20));
            return y * twom1000;
        }
    }
    // §20.11 log(x) — fdlibm e_log (__ieee754_log), exact-decimal constants.
    public static double log(double x) {
        double ln2_hi = 6.93147180369123816490e-01;
        double ln2_lo = 1.90821492927058770002e-10;
        double two54 = 1.80143985094819840000e+16;
        double Lg1 = 6.666666666666735130e-01, Lg2 = 3.999999999940941908e-01;
        double Lg3 = 2.857142874366239149e-01, Lg4 = 2.222219843214978396e-01;
        double Lg5 = 1.818357216161805012e-01, Lg6 = 1.531383769920937332e-01;
        double Lg7 = 1.479819860511658591e-01;
        double zero = 0.0;
        double hfsq, f, s, z, R, w, t1, t2, dk;
        int k, hx, i, j, lx;
        hx = __HI(x); lx = __LO(x);
        k = 0;
        if (hx < 0x00100000) {                                  // x < 2^-1022
            if (((hx & 0x7fffffff) | lx) == 0) return -two54 / zero;   // log(+-0) = -inf
            if (hx < 0) return (x - x) / zero;                        // log(-#) = NaN
            k -= 54; x = x * two54;
            hx = __HI(x);
        }
        if (hx >= 0x7ff00000) return x + x;
        k += (hx >> 20) - 1023;
        hx &= 0x000fffff;
        i = (hx + 0x95f64) & 0x100000;
        x = __HI(x, hx | (i ^ 0x3ff00000));                     // normalize x or x/2
        k += (i >> 20);
        f = x - 1.0;
        if ((0x000fffff & (2 + hx)) < 3) {                      // |f| < 2^-20
            if (f == zero) {
                if (k == 0) return zero;
                else { dk = (double) k; return dk * ln2_hi + dk * ln2_lo; }
            }
            R = f * f * (0.5 - 0.33333333333333333 * f);
            if (k == 0) return f - R;
            else { dk = (double) k; return dk * ln2_hi - ((R - dk * ln2_lo) - f); }
        }
        s = f / (2.0 + f);
        dk = (double) k;
        z = s * s;
        i = hx - 0x6147a;
        w = z * z;
        j = 0x6b851 - hx;
        t1 = w * (Lg2 + w * (Lg4 + w * Lg6));
        t2 = z * (Lg1 + w * (Lg3 + w * (Lg5 + w * Lg7)));
        i |= j;
        R = t2 + t1;
        if (i > 0) {
            hfsq = 0.5 * f * f;
            if (k == 0) return f - (hfsq - s * (hfsq + R));
            else return dk * ln2_hi - ((hfsq - (s * (hfsq + R) + dk * ln2_lo)) - f);
        } else {
            if (k == 0) return f - s * (f - R);
            else return dk * ln2_hi - ((s * (f - R) - dk * ln2_lo) - f);
        }
    }
    public static native double sqrt(double a);
    // fdlibm copysign + scalbn (needed by pow's subnormal-output path).
    private static double copysign(double x, double y) {
        long m = 0x7fffffffffffffffL;
        return Double.longBitsToDouble((Double.doubleToRawLongBits(x) & m) | (Double.doubleToRawLongBits(y) & ~m));
    }
    private static double scalb(double x, int n) {
        double two54 = 1.80143985094819840000e+16, twom54 = 5.55111512312578270212e-17;
        double huge = 1.0e300, tiny = 1.0e-300;
        int k, hx, lx;
        hx = __HI(x); lx = __LO(x);
        k = (hx & 0x7ff00000) >> 20;
        if (k == 0) {
            if ((lx | (hx & 0x7fffffff)) == 0) return x;
            x = x * two54; hx = __HI(x);
            k = ((hx & 0x7ff00000) >> 20) - 54;
            if (n < -50000) return tiny * x;
        }
        if (k == 0x7ff) return x + x;
        k = k + n;
        if (k > 0x7fe) return huge * copysign(huge, x);
        if (k > 0) return __HI(x, (hx & 0x800fffff) | (k << 20));
        if (k <= -54) return (n > 50000) ? huge * copysign(huge, x) : tiny * copysign(tiny, x);
        k = k + 54;
        x = __HI(x, (hx & 0x800fffff) | (k << 20));
        return x * twom54;
    }

    // §20.11 pow(x,y) — fdlibm e_pow (OpenJDK FdLibm.Pow). Exact-comment decimals; the 3 huge-|y|
    // thresholds computed exactly from the hex-float source.
    public static double pow(double x, double y) {
        double INFINITY = Double.POSITIVE_INFINITY;
        double z, r, s, t, u, v, w;
        int i, j, k, n;
        if (y == 0.0) return 1.0;
        if (Double.isNaN(x) || Double.isNaN(y)) return x + y;
        double y_abs = abs(y);
        double x_abs = abs(x);
        if (y == 2.0) return x * x;
        else if (y == 0.5) { if (x >= -Double.MAX_VALUE) return sqrt(x + 0.0); }
        else if (y_abs == 1.0) return (y == 1.0) ? x : 1.0 / x;
        else if (y_abs == INFINITY) {
            if (x_abs == 1.0) return y - y;
            else if (x_abs > 1.0) return (y >= 0) ? y : 0.0;
            else return (y < 0) ? -y : 0.0;
        }
        int hx = __HI(x), ix = hx & 0x7fffffff;
        int y_is_int = 0;
        if (hx < 0) {
            if (y_abs >= 9.007199254740992E15) y_is_int = 2;                 // |y| >= 2^53
            else if (y_abs >= 1.0) {
                long y_abs_as_long = (long) y_abs;
                if (((double) y_abs_as_long) == y_abs) y_is_int = 2 - (int) (y_abs_as_long & 0x1L);
            }
        }
        if (x_abs == 0.0 || x_abs == INFINITY || x_abs == 1.0) {
            z = x_abs;
            if (y < 0.0) z = 1.0 / z;
            if (hx < 0) {
                if (((ix - 0x3ff00000) | y_is_int) == 0) z = (z - z) / (z - z);
                else if (y_is_int == 1) z = -1.0 * z;
            }
            return z;
        }
        n = (hx >> 31) + 1;
        if ((n | y_is_int) == 0) return (x - x) / (x - x);
        s = 1.0;
        if ((n | (y_is_int - 1)) == 0) s = -1.0;
        double p_h, p_l, t1, t2;
        if (y_abs > 2147485695.9999995) {                                    // |y| > ~2^31
            double INV_LN2 = 1.44269504088896338700e+00;
            double INV_LN2_H = 1.44269502162933349609e+00;
            double INV_LN2_L = 1.92596299112661746887e-08;
            if (x_abs < 0.9999995231628418) return (y < 0.0) ? s * INFINITY : s * 0.0;
            if (x_abs > 1.0000009536743162) return (y > 0.0) ? s * INFINITY : s * 0.0;
            t = x_abs - 1.0;
            w = (t * t) * (0.5 - t * (0.3333333333333333333333 - t * 0.25));
            u = INV_LN2_H * t;
            v = t * INV_LN2_L - w * INV_LN2;
            t1 = u + v; t1 = __LO(t1, 0);
            t2 = v - (t1 - u);
        } else {
            double CP = 9.61796693925975554329e-01;
            double CP_H = 9.61796700954437255859e-01;
            double CP_L = -7.02846165095275826516e-09;
            double z_h, z_l, ss, s2, s_h, s_l, t_h, t_l;
            n = 0;
            if (ix < 0x00100000) { x_abs = x_abs * 9007199254740992.0; n -= 53; ix = __HI(x_abs); }
            n += ((ix) >> 20) - 0x3ff;
            j = ix & 0x000fffff;
            ix = j | 0x3ff00000;
            if (j <= 0x3988E) k = 0;
            else if (j < 0xBB67A) k = 1;
            else { k = 0; n += 1; ix -= 0x00100000; }
            x_abs = __HI(x_abs, ix);
            double BP_k = (k == 0) ? 1.0 : 1.5;
            double DP_H_k = (k == 0) ? 0.0 : 5.84962487220764160156e-01;
            double DP_L_k = (k == 0) ? 0.0 : 1.35003920212974897128e-08;
            double L1 = 5.99999999999994648725e-01;
            double L2 = 4.28571428578550184252e-01;
            double L3 = 3.33333329818377432918e-01;
            double L4 = 2.72728123808534006489e-01;
            double L5 = 2.30660745775561754067e-01;
            double L6 = 2.06975017800338417784e-01;
            u = x_abs - BP_k;
            v = 1.0 / (x_abs + BP_k);
            ss = u * v;
            s_h = ss; s_h = __LO(s_h, 0);
            t_h = 0.0; t_h = __HI(t_h, ((ix >> 1) | 0x20000000) + 0x00080000 + (k << 18));
            t_l = x_abs - (t_h - BP_k);
            s_l = v * ((u - s_h * t_h) - s_h * t_l);
            s2 = ss * ss;
            r = s2 * s2 * (L1 + s2 * (L2 + s2 * (L3 + s2 * (L4 + s2 * (L5 + s2 * L6)))));
            r += s_l * (s_h + ss);
            s2 = s_h * s_h;
            t_h = 3.0 + s2 + r; t_h = __LO(t_h, 0);
            t_l = r - ((t_h - 3.0) - s2);
            u = s_h * t_h;
            v = s_l * t_h + t_l * ss;
            p_h = u + v; p_h = __LO(p_h, 0);
            p_l = v - (p_h - u);
            z_h = CP_H * p_h;
            z_l = CP_L * p_h + p_l * CP + DP_L_k;
            t = (double) n;
            t1 = (((z_h + z_l) + DP_H_k) + t); t1 = __LO(t1, 0);
            t2 = z_l - (((t1 - t) - DP_H_k) - z_h);
        }
        double y1 = y; y1 = __LO(y1, 0);
        p_l = (y - y1) * t1 + y * t2;
        p_h = y1 * t1;
        z = p_l + p_h;
        j = __HI(z);
        i = __LO(z);
        if (j >= 0x40900000) {
            if (((j - 0x40900000) | i) != 0) return s * INFINITY;
            else { double OVT = 8.0085662595372944372e-17; if (p_l + OVT > z - p_h) return s * INFINITY; }
        } else if ((j & 0x7fffffff) >= 0x4090cc00) {
            if (((j - 0xc090cc00) | i) != 0) return s * 0.0;
            else { if (p_l <= z - p_h) return s * 0.0; }
        }
        double P1 = 1.66666666666666019037e-01;
        double P2 = -2.77777777770155933842e-03;
        double P3 = 6.61375632143793436117e-05;
        double P4 = -1.65339022054652515390e-06;
        double P5 = 4.13813679705723846039e-08;
        double LG2 = 6.93147180559945286227e-01;
        double LG2_H = 6.93147182464599609375e-01;
        double LG2_L = -1.90465429995776804525e-09;
        i = j & 0x7fffffff;
        k = (i >> 20) - 0x3ff;
        n = 0;
        if (i > 0x3fe00000) {
            n = j + (0x00100000 >> (k + 1));
            k = ((n & 0x7fffffff) >> 20) - 0x3ff;
            t = 0.0; t = __HI(t, (n & ~(0x000fffff >> k)));
            n = ((n & 0x000fffff) | 0x00100000) >> (20 - k);
            if (j < 0) n = -n;
            p_h -= t;
        }
        t = p_l + p_h; t = __LO(t, 0);
        u = t * LG2_H;
        v = (p_l - (t - p_h)) * LG2 + t * LG2_L;
        z = u + v;
        w = v - (z - u);
        t = z * z;
        t1 = z - t * (P1 + t * (P2 + t * (P3 + t * (P4 + t * P5))));
        r = (z * t1) / (t1 - 2.0) - (w + z * w);
        z = 1.0 - (r - z);
        j = __HI(z);
        j += (n << 20);
        if ((j >> 20) <= 0) z = scalb(z, n);
        else { int z_hi = __HI(z); z_hi += (n << 20); z = __HI(z, z_hi); }
        return s * z;
    }
    // unsigned 32-bit less-than (JLS 1.0 has no Integer.compareUnsigned).
    private static boolean ltu(int a, int b) { return (a ^ 0x80000000) < (b ^ 0x80000000); }

    // §15.17.3 float remainder: the compiler lowers `float % float` here (WASM has no f32.rem).
    // Computing through the double fmod is EXACT, not an approximation: the true remainder
    // r = x - y*trunc(x/y) of two binary32 values is itself exactly representable in binary32
    // (|r| < |y|, and r is computed without rounding), and float->double is exact both ways for
    // such a value. NaN/infinity/zero operands widen and narrow unchanged, so §15.17.3's special
    // cases carry through untouched.
    private static float fmod(float x, float y) {
        return (float) fmod((double) x, (double) y);
    }

    // fdlibm e_fmod (__ieee754_fmod) — fixed-point; C's `unsigned` low words → Java int with >>> / ltu.
    private static double fmod(double x, double y) {
        double one = 1.0;
        double[] Zero = {0.0, -0.0};
        int n, hx, hy, hz, ix, iy, sx, i, lx, ly, lz;
        hx = __HI(x); lx = __LO(x); hy = __HI(y); ly = __LO(y);
        sx = hx & 0x80000000; hx ^= sx; hy &= 0x7fffffff;
        if ((hy | ly) == 0 || (hx >= 0x7ff00000) || ((hy | ((ly | -ly) >>> 31)) > 0x7ff00000))
            return (x * y) / (x * y);
        if (hx <= hy) {
            if ((hx < hy) || ltu(lx, ly)) return x;
            if (lx == ly) return Zero[sx >>> 31];
        }
        if (hx < 0x00100000) {
            if (hx == 0) { for (ix = -1043, i = lx; i > 0; i <<= 1) ix -= 1; }
            else { for (ix = -1022, i = (hx << 11); i > 0; i <<= 1) ix -= 1; }
        } else ix = (hx >> 20) - 1023;
        if (hy < 0x00100000) {
            if (hy == 0) { for (iy = -1043, i = ly; i > 0; i <<= 1) iy -= 1; }
            else { for (iy = -1022, i = (hy << 11); i > 0; i <<= 1) iy -= 1; }
        } else iy = (hy >> 20) - 1023;
        if (ix >= -1022) hx = 0x00100000 | (0x000fffff & hx);
        else { n = -1022 - ix; if (n <= 31) { hx = (hx << n) | (lx >>> (32 - n)); lx <<= n; } else { hx = lx << (n - 32); lx = 0; } }
        if (iy >= -1022) hy = 0x00100000 | (0x000fffff & hy);
        else { n = -1022 - iy; if (n <= 31) { hy = (hy << n) | (ly >>> (32 - n)); ly <<= n; } else { hy = ly << (n - 32); ly = 0; } }
        n = ix - iy;
        while (n-- != 0) {
            hz = hx - hy; lz = lx - ly; if (ltu(lx, ly)) hz -= 1;
            if (hz < 0) { hx = hx + hx + (lx >>> 31); lx = lx + lx; }
            else { if ((hz | lz) == 0) return Zero[sx >>> 31]; hx = hz + hz + (lz >>> 31); lx = lz + lz; }
        }
        hz = hx - hy; lz = lx - ly; if (ltu(lx, ly)) hz -= 1;
        if (hz >= 0) { hx = hz; lx = lz; }
        if ((hx | lx) == 0) return Zero[sx >>> 31];
        while (hx < 0x00100000) { hx = hx + hx + (lx >>> 31); lx = lx + lx; iy -= 1; }
        if (iy >= -1022) { hx = ((hx - 0x00100000) | ((iy + 1023) << 20)); x = __HI(x, hx | sx); x = __LO(x, lx); }
        else {
            n = -1022 - iy;
            if (n <= 20) { lx = (lx >>> n) | (hx << (32 - n)); hx >>= n; }
            else if (n <= 31) { lx = (hx << (32 - n)) | (lx >>> n); hx = sx; }
            else { lx = hx >> (n - 32); hx = sx; }
            x = __HI(x, hx | sx); x = __LO(x, lx);
            x *= one;
        }
        return x;
    }

    // §20.11 IEEEremainder(x,p) — fdlibm e_remainder (uses fmod).
    public static double IEEEremainder(double x, double p) {
        double zero = 0.0;
        int hx, hp, sx, lx, lp;
        double p_half;
        hx = __HI(x); lx = __LO(x); hp = __HI(p); lp = __LO(p);
        sx = hx & 0x80000000; hp &= 0x7fffffff; hx &= 0x7fffffff;
        if ((hp | lp) == 0) return (x * p) / (x * p);                     // p = 0
        if ((hx >= 0x7ff00000) || ((hp >= 0x7ff00000) && (((hp - 0x7ff00000) | lp) != 0))) return (x * p) / (x * p);
        if (hp <= 0x7fdfffff) x = fmod(x, p + p);                         // now x < 2p
        if (((hx - hp) | (lx - lp)) == 0) return zero * x;
        x = abs(x); p = abs(p);
        if (hp < 0x00200000) { if (x + x > p) { x -= p; if (x + x >= p) x -= p; } }
        else { p_half = 0.5 * p; if (x > p_half) { x -= p; if (x >= p_half) x -= p; } }
        x = __HI(x, __HI(x) ^ sx);
        return x;
    }
    public static native double ceil(double a);
    public static native double floor(double a);
    public static native double rint(double a);
    public static int round(float a)   { return (int)  floor((double) a + 0.5d); }   // §20.11.20
    public static long round(double a) { return (long) floor(a + 0.5d); }            // §20.11.21
    // §20.11.20: on the first call, create a single generator exactly as by `new java.util.Random()`
    // (which seeds itself from the clock) and use it thereafter for all calls, and nowhere else.
    private static java.util.Random randomNumberGenerator;
    public static double random() {
        if (randomNumberGenerator == null) randomNumberGenerator = new java.util.Random();
        return randomNumberGenerator.nextDouble();
    }
    // §20.11 abs: |MIN_VALUE| overflows back to MIN_VALUE for int/long (spec).
    // The float/double `(a <= 0) ? 0 - a : a` form is exact for ±0.0, ±inf, and NaN.
    public static int abs(int a)       { return (a < 0) ? -a : a; }
    public static long abs(long a)     { return (a < 0L) ? -a : a; }
    public static float abs(float a)   { return (a <= 0.0f) ? 0.0f - a : a; }
    public static double abs(double a) { return (a <= 0.0) ? 0.0 - a : a; }
    public static int min(int a, int b)    { return (a <= b) ? a : b; }
    public static long min(long a, long b) { return (a <= b) ? a : b; }
    public static int max(int a, int b)    { return (a >= b) ? a : b; }
    public static long max(long a, long b) { return (a >= b) ? a : b; }
    // §20.11 float/double min/max — the JDK algorithm: NaN propagates; the -0.0/+0.0 tie breaks by
    // raw sign bit (Float/Double.*RawBits are wasm reinterpret intrinsics, not host calls).
    public static float min(float a, float b) {
        if (a != a) return a;                                              // a is NaN
        if (a == 0.0f && b == 0.0f && Float.floatToRawIntBits(b) == 0x80000000) return b;   // b is -0.0
        return (a <= b) ? a : b;
    }
    public static double min(double a, double b) {
        if (a != a) return a;
        if (a == 0.0d && b == 0.0d && Double.doubleToRawLongBits(b) == 0x8000000000000000L) return b;
        return (a <= b) ? a : b;
    }
    public static float max(float a, float b) {
        if (a != a) return a;
        if (a == 0.0f && b == 0.0f && Float.floatToRawIntBits(a) == 0x80000000) return b;   // a is -0.0 → +0.0 side
        return (a >= b) ? a : b;
    }
    public static double max(double a, double b) {
        if (a != a) return a;
        if (a == 0.0d && b == 0.0d && Double.doubleToRawLongBits(a) == 0x8000000000000000L) return b;
        return (a >= b) ? a : b;
    }
}
