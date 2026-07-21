public class V {
    public static void main(String[] args) {
        Shape[] s = new Shape[3];
        s[0] = new Sq(3); s[1] = new Ci(4); s[2] = new Tr(5);
        int h = 0;
        for (int i = 0; i < 9; i++) h += s[i % 3].area(i);
        System.out.println(h);
    }
}
abstract class Shape { abstract int area(int x); }
class Sq extends Shape { int s; Sq(int s){this.s=s;} int area(int x){ return s*s + x; } }
class Ci extends Shape { int r; Ci(int r){this.r=r;} int area(int x){ return 3*r*r - x; } }
class Tr extends Shape { int b; Tr(int b){this.b=b;} int area(int x){ return (b*x) >> 1; } }
